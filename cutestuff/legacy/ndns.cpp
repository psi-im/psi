/*
 * ndns.cpp - native DNS resolution
 * Copyright (C) 2001, 2002  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

//! \class NDns ndns.h
//! \brief Simple DNS resolution using native system calls
//!
//! This class is to be used when Qt's QDns is not good enough.  Because QDns
//! does not use threads, it cannot make a system call asyncronously.  Thus,
//! QDns tries to imitate the behavior of each platform's native behavior, and
//! generally falls short.
//!
//! NDns uses a thread to make the system call happen in the background.  This
//! gives your program native DNS behavior, at the cost of requiring threads
//! to build.
//!
//! \code
//! #include "ndns.h"
//!
//! ...
//!
//! NDns dns;
//! dns.resolve("psi.affinix.com");
//!
//! // The class will emit the resultsReady() signal when the resolution
//! // is finished. You may then retrieve the results:
//!
//! uint ip_address = dns.result();
//!
//! // or if you want to get the IP address as a string:
//!
//! QString ip_address = dns.resultString();
//! \endcode

#include "ndns.h"

#include <qapplication.h>
#include <q3socketdevice.h>
#include <q3ptrlist.h>
#include <qeventloop.h>
//Added by qt3to4:
#include <QCustomEvent>
#include <QEvent>
#include <Q3CString>

#ifdef Q_OS_UNIX
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifdef Q_OS_WIN32
#include <windows.h>
#endif

// CS_NAMESPACE_BEGIN

//! \if _hide_doc_
class NDnsWorkerEvent : public QCustomEvent
{
public:
	enum Type { WorkerEvent = QEvent::User + 100 };
	NDnsWorkerEvent(NDnsWorker *);

	NDnsWorker *worker;
};

class NDnsWorker : public QThread
{
public:
	NDnsWorker(QObject *, const Q3CString &);

	bool success;
	bool cancelled;
	QHostAddress addr;

protected:
	void run();

private:
	Q3CString host;
	QObject *par;
};
//! \endif

//----------------------------------------------------------------------------
// NDnsManager
//----------------------------------------------------------------------------
#ifndef HAVE_GETHOSTBYNAME_R
static QMutex *workerMutex = 0;
static QMutex *workerCancelled = 0;
#endif
static NDnsManager *man = 0;
bool winsock_init = false;

class NDnsManager::Item
{
public:
	NDns *ndns;
	NDnsWorker *worker;
};

class NDnsManager::Private
{
public:
	Item *find(const NDns *n)
	{
		Q3PtrListIterator<Item> it(list);
		for(Item *i; (i = it.current()); ++it) {
			if(i->ndns == n)
				return i;
		}
		return 0;
	}

	Item *find(const NDnsWorker *w)
	{
		Q3PtrListIterator<Item> it(list);
		for(Item *i; (i = it.current()); ++it) {
			if(i->worker == w)
				return i;
		}
		return 0;
	}

	Q3PtrList<Item> list;
};

NDnsManager::NDnsManager()
{
#ifndef HAVE_GETHOSTBYNAME_R
	workerMutex = new QMutex;
	workerCancelled = new QMutex;
#endif

#ifdef Q_OS_WIN32
	if(!winsock_init) {
		winsock_init = true;
		Q3SocketDevice *sd = new Q3SocketDevice;
		delete sd;
	}
#endif

	d = new Private;
	d->list.setAutoDelete(true);

	connect(qApp, SIGNAL(aboutToQuit()), SLOT(app_aboutToQuit()));
}

NDnsManager::~NDnsManager()
{
	delete d;

#ifndef HAVE_GETHOSTBYNAME_R
	delete workerMutex;
	workerMutex = 0;
	delete workerCancelled;
	workerCancelled = 0;
#endif
}

void NDnsManager::resolve(NDns *self, const QString &name)
{
	Item *i = new Item;
	i->ndns = self;
	i->worker = new NDnsWorker(this, name.utf8());
	d->list.append(i);

	i->worker->start();
}

void NDnsManager::stop(NDns *self)
{
	Item *i = d->find(self);
	if(!i)
		return;
	// disassociate
	i->ndns = 0;

#ifndef HAVE_GETHOSTBYNAME_R
	// cancel
	workerCancelled->lock();
	i->worker->cancelled = true;
	workerCancelled->unlock();
#endif
}

bool NDnsManager::isBusy(const NDns *self) const
{
	Item *i = d->find(self);
	return (i ? true: false);
}

bool NDnsManager::event(QEvent *e)
{
	if((int)e->type() == (int)NDnsWorkerEvent::WorkerEvent) {
		NDnsWorkerEvent *we = static_cast<NDnsWorkerEvent*>(e);
		we->worker->wait(); // ensure that the thread is terminated

		Item *i = d->find(we->worker);
		if(!i) {
			// should NOT happen
			return true;
		}
		QHostAddress addr = i->worker->addr;
		NDns *ndns = i->ndns;
		delete i->worker;
		d->list.removeRef(i);

		// nuke manager if no longer needed (code that follows MUST BE SAFE!)
		tryDestroy();

		// requestor still around?
		if(ndns)
			ndns->finished(addr);
		return true;
	}
	return false;
}

void NDnsManager::tryDestroy()
{
	if(d->list.isEmpty()) {
		man = 0;
		delete this;
	}
}

void NDnsManager::app_aboutToQuit()
{
	while(man) {
		qApp->processEvents(QEventLoop::WaitForMoreEvents);
	}
}


//----------------------------------------------------------------------------
// NDns
//----------------------------------------------------------------------------

//! \fn void NDns::resultsReady()
//! This signal is emitted when the DNS resolution succeeds or fails.

//!
//! Constructs an NDns object with parent \a parent.
NDns::NDns(QObject *parent)
:QObject(parent)
{
}

//!
//! Destroys the object and frees allocated resources.
NDns::~NDns()
{
	stop();
}

//!
//! Resolves hostname \a host (eg. psi.affinix.com)
void NDns::resolve(const QString &host)
{
	stop();
	if(!man)
		man = new NDnsManager;
	man->resolve(this, host);
}

//!
//! Cancels the lookup action.
//! \note This will not stop the underlying system call, which must finish before the next lookup will proceed.
void NDns::stop()
{
	if(man)
		man->stop(this);
}

//!
//! Returns the IP address as a 32-bit integer in host-byte-order.  This will be 0 if the lookup failed.
//! \sa resultsReady()
uint NDns::result() const
{
	return addr.ip4Addr();
}

//!
//! Returns the IP address as a string.  This will be an empty string if the lookup failed.
//! \sa resultsReady()
QString NDns::resultString() const
{
	return addr.toString();
}

//!
//! Returns TRUE if busy resolving a hostname.
bool NDns::isBusy() const
{
	if(!man)
		return false;
	return man->isBusy(this);
}

void NDns::finished(const QHostAddress &a)
{
	addr = a;
	resultsReady();
}

//----------------------------------------------------------------------------
// NDnsWorkerEvent
//----------------------------------------------------------------------------
NDnsWorkerEvent::NDnsWorkerEvent(NDnsWorker *p)
:QCustomEvent(WorkerEvent)
{
	worker = p;
}

//----------------------------------------------------------------------------
// NDnsWorker
//----------------------------------------------------------------------------
NDnsWorker::NDnsWorker(QObject *_par, const Q3CString &_host)
{
	success = cancelled = false;
	par = _par;
	host = _host.copy(); // do we need this to avoid sharing across threads?
}

void NDnsWorker::run()
{
	hostent *h = 0;

#ifdef HAVE_GETHOSTBYNAME_R
	hostent buf;
	char char_buf[1024];
	int err;
	gethostbyname_r(host.data(), &buf, char_buf, sizeof(char_buf), &h, &err);
#else
	// lock for gethostbyname
	QMutexLocker locker(workerMutex);

	// check for cancel
	workerCancelled->lock();
	bool cancel = cancelled;
	workerCancelled->unlock();

	if(!cancel)
		h = gethostbyname(host.data());
#endif

	if(!h) {
		success = false;
		QApplication::postEvent(par, new NDnsWorkerEvent(this));
		return;
	}

	in_addr a = *((struct in_addr *)h->h_addr_list[0]);
	addr.setAddress(ntohl(a.s_addr));
	success = true;

	QApplication::postEvent(par, new NDnsWorkerEvent(this));
}

// CS_NAMESPACE_END
