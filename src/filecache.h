/*
 * filecache.h - File storage with age and size control
 * Copyright (C) 2010  Sergey Ilinykh
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef FILECACHE_H
#define FILECACHE_H

#include "filecachememory.h"
#include "iris/xmpp_hash.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QObject>
#include <QVariantMap>
#include <limits>
#include <optional>

class FileCache;
class MediaChunkStore;
class QTimer;

class FileCacheItem : public QObject {
    Q_OBJECT
public:
    enum Flags {
        OnDisk             = 0x1,
        Registered         = 0x2,
        SessionUndeletable = 0x4 // The item is undeletable by expiration or cache size limits during this session
        // Unloadable  = 0x8 // another good idea
    };

    bool                            inMemory() const;
    inline bool                     isOnDisk() const { return bool(_flags & OnDisk); }
    inline bool                     isRegistered() const { return bool(_flags & Registered); }
    bool              isExpired(bool finishSession = false) const;
    inline XMPP::Hash id() const { return _sums.value(0); }
    inline const QList<XMPP::Hash> &sums() const { return _sums; }
    inline QVariantMap              metadata() const { return _metadata; }
    inline void                     setMetadata(const QVariantMap &md)
    {
        _metadata = md;
        _flags &= ~Registered;
    } // we have to update registry eventually
    inline QDateTime    created() const { return _ctime; }
    inline void         reborn() { _ctime = QDateTime::currentDateTime(); }
    inline unsigned int maxAge() const { return _maxAge; }
    inline quint64      size() const { return _size; }
    QByteArray          data();
    inline QString fileName() const { return _fileName; } // Relative managed name; use filePath() for either placement.
    QString        filePath() const;
    bool           isExternal() const { return !_externalPath.isEmpty(); }

    inline void setSessionUndeletable(bool state = true)
    {
        if (state)
            _flags |= SessionUndeletable;
        else
            _flags &= ~SessionUndeletable;
    }
    void        setUndeletable(bool state = true); // survives restarts
    bool        isDeletable() const;

private:
    friend class FileCache;

    FileCacheItem(FileCache *parent, const QList<XMPP::Hash> &sums, const QVariantMap &metadata, const QDateTime &dt,
                  unsigned int maxAge, quint64 size, const QByteArray &data = {});
    FileCache *parentCache() const;
    void       flushToDisk();
    bool       remove() const;
    QByteArray memoryKey() const;

    QList<XMPP::Hash> _sums;
    QVariantMap       _metadata;
    QDateTime         _ctime;
    unsigned int      _maxAge;
    quint64           _size;
    QByteArray        _pendingData; // Uncommitted bytes cannot be evicted.

    quint16    _flags;
    QString    _fileName;
    QByteArray            _recordId;
    QString               _externalPath;
    std::optional<qint64> _externalModified;
};

class FileCache : public QObject {
    Q_OBJECT
public:
    static constexpr unsigned int Session = 0; // remove data when application exits
    static constexpr unsigned int Forever = std::numeric_limits<unsigned int>::max(); // never remove

    static constexpr unsigned int DefaultMemoryCacheSize = 1 * 1024 * 1024;  // 1 Mb
    static constexpr unsigned int DefaultFileCacheSize   = 50 * 1024 * 1024; // 50 Mb

    enum SyncPolicy {
        InstantFLush, // always flush all data to disk (keeps copy in memory if fit)
        FlushOverflow // flush to disk only when memory cache limit is exceeded
    };

    // A partition has its own items, payload directory and limits in the profile's database.
    FileCache(const QString &partition, const QString &cacheDir, QObject *parent = nullptr);
    ~FileCache();

    // Takes over the items of a directory from before caches moved under the profile,
    // copying their payload. Other profiles may need the same directory, so it stays.
    // Runs once per partition.
    void importLegacy(const QString &legacyDir);

    void gc();

    inline QString cacheDir() const { return _cacheDir; }

    inline void setMemoryCacheSize(unsigned int size)
    {
        _memory.setLimit(size);
        lazySync();
    }
    inline unsigned int memoryCacheSize() const { return unsigned(_memory.limit()); }
    quint64             memoryUsed() const { return _memory.bytes(); }

    inline void setFileCacheSize(unsigned int size)
    {
        _fileCacheSize = size;
        lazySync();
    }
    inline unsigned int fileCacheSize() const { return _fileCacheSize; }

    inline void         setDefaultMaxAge(unsigned int maxAge) { _defaultMaxAge = maxAge; }
    inline unsigned int defaultMaxAge() const { return _defaultMaxAge; }

    inline void       setSyncPolicy(SyncPolicy sp) { _syncPolicy = sp; }
    inline SyncPolicy syncPolicy() const { return _syncPolicy; }

    /**
     * @brief Add data to cache
     * @param sums - hash sums of the data (at least 1)
     * @param type e.g. content-type
     * @param data Payload bytes. Empty data creates a real empty managed file.
     * @param maxAge Session/Forever or just seconds to live
     * @return New item, not yet synchronized to disk; nullptr if a prior generation cannot be retired.
     */
    FileCacheItem        *append(const QList<XMPP::Hash> &sums, const QByteArray &data,
                                 const QVariantMap &metadata = QVariantMap(), unsigned int maxAge = Forever);
    inline FileCacheItem *append(const XMPP::Hash &id, const QByteArray &data,
                                 const QVariantMap &metadata = QVariantMap(), unsigned int maxAge = Forever)
    {
        return append(QList<XMPP::Hash>() << id, data, metadata, maxAge);
    }

    // similar to append but instead of `data` moves file to the cache directory
    FileCacheItem        *moveToCache(const QList<XMPP::Hash> &sums, const QFileInfo &file,
                                      const QVariantMap &metadata = QVariantMap(), unsigned int maxAge = Forever);
    inline FileCacheItem *moveToCache(const XMPP::Hash &id, const QFileInfo &file,
                                      const QVariantMap &metadata = QVariantMap(), unsigned int maxAge = Forever)
    {
        return moveToCache(QList<XMPP::Hash>() << id, file, metadata, maxAge);
    }
    // Records an original without taking ownership of its bytes. A changed size
    // or modification time invalidates the record; removal never unlinks it.
    FileCacheItem *trackExternal(const QList<XMPP::Hash> &sums, const QFileInfo &file, const QVariantMap &metadata = {},
                                 unsigned int maxAge = Forever);
    void remove(const XMPP::Hash &id, bool needSync = true);

    /**
     * @brief get cache item metadata from cache (does not involve actual data loading)
     * @param id uniqie id
     * @param reborn - if more than half of the item age passed then set create-date to current
     * @return
     */
    FileCacheItem *get(const XMPP::Hash &id, bool reborn = false);
    QByteArray     getData(const XMPP::Hash &id, bool reborn = false);
    // Commit one payload and its ownership before a durable publication names
    // its path. Does not trigger a quota sweep.
    bool           persist(const XMPP::Hash &id);
    // Private range backend used by the transitional chunk-cache adapter.
    // Shares the profile database, retirement protocol and this partition's quota.
    MediaChunkStore *rangeStore();
    void           sync(bool finishSession);

protected:
    /**
     * @brief removeItem item from disk, shedules registry update as well if required.
     *   When called explicitly, removes even "Undeletable" items.
     * @param item - item to delete
     * @param needSync - shedule regitry update.
     */
    virtual void removeItem(FileCacheItem *item, bool needSync);

signals:
    void contentsChanged();

public slots:
    void sync();
    void lazySync();

private:
    friend class FileCacheItem;
    bool toRegistry(FileCacheItem *);
    void load();
    bool replaceRecord(const QList<XMPP::Hash> &sums);
    void drainTrash();

protected:
    QHash<XMPP::Hash, FileCacheItem *> _items;

private:
    QString                            _partition;
    QString                            _cacheDir;
    FileCacheMemory                    _memory;
    unsigned int                       _fileCacheSize;
    unsigned int                       _defaultMaxAge;
    SyncPolicy                         _syncPolicy;
    QTimer                            *_syncTimer;

    bool _inTransaction = false;
    MediaChunkStore *_rangeStore    = nullptr;
};

#endif // FILECACHE_H
