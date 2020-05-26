/*
 * filecache.cpp - File storage with age and size control
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

#include "filecache.h"

#include "applicationinfo.h"
#include "fileutil.h"
#include "optionstree.h"
#include "xmpp_hash.h"

#include <QDebug>
#include <QDir>
#include <QTimer>

#define FC_META_PERSISTENT QStringLiteral("fc_persistent")

FileCacheItem::FileCacheItem(FileCache *parent, const QList<XMPP::Hash> &sums, const QVariantMap &metadata,
                             const QDateTime &dt, unsigned int maxAge, qint64 size, const QByteArray &data) :
    QObject(parent),
    _sums(sums), _metadata(metadata), _ctime(dt), _maxAge(maxAge), _size(size), _data(data),
    _flags(size > 0 ? 0 : OnDisk) /* empty is never saved to disk. let's say it's there already */
{
    Q_ASSERT(sums.size() > 0);
    std::sort(_sums.begin(), _sums.end(),
              [](const XMPP::Hash &a, const XMPP::Hash &b) -> bool { return int(a.type()) < int(b.type()); });

    auto    metaType = _metadata.value(QLatin1String("type"));
    QString ext      = metaType.isNull() ? QString() : FileUtil::mimeToFileExt(metaType.toString());
    _fileName        = _sums.value(0).toHex() + (ext.isEmpty() ? "" : "." + ext);
}

bool FileCacheItem::inMemory() const { return _data.size() > 0; }

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
            qWarning("Can't open file %s for writing", qPrintable(_fileName));
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
    return _maxAge == FileCache::Session
        ? finishSession
        : _maxAge == FileCache::Forever ? false : _ctime.addSecs(_maxAge) < QDateTime::currentDateTime();
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
            qWarning("Can't open file %s for reading", qPrintable(_fileName));
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
FileCache::FileCache(const QString &cacheDir, QObject *parent) :
    QObject(parent), _cacheDir(cacheDir), _memoryCacheSize(FileCache::DefaultMemoryCacheSize),
    _fileCacheSize(FileCache::DefaultFileCacheSize), _defaultMaxAge(Forever), _syncPolicy(InstantFLush),
    _registryChanged(false)
{
    _registry  = new OptionsTree(this);
    _syncTimer = new QTimer(this);
    _syncTimer->setSingleShot(true);
    _syncTimer->setInterval(1000);
    connect(_syncTimer, SIGNAL(timeout()), SLOT(sync()));

    _registry->loadOptions(_cacheDir + "/cache.xml", "items", ApplicationInfo::fileCacheNS());

    bool needCleanup = false;
    for (const QString &prefix : _registry->getChildOptionNames("", true, true)) {
        QByteArray id = QByteArray::fromHex(prefix.section('.', -1).midRef(1).toLatin1());
        if (id.isEmpty())
            continue;
        auto hAlgo = _registry->getOption(prefix + ".ha", QString()).toString();
        auto hash  = XMPP::Hash(QStringRef(&hAlgo));
        if (!hash.isValid()) {
            needCleanup = true;
            continue;
        }
        hash.setData(id);

        auto item = new FileCacheItem(
            this, hash, _registry->getOption(prefix + ".metadata", QVariantMap()).toMap(),
            QDateTime::fromString(_registry->getOption(prefix + ".ctime").toString(), Qt::ISODate),
            _registry->getOption(prefix + ".max-age").toUInt(), _registry->getOption(prefix + ".size").toULongLong());

        const auto aliases = _registry->getOption(prefix + ".aliases").toStringList();
        for (const auto &s : aliases) {
            auto ind = s.indexOf('+');
            if (ind == -1)
                continue;
            auto       type = XMPP::Hash::parseType(s.leftRef(ind));
            auto       ba   = QByteArray::fromHex(s.midRef(ind + 1).toLatin1());
            XMPP::Hash hash(type, ba);
            if (hash.isValid() && ba.size()) {
                item->addHashSum(hash);
            }
        }

        item->_flags |= (FileCacheItem::OnDisk | FileCacheItem::Registered);
        _items.insert(hash, item);
        if (item->isExpired()) {
            remove(item->id());
        }
    }

    if (needCleanup) {
        _registryChanged = true;
        _syncTimer->start();
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
    for (const XMPP::Hash &id : _items.keys()) {
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

FileCacheItem *FileCache::append(const QList<XMPP::Hash> &sums, const QByteArray &data, const QVariantMap &metadata,
                                 unsigned int maxAge)
{
    Q_ASSERT(sums.size() > 0);

    FileCacheItem *item
        = new FileCacheItem(this, sums, metadata, QDateTime::currentDateTime(), maxAge, size_t(data.size()), data);
    for (auto const &s : sums)
        _items.insert(s, item);
    _pendingRegisterItems.insert(sums[0], item);
    _syncTimer->start();
    return item;
}

FileCacheItem *FileCache::moveToCache(const QList<XMPP::Hash> &sums, const QFileInfo &file, const QVariantMap &metadata,
                                      unsigned int maxAge)
{
    Q_ASSERT(sums.size());
    auto item = new FileCacheItem(this, sums, metadata, file.lastModified(), maxAge, file.size());

    QString cachedFilePath = QString("%1/%2").arg(_cacheDir, item->fileName());
    QFile   cachedFile(cachedFilePath);
    if (cachedFile.exists())
        cachedFile.remove();

    if (!QFile(file.filePath()).rename(cachedFilePath)) {
        delete item;
        return nullptr;
    }
    item->_flags |= FileCacheItem::OnDisk;
    for (auto const &s : sums)
        _items.insert(s, item);
    _pendingRegisterItems.insert(sums[0], item);
    _syncTimer->start();

    return item;
}

void FileCache::remove(const XMPP::Hash &id, bool needSync)
{
    FileCacheItem *item = _items.value(id);
    if (item) {
        removeItem(item, needSync);
    }
}

void FileCache::removeItem(FileCacheItem *item, bool needSync)
{
    if (item->isOnDisk()) {
        _registry->removeOption("h" + item->id().toHex(), true);
        _registryChanged = true;
    }
    item->remove();
    for (auto const &a : item->sums()) {
        _items.remove(a);
    }
    _pendingRegisterItems.remove(item->id());
    delete item;
    if (needSync) {
        _syncTimer->start();
    }
}

FileCacheItem *FileCache::get(const XMPP::Hash &id, bool reborn)
{
    if (!id.isValid())
        return nullptr;

    FileCacheItem *item = _items.value(id);
    if (item) {
        if (!item->isExpired()) {
            if (reborn && item->maxAge() > 0u
                && item->created().secsTo(QDateTime::currentDateTime()) < int(item->maxAge()) / 2) {
                item->reborn();
                toRegistry(item);
            }
            return item;
        }
        remove(id);
    }
    return nullptr;
}

QByteArray FileCache::getData(const XMPP::Hash &id, bool reborn)
{
    FileCacheItem *item = get(id, reborn);
    return item ? item->data() : QByteArray();
}

bool ctimeLessThan(FileCacheItem *a, FileCacheItem *b) { return a->created() < b->created(); }

void FileCache::sync() { sync(false); }

void FileCache::sync(bool finishSession)
{
    QList<FileCacheItem *> loadedItems;
    QList<FileCacheItem *> onDiskItems;
    qint64                 sumMemorySize = 0;
    qint64                 sumFileSize   = 0;
    FileCacheItem *        item;

    QHashIterator<XMPP::Hash, FileCacheItem *> it(_items);
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
            toRegistry(item);               // save item to registry if not yet
        }
    }

    // if (sumDataSize > _fileCacheSize || sumMemorySize > _memoryCacheSize) {

    // flush overflowed in-memory data to disk
    if (sumMemorySize > _memoryCacheSize) {
        std::sort(loadedItems.begin(), loadedItems.end(), ctimeLessThan); // by create time from oldest to newest
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
    it = QHashIterator<XMPP::Hash, FileCacheItem *>(_pendingRegisterItems);
    while (it.hasNext()) {
        item = it.next().value();
        toRegistry(item); // FIXME do this only after we have a file on disk (or data size = 0)
        if (_syncPolicy == InstantFLush) {
            item->flushToDisk();
        }
    }

    // remove overflowed disk data
    if (sumFileSize > _fileCacheSize) {
        std::sort(onDiskItems.begin(), onDiskItems.end(), ctimeLessThan);
        while (sumFileSize > _fileCacheSize && onDiskItems.size()) {
            item = onDiskItems.takeFirst();
            if (!item->isDeletable()) {
                continue;
            }
            auto id = item->id();
            auto sz = item->size();
            removeItem(item, false);
            if (!_items.value(id)) { // really removed
                sumFileSize -= sz;
            }
        }
    }

    if (_registryChanged) {
        _registry->saveOptions(_cacheDir + "/cache.xml", "items", ApplicationInfo::fileCacheNS(),
                               ApplicationInfo::version());
        _registryChanged = false;
    }
}

void FileCache::toRegistry(FileCacheItem *item)
{
    // TODO build Map/Hash instead and insert it to options
    QString prefix = QString("h") + QString::fromLatin1(item->id().toHex());
    _registry->setOption(prefix + ".ha", item->id().stringType());
    _registry->setOption(prefix + ".metadata", item->metadata());
    _registry->setOption(prefix + ".ctime", item->created().toString(Qt::ISODate));
    _registry->setOption(prefix + ".max-age", int(item->maxAge()));
    _registry->setOption(prefix + ".size", qulonglong(item->size()));

    QStringList aliases;
    auto        it = item->sums().cbegin() + 1;
    while (it != item->sums().cend()) {
        aliases.append(QString("%1+%2").arg(it->stringType(), QString::fromLatin1(it->toHex())));
        ++it;
    }
    _registry->setOption(prefix + ".aliases", aliases);

    item->_flags |= FileCacheItem::Registered;
    _pendingRegisterItems.remove(item->id());
    _registryChanged = true;
}
