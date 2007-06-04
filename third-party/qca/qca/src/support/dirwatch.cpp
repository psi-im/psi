/*
 * Copyright (C) 2003-2007  Justin Karneges <justin@affinix.com>
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

#include "qca_support.h"

#include <QFileSystemWatcher>

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
	}

	d->watcher = new QFileSystemWatcher(this);
	d->watcher_relay = new QFileSystemWatcherRelay(d->watcher, this);
	connect(d->watcher_relay, SIGNAL(directoryChanged(const QString &)), SLOT(watcher_changed(const QString &)));

	d->dirName = dir;
	d->watcher->addPath(d->dirName);
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
	QString fileName;

	Private(FileWatch *_q) : QObject(_q), q(_q), watcher(0), watcher_relay(0)
	{
	}

private slots:
	void watcher_changed(const QString &path)
	{
		Q_UNUSED(path);
		emit q->changed();
	}
};

FileWatch::FileWatch(const QString &file, QObject *parent)
:QObject(parent)
{
	d = new Private(this);
	setFileName(file);
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
	if(d->watcher)
	{
		delete d->watcher;
		delete d->watcher_relay;
	}

	d->watcher = new QFileSystemWatcher(this);
	d->watcher_relay = new QFileSystemWatcherRelay(d->watcher, this);
	connect(d->watcher_relay, SIGNAL(fileChanged(const QString &)), SLOT(watcher_changed(const QString &)));

	d->fileName = file;
	d->watcher->addPath(d->fileName);
}

}

#include "dirwatch.moc"
