/*
 * Copyright (C) 2003-2005  Justin Karneges
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "dirwatch_p.h"

// A great many hints taken from KDE's KDirWatch system.  Thank you KDE!

#ifdef HAVE_DNOTIFY
# include <unistd.h>
# include <fcntl.h>
# include <signal.h>
# include <sys/utsname.h>
# define DNOTIFY_SIGNAL (SIGRTMIN + 8)
#endif

namespace QCA {

#ifdef HAVE_DNOTIFY
class DWEntry
{
public:
	static DWEntry *open(const QString &s)
	{
		int fd = ::open(QFile::encodeName(s).data(), O_RDONLY);
		if(fd == -1)
			return 0;

		int mask = DN_DELETE|DN_CREATE|DN_RENAME|DN_MULTISHOT|DN_MODIFY|DN_ATTRIB;
		if(fcntl(fd, F_SETSIG, DNOTIFY_SIGNAL) == -1 || fcntl(fd, F_NOTIFY, mask) == -1)
		{
			close(fd);
			return 0;
		}

		DWEntry *e = new DWEntry(s, fd);
		return e;
	}

	~DWEntry()
	{
		close(fd);
	}

	QString dir;
	int fd;
	QList<int> idList;
	bool dirty;

private:
	DWEntry(const QString &_dir, int _fd)
	{
		dir = _dir;
		fd = _fd;
		dirty = false;
	}
};
#endif

class DirWatchPlatform::Private : public QObject
{
	Q_OBJECT
public:
	static Private *self;

	DirWatchPlatform *q;
	QTimer t;

#ifdef HAVE_DNOTIFY
	QSocketNotifier *sn;
	int pipe_notify[2];
	bool ok;
	QList<DWEntry*> list;
#endif

	Private(DirWatchPlatform *_q) : QObject(_q), q(_q), t(this)
	{
		self = this;

#ifdef HAVE_DNOTIFY
		ok = false;

		// from KDE
		bool supports_dnotify = true; // not guilty until proven guilty
		struct utsname uts;
		int major, minor, patch;
		if(uname(&uts) < 0)
			supports_dnotify = false; // *shrug*
		else if(sscanf(uts.release, "%d.%d.%d", &major, &minor, &patch) != 3)
			supports_dnotify = false; // *shrug*
		else if( major * 1000000 + minor * 1000 + patch < 2004019 ) // <2.4.19
			supports_dnotify = false;

		if(!supports_dnotify)
			return;

		if(pipe(pipe_notify) == -1)
			return;

		if(fcntl(pipe_notify[0], F_SETFL, O_NONBLOCK) == -1)
			return;

		t.setSingleShot(true);
		connect(&t, SIGNAL(timeout()), this, SLOT(slotNotify()));

		sn = new QSocketNotifier(pipe_notify[0], QSocketNotifier::Read, this);
		connect(sn, SIGNAL(activated(int)), SLOT(slotActivated()));

		struct sigaction act;
		act.sa_sigaction = dnotify_handler;
		sigemptyset(&act.sa_mask);
		act.sa_flags = SA_SIGINFO;
#ifdef SA_RESTART
		act.sa_flags |= SA_RESTART;
#endif
		sigaction(DNOTIFY_SIGNAL, &act, NULL);

		ok = true;
#endif
	}

	~Private()
	{
#ifdef HAVE_DNOTIFY
		if(ok)
		{
			delete sn;
			close(pipe_notify[0]);
			close(pipe_notify[1]);
		}
		qDeleteAll(list);
		list.clear();
#endif

		self = 0;
	}

#ifdef HAVE_DNOTIFY
	static void dnotify_handler(int, siginfo_t *si, void *)
	{
		// bad news
		if(!si)
			return;

		int fd = si->si_fd;
		//printf("dnotify_handler: fd=%d\n", fd);

		DWEntry *e = self->findEntryByFd(fd);
		if(!e)
			return;
		e->dirty = true;

		// write a byte into the pipe
		char c = 1;
		write(self->pipe_notify[1], &c, 1);
	}
#endif

private slots:
	void slotActivated()
	{
#ifdef HAVE_DNOTIFY
		// clear out the pipe
		while(1)
		{
			char buf[64];
			int ret = read(pipe_notify[0], buf, 64);
			if(ret < 64)
				break;
		}

		// use a timer to combine multiple dnotify events into one
		if(!t.isActive())
			t.start(200);
#endif
	}

	void slotNotify()
	{
#ifdef HAVE_DNOTIFY
		foreach(DWEntry *e, list)
		{
			if(e->dirty)
			{
				e->dirty = false;
				for(int n = 0; n < e->idList.count(); ++n)
					emit q->dirChanged(e->idList[n]);
			}
		}
#endif
	}

public:
#ifdef HAVE_DNOTIFY
	int addItem(const QString &s)
	{
		//printf("setting up DNOTIFY for [%s]\n", s.latin1());

		DWEntry *e = findEntryByDir(s);
		if(!e)
		{
			e = DWEntry::open(s);
			if(!e)
				return -1;
			list.append(e);
		}
		int id = getUniqueId();
		e->idList.append(id);

		//printf("success (fd=%d)!\n", e->fd);
		return id;
	}

	void removeItem(int id)
	{
		int n = findEntryPosById(id);
		if(n == -1)
			return;
		DWEntry *e = list[n];
		e->idList.removeAll(id);
		if(e->idList.isEmpty())
		{
			list.removeAt(n);
			delete e;
		}
	}

	int getUniqueId()
	{
		for(int id = 0;; ++id)
		{
			bool found = false;
			foreach(DWEntry *e, list)
			{
				foreach(int idi, e->idList)
				{
					if(idi == id)
					{
						found = true;
						break;
					}
				}
				if(found)
					break;
			}
			if(!found)
				return id;
		}
	}

	DWEntry *findEntryByDir(const QString &s)
	{
		foreach(DWEntry *e, list)
		{
			if(e->dir == s)
				return e;
		}
		return 0;
	}

	int findEntryPosById(int id)
	{
		for(int n = 0; n < list.count(); ++n)
		{
			DWEntry *e = list[n];
			for(int i = 0; i < e->idList.count(); ++i)
			{
				if(e->idList[i] == id)
					return n;
			}
		}
		return -1;
	}

	DWEntry *findEntryByFd(int fd)
	{
		foreach(DWEntry *e, list)
		{
			if(e->fd == fd)
				return e;
		}
		return 0;
	}
#endif
};

DirWatchPlatform::Private *DirWatchPlatform::Private::self;

#ifdef HAVE_DNOTIFY

DirWatchPlatform::DirWatchPlatform(QObject *parent)
:QObject(parent)
{
	d = 0;
}

DirWatchPlatform::~DirWatchPlatform()
{
	delete d;
}

bool DirWatchPlatform::isSupported()
{
	return true;
}

bool DirWatchPlatform::init()
{
	Private *p = new Private(this);
	if(p->ok) {
		d = p;
		//printf("DirWatchPlatform::init() ok!\n");
		return true;
	}
	else {
		delete p;
		//printf("DirWatchPlatform::init() fail!\n");
		return false;
	}
}

int DirWatchPlatform::addDir(const QString &s)
{
	return d->addItem(s);
}

void DirWatchPlatform::removeDir(int id)
{
	d->removeItem(id);
}

#else

DirWatchPlatform::DirWatchPlatform(QObject *parent) : QObject(parent) {}
DirWatchPlatform::~DirWatchPlatform() {}
bool DirWatchPlatform::isSupported() { return false; }
bool DirWatchPlatform::init() { return false; }
int DirWatchPlatform::addDir(const QString &) { return -1; }
void DirWatchPlatform::removeDir(int) {}

#endif

}

#include "dirwatch_unix.moc"
