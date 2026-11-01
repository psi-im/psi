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
#include "filedatabase.h"
#include "fileutil.h"
#include "iris/xmpp_hash.h"
#include "optionstree.h"

#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QUuid>

#define FC_META_PERSISTENT QStringLiteral("fc_persistent")

FileCacheItem::FileCacheItem(FileCache *parent, const QList<XMPP::Hash> &sums, const QVariantMap &metadata,
                             const QDateTime &dt, unsigned int maxAge, quint64 size, const QByteArray &data) :
    QObject(parent), _sums(sums), _metadata(metadata), _ctime(dt), _maxAge(maxAge), _size(size), _data(data),
    _flags(quint16(size > 0 ? 0 : OnDisk)) /* empty is never saved to disk. let's say it's there already */
{
    Q_ASSERT(sums.size() > 0);
    std::sort(_sums.begin(), _sums.end(),
              [](const XMPP::Hash &a, const XMPP::Hash &b) -> bool { return int(a.type()) < int(b.type()); });

    auto    metaType = _metadata.value(QLatin1String("type"));
    QString ext      = metaType.isNull() ? QString() : FileUtil::mimeToFileExt(metaType.toString());
    _fileName        = _sums.value(0).toHex() + (ext.isEmpty() ? "" : "." + ext);
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
    return _maxAge == FileCache::Session ? finishSession
        : _maxAge == FileCache::Forever  ? false
                                         : _ctime.addSecs(_maxAge) < QDateTime::currentDateTime();
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
        if (!_metadata.contains(FC_META_PERSISTENT)) {
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
namespace {
QByteArray encodeMetadata(const QVariantMap &metadata)
{
    QByteArray  bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_15); // One profile may be used by Qt 5 and Qt 6 builds.
    stream << metadata;
    return bytes;
}

QVariantMap decodeMetadata(const QByteArray &bytes)
{
    QVariantMap metadata;
    QDataStream stream(bytes);
    stream.setVersion(QDataStream::Qt_5_15);
    stream >> metadata;
    return metadata;
}
}

FileCache::FileCache(const QString &partition, const QString &cacheDir, QObject *parent) :
    QObject(parent), _partition(partition), _cacheDir(cacheDir), _memoryCacheSize(FileCache::DefaultMemoryCacheSize),
    _fileCacheSize(FileCache::DefaultFileCacheSize), _defaultMaxAge(Forever), _syncPolicy(InstantFLush)
{
    _syncTimer = new QTimer(this);
    _syncTimer->setSingleShot(true);
    _syncTimer->setInterval(1000);
    connect(_syncTimer, SIGNAL(timeout()), SLOT(sync()));
    load();
}

void FileCache::load()
{
    auto db = FileDatabase::connection();
    if (!db.isOpen())
        return;
    QHash<QByteArray, QList<XMPP::Hash>> hashes;
    QSqlQuery                            query(db);
    query.prepare(QStringLiteral("SELECT h.file_id,h.algorithm,h.digest FROM file_hashes h JOIN files f ON f.id=h.file_id "
                                 "WHERE f.partition=:partition ORDER BY h.file_id,h.position"));
    query.bindValue(QStringLiteral(":partition"), _partition);
    if (!query.exec())
        qWarning() << "Cannot read file hashes:" << query.lastError().text();
    while (query.next()) {
        XMPP::Hash hash(query.value(1).toString());
        if (!hash.isValid())
            continue;
        hash.setData(query.value(2).toByteArray());
        hashes[query.value(0).toByteArray()].append(hash);
    }
    query.prepare(QStringLiteral("SELECT id,size,created,max_age,metadata FROM files WHERE partition=:partition"));
    query.bindValue(QStringLiteral(":partition"), _partition);
    if (!query.exec())
        qWarning() << "Cannot read file records:" << query.lastError().text();
    QList<FileCacheItem *> items;
    while (query.next()) {
        const auto id   = query.value(0).toByteArray();
        const auto sums = hashes.value(id);
        if (sums.isEmpty())
            continue;
        auto *item = new FileCacheItem(this, sums, decodeMetadata(query.value(4).toByteArray()),
                                       QDateTime::fromMSecsSinceEpoch(query.value(2).toLongLong()),
                                       query.value(3).toUInt(), query.value(1).toULongLong());
        item->_recordId = id;
        item->_flags |= (FileCacheItem::OnDisk | FileCacheItem::Registered);
        for (const auto &sum : sums)
            _items.insert(sum, item);
        items.append(item);
    }
    query.finish();
    for (auto *item : std::as_const(items))
        if (item->isExpired())
            remove(item->id());
}

void FileCache::importLegacy(const QString &legacyDir)
{
    auto db = FileDatabase::connection();
    if (!db.isOpen() || !db.transaction())
        return;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO file_legacy_imports(partition) VALUES(:partition)"));
    query.bindValue(QStringLiteral(":partition"), _partition);
    if (!query.exec() || query.numRowsAffected() < 1) { // Imported earlier, or the insert failed.
        db.rollback();
        return;
    }
    query.finish();

    _inTransaction = true;
    OptionsTree registry;
    const auto  registryPath = legacyDir + QStringLiteral("/cache.xml");
    if (QFile::exists(registryPath))
        registry.loadOptions(registryPath, "items", ApplicationInfo::fileCacheNS());
    const auto prefixes = registry.getChildOptionNames("", true, true);
    for (const QString &prefix : prefixes) {
        const auto section = prefix.section('.', -1);
        XMPP::Hash hash(registry.getOption(prefix + ".ha", QString()).toString());
        if (!hash.isValid())
            continue;
        hash.setData(QByteArray::fromHex(QStringView { section }.mid(1).toLatin1()));
        if (hash.data().isEmpty() || _items.contains(hash))
            continue;
        QList<XMPP::Hash> sums { hash };
        const auto        aliases = registry.getOption(prefix + ".aliases").toStringList();
        for (const auto &alias : aliases) {
            const auto separator = alias.indexOf('+');
            if (separator == -1)
                continue;
            XMPP::Hash other(XMPP::Hash::parseType(QStringView { alias }.left(separator)),
                             QByteArray::fromHex(QStringView { alias }.mid(separator + 1).toLatin1()));
            if (other.isValid() && !other.data().isEmpty())
                sums.append(other);
        }
        const auto maxAge = registry.getOption(prefix + ".max-age").toUInt();
        auto      *item   = new FileCacheItem(
            this, sums, registry.getOption(prefix + ".metadata", QVariantMap()).toMap(),
            QDateTime::fromString(registry.getOption(prefix + ".ctime").toString(), Qt::ISODate), maxAge,
            registry.getOption(prefix + ".size").toULongLong());
        bool usable = maxAge != Session && !item->isExpired();
        if (usable && item->size()) {
            const auto source = legacyDir + QLatin1Char('/') + item->fileName();
            const auto target = _cacheDir + QLatin1Char('/') + item->fileName();
            if (QFileInfo(target).size() != qint64(item->size()))
                QFile::remove(target);
            usable = QFile::exists(target) || (QFile::exists(source) && QFile::copy(source, target));
        }
        if (!usable) {
            delete item;
            continue;
        }
        item->_flags |= FileCacheItem::OnDisk;
        for (const auto &sum : std::as_const(sums))
            _items.insert(sum, item);
        toRegistry(item);
    }
    _inTransaction = false;
    if (!db.commit()) {
        qWarning() << "Cannot import a legacy file cache:" << db.lastError().text();
        db.rollback();
        return;
    }
    if (QDir(legacyDir).exists())
        FileDatabase::markLegacyDirectoryImported(legacyDir);
}

FileCache::~FileCache()
{
    gc();
    sync(true);
}

void FileCache::gc()
{
    QDir        dir(_cacheDir);
    const auto &ids = _items.keys();
    for (const XMPP::Hash &id : ids) {
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
        = new FileCacheItem(this, sums, metadata, QDateTime::currentDateTime(), maxAge, qint64(data.size()), data);
    replaceRecord(item, sums);
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
    replaceRecord(item, sums);
    for (auto const &s : sums)
        _items.insert(s, item);
    _pendingRegisterItems.insert(sums[0], item);
    _syncTimer->start();

    return item;
}

// An item appended again under a known hash takes over the record of the old one.
void FileCache::replaceRecord(FileCacheItem *item, const QList<XMPP::Hash> &sums)
{
    for (const auto &sum : sums) {
        auto *old = _items.value(sum);
        if (old && !old->_recordId.isEmpty()) {
            item->_recordId = old->_recordId;
            old->_recordId.clear();
            return;
        }
    }
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
    if (!item->_recordId.isEmpty()) {
        QSqlQuery query(FileDatabase::connection());
        query.prepare(QStringLiteral("DELETE FROM files WHERE id=:id"));
        query.bindValue(QStringLiteral(":id"), item->_recordId);
        if (!query.exec())
            qWarning() << "Cannot remove a file record:" << query.lastError().text();
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

void FileCache::lazySync() { _syncTimer->start(); }

void FileCache::sync(bool finishSession)
{
    if (_syncTimer->isActive()) {
        _syncTimer->stop();
    }

    // A batch of record changes commits at once, unless the caller already has a transaction.
    auto db    = FileDatabase::connection();
    bool batch = !_inTransaction && db.isOpen() && db.transaction();
    if (batch)
        _inTransaction = true;

    QList<FileCacheItem *> loadedItems;
    QList<FileCacheItem *> onDiskItems;
    qint64                 sumMemorySize = 0;
    qint64                 sumFileSize   = 0;
    FileCacheItem         *item;

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
        }
        if (!item->isRegistered()) { // just put to registry item without data and stop reviewing it
            toRegistry(item);        // save item to registry if not yet
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
            // if (!item->isRegistered()) {
            //     toRegistry(item); // save item to registry if not yet
            // }
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

    if (batch) {
        _inTransaction = false;
        if (!db.commit()) {
            qWarning() << "Cannot save file records:" << db.lastError().text();
            db.rollback();
        }
    }
}

void FileCache::toRegistry(FileCacheItem *item)
{
    item->_flags |= FileCacheItem::Registered;
    _pendingRegisterItems.remove(item->id());

    auto db = FileDatabase::connection();
    if (!db.isOpen())
        return;
    if (item->_recordId.isEmpty())
        item->_recordId = QUuid::createUuid().toRfc4122();
    const bool transaction = !_inTransaction && db.transaction();
    QSqlQuery  query(db);
    query.prepare(QStringLiteral("INSERT INTO files(id,partition,size,created,max_age,metadata) "
                                 "VALUES(:id,:partition,:size,:created,:max_age,:metadata) "
                                 "ON CONFLICT(id) DO UPDATE SET size=excluded.size,created=excluded.created,"
                                 "max_age=excluded.max_age,metadata=excluded.metadata"));
    query.bindValue(QStringLiteral(":id"), item->_recordId);
    query.bindValue(QStringLiteral(":partition"), _partition);
    query.bindValue(QStringLiteral(":size"), qlonglong(item->size()));
    query.bindValue(QStringLiteral(":created"), item->created().toMSecsSinceEpoch());
    query.bindValue(QStringLiteral(":max_age"), qlonglong(item->maxAge()));
    query.bindValue(QStringLiteral(":metadata"), encodeMetadata(item->metadata()));
    bool ok = query.exec();
    query.prepare(QStringLiteral("DELETE FROM file_hashes WHERE file_id=:id"));
    query.bindValue(QStringLiteral(":id"), item->_recordId);
    ok = ok && query.exec();
    query.prepare(QStringLiteral("INSERT INTO file_hashes(file_id,position,algorithm,digest) "
                                 "VALUES(:id,:position,:algorithm,:digest)"));
    int position = 0;
    for (const auto &sum : item->sums()) {
        query.bindValue(QStringLiteral(":id"), item->_recordId);
        query.bindValue(QStringLiteral(":position"), position++);
        query.bindValue(QStringLiteral(":algorithm"), sum.stringType());
        query.bindValue(QStringLiteral(":digest"), sum.data());
        ok = ok && query.exec();
    }
    if (!ok)
        qWarning() << "Cannot save a file record:" << query.lastError().text();
    if (transaction && !db.commit())
        db.rollback();
}
