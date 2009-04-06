/*
 * Copyright (C) 2003-2008  Justin Karneges <justin@affinix.com>
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

#include "qca_support.h"

#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QDir>
#include <QList>
#include <QDateTime>
#include "qca_safeobj.h"

namespace QCA {

// this gets us DOR-SS and SR, provided we delete the object between uses.
// we assume QFileSystemWatcher complies to DS,NE.
class QFileSystemWatcherRelay : public QObject
{
	Q_OBJECT
public:
	QFileSystemWatcher *watcher;

	QFileSystemWatcherRelay(QFileSystemWatcher *_watcher, QObject *parent = 0)
	:QObject(parent), watcher(_watcher)
	{
		connect(watcher, SIGNAL(directoryChanged(const QString &)), SIGNAL(directoryChanged(const QString &)), Qt::QueuedConnection);
		connect(watcher, SIGNAL(fileChanged(const QString &)), SIGNAL(fileChanged(const QString &)), Qt::QueuedConnection);
	}

signals:
	void directoryChanged(const QString &path);
	void fileChanged(const QString &path);
};

//----------------------------------------------------------------------------
// DirWatch
//----------------------------------------------------------------------------
class DirWatch::Private : public QObject
{
	Q_OBJECT
public:
	DirWatch *q;
	QFileSystemWatcher *watcher;
	QFileSystemWatcherRelay *watcher_relay;
	QString dirName;

	Private(DirWatch *_q) : QObject(_q), q(_q), watcher(0), watcher_relay(0)
	{
	}

private slots:
	void watcher_changed(const QString &path)
	{
		Q_UNUSED(path);
		emit q->changed();
	}
};

DirWatch::DirWatch(const QString &dir, QObject *parent)
:QObject(parent)
{
	d = new Private(this);
	setDirName(dir);
}

DirWatch::~DirWatch()
{
	delete d;
}

QString DirWatch::dirName() const
{
	return d->dirName;
}

void DirWatch::setDirName(const QString &dir)
{
	if(d->watcher)
	{
		delete d->watcher;
		delete d->watcher_relay;
		d->watcher = 0;
		d->watcher_relay = 0;
	}

	d->dirName = dir;

	if(!d->dirName.isEmpty() && QFileInfo(d->dirName).isDir())
	{
		d->watcher = new QFileSystemWatcher(this);
		d->watcher_relay = new QFileSystemWatcherRelay(d->watcher, this);
		connect(d->watcher_relay, SIGNAL(directoryChanged(const QString &)), d, SLOT(watcher_changed(const QString &)));

		d->watcher->addPath(d->dirName);
	}
}

//----------------------------------------------------------------------------
// FileWatch
//----------------------------------------------------------------------------

class FileWatch::Private : public QObject
{
	Q_OBJECT
public:
	FileWatch *q;
	QFileSystemWatcher *watcher;
	QFileSystemWatcherRelay *watcher_relay;
	QString fileName; // file (optionally w/ path) as provided by user
	QString filePath; // absolute path of file, calculated by us
	bool fileExisted;

	Private(FileWatch *_q) : QObject(_q), q(_q), watcher(0), watcher_relay(0)
	{
	}

	void start(const QString &_fileName)
	{
		fileName = _fileName;

		watcher = new QFileSystemWatcher(this);
		watcher_relay = new QFileSystemWatcherRelay(watcher, this);
		connect(watcher_relay, SIGNAL(directoryChanged(const QString &)), SLOT(dir_changed(const QString &)));
		connect(watcher_relay, SIGNAL(fileChanged(const QString &)), SLOT(file_changed(const QString &)));

		QFileInfo fi(fileName);
		fi.makeAbsolute();
		filePath = fi.filePath();
		QDir dir = fi.dir();

		// we watch both the directory and the file itself.  the
		//   reason we watch the directory is so we can detect when
		//   the file is deleted/created

		// we don't bother checking for dir existence before adding,
		//   since there isn't an atomic way to do both at once.  if
		//   it turns out that the dir doesn't exist, then the
		//   monitoring will just silently not work at all.

		watcher->addPath(dir.path());

		// similarly, we don't check for existence of the file.  if
		//   the add works, then it works.  however, unlike the dir,
		//   if the file add fails then the overall monitoring could
		//   still potentially work...

		watcher->addPath(filePath);

		// save whether or not the file exists
		fileExisted = fi.exists();

		// TODO: address race conditions and think about error
		//   reporting instead of silently failing.  probably this
		//   will require a Qt API update.
	}

	void stop()
	{
		if(watcher)
		{
			delete watcher;
			delete watcher_relay;
			watcher = 0;
			watcher_relay = 0;
		}

		fileName.clear();
		filePath.clear();
	}

private slots:
	void dir_changed(const QString &path)
	{
		Q_UNUSED(path);
		QFileInfo fi(filePath);
		bool exists = fi.exists();
		if(exists && !fileExisted)
		{
			// this means the file was created.  put a
			//   watch on it.
			fileExisted = true;
			watcher->addPath(filePath);
			emit q->changed();
		}
	}

	void file_changed(const QString &path)
	{
		Q_UNUSED(path);
		QFileInfo fi(filePath);
		if(!fi.exists())
			fileExisted = false;
		emit q->changed();
	}
};

FileWatch::FileWatch(const QString &file, QObject *parent)
:QObject(parent)
{
	d = new Private(this);
	d->start(file);
}

FileWatch::~FileWatch()
{
	delete d;
}

QString FileWatch::fileName() const
{
	return d->fileName;
}

void FileWatch::setFileName(const QString &file)
{
	d->stop();
	d->start(file);
}

}

#include "dirwatch.moc"
