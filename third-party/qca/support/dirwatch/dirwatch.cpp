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

namespace QCA {

static DirWatchPlatform *platform = 0;
static int platform_ref = 0;

//----------------------------------------------------------------------------
// DirWatch
//----------------------------------------------------------------------------
struct file_info
{
	QString name;
	qint64 size;
	QDateTime lastModified;
};

typedef QList<file_info> DirInfo;

static DirInfo getDirInfo(const QString &dir)
{
	DirInfo info;
	QDir d(dir);
	if(!d.exists())
		return info;
	QStringList el = d.entryList();
	for(QStringList::ConstIterator it = el.begin(); it != el.end(); ++it)
	{
		QFileInfo fi(d.filePath(*it));
		file_info i;
		i.name = fi.fileName();
		i.size = fi.size();
		i.lastModified = fi.lastModified();
		info += i;
	}
	return info;
}

class DirWatch::Private : public QObject
{
	Q_OBJECT
public:
	DirWatch *q;
	QString dir;

	// for non-platform watching
	DirInfo info;
	QTimer checkTimer;
	int freq;
	int id;

	Private(DirWatch *_q) : QObject(_q), q(_q), checkTimer(this)
	{
		freq = 5000;
		id = -1;

		// try to use platform
		if(!platform)
		{
			DirWatchPlatform *p = new DirWatchPlatform;
			if(p->init())
				platform = p;
			else
				delete p;
		}
		if(platform)
		{
			++platform_ref;
			connect(platform, SIGNAL(dirChanged(int)), SLOT(dp_dirChanged(int)));
		}
		else
		{
			connect(&checkTimer, SIGNAL(timeout()), SLOT(doCheck()));
		}
	}

	~Private()
	{
		if(platform)
		{
			--platform_ref;
			if(platform_ref == 0)
			{
				delete platform;
				platform = 0;
			}
		}
	}

	void setDir(const QString &s)
	{
		if(dir == s)
			return;

		if(!dir.isEmpty())
		{
			if(platform)
			{
				if(id != -1)
					platform->removeDir(id);
			}
			else
				checkTimer.stop();

			dir = QString();
		}

		if(s.isEmpty())
			return;

		dir = s;

		if(platform)
		{
			id = platform->addDir(dir);
		}
		else
		{
			info = getDirInfo(dir);
			checkTimer.start(freq);
		}
	}

public slots:
	void dp_dirChanged(int id)
	{
		if(this->id != id)
			return;

		emit q->changed();
	}

	void doCheck()
	{
		DirInfo newInfo = getDirInfo(dir);

		bool info_changed = false;
		QDir dir(dir);
		file_info ni;

		// look for files that are missing or modified
		for(DirInfo::ConstIterator it = info.begin(); it != info.end(); ++it)
		{
			const file_info &i = *it;

			bool found = false;
			for(DirInfo::ConstIterator nit = newInfo.begin(); nit != newInfo.end(); ++nit)
			{
				const file_info &tmp = *nit;
				if(i.name == tmp.name)
				{
					found = true;
					ni = tmp;
					break;
				}
			}
			if(!found)
			{
				//printf("%s: not found\n", i.name.latin1());
				info_changed = true;
				break;
			}

			if(i.lastModified != ni.lastModified || i.size != ni.size)
			{
				//printf("%s: modified (is: %s, was: %s)\n", i.name.latin1(), ni.lastModified.toString().latin1(), i.lastModified.toString().latin1());
				info_changed = true;
				break;
			}
			//printf("%s: no change\n", i.name.latin1());
		}

		// look for files that have been added
		for(DirInfo::ConstIterator nit = newInfo.begin(); nit != newInfo.end(); ++nit)
		{
			const file_info &i = *nit;
			bool found = false;
			for(DirInfo::ConstIterator it = info.begin(); it != info.end(); ++it)
			{
				const file_info &tmp = *it;
				if(i.name == tmp.name)
				{
					found = true;
					break;
				}
			}
			if(!found)
			{
				info_changed = true;
				break;
			}
		}

		info = newInfo;

		if(info_changed)
			emit q->changed();
	}
};

DirWatch::DirWatch(const QString &dir, QObject *parent)
:QObject(parent)
{
	d = new Private(this);
	if(!dir.isEmpty())
		setDirName(dir);
}

DirWatch::~DirWatch()
{
	setDirName(QString());
	delete d;
}

QString DirWatch::dirName() const
{
	return d->dir;
}

void DirWatch::setDirName(const QString &s)
{
	d->setDir(s);
}

bool DirWatch::platformSupported()
{
	return DirWatchPlatform::isSupported();
}

//----------------------------------------------------------------------------
// FileWatch
//----------------------------------------------------------------------------
class FileWatch::Private : public QObject
{
	Q_OBJECT
public:
	FileWatch *q;
	QString fname;
	DirWatch *dw;
	qint64 lastSize;
	QDateTime lastModified;

	Private(FileWatch *_q) : QObject(_q), q(_q)
	{
		dw = 0;
	}

	~Private()
	{
		delete dw;
	}

	void setFileName(const QString &s)
	{
		if(fname == s)
			return;

		// remove old watch if necessary
		if(dw)
		{
			delete dw;
			dw = 0;
		}
		fname = s;

		if(fname.isEmpty())
			return;

		// setup the watch
		dw = new DirWatch(QString(), this);
		connect(dw, SIGNAL(changed()), SLOT(dirChanged()));

		QFileInfo fi(fname);
		if(fi.exists())
		{
			lastSize = fi.size();
			lastModified = fi.lastModified();
		}
		else
		{
			lastSize = -1;
			lastModified = QDateTime();
		}
		dw->setDirName(fi.dir().path());
	}

public slots:
	void dirChanged()
	{
		bool doChange = false;
		QFileInfo fi(fname);
		if(fi.size() != lastSize || fi.lastModified() != lastModified)
			doChange = true;
		lastSize = fi.size();
		lastModified = fi.lastModified();

		if(doChange)
			emit q->changed();
	}
};

FileWatch::FileWatch(const QString &file, QObject *parent)
:QObject(parent)
{
	d = new Private(this);
	if(!file.isEmpty())
		setFileName(file);
}

FileWatch::~FileWatch()
{
	delete d;
}

QString FileWatch::fileName() const
{
	return d->fname;
}

void FileWatch::setFileName(const QString &s)
{
	d->setFileName(s);
}

}

#include "dirwatch.moc"
