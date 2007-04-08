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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "dirwatch_p.h"

#include <QtCore>
#include <windows.h>

namespace QCA {

class DWThread : public QThread
{
	Q_OBJECT
public:
	HANDLE h;

	DWThread(HANDLE _h, QObject *parent = 0) : QThread(parent)
	{
		h = _h;
	}

signals:
	void notify();

protected:
	void run()
	{
		while(WaitForSingleObject(h, INFINITE) == WAIT_OBJECT_0)
		{
			// send a changed event
			emit notify();
			if(!FindNextChangeNotification(h))
				return; // will emit finished()
		}
	}
};

class DWEntry : public QObject
{
	Q_OBJECT
public:
	QString dir;
	QList<int> idList;
	bool dirty;

	HANDLE h;
	DWThread *thread;

	static DWEntry *create(const QString &s)
	{
		HANDLE h = FindFirstChangeNotificationA(QFile::encodeName(QDir::convertSeparators(s)), TRUE,
			FILE_NOTIFY_CHANGE_FILE_NAME |
			FILE_NOTIFY_CHANGE_DIR_NAME |
			FILE_NOTIFY_CHANGE_ATTRIBUTES |
			FILE_NOTIFY_CHANGE_SIZE |
			FILE_NOTIFY_CHANGE_LAST_WRITE |
			FILE_NOTIFY_CHANGE_SECURITY);

		if(h == INVALID_HANDLE_VALUE)
			return 0;

		DWEntry *e = new DWEntry;
		e->dir = s;
		e->h = h;
		e->thread = new DWThread(h, e);
		QObject::connect(e->thread, SIGNAL(notify()), e, SLOT(thread_notify()));
		QObject::connect(e->thread, SIGNAL(finished()), e, SLOT(thread_finished()));
		e->thread->start();
		return e;
	}

	DWEntry()
	{
		dirty = false;
	}

	~DWEntry()
	{
		if(thread)
		{
			FindCloseChangeNotification(h);
			thread->disconnect(this);
			thread->wait();
			delete thread;
		}
	}

signals:
	void changed();

public slots:
	void thread_notify()
	{
		dirty = true;
		emit changed();
	}

	void thread_finished()
	{
		thread->disconnect(this);
		thread->deleteLater();
		thread = 0;
	}
};

class DirWatchPlatform::Private : public QObject
{
	Q_OBJECT
public:
	DirWatchPlatform *q;
	QTimer t;
	QList<DWEntry*> list;

	Private(DirWatchPlatform *_q) : QObject(_q), q(_q), t(this)
	{
		t.setSingleShot(true);
		connect(&t, SIGNAL(timeout()), this, SLOT(slotNotify()));
	}

	~Private()
	{
		qDeleteAll(list);
		list.clear();
	}

	int addItem(const QString &s)
	{
		DWEntry *e = findEntryByDir(s);
		if(!e)
		{
			e = DWEntry::create(s);
			if(!e)
				return -1;
			connect(e, SIGNAL(changed()), SLOT(slotChanged()));
			list.append(e);
		}
		int id = getUniqueId();
		e->idList.append(id);
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
			foreach(int idi, e->idList)
			{
				if(idi == id)
					return n;
			}
		}
		return -1;
	}

private slots:
	void slotChanged()
	{
		// use a timer to combine multiple changed events into one
		if(!t.isActive())
			t.start(200);
	}

	void slotNotify()
	{
		// see who is dirty
		foreach(DWEntry *e, list)
		{
			if(e->dirty)
			{
				e->dirty = false;
				foreach(int id, e->idList)
					emit q->dirChanged(id);
			}
		}
	}
};

DirWatchPlatform::DirWatchPlatform(QObject *parent)
:QObject(parent)
{
	d = new Private(this);
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
	return true;
}

int DirWatchPlatform::addDir(const QString &s)
{
	return d->addItem(s);
}

void DirWatchPlatform::removeDir(int id)
{
	d->removeItem(id);
}

}

#include "dirwatch_win.moc"
