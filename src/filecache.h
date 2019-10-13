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

#include "xmpp_hash.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QObject>
#include <QVariantMap>
#include <memory>

class FileCache;
class OptionsTree;
class QTimer;

class FileCacheItem : public QObject {
    Q_OBJECT
public:
    using Ptr = std::shared_ptr<FileCacheItem>;

    enum Flags {
        OnDisk             = 0x1,
        Registered         = 0x2,
        SessionUndeletable = 0x4 // The item is undeletable by expiration or cache size limits during this session
        // Unloadable  = 0x8 // another good idea
    };

    FileCacheItem(FileCache *parent, const QList<XMPP::Hash> &sums, const QVariantMap &metadata, const QDateTime &dt,
                  unsigned int maxAge, qint64 size, const QByteArray &data = QByteArray());
    inline FileCacheItem(FileCache *parent, const XMPP::Hash &id, const QVariantMap &metadata, const QDateTime &dt,
                         unsigned int maxAge, qint64 size, const QByteArray &data = QByteArray()) :
        FileCacheItem(parent, QList<XMPP::Hash>() << id, metadata, dt, maxAge, size, data)
    {
    }

    void
                      flushToDisk(); // put data to disk, but not to registry. don't call this directly. FileCache will care about it.
    bool              remove() const; // remove file from disk but not from registry. don't call this directly.
    void              unload();       // drop file to disk, deallocate memory
    inline bool       inMemory() const;
    inline bool       isOnDisk() const { return _flags & OnDisk; }         // data is on disk and not in rgistry
    inline bool       isRegistered() const { return _flags & Registered; } // data is on disk and not in rgistry
    bool              isExpired(bool finishSession = false) const;
    inline FileCache *parentCache() const { return (FileCache *)parent(); }
    inline XMPP::Hash id() const { return _sums.value(0); }
    inline void       addHashSum(const XMPP::Hash &id)
    {
        _sums += id;
        _flags &= ~Registered;
    }
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
    inline qint64       size() const { return _size; }
    QByteArray          data();
    inline QString      fileName() const { return _fileName; }

    inline void setSessionUndeletable(bool state = true)
    {
        if (state)
            _flags |= SessionUndeletable;
        else
            _flags &= ~SessionUndeletable;
    }
    void        setUndeletable(bool state = true); // survives restarts
    inline bool isDeletable() const;

private:
    friend class FileCache;

    QList<XMPP::Hash> _sums;
    QVariantMap       _metadata;
    QDateTime         _ctime;
    unsigned int      _maxAge;
    qint64            _size;
    QByteArray        _data;

    quint16 _flags;
    QString _fileName;
};

class FileCache : public QObject {
    Q_OBJECT
public:
    static constexpr unsigned int Session = 0;  // remove data when application exits
    static constexpr unsigned int Forever = -1; // never remove

    static constexpr unsigned int DefaultMemoryCacheSize = 1 * 1024 * 1024;  // 1 Mb
    static constexpr unsigned int DefaultFileCacheSize   = 50 * 1024 * 1024; // 50 Mb

    enum SyncPolicy {
        InstantFLush, // always flush all data to disk (keeps copy in memory if fit)
        FlushOverflow // flush to disk only when memory cache limit is exceeded
    };

    FileCache(const QString &cacheDir, QObject *parent = nullptr);
    ~FileCache();

    void gc();

    inline QString cacheDir() const { return _cacheDir; }

    inline void         setMemoryCacheSize(unsigned int size) { _memoryCacheSize = size; }
    inline unsigned int memoryCacheSize() const { return _memoryCacheSize; }

    inline void         setFileCacheSize(unsigned int size) { _fileCacheSize = size; }
    inline unsigned int fileCacheSize() const { return _fileCacheSize; }

    inline void         setDefaultMaxAge(unsigned int maxAge) { _defaultMaxAge = maxAge; }
    inline unsigned int defaultMaxAge() const { return _defaultMaxAge; }

    inline void       setSyncPolicy(SyncPolicy sp) { _syncPolicy = sp; }
    inline SyncPolicy syncPolicy() const { return _syncPolicy; }

    /**
     * @brief Add data to cache
     * @param sums - hash sums of the data (at least 1)
     * @param type e.g. content-type
     * @param data / if no data(size=0) memory size restiction won't affect this item, also no files will be created
     * @param maxAge Session/Forever or just seconds to live
     * @return a new cache item. Not yet synchronized to disk
     */
    FileCacheItem *       append(const QList<XMPP::Hash> &sums, const QByteArray &data,
                                 const QVariantMap &metadata = QVariantMap(), unsigned int maxAge = Forever);
    inline FileCacheItem *append(const XMPP::Hash &id, const QByteArray &data,
                                 const QVariantMap &metadata = QVariantMap(), unsigned int maxAge = Forever)
    {
        return append(QList<XMPP::Hash>() << id, data, metadata, maxAge);
    }

    // similar to append but instead of `data` moves file to the cache directory
    FileCacheItem *       moveToCache(const QList<XMPP::Hash> &sums, const QFileInfo &file,
                                      const QVariantMap &metadata = QVariantMap(), unsigned int maxAge = Forever);
    inline FileCacheItem *moveToCache(const XMPP::Hash &id, const QFileInfo &file,
                                      const QVariantMap &metadata = QVariantMap(), unsigned int maxAge = Forever)
    {
        return moveToCache(QList<XMPP::Hash>() << id, file, metadata, maxAge);
    }
    void remove(const XMPP::Hash &id, bool needSync = true);

    /**
     * @brief get cache item metadata from cache (does not involve actual data loading)
     * @param id uniqie id
     * @param reborn - if more than half of the item age passed then set create-date to current
     * @return
     */
    FileCacheItem *get(const XMPP::Hash &id, bool reborn = false);
    QByteArray     getData(const XMPP::Hash &id, bool reborn = false);
    void           sync(bool finishSession);

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
    QHash<XMPP::Hash, FileCacheItem *> _items;

private:
    QString                            _cacheDir;
    unsigned int                       _memoryCacheSize;
    unsigned int                       _fileCacheSize;
    unsigned int                       _defaultMaxAge;
    SyncPolicy                         _syncPolicy;
    QTimer *                           _syncTimer;
    OptionsTree *                      _registry;
    QHash<XMPP::Hash, FileCacheItem *> _pendingRegisterItems;

    bool _registryChanged;
};

#endif // FILECACHE_H
