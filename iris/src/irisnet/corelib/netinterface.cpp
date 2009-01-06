/*
 * Copyright (C) 2006  Justin Karneges
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 */

#include "netinterface.h"

#include "irisnetplugin.h"
#include "irisnetglobal_p.h"

namespace XMPP {

//----------------------------------------------------------------------------
// NetTracker
//----------------------------------------------------------------------------
class NetTracker : public QObject
{
	Q_OBJECT
public:
	static NetTracker *self;

	NetInterfaceProvider *c;
	QList<NetInterfaceProvider::Info> info;
	QMutex m;

	NetTracker()
	{
		self = this;
		QList<IrisNetProvider*> list = irisNetProviders();
		c = 0;
		for(int n = 0; n < list.count(); ++n)
		{
			IrisNetProvider *p = list[n];
			c = p->createNetInterfaceProvider();
			if(c)
				break;
		}
		Q_ASSERT(c); // we have built-in support, so this should never fail
		connect(c, SIGNAL(updated()), SLOT(c_updated()));
	}

	~NetTracker()
	{
		delete c;
		self = 0;
	}

	static NetTracker *instance()
	{
		return self;
	}

	void start()
	{
		c->start();
		info = filterList(c->interfaces());
	}

	QList<NetInterfaceProvider::Info> getInterfaces()
	{
		QMutexLocker locker(&m);
		return info;
	}

signals:
	void updated();

public slots:
	void c_updated()
	{
		{
			QMutexLocker locker(&m);
			info = filterList(c->interfaces());
		}
		emit updated();
	}

private:
	static QList<NetInterfaceProvider::Info> filterList(const QList<NetInterfaceProvider::Info> &in)
	{
		QList<NetInterfaceProvider::Info> out;
		for(int n = 0; n < in.count(); ++n)
		{
			if(!in[n].isLoopback)
				out += in[n];
		}
		return out;
	}
};

NetTracker *NetTracker::self = 0;

class SyncThread : public QThread
{
	Q_OBJECT
public:
	QMutex control_mutex;
	QWaitCondition control_wait;
	QEventLoop *loop;

	SyncThread(QObject *parent = 0) : QThread(parent)
	{
		loop = 0;
	}

	~SyncThread()
	{
		stop();
	}

	void start()
	{
		control_mutex.lock();
		QThread::start();
		control_wait.wait(&control_mutex);
		control_mutex.unlock();
	}

	void stop()
	{
		{
			QMutexLocker locker(&control_mutex);
			if(loop)
				QMetaObject::invokeMethod(loop, "quit");
		}
		wait();
	}

	virtual void run()
	{
		control_mutex.lock();
		loop = new QEventLoop;
		begin();
		control_wait.wakeOne();
		control_mutex.unlock();
		loop->exec();
		QMutexLocker locker(&control_mutex);
		end();
		delete loop;
		loop = 0;
	}

	virtual void begin()
	{
	}

	virtual void end()
	{
	}
};

class NetThread : public SyncThread
{
	Q_OBJECT
public:
	NetTracker *tracker;

	NetThread(QObject *parent = 0) : SyncThread(parent)
	{
	}

	~NetThread()
	{
		stop();
	}

	virtual void begin()
	{
		tracker = new NetTracker;
		tracker->start();
	}

	virtual void end()
	{
		delete tracker;
	}
};

//----------------------------------------------------------------------------
// NetInterface
//----------------------------------------------------------------------------
class NetInterfacePrivate : public QObject
{
	Q_OBJECT
public:
	friend class NetInterfaceManagerPrivate;

	NetInterface *q;

	NetInterfaceManager *man;
	bool valid;
	QString id, name;
	QList<QHostAddress> addrs;
	QHostAddress gw;

	NetInterfacePrivate(NetInterface *_q) : QObject(_q), q(_q)
	{
		valid = false;
	}

	void doUnavailable()
	{
		man->unreg(q);
		valid = false;
		emit q->unavailable();
	}
};

NetInterface::NetInterface(const QString &id, NetInterfaceManager *manager)
:QObject(manager)
{
	d = new NetInterfacePrivate(this);
	d->man = manager;

	NetInterfaceProvider::Info *info = (NetInterfaceProvider::Info *)d->man->reg(id, this);
	if(info)
	{
		d->valid = true;
		d->id = info->id;
		d->name = info->name;
		d->addrs = info->addresses;
		d->gw = info->gateway;
		delete info;
	}
}

NetInterface::~NetInterface()
{
	d->man->unreg(this);
	delete d;
}

bool NetInterface::isValid() const
{
	return d->valid;
}

QString NetInterface::id() const
{
	return d->id;
}

QString NetInterface::name() const
{
	return d->name;
}

QList<QHostAddress> NetInterface::addresses() const
{
	return d->addrs;
}

QHostAddress NetInterface::gateway() const
{
	return d->gw;
}

//----------------------------------------------------------------------------
// NetInterfaceManager
//----------------------------------------------------------------------------
class NetInterfaceManagerGlobal;

Q_GLOBAL_STATIC(QMutex, nim_mutex)
static NetInterfaceManagerGlobal *g_nim = 0;

class NetInterfaceManagerGlobal
{
public:
	NetThread *thread;
	int refs;

	NetInterfaceManagerGlobal()
	{
		thread = 0;
		refs = 0;
	}

	~NetInterfaceManagerGlobal()
	{
	}

	// global mutex must be locked while calling this
	void addRef()
	{
		if(refs == 0)
		{
			thread = new NetThread;
			thread->moveToThread(QCoreApplication::instance()->thread());
			thread->start();
		}
		++refs;
	}

	// global mutex must be locked while calling this
	void removeRef()
	{
		Q_ASSERT(refs > 0);
		--refs;
		if(refs == 0)
		{
			delete thread;
			thread = 0;
		}
	}
};

class NetInterfaceManagerPrivate : public QObject
{
	Q_OBJECT
public:
	NetInterfaceManager *q;

	QMutex m;
	QList<NetInterfaceProvider::Info> info;
	QList<NetInterface*> listeners;
	bool pending;

	NetInterfaceManagerPrivate(NetInterfaceManager *_q) : QObject(_q), q(_q)
	{
		pending = false;
	}

	static int lookup(const QList<NetInterfaceProvider::Info> &list, const QString &id)
	{
		for(int n = 0; n < list.count(); ++n)
		{
			if(list[n].id == id)
				return n;
		}
		return -1;
	}

	static bool sameContent(const NetInterfaceProvider::Info &a, const NetInterfaceProvider::Info &b)
	{
		// assume ids are the same already
		if(a.name == b.name && a.isLoopback == b.isLoopback && a.addresses == b.addresses && a.gateway == b.gateway)
			return true;
		return false;
	}

	void do_update()
	{
		// grab the latest info
		QList<NetInterfaceProvider::Info> newinfo = NetTracker::instance()->getInterfaces();

		QStringList here_ids, gone_ids;

		// removed / changed
		for(int n = 0; n < info.count(); ++n)
		{
			int i = lookup(newinfo, info[n].id);
			// id is still here
			if(i != -1)
			{
				// content changed?
				if(!sameContent(info[n], newinfo[i]))
				{
					gone_ids += info[n].id;
					here_ids += info[n].id;
				}
			}
			// id is gone
			else
				gone_ids += info[n].id;
		}

		// added
		for(int n = 0; n < newinfo.count(); ++n)
		{
			int i = lookup(info, newinfo[n].id);
			if(i == -1)
				here_ids += newinfo[n].id;
		}
		info = newinfo;

		// announce gone
		for(int n = 0; n < gone_ids.count(); ++n)
		{
			// work on a copy, just in case the list changes.
			//   it is important to make the copy here, and not
			//   outside the outer loop, in case the items
			//   get deleted
			QList<NetInterface*> list = listeners;
			for(int i = 0; i < list.count(); ++i)
			{
				if(list[i]->d->id == gone_ids[n])
					list[i]->d->doUnavailable();
			}
		}

		// announce here
		for(int n = 0; n < here_ids.count(); ++n)
			emit q->interfaceAvailable(here_ids[n]);
	}

public slots:
	void tracker_updated()
	{
		QMutexLocker locker(&m);
		if(!pending)
		{
			QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
			pending = true;
		}
	}

	void update()
	{
		m.lock();
		pending = false;
		m.unlock();

		do_update();
	}
};

NetInterfaceManager::NetInterfaceManager(QObject *parent)
:QObject(parent)
{
	QMutexLocker locker(nim_mutex());
	if(!g_nim)
		g_nim = new NetInterfaceManagerGlobal;

	d = new NetInterfaceManagerPrivate(this);
	g_nim->addRef();
	d->connect(NetTracker::instance(), SIGNAL(updated()), SLOT(tracker_updated()), Qt::DirectConnection);
}

NetInterfaceManager::~NetInterfaceManager()
{
	QMutexLocker locker(nim_mutex());
	g_nim->removeRef();
	delete d;
	if(g_nim->refs == 0)
	{
		delete g_nim;
		g_nim = 0;
	}
}

QStringList NetInterfaceManager::interfaces() const
{
	d->info = NetTracker::instance()->getInterfaces();
	QStringList out;
	for(int n = 0; n < d->info.count(); ++n)
		out += d->info[n].id;
	return out;
}

QString NetInterfaceManager::interfaceForAddress(const QHostAddress &a)
{
	NetInterfaceManager netman;
	QStringList list = netman.interfaces();
	for(int n = 0; n < list.count(); ++n)
	{
		NetInterface iface(list[n], &netman);
		if(iface.addresses().contains(a))
			return list[n];
	}
	return QString();
}

void *NetInterfaceManager::reg(const QString &id, NetInterface *i)
{
	for(int n = 0; n < d->info.count(); ++n)
	{
		if(d->info[n].id == id)
		{
			d->listeners += i;
			return new NetInterfaceProvider::Info(d->info[n]);
		}
	}
	return 0;
}

void NetInterfaceManager::unreg(NetInterface *i)
{
	d->listeners.removeAll(i);
}

}

#include "netinterface.moc"
