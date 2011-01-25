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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef FILECACHE_H
#define FILECACHE_H

#include <QObject>
#include <QDateTime>
#include <QFile>
#include <QHash>

class QTimer;
class OptionsTree;
class FileCache;

class FileCacheItem : public QObject
{
	Q_OBJECT
public:
	FileCacheItem(FileCache *parent, const QString &itemId, const QString &type,
				  const QDateTime &dt, unsigned int maxAge, unsigned int size,
				  const QByteArray &data = QByteArray());

	void sync();
	bool remove() const;
	void unload();
	inline bool inMemory() const;
	inline bool isSynced() const { return _synced; }
	inline void setSynced(bool state) { _synced = state; }
	bool isExpired(bool finishSession = false) const;
	inline FileCache *parentCache() const { return (FileCache *)parent(); }
	inline QString id() const { return _id; }
	inline QString type() const { return _type; }
	inline QDateTime created() const { return _ctime; }
	inline unsigned int maxAge() const { return _maxAge; }
	inline unsigned int size() const { return _size; }
	QByteArray data();
	inline QString fileName() const { return _fileName; }
	inline QString hash() const { return _hash; }

private:
	QString _id;
	QString _type;
	QDateTime _ctime;
	unsigned int _maxAge;
	unsigned int _size;
	QByteArray _data;

	bool _synced;
	QString _fileName;
	QString _hash;
};

class FileCache : public QObject
{
	Q_OBJECT
public:
	static const unsigned int Session = 0; //remove data when application exists
	static const unsigned int Forever = -1; //never remove

	static const unsigned int DefaultMemoryCacheSize = 1*1024*1024; //1 Mb
	static const unsigned int DefaultFileCacheSize = 50*1024*1024; //50 Mb

	enum SyncPolicy {
		InstantFLush, // always flush all data to disk
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

	FileCacheItem *append(const QString &id, const QString &type,
						  const QByteArray &data,
						  unsigned int maxAge = Forever);
	void remove(const QString &id, bool needSync = true);
	FileCacheItem *get(const QString &id);
	QByteArray getData(const QString &id);
	void sync(bool finishSession);

public slots:
	void sync();

private:
	void toRegistry(FileCacheItem *);

private:
	QString _cacheDir;
	unsigned int _memoryCacheSize;
	unsigned int _fileCacheSize;
	unsigned int _defaultMaxAge;
	SyncPolicy _syncPolicy;
	QTimer *_syncTimer;
	OptionsTree *_registry;
	QHash<QString, FileCacheItem*> _items;
	QHash<QString, FileCacheItem*> _pendingSyncItems;

	bool _registryChanged;
};

#endif //FILECACHE_H
