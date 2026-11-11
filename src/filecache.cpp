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
#include "mediachunkstore.h"
#include "optionstree.h"

#include <QDebug>
#include <QDir>
#include <QPointer>
#include <QSaveFile>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QUuid>
#include <algorithm>
#include <utility>

#define FC_META_PERSISTENT QStringLiteral("fc_persistent")

FileCacheItem::FileCacheItem(FileCache *parent, const QList<XMPP::Hash> &sums, const QVariantMap &metadata,
                             const QDateTime &dt, unsigned int maxAge, quint64 size, const QByteArray &data) :
    QObject(parent), _sums(sums), _metadata(metadata), _ctime(dt), _maxAge(maxAge), _size(size), _pendingData(data),
    _flags(0), _recordId(QUuid::createUuid().toRfc4122())
{
    Q_ASSERT(sums.size() > 0);
    std::sort(_sums.begin(), _sums.end(),
              [](const XMPP::Hash &a, const XMPP::Hash &b) -> bool { return int(a.type()) < int(b.type()); });

    auto    metaType = _metadata.value(QLatin1String("type"));
    QString ext      = metaType.isNull() ? QString() : FileUtil::mimeToFileExt(metaType.toString());
    _fileName        = QString::fromLatin1(_recordId.toHex()) + (ext.isEmpty() ? "" : "." + ext);
}

FileCache *FileCacheItem::parentCache() const { return static_cast<FileCache *>(parent()); }

QString FileCacheItem::filePath() const
{
    return isExternal() ? _externalPath : parentCache()->cacheDir() + QLatin1Char('/') + _fileName;
}

void FileCacheItem::flushToDisk()
{
    if (_flags & OnDisk)
        return;
    QSaveFile file(filePath());
    if (file.open(QIODevice::WriteOnly) && file.write(_pendingData) == _pendingData.size() && file.commit()) {
        _flags |= OnDisk;
        parentCache()->_memory.put(memoryKey(), 0, std::move(_pendingData));
        _pendingData.clear();
    } else
        qWarning() << "Cannot save cached file:" << file.errorString();
}

bool FileCacheItem::remove() const
{
    if (isExternal())
        return true;
    return !(_flags & OnDisk) || QFile::remove(filePath()) || !QFileInfo::exists(filePath());
}

QByteArray FileCacheItem::memoryKey() const
{
    if (isExternal() && _externalPath.startsWith(parentCache()->cacheDir() + QStringLiteral("/ranges/"))) {
        const auto name = QFileInfo(_externalPath).completeBaseName().toLatin1();
        const auto id   = QByteArray::fromHex(name);
        if (id.size() == 16 && id.toHex() == name)
            return id;
    }
    return _recordId;
}

bool FileCacheItem::inMemory() const
{
    return !_pendingData.isEmpty() || parentCache()->_memory.contains(memoryKey(), 0, _size);
}

bool FileCacheItem::isExpired(bool finishSession) const
{
    return _maxAge == FileCache::Session ? finishSession
        : _maxAge == FileCache::Forever  ? false
                                         : _ctime.addSecs(_maxAge) < QDateTime::currentDateTime();
}

QByteArray FileCacheItem::data()
{
    if (!_pendingData.isEmpty())
        return _pendingData;
    if (!_size || _size > quint64(std::numeric_limits<qint64>::max()))
        return {};
    const auto generation = memoryKey();
    if (generation != _recordId && !parentCache()->rangeStore()->isCompleteFile(filePath()))
        return {};
    if (auto bytes = parentCache()->_memory.get(generation, 0, _size))
        return *bytes;
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Cannot open cached file %s for reading", qPrintable(_fileName));
        return {};
    }
    const auto bytes = file.read(qint64(_size));
    if (quint64(bytes.size()) != _size)
        return {};
    parentCache()->_memory.put(generation, 0, bytes);
    return bytes;
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
QString legacyFileName(QList<XMPP::Hash> sums, const QVariantMap &metadata)
{
    std::sort(sums.begin(), sums.end(), [](const auto &a, const auto &b) { return int(a.type()) < int(b.type()); });
    const auto ext = FileUtil::mimeToFileExt(metadata.value(QStringLiteral("type")).toString());
    return QString::fromLatin1(sums.value(0).toHex()) + (ext.isEmpty() ? QString() : QLatin1Char('.') + ext);
}
}

FileCache::FileCache(const QString &partition, const QString &cacheDir, QObject *parent) :
    QObject(parent), _partition(partition), _cacheDir(cacheDir), _memory(FileCache::DefaultMemoryCacheSize),
    _fileCacheSize(FileCache::DefaultFileCacheSize), _defaultMaxAge(Forever), _syncPolicy(InstantFLush)
{
    _syncTimer = new QTimer(this);
    _syncTimer->setSingleShot(true);
    _syncTimer->setInterval(1000);
    connect(_syncTimer, SIGNAL(timeout()), SLOT(sync()));
    QDir().mkpath(_cacheDir);
    load();
    drainTrash();
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
    query.prepare(QStringLiteral(
        "SELECT id,size,created,max_age,metadata,path,placement,external_mtime FROM files WHERE partition=:partition"));
    query.bindValue(QStringLiteral(":partition"), _partition);
    if (!query.exec())
        qWarning() << "Cannot read file records:" << query.lastError().text();
    QList<FileCacheItem *> items, legacy;
    while (query.next()) {
        const auto id   = query.value(0).toByteArray();
        const auto sums = hashes.value(id);
        if (sums.isEmpty())
            continue;
        auto *item      = new FileCacheItem(this, sums, FileDatabase::decodeMetadata(query.value(4).toByteArray()),
                                            QDateTime::fromMSecsSinceEpoch(query.value(2).toLongLong()),
                                            query.value(3).toUInt(), query.value(1).toULongLong());
        item->_recordId = id;
        if (query.value(5).isNull()) {
            const auto link = item->_metadata.take(QStringLiteral("link")).toString();
            if (!link.isEmpty()) {
                const QFileInfo external(link);
                item->_externalPath = external.absoluteFilePath();
                // Old links recorded neither size nor modification time. Do
                // not certify today's bytes against an old digest. The catalog
                // can rebind a previously published original with its own stamp.
                item->_externalModified.reset();
            } else {
                item->_fileName = legacyFileName(sums, item->_metadata);
                if (!item->size() && !QFileInfo::exists(item->filePath()))
                    item->flushToDisk(); // Materialize old empty records for ordinary file access.
            }
            legacy.append(item);
        } else if (query.value(6).toInt() == 1) {
            item->_externalPath = query.value(5).toString();
            if (!query.value(7).isNull())
                item->_externalModified = query.value(7).toLongLong();
            if (!QDir::isAbsolutePath(item->_externalPath)) {
                delete item;
                continue;
            }
        } else {
            item->_fileName = query.value(5).toString();
            if (!FileDatabase::safePayloadName(item->_fileName)) {
                delete item;
                continue;
            }
        }
        item->_flags |= (FileCacheItem::OnDisk | FileCacheItem::Registered);
        for (const auto &sum : sums)
            _items.insert(sum, item);
        items.append(item);
    }
    query.finish();
    // Resolve all legacy paths before draining trash, including aliases sharing
    // one old hash-named payload. Migration never moves or deletes those bytes.
    if (!legacy.isEmpty() && db.transaction()) {
        _inTransaction = true;
        bool ok        = true;
        for (auto *item : std::as_const(legacy))
            ok = toRegistry(item) && ok;
        _inTransaction = false;
        if (!ok || !db.commit()) {
            db.rollback();
            for (auto item : std::as_const(legacy)) {
                item->_flags &= ~FileCacheItem::Registered;
            }
            _syncTimer->start(5000);
        }
    }
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
    QList<FileCacheItem *> imported;
    bool                   ok = true;
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
        if (std::any_of(sums.cbegin(), sums.cend(), [this](const auto &sum) { return _items.contains(sum); }))
            continue; // Existing profile records take precedence over every alias in the old registry.
        const auto maxAge = registry.getOption(prefix + ".max-age").toUInt();
        auto      *item   = new FileCacheItem(
            this, sums, registry.getOption(prefix + ".metadata", QVariantMap()).toMap(),
            QDateTime::fromString(registry.getOption(prefix + ".ctime").toString(), Qt::ISODate), maxAge,
            registry.getOption(prefix + ".size").toULongLong());
        const auto link = item->_metadata.take(QStringLiteral("link")).toString();
        if (!link.isEmpty()) {
            const QFileInfo external(link);
            item->_externalPath = external.absoluteFilePath();
            item->_externalModified.reset();
        }
        bool usable = maxAge != Session && !item->isExpired();
        if (usable && !item->isExternal() && item->size()) {
            const auto source = legacyDir + QLatin1Char('/') + legacyFileName(sums, item->_metadata);
            const auto target = _cacheDir + QLatin1Char('/') + item->fileName();
            if (QFileInfo(target).size() != qint64(item->size()))
                QFile::remove(target);
            usable = QFile::exists(target) || (QFile::exists(source) && QFile::copy(source, target));
        }
        if (!usable) {
            if (!item->isExternal())
                QFile::remove(item->filePath());
            delete item;
            continue;
        }
        if (!item->size() && !item->isExternal())
            item->flushToDisk();
        item->_flags |= FileCacheItem::OnDisk;
        for (const auto &sum : std::as_const(sums))
            _items.insert(sum, item);
        imported.append(item);
        ok = toRegistry(item) && ok;
        if (!item->isExternal()) {
            // A crash during the old hash-named import could leave a partial
            // copy. Retire it only after the new generation is recorded.
            const auto previous = legacyFileName(sums, item->_metadata);
            if (previous != item->fileName() && QFileInfo::exists(_cacheDir + QLatin1Char('/') + previous)) {
                query.prepare(
                    QStringLiteral("INSERT OR IGNORE INTO file_trash(partition,path) VALUES(:partition,:path)"));
                query.bindValue(QStringLiteral(":partition"), _partition);
                query.bindValue(QStringLiteral(":path"), previous);
                ok = query.exec() && ok;
            }
        }
    }
    _inTransaction = false;
    if (!ok || !db.commit()) {
        qWarning() << "Cannot import a legacy file cache:" << db.lastError().text();
        db.rollback();
        for (auto item : std::as_const(imported)) {
            for (const auto &hash : item->sums())
                if (_items.value(hash) == item)
                    _items.remove(hash);
            item->remove();
            delete item;
        }
        return;
    }
    drainTrash();
    if (QDir(legacyDir).exists())
        FileDatabase::markLegacyDirectoryImported(legacyDir);
}

FileCache::~FileCache()
{
    gc();
    sync(true);
    delete _rangeStore;
}

MediaChunkStore *FileCache::rangeStore()
{
    if (!_rangeStore) {
        _rangeStore = new MediaChunkStore(_partition + QStringLiteral(":ranges"), _cacheDir + QStringLiteral("/ranges"),
                                          FileDatabase::path(), _fileCacheSize, &_memory, this,
                                          [](const QString &algorithm, QIODevice *device) {
                                              XMPP::Hash hash { QStringView(algorithm) };
                                              return hash.compute(device) ? hash.data() : QByteArray();
                                          });
        connect(_rangeStore, &MediaChunkStore::availabilityChanged, this, &FileCache::lazySync);
        lazySync();
    }
    return _rangeStore;
}

void FileCache::gc()
{
    const auto                  values = _items.values();
    const QSet<FileCacheItem *> items(values.cbegin(), values.cend());
    for (auto item : items) {
        const QFileInfo file(item->filePath());
        const bool      broken = item->isOnDisk()
            && (!file.isFile() || file.isSymLink() || file.size() != qint64(item->size())
                || (item->isExternal()
                    && (!item->_externalModified
                        || file.lastModified().toMSecsSinceEpoch() != *item->_externalModified)));
        if (broken || (item->isExpired() && item->isDeletable()))
            removeItem(item, false);
    }
    drainTrash();
}

FileCacheItem *FileCache::append(const QList<XMPP::Hash> &sums, const QByteArray &data, const QVariantMap &metadata,
                                 unsigned int maxAge)
{
    Q_ASSERT(sums.size() > 0);

    FileCacheItem *item
        = new FileCacheItem(this, sums, metadata, QDateTime::currentDateTime(), maxAge, qint64(data.size()), data);
    if (!replaceRecord(sums)) {
        delete item;
        return nullptr;
    }
    for (auto const &s : sums)
        _items.insert(s, item);
    _syncTimer->start(1000);
    emit contentsChanged();
    return item;
}

FileCacheItem *FileCache::moveToCache(const QList<XMPP::Hash> &sums, const QFileInfo &file, const QVariantMap &metadata,
                                      unsigned int maxAge)
{
    if (sums.isEmpty() || !file.isFile() || file.isSymLink())
        return nullptr;
    auto item = new FileCacheItem(this, sums, metadata, QDateTime::currentDateTime(), maxAge, quint64(file.size()));
    // Generation paths are never overwritten. A delayed deletion can only
    // address the previous generation, even when its digest is reused.
    if (!QFile(file.filePath()).rename(item->filePath())) {
        delete item;
        return nullptr;
    }
    item->_flags |= FileCacheItem::OnDisk;
    if (!replaceRecord(sums)) {
        if (!QFile(item->filePath()).rename(file.filePath()))
            qWarning() << "Cannot restore a file after cache retirement failed:" << item->filePath();
        delete item;
        return nullptr;
    }
    for (const auto &sum : sums)
        _items.insert(sum, item);
    _syncTimer->start(1000);
    emit contentsChanged();
    return item;
}

FileCacheItem *FileCache::trackExternal(const QList<XMPP::Hash> &sums, const QFileInfo &file,
                                        const QVariantMap &metadata, unsigned int maxAge)
{
    const auto path = file.canonicalFilePath();
    if (sums.isEmpty() || path.isEmpty() || !file.isFile() || !file.isReadable() || !file.lastModified().isValid())
        return nullptr;
    const auto root = QFileInfo(_cacheDir).canonicalFilePath();
    if (FileDatabase::safePayloadName(QDir(root.isEmpty() ? _cacheDir : root).relativeFilePath(path))) {
        for (const auto &hash : sums) {
            auto existing = _items.value(hash);
            if (!existing || existing->isExternal() || QFileInfo(existing->filePath()).canonicalFilePath() != path)
                continue;
            for (const auto &sum : sums)
                if (_items.value(sum) != existing)
                    return nullptr;
            auto merged = existing->metadata();
            for (auto it = metadata.cbegin(); it != metadata.cend(); ++it)
                merged.insert(it.key(), it.value());
            existing->setMetadata(merged);
            lazySync();
            return existing;
        }
        return nullptr; // An unbound generation in our root may still await retirement.
    }
    if (!replaceRecord(sums))
        return nullptr;
    auto item = new FileCacheItem(this, sums, metadata, QDateTime::currentDateTime(), maxAge, quint64(file.size()));
    item->_externalPath     = path;
    item->_externalModified = file.lastModified().toMSecsSinceEpoch();
    item->_flags |= FileCacheItem::OnDisk;
    for (const auto &sum : sums)
        _items.insert(sum, item);
    _syncTimer->start(1000);
    emit contentsChanged();
    return item;
}

// A replacement is a new generation. Retiring the old row never reuses its
// payload name, even if the caller announces the same digest again.
bool FileCache::replaceRecord(const QList<XMPP::Hash> &sums)
{
    QSet<FileCacheItem *> obsolete;
    for (const auto &sum : sums)
        if (auto old = _items.value(sum))
            obsolete.insert(old);
    for (auto old : std::as_const(obsolete)) {
        const QPointer<FileCacheItem> guard(old);
        FileCache::removeItem(old, false);
        if (guard)
            return false;
    }
    return true;
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
    auto db = FileDatabase::connection();
    if (db.isOpen()) {
        if (!FileDatabase::retireFile(db, item->_recordId, _partition,
                                      !item->isExternal() && item->isOnDisk() ? item->fileName() : QString(),
                                      _inTransaction)) {
            qWarning() << "Cannot retire a cached file:" << db.lastError().text();
            return;
        }
    } else {
        item->remove(); // Session-only mode has no durable registry to retire first.
    }
    for (const auto &sum : item->sums())
        if (_items.value(sum) == item)
            _items.remove(sum);
    _memory.remove(item->memoryKey());
    delete item;
    emit contentsChanged();
    if (!_inTransaction)
        drainTrash();
    if (needSync)
        _syncTimer->start(1000);
}

void FileCache::drainTrash()
{
    if (_inTransaction)
        return;
    auto db = FileDatabase::connection();
    FileDatabase::drainTrash(db, _partition, _cacheDir);
}

FileCacheItem *FileCache::get(const XMPP::Hash &id, bool reborn)
{
    if (!id.isValid())
        return nullptr;

    FileCacheItem *item = _items.value(id);
    if (item) {
        if (item->isOnDisk()) {
            const QFileInfo file(item->filePath());
            if (!file.isFile() || file.isSymLink() || !file.isReadable() || file.size() != qint64(item->size())
                || (item->isExternal()
                    && (!item->_externalModified
                        || file.lastModified().toMSecsSinceEpoch() != *item->_externalModified))) {
                removeItem(item, true);
                return nullptr;
            }
        }
        if (!item->isExpired()) {
            if (reborn && item->maxAge() > 0u
                && item->created().secsTo(QDateTime::currentDateTime()) >= qint64(item->maxAge()) / 2) {
                item->reborn();
                if (!toRegistry(item))
                    _syncTimer->start(5000);
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

bool FileCache::persist(const XMPP::Hash &id)
{
    if (_inTransaction)
        return false;
    auto item = get(id);
    if (!item)
        return false;
    item->flushToDisk();
    const bool stored = toRegistry(item);
    if (!stored && FileDatabase::connection().isOpen())
        _syncTimer->start(5000);
    return stored;
}

bool ctimeLessThan(FileCacheItem *a, FileCacheItem *b) { return a->created() < b->created(); }

void FileCache::sync() { sync(false); }

void FileCache::lazySync() { _syncTimer->start(1000); }

void FileCache::sync(bool finishSession)
{
    _syncTimer->stop();
    auto       db    = FileDatabase::connection();
    const bool batch = !_inTransaction && db.isOpen();
    if (batch && !db.transaction()) {
        _syncTimer->start(5000);
        return;
    }
    if (batch)
        _inTransaction = true;

    QList<QPointer<FileCacheItem>> snapshot;
    QSet<FileCacheItem *>          seen;
    for (auto item : _items) {
        if (!seen.contains(item)) {
            seen.insert(item);
            snapshot.append(item);
        }
    }
    QList<FileCacheItem *> onDiskItems;
    quint64                diskSize = 0;
    bool                   ok = true;
    for (const auto &guard : snapshot) {
        auto item = guard.data();
        if (!item)
            continue;
        if (item->isExpired(finishSession)) {
            removeItem(item, false);
            continue;
        }
        item->flushToDisk();
        if (!item->isRegistered())
            ok = toRegistry(item) && ok;
        if (item->isOnDisk() && !item->isExternal()) {
            onDiskItems.append(item);
            diskSize += item->size();
        }
    }
    std::sort(onDiskItems.begin(), onDiskItems.end(), ctimeLessThan);
    const quint64 rangeBytes     = _rangeStore ? _rangeStore->diskBytes() : 0;
    const quint64 completeBudget = rangeBytes >= _fileCacheSize ? 0 : _fileCacheSize - rangeBytes;
    for (auto item : std::as_const(onDiskItems)) {
        if (diskSize <= completeBudget)
            break;
        if (!item->isDeletable())
            continue;
        const auto                    size = item->size();
        const QPointer<FileCacheItem> guard(item);
        removeItem(item, false);
        if (!guard)
            diskSize -= size;
    }
    if (batch) {
        _inTransaction = false;
        if (!ok || !db.commit()) {
            qWarning() << "Cannot save file records:" << db.lastError().text();
            db.rollback();
            for (const auto &item : snapshot) {
                if (item) {
                    item->_flags &= ~FileCacheItem::Registered;
                }
            }
            _syncTimer->start(5000);
        }
    }
    drainTrash();
    if (_rangeStore)
        _rangeStore->setDiskLimit(diskSize >= _fileCacheSize ? 0 : _fileCacheSize - diskSize);
}

bool FileCache::toRegistry(FileCacheItem *item)
{
    item->_flags &= ~FileCacheItem::Registered;
    if (!item->isOnDisk())
        return false;

    auto db = FileDatabase::connection();
    if (!db.isOpen())
        return false;
    if (item->_recordId.isEmpty())
        item->_recordId = QUuid::createUuid().toRfc4122();
    const bool transaction = !_inTransaction;
    if (transaction && !db.transaction())
        return false;
    QSqlQuery  query(db);
    query.prepare(
        QStringLiteral("INSERT INTO files(id,partition,size,created,max_age,metadata,path,placement,external_mtime) "
                       "VALUES(:id,:partition,:size,:created,:max_age,:metadata,:path,:placement,:modified) "
                       "ON CONFLICT(id) DO UPDATE SET size=excluded.size,created=excluded.created,"
                       "max_age=excluded.max_age,metadata=excluded.metadata,path=excluded.path,"
                       "placement=excluded.placement,external_mtime=excluded.external_mtime"));
    query.bindValue(QStringLiteral(":id"), item->_recordId);
    query.bindValue(QStringLiteral(":partition"), _partition);
    query.bindValue(QStringLiteral(":size"), qlonglong(item->size()));
    query.bindValue(QStringLiteral(":created"), item->created().toMSecsSinceEpoch());
    query.bindValue(QStringLiteral(":max_age"), qlonglong(item->maxAge()));
    query.bindValue(QStringLiteral(":metadata"), FileDatabase::encodeMetadata(item->metadata()));
    query.bindValue(QStringLiteral(":path"), item->isExternal() ? item->_externalPath : item->fileName());
    query.bindValue(QStringLiteral(":placement"), item->isExternal() ? 1 : 0);
    query.bindValue(QStringLiteral(":modified"),
                    item->_externalModified ? QVariant(*item->_externalModified) : QVariant());
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
    if (transaction) {
        if (!ok || !db.commit()) {
            db.rollback();
            return false;
        }
    }
    if (ok) {
        item->_flags |= FileCacheItem::Registered;
    }
    return ok;
}
