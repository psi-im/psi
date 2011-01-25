/*
 * filecache.cpp - File storage with age and size control
 * Copyright (C) 2010 Rion
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <QTimer>
#include <QCryptographicHash>
#include <QDir>
#include <QDebug>
#include "filecache.h"
#include "optionstree.h"
#include "fileutil.h"
#include "applicationinfo.h"

FileCacheItem::FileCacheItem(FileCache *parent, const QString &itemId,
							 const QString &type, const QDateTime &dt,
							 unsigned int maxAge, unsigned int size,
							 const QByteArray &data)
	: QObject(parent)
	, _id(itemId)
	, _type(type)
	, _ctime(dt)
	, _maxAge(maxAge)
	, _size(size)
	, _data(data)
	, _synced(false)
{
	_hash = QCryptographicHash::hash(_id.toUtf8(),
									 QCryptographicHash::Sha1).toHex();
	QString ext = FileUtil::mimeToFileExt(_type);
	_fileName = _hash + (ext.isEmpty()? "":"." + ext);
}

bool FileCacheItem::inMemory() const
{
	return !_data.isNull();
}

void FileCacheItem::sync()
{
	if (!_synced && !_data.isNull()) {
		if (_data.size()) {
			QFile f(parentCache()->cacheDir() + "/" + _fileName);
			if (f.open(QIODevice::WriteOnly)) {
				f.write(_data);
				f.close();
				_synced = true;
			} else {
				qWarning("Can't open file %s for writing",
						 qPrintable(_fileName));
			}
		} else {
			_synced = true;
		}
	}
}

bool FileCacheItem::remove() const
{
	if (_synced && _size) {
		QFile f(parentCache()->cacheDir() + "/" + _fileName);
		if (!f.remove()) {
			return !f.exists();
		}
	}
	return true;
}

void FileCacheItem::unload()
{
	if (!_synced) {
		sync();
	}
	_data = QByteArray();
}

bool FileCacheItem::isExpired(bool finishSession) const
{
	return _maxAge == FileCache::Session ? finishSession :
			_maxAge == FileCache::Forever ? false :
			_ctime.addSecs(_maxAge) < QDateTime::currentDateTime();
}

QByteArray FileCacheItem::data()
{
	if (_data.isNull()) {
		if (_size) {
			QFile f(parentCache()->cacheDir() + "/" + _fileName);
			if (f.open(QIODevice::ReadOnly)) {
				_data = f.readAll();
				// TODO check if filesize differs
				f.close();
			} else {
				qWarning("Can't open file %s for reading",
						 qPrintable(_fileName));
			}
		} else {
			_data = QByteArray(""); // empty but not null
		}
	}
	return _data;
}




//------------------------------------------------------------------------------
// FileCache
//------------------------------------------------------------------------------
FileCache::FileCache(const QString &cacheDir, QObject *parent)
	: QObject(parent)
	, _cacheDir(cacheDir)
	, _memoryCacheSize(FileCache::DefaultMemoryCacheSize)
	, _fileCacheSize(FileCache::DefaultFileCacheSize)
	, _defaultMaxAge(Forever)
	, _syncPolicy(InstantFLush)
	, _registryChanged(false)
{
	_registry = new OptionsTree(this);
	_syncTimer = new QTimer(this);
	_syncTimer->setSingleShot(true);
	_syncTimer->setInterval(1000);
	connect(_syncTimer, SIGNAL(timeout()), SLOT(sync()));

	_registry->loadOptions(_cacheDir + "/cache.xml", "items",
						   ApplicationInfo::fileCacheNS());

	foreach(const QString &prefix, _registry->getChildOptionNames("", true, true)) {
		QString id = _registry->getOption(prefix + ".id").toString();
		FileCacheItem *item = new FileCacheItem(this, id,
			_registry->getOption(prefix + ".type").toString(),
			QDateTime::fromString(_registry->getOption(prefix + ".ctime").toString(), Qt::ISODate),
			_registry->getOption(prefix + ".max-age").toInt(),
			_registry->getOption(prefix + ".size").toInt()
		);
		item->setSynced(true);
		_items[id] = item;
		if (item->isExpired()) {
			remove(item->id());
		}
	}
}

FileCache::~FileCache()
{
	gc();
	sync(true);
}

void FileCache::gc()
{
	QDir dir(_cacheDir);
	foreach(const QString &id, _items.keys()) {
		FileCacheItem *item = _items.value(id);
		// remove broken cache items
		if (item->isSynced() && item->size() && !dir.exists(item->fileName())) {
			remove(id, false);
			continue;
		}
		// remove expired items
		if (item->isExpired()) {
			remove(id, false);
		}
	}
}

FileCacheItem *FileCache::append(const QString &id, const QString &type,
					   const QByteArray &data, unsigned int maxAge)
{
	FileCacheItem *item = new FileCacheItem(this, id, type,
											QDateTime::currentDateTime(),
											maxAge, data.size(), data);
	_items[id] = item;
	_pendingSyncItems[id] = item;
	_syncTimer->start();

	return item;
}

void FileCache::remove(const QString &id, bool needSync)
{
	FileCacheItem *item = _items.value(id);
	if (item) {
		if (item->isSynced()) {
			_registry->removeOption("h" + item->hash(), true);
			_registryChanged = true;
		}
		item->remove();
		_items.remove(id);
		_pendingSyncItems.remove(id);
		delete item;
		if (needSync) {
			_syncTimer->start();
		}
	}
}

FileCacheItem *FileCache::get(const QString &id)
{
	FileCacheItem *item = _items.value(id);
	if (item && !item->isExpired()) {
		if (!item->isExpired()) {
			return item;
		}
		remove(id);
	}
	return 0;
}

QByteArray FileCache::getData(const QString &id)
{
	FileCacheItem *item = get(id);
	return item ? item->data() : QByteArray();
}

bool ctimeLessThan(FileCacheItem *a, FileCacheItem *b)
{
	return a->created() < b->created();
};

void FileCache::sync()
{
	sync(false);
}

void FileCache::sync(bool finishSession)
{
	QList<FileCacheItem *> loadedItems;
	QList<FileCacheItem *> syncedItems;
	unsigned int sumMemorySize = 0;
	unsigned int sumFileSize = 0;
	FileCacheItem *item;

	foreach(const QString &id, _items.keys()) {
		item = _items.value(id);
		if (item->isExpired(finishSession)) {
			remove(id, false);
			continue;
		}
		if (item->size()) {
			if (item->inMemory()) {
				loadedItems.append(item);
				sumMemorySize += item->size();
			}
			if (item->isSynced()) {
				sumFileSize += item->size();
				syncedItems.append(item);
			}
		}
	}

	// flush overflowed in-memory data to disk
	if (sumMemorySize > _memoryCacheSize) {
		qSort(loadedItems.begin(), loadedItems.end(), ctimeLessThan);
		while (sumMemorySize > _memoryCacheSize && loadedItems.size()) {
			item = loadedItems.takeFirst();
			if (!item->isSynced()) {
				if (_pendingSyncItems.contains(item->id())) {
					toRegistry(item); // save item to registry if not yet
					_pendingSyncItems.remove(item->id());
				}
				syncedItems.append(item); // unload below will sync item
				sumFileSize += item->size();
			}
			item->unload(); // will flush data to disk if necesary
			sumMemorySize -= item->size();
		}
	}

	// register pending items and flush them if necessary
	foreach (FileCacheItem *item, _pendingSyncItems.values()) {
		toRegistry(item);
		if (_syncPolicy == InstantFLush) {
			item->sync();
		}
		_pendingSyncItems.remove(item->id());
	}

	// remove overflowed disk data
	if (sumFileSize > _fileCacheSize) {
		qSort(syncedItems.begin(), syncedItems.end(), ctimeLessThan);
		while (sumFileSize > _fileCacheSize && syncedItems.size()) {
			item = syncedItems.takeFirst();
			remove(item->id(), false);
		}
	}

	if (_registryChanged) {
		_registry->saveOptions(_cacheDir + "/cache.xml", "items",
							   ApplicationInfo::fileCacheNS(),
							   ApplicationInfo::version());
		_registryChanged = false;
	}
}

void FileCache::toRegistry(FileCacheItem *item)
{
	QString prefix = QString("h") + item->hash();
	_registry->setOption(prefix + ".id", item->id());
	_registry->setOption(prefix + ".type", item->type());
	_registry->setOption(prefix + ".ctime", item->created().toString(Qt::ISODate));
	_registry->setOption(prefix + ".max-age", (int)item->maxAge());
	_registry->setOption(prefix + ".size", (int)item->size());

	_registryChanged = true;
}
