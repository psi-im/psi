/*
 * filecache.h - File storage with age and size control
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

#ifndef FILECACHE_H
#define FILECACHE_H

#include <QObject>
#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QVariantMap>

class QTimer;
class OptionsTree;
class FileCache;

class FileCacheItem : public QObject
{
	Q_OBJECT
public:
	enum Flags {
		OnDisk       = 0x1,
		Registered   = 0x2,
		SessionUndeletable  = 0x4 // The item is undeletable by expiration or cache size limits during this session
		//Unloadable  = 0x8 // another good idea
	};

	FileCacheItem(FileCache *parent, const QString &itemId, const QVariantMap &metadata,
				  const QDateTime &dt, unsigned int maxAge, unsigned int size,
				  const QByteArray &data = QByteArray());

	void flushToDisk(); // put data to disk, but not to registry. don't call this directly. FileCache will care about it.
	bool remove() const; // remove file from disk but not from registry. don't call this directly.
	void unload(); // drop file to disk, deallocate memory
	inline bool inMemory() const;
	inline bool isOnDisk() const { return _flags & OnDisk; } // data is on disk and not in rgistry
	inline bool isRegistered() const { return _flags & Registered; } // data is on disk and not in rgistry
	bool isExpired(bool finishSession = false) const;
	inline FileCache *parentCache() const { return (FileCache *)parent(); }
	inline QString id() const { return _id; }
	inline QVariantMap metadata() const { return _metadata; }
	inline void setMetadata(const QVariantMap &md) { _metadata = md; _flags &= ~Registered; } // we have to update registry eventually
	inline QDateTime created() const { return _ctime; }
	inline void reborn() { _ctime = QDateTime::currentDateTime(); }
	inline unsigned int maxAge() const { return _maxAge; }
	inline unsigned int size() const { return _size; }
	QByteArray data();
	inline QString fileName() const { return _fileName; }
	inline QString hash() const { return _hash; } // it's a hash from 'id'. so you can have string for id. the hash will be used for filename

	inline void setSessionUndeletable(bool state = true) { if (state) _flags |= SessionUndeletable; else _flags &= ~SessionUndeletable; }
	void setUndeletable(bool state = true); // survives restarts
	inline bool isDeletable() const;

private:
	friend class FileCache;

	QString _id;
	QVariantMap _metadata;
	QDateTime _ctime;
	unsigned int _maxAge;
	unsigned int _size;
	QByteArray _data;

	quint16 _flags;
	QString _fileName;
	QString _hash;
};

class FileCache : public QObject
{
	Q_OBJECT
public:
	static const unsigned int Session = 0; //remove data when application exits
	static const unsigned int Forever = -1; //never remove

	static const unsigned int DefaultMemoryCacheSize = 1*1024*1024; //1 Mb
	static const unsigned int DefaultFileCacheSize = 50*1024*1024; //50 Mb

	enum SyncPolicy {
		InstantFLush, // always flush all data to disk (keeps copy in memory if fit)
		FlushOverflow // flush to disk only when memory cache limit is exceeded
	};

	FileCache(const QString &cacheDir, QObject *parent = 0);
	~FileCache();

	void gc();

	inline QString cacheDir() const { return _cacheDir; }

	inline void setMemoryCacheSize(unsigned int size)
				{ _memoryCacheSize = size; }
	inline unsigned int memoryCacheSize() const { return _memoryCacheSize; }

	inline void setFileCacheSize(unsigned int size) { _fileCacheSize = size; }
	inline unsigned int fileCacheSize() const { return _fileCacheSize; }

	inline void setDefaultMaxAge(unsigned int maxAge)
				{ _defaultMaxAge = maxAge; }
	inline unsigned int defaultMaxAge() const { return _defaultMaxAge; }

	inline void setSyncPolicy(SyncPolicy sp) { _syncPolicy = sp; }
	inline SyncPolicy syncPolicy() const { return _syncPolicy; }

	/**
	 * @brief Add data to cache
	 * @param id unique id (e.g. sha1 of data)
	 * @param type e.g. content-type
	 * @param data / if no data(size=0) memory size restiction won't affect this item, also no files will be created
	 * @param maxAge Session/Forever or just seconds to live
	 * @return a new cache item. Not yet synchronized to disk
	 */
	FileCacheItem *append(const QString &id, const QByteArray &data,
						  const QVariantMap &metadata = QVariantMap(), unsigned int maxAge = Forever);
	void remove(const QString &id, bool needSync = true);

	/**
	 * @brief get cache item metadata from cache (does not involve actual data loading)
	 * @param id uniqie id
	 * @param reborn - if more than half of the item age passed then set create-date to current
	 * @return
	 */
	FileCacheItem *get(const QString &id, bool reborn = false);
	QByteArray getData(const QString &id, bool reborn = false);
	void sync(bool finishSession);

protected:
	/**
	 * @brief removeItem item from disk, shedules registry update as well if required.
	 *   When called explicitly, removes even "Undeletable" items.
	 * @param item - item to delete
	 * @param needSync - shedule regitry update.
	 */
	virtual void removeItem(FileCacheItem *item, bool needSync);

public slots:
	void sync();

private:
	void toRegistry(FileCacheItem *);

protected:
	QHash<QString, FileCacheItem*> _items;

private:
	QString _cacheDir;
	unsigned int _memoryCacheSize;
	unsigned int _fileCacheSize;
	unsigned int _defaultMaxAge;
	SyncPolicy _syncPolicy;
	QTimer *_syncTimer;
	OptionsTree *_registry;
	QHash<QString, FileCacheItem*> _pendingRegisterItems;

	bool _registryChanged;
};

#endif //FILECACHE_H
