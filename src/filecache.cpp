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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
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

#ifdef HAVE_QT5
# define FC_META_PERSISTENT QStringLiteral("fc_persistent")
#else
# define FC_META_PERSISTENT QLatin1String("fc_persistent")
#endif

FileCacheItem::FileCacheItem(FileCache *parent, const QString &itemId,
							 const QVariantMap &metadata, const QDateTime &dt,
							 unsigned int maxAge, unsigned int size,
							 const QByteArray &data)
	: QObject(parent)
	, _id(itemId)
	, _metadata(metadata)
	, _ctime(dt)
	, _maxAge(maxAge)
	, _size(size)
	, _data(data)
	, _flags(size > 0? 0 : OnDisk) /* empty is never saved to disk. let's say it's there already */
{
	_hash = QCryptographicHash::hash(_id.toUtf8(),
									 QCryptographicHash::Sha1).toHex();
	QString ext = FileUtil::mimeToFileExt(_metadata.value(QLatin1String("type")).toString());
	_fileName = _hash + (ext.isEmpty()? "":"." + ext);
}

bool FileCacheItem::inMemory() const
{
	return _data.size() > 0;
}

void FileCacheItem::flushToDisk()
{
	if (_flags & OnDisk) {
		return;
	}

	if (_data.size()) {
		QFile f(parentCache()->cacheDir() + "/" + _fileName);
		if (f.open(QIODevice::WriteOnly)) {
			f.write(_data);
			f.close();
			_flags |= OnDisk;
		} else {
			qWarning("Can't open file %s for writing",
					 qPrintable(_fileName));
		}
	} else {
		_flags |= OnDisk;
	}
}

bool FileCacheItem::remove() const
{
	if ((_flags & OnDisk) && _size) {
		QFile f(parentCache()->cacheDir() + "/" + _fileName);
		if (!f.remove()) {
			return !f.exists();
		}
	}
	return true;
}

void FileCacheItem::unload()
{
	flushToDisk();
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
	if (!_size) {
		return QByteArray();
	}
	if (!_data.size()) {
		QFile f(parentCache()->cacheDir() + "/" + _fileName);
		if (f.open(QIODevice::ReadOnly)) {
			_data = f.readAll();
			// TODO check if filesize differs
			// TODO notify FileCache to check memory restrictions
			f.close();
		} else {
			qWarning("Can't open file %s for reading",
					 qPrintable(_fileName));
		}
	}
	return _data;
}

void FileCacheItem::setUndeletable(bool state)
{
	if (state) {
		if (_metadata.contains(FC_META_PERSISTENT)) {
			_metadata.insert(FC_META_PERSISTENT, true);
			_flags &= ~Registered; // we have to update registry eventually
		}
	} else {
		if (_metadata.remove(FC_META_PERSISTENT) > 0) {
			_flags &= ~Registered; // we have to update registry eventually
		}
	}
}

bool FileCacheItem::isDeletable() const
{
	return !(_flags & SessionUndeletable) && !_metadata.contains(FC_META_PERSISTENT);
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
			_registry->getOption(prefix + ".metadata", QVariantMap()).toMap(),
			QDateTime::fromString(_registry->getOption(prefix + ".ctime").toString(), Qt::ISODate),
			_registry->getOption(prefix + ".max-age").toInt(),
			_registry->getOption(prefix + ".size").toInt()
		);
		item->_flags |= (FileCacheItem::OnDisk | FileCacheItem::Registered);
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
		if (item->isOnDisk() && item->size() && !dir.exists(item->fileName())) {
			remove(id, false);
			continue;
		}
		// remove expired items
		if (item->isExpired() && item->isDeletable()) {
			removeItem(item, false);
		}
	}
}

FileCacheItem *FileCache::append(const QString &id, const QByteArray &data,
                                 const QVariantMap &metadata, unsigned int maxAge)
{
	FileCacheItem *item = new FileCacheItem(this, id, metadata,
	                                        QDateTime::currentDateTime(),
	                                        maxAge, data.size(), data);
	_items[id] = item;
	_pendingRegisterItems[id] = item;
	_syncTimer->start();

	return item;
}

void FileCache::remove(const QString &id, bool needSync)
{
	FileCacheItem *item = _items.value(id);
	if (item) {
		removeItem(item, needSync);
	}
}

void FileCache::removeItem(FileCacheItem *item, bool needSync)
{
	if (item->isOnDisk()) {
		_registry->removeOption("h" + item->hash(), true);
		_registryChanged = true;
	}
	item->remove();
	_items.remove(item->id());
	_pendingRegisterItems.remove(item->id());
	delete item;
	if (needSync) {
		_syncTimer->start();
	}
}

FileCacheItem *FileCache::get(const QString &id, bool reborn)
{
	FileCacheItem *item = _items.value(id);
	if (item) {
		if (!item->isExpired()) {
			if (reborn && item->maxAge() > 0u && item->created().secsTo(QDateTime::currentDateTime()) < (int)item->maxAge() / 2) {
				item->reborn();
				toRegistry(item);
			}
			return item;
		}
		remove(id);
	}
	return 0;
}

QByteArray FileCache::getData(const QString &id, bool reborn)
{
	FileCacheItem *item = get(id, reborn);
	return item ? item->data() : QByteArray();
}

bool ctimeLessThan(FileCacheItem *a, FileCacheItem *b)
{
	return a->created() < b->created();
}

void FileCache::sync()
{
	sync(false);
}

void FileCache::sync(bool finishSession)
{
	QList<FileCacheItem *> loadedItems;
	QList<FileCacheItem *> onDiskItems;
	quint32 sumMemorySize = 0;
	quint32 sumFileSize = 0;
	FileCacheItem *item;

	QHashIterator<QString, FileCacheItem*> it(_items);
	while (it.hasNext()) {
		it.next();
		item = it.value();
		item->flushToDisk(); /* even if we are going to remove it. it's quite rare to worry about */
		if (item->isExpired(finishSession)) {
			removeItem(item, false); // even if virtual method stopped removing, we don't touch this item below.
			continue;
		}
		if (item->size()) {
			if (item->inMemory()) {
				loadedItems.append(item);
				sumMemorySize += item->size();
			}
			if (item->isOnDisk()) {
				sumFileSize += item->size();
				onDiskItems.append(item);
			}
		} else if (!item->isRegistered()) { // just put to registry item without data and stop reviewing it
			toRegistry(item); // save item to registry if not yet
		}
	}

	//if (sumDataSize > _fileCacheSize || sumMemorySize > _memoryCacheSize) {

	// flush overflowed in-memory data to disk
	if (sumMemorySize > _memoryCacheSize) {
		qSort(loadedItems.begin(), loadedItems.end(), ctimeLessThan); // by create time from oldest to newest
		while (sumMemorySize > _memoryCacheSize && loadedItems.size()) {
			item = loadedItems.takeFirst();
			if (!item->isOnDisk()) { // was kept in memory only. unload() will flush to disk
				sumFileSize += item->size();
				onDiskItems.append(item);
			}
			item->unload(); // will flush data to disk if necesary
			sumMemorySize -= item->size();
			if (!item->isRegistered()) {
				toRegistry(item); // save item to registry if not yet
			}
		}
	}

	// register pending items and flush them if necessary
	foreach (FileCacheItem *item, _pendingRegisterItems.values()) {
		toRegistry(item); // FIXME do this only after we have a file on disk (or data size = 0)
		if (_syncPolicy == InstantFLush) {
			item->flushToDisk();
		}
	}

	// remove overflowed disk data
	if (sumFileSize > _fileCacheSize) {
		qSort(onDiskItems.begin(), onDiskItems.end(), ctimeLessThan);
		while (sumFileSize > _fileCacheSize && onDiskItems.size()) {
			item = onDiskItems.takeFirst();
			if (!item->isDeletable()) {
				continue;
			}
			QString id = item->id();
			unsigned int sz = item->size();
			removeItem(item, false);
			if (!_items.value(id)) { // really removed
				sumFileSize -= sz;
			}
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
	// TODO build Map/Hash instead and insert it to options
	QString prefix = QString("h") + item->hash();
	_registry->setOption(prefix + ".id", item->id());
	_registry->setOption(prefix + ".metadata", item->metadata());
	_registry->setOption(prefix + ".ctime", item->created().toString(Qt::ISODate));
	_registry->setOption(prefix + ".max-age", (int)item->maxAge());
	_registry->setOption(prefix + ".size", (int)item->size());

	item->_flags |= FileCacheItem::Registered;
	_pendingRegisterItems.remove(item->id());
	_registryChanged = true;
}
