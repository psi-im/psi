#include "mediachunkstore.h"
#include "filecachememory.h"
#include "filedatabase.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QPointer>
#include <QQueue>
#include <QRegularExpression>
#include <QRunnable>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStorageInfo>
#include <QTemporaryFile>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QUuid>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <limits>
#include <optional>
#include <utility>
#ifdef Q_OS_WIN
#include <io.h>
#include <qt_windows.h>
#include <winioctl.h>
#else
#include <unistd.h>
#ifdef Q_OS_MAC
#include <fcntl.h>
#endif
#endif

namespace {
bool syncPayload(QFile &file)
{
    if (!file.flush() || file.handle() == -1)
        return false;
#ifdef Q_OS_WIN
    const auto native = _get_osfhandle(file.handle());
    return native != -1 && FlushFileBuffers(reinterpret_cast<HANDLE>(native));
#else
#ifdef Q_OS_MAC
    if (fcntl(file.handle(), F_FULLFSYNC) == 0)
        return true;
#endif
    int result;
    do {
        result = fsync(file.handle());
    } while (result != 0 && errno == EINTR);
    return result == 0;
#endif
}

bool covers(const MediaFileRanges &ranges, quint64 offset, quint64 length)
{
    for (const auto &r : ranges)
        if (r.first <= offset && offset - r.first <= r.second && length <= r.second - (offset - r.first))
            return true;
    return !length;
}
void insertRange(MediaFileRanges &ranges, quint64 offset, quint64 length)
{
    if (!length)
        return;
    ranges.append({ offset, length });
    std::sort(ranges.begin(), ranges.end());
    MediaFileRanges merged;
    for (const auto &r : ranges) {
        if (!merged.isEmpty() && r.first <= merged.last().first + merged.last().second)
            merged.last().second
                = std::max(merged.last().first + merged.last().second, r.first + r.second) - merged.last().first;
        else
            merged.append(r);
    }
    ranges = std::move(merged);
}
void removeRange(MediaFileRanges &ranges, quint64 offset, quint64 length)
{
    MediaFileRanges remaining;
    const auto      end = offset + length;
    for (const auto &r : ranges) {
        const auto rend = r.first + r.second;
        if (rend <= offset || r.first >= end)
            remaining.append(r);
        else {
            if (r.first < offset)
                remaining.append({ r.first, offset - r.first });
            if (rend > end)
                remaining.append({ end, rend - end });
        }
    }
    ranges = std::move(remaining);
}
quint64 bytesIn(const MediaFileRanges &ranges)
{
    quint64 total = 0;
    for (const auto &r : ranges)
        total += r.second;
    return total;
}
bool validKey(const QString &key)
{
    static const QRegularExpression regex(QStringLiteral("^[0-9a-f]{64}$"));
    return regex.match(key).hasMatch();
}
QString checksumId(const MediaFileChecksum &h)
{
    return QString::number(h.offset) + ':' + QString::number(h.length) + ':' + h.algorithm + ':'
        + QString::fromLatin1(h.digest.toHex());
}
class HashRange : public QIODevice {
public:
    HashRange(QFile &file, quint64 offset, quint64 length, std::shared_ptr<std::atomic_bool> cancelled = {}) :
        file(file), left(length), cancelled(std::move(cancelled))
    {
        if (file.seek(qint64(offset)))
            QIODevice::open(ReadOnly);
    }
    bool   isSequential() const override { return true; }
    bool   atEnd() const override { return !left; }
    qint64 readData(char *out, qint64 count) override
    {
        if (cancelled && cancelled->load())
            return -1;
        const auto got = file.read(out, qint64(std::min<quint64>(quint64(count), left)));
        if (got > 0)
            left -= quint64(got);
        return got;
    }
    qint64 writeData(const char *, qint64) override { return -1; }
    bool   complete() const { return !left; }

private:
    QFile                            &file;
    quint64                           left;
    std::shared_ptr<std::atomic_bool> cancelled;
};
QByteArray qtHash(const QString &algorithm, QIODevice *device)
{
    static const QHash<QString, QCryptographicHash::Algorithm> algorithms {
        { "sha-1", QCryptographicHash::Sha1 },
        { "sha-256", QCryptographicHash::Sha256 },
        { "sha-512", QCryptographicHash::Sha512 },
        { "sha3-256", QCryptographicHash::Sha3_256 },
        { "sha3-512", QCryptographicHash::Sha3_512 }
    };
    const auto it = algorithms.constFind(algorithm);
    if (it == algorithms.cend())
        return {};
    QCryptographicHash hash(*it);
    return hash.addData(device) ? hash.result() : QByteArray();
}
class DiskWorker : public QObject {
    Q_OBJECT
    struct Entry {
        QString               key;
        quint64               revision = 0;
        MediaFileState        state;
        QByteArray            id;
        quint64               age       = 0;
        quint64               accounted = 0;
        QSet<QString>         verified;
        MediaFileRanges       checkpointed;
        std::optional<qint64> modified;
        bool                  dirty = false, payloadDirty = false;
    };
    static constexpr int MaxFiles = 2048, MaxRanges = 4096, MaxChecksums = 4096;

public:
    DiskWorker(QString partition, QString directory, QString databasePath, quint64 limit,
               MediaChunkStore::Hasher hasher) :
        partition_(std::move(partition)), directory_(std::move(directory)), databasePath_(std::move(databasePath)),
        limit_(limit), hasher_(hasher ? std::move(hasher) : qtHash)
    {
        hashPool_ = new QThreadPool(this);
        hashPool_->setMaxThreadCount(1);
    }
    void initialize()
    {
        QDir().mkpath(directory_);
        auto db = database();
        if (!db.isOpen())
            return;
        QSqlQuery query(db);
        // A withdrawal must survive power loss before bytes can be overwritten.
        if (!query.exec(QStringLiteral("PRAGMA synchronous=FULL"))
            || !query.exec(QStringLiteral("PRAGMA fullfsync=ON")))
            return;
        timer_ = new QTimer(this);
        timer_->setSingleShot(true);
        timer_->setInterval(5000);
        connect(timer_, &QTimer::timeout, this, [this] { checkpointAll(); });
        query.prepare(QStringLiteral("SELECT id,resource_key,size,path,metadata,complete,error,payload_modified "
                                     "FROM files WHERE partition=:partition AND resource_key IS NOT NULL "
                                     "ORDER BY created LIMIT :limit"));
        query.bindValue(QStringLiteral(":partition"), partition_);
        query.bindValue(QStringLiteral(":limit"), MaxFiles);
        if (!query.exec())
            return;
        QHash<QByteArray, QString> keys;
        while (query.next()) {
            const auto key  = query.value(1).toString();
            const auto size = query.value(2).toLongLong();
            const auto path = query.value(3).toString();
            if (!validKey(key) || size < 0 || !FileDatabase::safePayloadName(path))
                continue;
            Entry entry;
            entry.key              = key;
            entry.id               = query.value(0).toByteArray();
            entry.state.generation = entry.id;
            entry.state.size       = quint64(size);
            entry.state.path       = directory_ + QLatin1Char('/') + path;
            entry.state.metadata   = FileDatabase::decodeMetadata(query.value(4).toByteArray());
            entry.state.complete   = query.value(5).toBool();
            entry.state.error      = query.value(6).toString();
            if (!query.value(7).isNull())
                entry.modified = query.value(7).toLongLong();
            entry.age = ++age_;
            keys.insert(entry.id, key);
            entries_.insert(key, std::move(entry));
        }
        query.finish();
        query.prepare(
            QStringLiteral("SELECT r.file_id,r.offset,r.length FROM file_ranges r "
                           "JOIN files f ON f.id=r.file_id WHERE f.partition=:partition ORDER BY r.file_id,r.offset"));
        query.bindValue(QStringLiteral(":partition"), partition_);
        if (!query.exec()) {
            entries_.clear();
            return;
        }
        while (query.next()) {
            auto it = entries_.find(keys.value(query.value(0).toByteArray()));
            if (it == entries_.end())
                continue;
            const auto offset = query.value(1).toLongLong(), length = query.value(2).toLongLong();
            if (offset < 0 || length <= 0 || quint64(offset) >= it->state.size
                || quint64(length) > it->state.size - quint64(offset) || it->state.ranges.size() >= MaxRanges) {
                it->state.error = QStringLiteral("Invalid cached file ranges");
                continue;
            }
            insertRange(it->state.ranges, quint64(offset), quint64(length));
        }
        query.finish();
        query.prepare(
            QStringLiteral("SELECT c.file_id,c.offset,c.length,c.algorithm,c.digest,c.verified "
                           "FROM file_checksums c JOIN files f ON f.id=c.file_id WHERE f.partition=:partition"));
        query.bindValue(QStringLiteral(":partition"), partition_);
        if (!query.exec()) {
            entries_.clear();
            return;
        }
        while (query.next()) {
            auto it = entries_.find(keys.value(query.value(0).toByteArray()));
            if (it == entries_.end())
                continue;
            const auto        offset = query.value(1).toLongLong(), length = query.value(2).toLongLong();
            MediaFileChecksum h { quint64(offset), quint64(length), query.value(3).toString(),
                                  query.value(4).toByteArray() };
            if (offset < 0 || length < 0 || quint64(offset) > it->state.size || (!length && it->state.size)
                || quint64(length) > it->state.size - quint64(offset) || h.algorithm.isEmpty() || h.digest.isEmpty()
                || it->state.checksums.size() >= MaxChecksums) {
                it->state.error = QStringLiteral("Invalid cached file checksums");
                continue;
            }
            it->state.checksums.append(h);
            if (query.value(5).toBool())
                it->verified.insert(checksumId(h));
        }
        query.finish();
        ready_ = true;
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            it->checkpointed = it->state.ranges;
            validatePayload(*it);
            updateComplete(*it); // No payload hashing during startup.
            publish(it.key());
        }
        FileDatabase::drainTrash(db, partition_, directory_);
        trim({});
    }
    void shutdown()
    {
        shuttingDown_ = true;
        if (hashCancelled_)
            hashCancelled_->store(true);
        hashPool_->waitForDone();
        hashQueue_.clear();
        queuedHashes_.clear();
        if (!hashKey_.isEmpty())
            pin(hashKey_, false);
        checkpointAll();
        if (timer_)
            timer_->stop();
        FileDatabase::closeConnection(databasePath_);
    }
    void checkpoint(quint64 id)
    {
        finish(id, {},
               checkpointAll() ? MediaChunkError() : MediaChunkError(QStringLiteral("Cannot checkpoint cached files")));
    }
    void setDiskLimit(quint64 limit)
    {
        limit_ = limit;
        trim({});
    }
    void configure(quint64 id, QString key, quint64 size, QList<MediaFileChecksum> checksums, QVariantMap metadata)
    {
        auto entry = get(key, size);
        if (!entry) {
            finish(id, {}, QStringLiteral("Cannot register shared file identity or size"));
            return;
        }
        validatePayload(*entry);
        if (!ensurePayload(*entry)) {
            publish(key);
            finish(id, {}, QStringLiteral("Cannot create shared file payload"));
            return;
        }
        for (auto it = metadata.cbegin(); it != metadata.cend(); ++it) {
            if (entry->state.metadata.value(it.key()) != it.value()) {
                entry->state.metadata.insert(it.key(), it.value());
                dirty(*entry);
            }
        }
        MediaChunkError error;
        for (const auto &h : checksums) {
            if ((!h.length && size) || h.offset > size || h.length > size - h.offset || h.digest.isEmpty()
                || h.algorithm.isEmpty() || entry->state.checksums.size() >= MaxChecksums) {
                entry->state.error    = QStringLiteral("Invalid shared file checksum range");
                entry->state.complete = false;
                error                 = entry->state.error;
                dirty(*entry);
                break;
            }
            bool found = false;
            for (const auto &old : entry->state.checksums) {
                if (old.offset == h.offset && old.length == h.length && old.algorithm == h.algorithm) {
                    if (old.digest == h.digest)
                        found = true;
                    else {
                        removeRange(entry->state.ranges, h.offset, h.length);
                        entry->verified.clear();
                        entry->state.error    = QStringLiteral("Conflicting checksums for shared file range");
                        entry->state.complete = false;
                        error                 = { MediaChunkError::IntegrityError, entry->state.error };
                        dirty(*entry);
                    }
                }
            }
            if (!found) {
                entry->state.checksums.append(h);
                entry->state.complete = false;
                dirty(*entry);
            }
        }
        if (error.isEmpty())
            error = verify(*entry);
        if (entry->dirty && !save(key, *entry)) {
            entry->state.complete = false;
            error                 = QStringLiteral("Cannot checkpoint shared file metadata");
        }
        publish(key);
        trim(key);
        finish(id, {}, error);
    }
    void read(quint64 id, QString key, quint64 size, quint64 offset, quint64 length)
    {
        if (!ready_ && validKey(key) && size <= quint64(std::numeric_limits<qint64>::max()) && offset <= size
            && length <= size - offset && length <= MediaChunkMaxSize) {
            finish(id, {}, {}); // An unavailable cache is a miss; a subsequent store still reports the I/O failure.
            return;
        }
        auto entry = get(key, size);
        if (!entry || offset > size || length > size - offset || length > MediaChunkMaxSize) {
            finish(id, {}, QStringLiteral("Invalid shared file range"));
            return;
        }
        const bool beforeComplete = entry->state.complete;
        auto       error          = verify(*entry);
        if (!error.isEmpty()) {
            if (entry->dirty)
                save(key, *entry);
            publish(key);
            finish(id, {}, error);
            return;
        }
        if (!covers(entry->state.ranges, offset, length)) {
            finish(id, {}, {});
            return;
        }
        QFile      file(entry->state.path);
        QByteArray bytes;
        if (file.open(QIODevice::ReadOnly) && file.seek(qint64(offset)))
            bytes = file.read(qint64(length));
        if (quint64(bytes.size()) != length) {
            removeRange(entry->state.ranges, offset, length);
            entry->verified.clear();
            entry->state.complete = false;
            dirty(*entry);
            save(key, *entry);
            bytes.clear();
            publish(key);
        } else if (entry->state.complete && entry->dirty && !save(key, *entry)) {
            entry->state.complete = false;
            publish(key);
            finish(id, {}, QStringLiteral("Cannot checkpoint completed shared file"));
            return;
        }
        accessed(*entry);
        if (beforeComplete != entry->state.complete)
            publish(key);
        finish(id, bytes, {}, entry->id);
    }
    void write(quint64 id, QString key, quint64 size, quint64 offset, QByteArray bytes)
    {
        if (offset >= size || bytes.isEmpty() || quint64(bytes.size()) > size - offset
            || quint64(bytes.size()) > MediaChunkMaxSize) {
            finish(id, {}, QStringLiteral("Invalid shared file write"));
            return;
        }
        auto entry = get(key, size);
        if (!entry) {
            finish(id, {}, { MediaChunkError::StorageUnavailable, QStringLiteral("Shared file storage unavailable") });
            return;
        }
        validatePayload(*entry);
        if (!entry->state.error.isEmpty()) {
            publish(key);
            finish(id, {}, { MediaChunkError::IntegrityError, entry->state.error });
            return;
        }
        auto nextRanges = entry->state.ranges;
        insertRange(nextRanges, offset, quint64(bytes.size()));
        if (nextRanges.size() > MaxRanges) {
            finish(id, {}, QStringLiteral("Too many cached file ranges"));
            return;
        }
        ++entry->revision;
        if (hashKey_ == key && hashCancelled_)
            hashCancelled_->store(true);
        bool overwrite = false;
        for (const auto &r : entry->checkpointed)
            overwrite |= r.first < offset + quint64(bytes.size()) && offset < r.first + r.second;
        removeRange(entry->state.ranges, offset, quint64(bytes.size()));
        for (const auto &h : entry->state.checksums)
            if (h.offset < offset + quint64(bytes.size()) && offset < h.offset + h.length)
                entry->verified.remove(checksumId(h));
        entry->state.complete = false;
        dirty(*entry);
        // Only previously checkpointed bytes require a durable withdrawal.
        if (overwrite && !save(key, *entry)) {
            publish(key);
            finish(id, {},
                   { MediaChunkError::StorageUnavailable, QStringLiteral("Cannot withdraw cached file range") });
            return;
        }
        const QStorageInfo volume(directory_);
        constexpr qint64   reserve = 16 * 1024 * 1024;
        if (volume.isValid() && volume.bytesAvailable() >= 0 && volume.bytesAvailable() < bytes.size() + reserve) {
            publish(key);
            finish(id, {},
                   { MediaChunkError::StorageUnavailable,
                     QStringLiteral("Not enough space to cache the file; playback can continue in memory") });
            return;
        }
        if (!ensurePayload(*entry)) {
            publish(key);
            finish(id, {},
                   { MediaChunkError::StorageUnavailable, QStringLiteral("Cannot create shared file payload") });
            return;
        }
        QFile file(entry->state.path);
        if (!file.open(QIODevice::ReadWrite | QIODevice::ExistingOnly)) {
            publish(key);
            finish(id, {},
                   { MediaChunkError::StorageUnavailable,
                     QStringLiteral("Cannot open shared file payload: ") + file.errorString() });
            return;
        }
        if (offset > quint64(file.size()) && !sparse(file)) {
            publish(key);
            finish(
                id, {},
                { MediaChunkError::StorageUnavailable,
                  QStringLiteral("This filesystem cannot cache sparse file ranges; playback can continue in memory") });
            return;
        }
        entry->payloadDirty = true;
        if (!file.seek(qint64(offset)) || file.write(bytes) != bytes.size() || !file.flush()) {
            publish(key);
            finish(id, {},
                   { MediaChunkError::StorageUnavailable,
                     QStringLiteral("Cannot write shared file payload: ") + file.errorString() });
            return;
        }
        file.close();
        insertRange(entry->state.ranges, offset, quint64(bytes.size()));
        auto error = verify(*entry);
        if ((entry->state.complete || !error.isEmpty()) && !save(key, *entry)) {
            entry->state.complete = false;
            if (error.code() != MediaChunkError::IntegrityError)
                error
                    = { MediaChunkError::StorageUnavailable, QStringLiteral("Cannot checkpoint shared file payload") };
        }
        accessed(*entry);
        publish(key);
        trim(key);
        finish(id, error.isEmpty() ? bytes : QByteArray(), error);
    }
    void verifyBytes(quint64 id, quint64 offset, QByteArray bytes, QList<MediaFileChecksum> checksums,
                     QByteArray generation)
    {
        for (const auto &h : checksums) {
            if (h.offset < offset || h.offset - offset > quint64(bytes.size())
                || h.length > quint64(bytes.size()) - (h.offset - offset))
                continue;
            auto    span = bytes.mid(qsizetype(h.offset - offset), qsizetype(h.length));
            QBuffer input(&span);
            input.open(QIODevice::ReadOnly);
            const auto actual = hasher_(h.algorithm, &input);
            if (actual.isEmpty() || actual != h.digest) {
                finish(id, {},
                       { MediaChunkError::IntegrityError, QStringLiteral("Shared file checksum verification failed") });
                return;
            }
        }
        finish(id, std::move(bytes), {}, generation);
    }
    void resume(quint64 id, const QString &key)
    {
        auto it = entries_.find(key);
        if (it == entries_.end()) {
            finish(id, {}, {});
            return;
        }
        it->state.error.clear();
        it->state.completionError.clear();
        dirty(*it);
        auto error = verify(*it);
        if (!save(key, *it)) {
            it->state.complete = false;
            error              = QStringLiteral("Cannot checkpoint shared file metadata");
        }
        publish(key);
        finish(id, {}, error);
    }
    void touch(const QString &key)
    {
        auto it = entries_.find(key);
        if (it != entries_.end())
            accessed(*it);
    }
    void pin(const QString &key, bool active)
    {
        if (active)
            ++pins_[key];
        else if (pins_.value(key) <= 1) {
            pins_.remove(key);
            trim({});
        } else
            --pins_[key];
    }
    void removeIncomplete(quint64 id, qint64 olderThan)
    {
        if (!ready_) {
            emit cleaned(id, 0, QStringLiteral("File storage unavailable"));
            return;
        }
        auto       db = database();
        QString    error;
        const auto keys = entries_.keys();
        for (const auto &key : keys) {
            const auto entry      = entries_.value(key);
            const auto lastAccess = entry.state.metadata
                                        .value(QStringLiteral("last-access"),
                                               QFileInfo(entry.state.path).lastModified().toSecsSinceEpoch())
                                        .toLongLong();
            if (entry.state.complete || pins_.contains(key)
                || entry.state.metadata.value(QStringLiteral("published")).toBool()
                || (olderThan && lastAccess >= olderThan))
                continue;
            if (!FileDatabase::retireFile(db, entry.id, partition_, QFileInfo(entry.state.path).fileName())) {
                error = QStringLiteral("Cannot retire an incomplete file");
                break;
            }
            bytes_ -= entry.accounted;
            entries_.remove(key);
            hashQueue_.removeAll(key);
            queuedHashes_.remove(key);
            emit changed(key, {}, diskBytes());
        }
        const auto freed = FileDatabase::drainTrash(db, partition_, directory_);
        emit       cleaned(id, freed, error);
    }
    void invalidate(quint64 id, QString key, quint64 size, quint64 offset, quint64 length, QString message)
    {
        auto entry = get(key, size);
        if (!entry || offset >= size || !length || length > size - offset) {
            finish(id, {}, QStringLiteral("Invalid shared file invalidation"));
            return;
        }
        ++entry->revision;
        if (hashKey_ == key && hashCancelled_)
            hashCancelled_->store(true);
        removeRange(entry->state.ranges, offset, length);
        entry->state.complete = false;
        entry->state.error    = message;
        entry->verified.clear();
        dirty(*entry);
        const bool saved = save(key, *entry);
        publish(key);
        finish(id, {}, saved ? MediaChunkError() : MediaChunkError(QStringLiteral("Cannot checkpoint invalidation")));
    }
signals:
    void changed(QString key, MediaFileState state, quint64 diskBytes);
    void cleaned(quint64 id, quint64 freed, QString error);
    void completed(quint64 id, QByteArray bytes, MediaChunkError error, quint64 diskBytes, QByteArray generation);

private:
    QSqlDatabase database() const { return FileDatabase::connection(databasePath_); }
    void         dirty(Entry &entry)
    {
        entry.dirty = true;
        if (timer_ && !timer_->isActive())
            timer_->start();
    }
    bool sparse(QFile &file)
    {
#ifdef Q_OS_WIN
        const auto native   = _get_osfhandle(file.handle());
        DWORD      returned = 0;
        return native != -1
            && DeviceIoControl(reinterpret_cast<HANDLE>(native), FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &returned,
                               nullptr);
#else
        if (!sparse_) {
            // Probe this payload directory with bounded disk usage. SEEK_HOLE
            // alone does not prove that holes avoid allocation on a filesystem.
            QTemporaryFile probe(directory_ + QStringLiteral("/.sparse-XXXXXX"));
            const bool written   = probe.open() && probe.seek(256 * 1024) && probe.write("x", 1) == 1 && probe.flush();
            const auto allocated = written ? FileDatabase::allocatedBytes(probe.fileName()) : 0;
            sparse_              = written && allocated > 0 && allocated < 256 * 1024;
        }
        Q_UNUSED(file);
        return *sparse_;
#endif
    }
    void accessed(Entry &entry)
    {
        entry.age      = ++age_;
        const auto now = QDateTime::currentSecsSinceEpoch();
        if (now - entry.state.metadata.value(QStringLiteral("last-access")).toLongLong() >= 60) {
            entry.state.metadata.insert(QStringLiteral("last-access"), now);
            dirty(entry);
        }
    }
    bool ensurePayload(Entry &entry)
    {
        const QFileInfo info(entry.state.path);
        if (info.isSymLink())
            return false;
        if (info.exists())
            return info.isFile();
        QFile file(entry.state.path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly))
            return false;
        entry.payloadDirty = true;
        dirty(entry);
        return true;
    }
    Entry *get(const QString &key, quint64 size)
    {
        if (!ready_ || !validKey(key) || size > quint64(std::numeric_limits<qint64>::max()))
            return nullptr;
        auto it = entries_.find(key);
        if (it != entries_.end())
            return it->state.size == size && !QFileInfo(it->state.path).isSymLink() ? &*it : nullptr;
        if (entries_.size() >= MaxFiles)
            return nullptr;
        Entry entry;
        entry.key              = key;
        entry.id               = QUuid::createUuid().toRfc4122();
        entry.state.generation = entry.id;
        entry.state.size       = size;
        entry.state.path
            = directory_ + QLatin1Char('/') + QString::fromLatin1(entry.id.toHex()) + QStringLiteral(".data");
        accessed(entry);
        // Register the generation before creating its payload. A crash here is a miss.
        if (!save(key, entry))
            return nullptr;
        return &*entries_.insert(key, std::move(entry));
    }
    void validatePayload(Entry &entry)
    {
        auto           &state = entry.state;
        const QFileInfo info(state.path);
        if (!info.isFile() || info.isSymLink() || !info.isReadable()) {
            if (!state.ranges.isEmpty() || state.complete) {
                state.ranges.clear();
                entry.verified.clear();
                state.complete = false;
                state.error    = QStringLiteral("Shared file payload is missing or unreadable");
                dirty(entry);
            }
            return;
        }
        const auto actualSize = quint64(info.size());
        if (!state.ranges.isEmpty() && actualSize < state.ranges.last().first + state.ranges.last().second) {
            for (const auto &h : state.checksums)
                if (h.offset + h.length > actualSize)
                    removeRange(state.ranges, h.offset, h.length);
            removeRange(state.ranges, actualSize, state.size - std::min(state.size, actualSize));
            entry.verified.clear();
            state.complete = false;
            state.error    = QStringLiteral("Shared file payload was truncated");
            dirty(entry);
        }
        if (!entry.payloadDirty && (!entry.modified || *entry.modified != info.lastModified().toMSecsSinceEpoch())) {
            ++entry.revision;
            if (hashKey_ == entry.key && hashCancelled_)
                hashCancelled_->store(true);
            if (!entry.verified.isEmpty()) {
                entry.verified.clear();
                dirty(entry);
            }
            state.complete     = false;
            entry.modified     = info.lastModified().toMSecsSinceEpoch();
            entry.payloadDirty = true;
            dirty(entry);
        }
    }
    void updateComplete(Entry &entry)
    {
        auto           &state    = entry.state;
        const bool      previous = state.complete;
        const QFileInfo info(state.path);
        state.complete = state.error.isEmpty() && state.completionError.isEmpty() && info.isFile() && !info.isSymLink()
            && info.isReadable() && covers(state.ranges, 0, state.size);
        for (const auto &h : state.checksums)
            state.complete &= entry.verified.contains(checksumId(h));
        if (previous != state.complete)
            dirty(entry);
    }
    void queueHash(Entry &entry)
    {
        if (shuttingDown_ || queuedHashes_.contains(entry.key))
            return;
        queuedHashes_.insert(entry.key);
        hashQueue_.enqueue(entry.key);
        startHash();
    }
    void startHash()
    {
        if (shuttingDown_ || hashCancelled_)
            return;
        while (!hashQueue_.isEmpty()) {
            const auto key = hashQueue_.dequeue();
            queuedHashes_.remove(key);
            auto it = entries_.find(key);
            if (it == entries_.end() || !it->state.error.isEmpty())
                continue;
            for (const auto &h : it->state.checksums) {
                if (h.length <= MediaChunkMaxSize || it->verified.contains(checksumId(h))
                    || !covers(it->state.ranges, h.offset, h.length))
                    continue;
                hashKey_       = key;
                hashCancelled_ = std::make_shared<std::atomic_bool>(false);
                pin(key, true);
                const QPointer<DiskWorker> guard(this);
                hashPool_->start(
                    QRunnable::create([guard, key, id = it->id, revision = it->revision, path = it->state.path, h,
                                       hasher = hasher_, cancelled = hashCancelled_] {
                        QFile      file(path);
                        QByteArray actual;
                        const auto before = QFileInfo(path).lastModified();
                        if (file.open(QIODevice::ReadOnly)) {
                            HashRange span(file, h.offset, h.length, cancelled);
                            actual = hasher(h.algorithm, &span);
                            if (!span.complete())
                                actual.clear();
                        }
                        const bool unchanged = before.isValid() && before == QFileInfo(path).lastModified();
                        if (guard)
                            QMetaObject::invokeMethod(
                                guard,
                                [guard, key, id, revision, h, actual, unchanged] {
                                    if (guard)
                                        guard->hashFinished(key, id, revision, h, actual, unchanged);
                                },
                                Qt::QueuedConnection);
                    }));
                return;
            }
        }
    }
    void hashFinished(const QString &key, const QByteArray &id, quint64 revision, const MediaFileChecksum &h,
                      const QByteArray &actual, bool unchanged)
    {
        if (shuttingDown_)
            return;
        hashCancelled_.reset();
        hashKey_.clear();
        auto it = entries_.find(key);
        if (it != entries_.end() && it->id == id && it->revision == revision && it->state.error.isEmpty()) {
            if (!unchanged || actual.isEmpty() || actual != h.digest) {
                removeRange(it->state.ranges, h.offset, h.length);
                it->verified.clear();
                it->state.error = QStringLiteral("Shared file checksum verification failed (%1, offset %2, length %3)")
                                      .arg(h.algorithm)
                                      .arg(h.offset)
                                      .arg(h.length);
            } else {
                it->verified.insert(checksumId(h));
                const auto error = verify(*it);
                if (!error.isEmpty() && it->state.error.isEmpty())
                    it->state.completionError = error.message();
            }
            dirty(*it);
            updateComplete(*it);
            if (!save(key, *it)) {
                it->state.complete        = false;
                it->state.completionError = QStringLiteral("Cannot checkpoint shared file verification");
            }
            publish(key);
            queueHash(*it); // Another announced algorithm may still need a pass.
        } else if (it != entries_.end() && it->state.error.isEmpty()) {
            queueHash(*it); // The payload changed while an obsolete pass was running.
        }
        pin(key, false);
        startHash();
    }
    MediaChunkError verify(Entry &entry)
    {
        validatePayload(entry);
        auto &state = entry.state;
        if (!state.error.isEmpty()) {
            state.complete = false;
            return { MediaChunkError::IntegrityError, state.error };
        }
        QFile file(state.path);
        for (const auto &h : state.checksums) {
            if (!covers(state.ranges, h.offset, h.length) || entry.verified.contains(checksumId(h)))
                continue;
            if (h.length > MediaChunkMaxSize) {
                queueHash(entry);
                continue;
            }
            if (!file.isOpen() && !file.open(QIODevice::ReadOnly)) {
                state.complete = false;
                return QStringLiteral("Cannot open shared file for verification");
            }
            HashRange  range(file, h.offset, h.length);
            const auto actual = hasher_(h.algorithm, &range);
            if (actual.isEmpty() || !range.complete() || actual != h.digest) {
                removeRange(state.ranges, h.offset, h.length);
                entry.verified.clear();
                state.complete = false;
                state.error    = QStringLiteral("Shared file checksum verification failed (%1, offset %2, length %3)")
                                  .arg(h.algorithm)
                                  .arg(h.offset)
                                  .arg(h.length);
                dirty(entry);
                return { MediaChunkError::IntegrityError, state.error };
            }
            entry.verified.insert(checksumId(h));
            dirty(entry);
        }
        updateComplete(entry);
        if (state.complete && QFileInfo(state.path).size() != qint64(state.size)) {
            file.close();
            if (!file.open(QIODevice::ReadWrite | QIODevice::ExistingOnly) || !file.resize(qint64(state.size))
                || !file.flush()) {
                state.complete = false;
                return QStringLiteral("Cannot finalize shared file size");
            }
            entry.payloadDirty = true;
            dirty(entry);
        }
        return {};
    }
    bool save(const QString &key, Entry &entry)
    {
        auto db = database();
        if (!db.isOpen())
            return false;
        validatePayload(entry);
        const QFileInfo info(entry.state.path);
        if (info.isSymLink())
            return false;
        if (!info.exists())
            entry.payloadDirty
                = false; // Only withdrawals remain; never recreate a deleted payload while checkpointing.
        if (entry.payloadDirty) {
            QFile file(entry.state.path);
            if (!file.open(QIODevice::ReadWrite | QIODevice::ExistingOnly) || !syncPayload(file)) {
                dirty(entry);
                return false;
            }
        }
        if (!db.transaction()) {
            dirty(entry);
            return false;
        }
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "INSERT INTO files(id,partition,size,created,max_age,metadata,path,placement,"
            "resource_key,complete,payload_modified,error) "
            "VALUES(:id,:partition,:size,:created,4294967295,:metadata,:path,0,:key,:complete,:modified,:error) "
            "ON CONFLICT(id) DO UPDATE SET metadata=excluded.metadata,complete=excluded.complete,"
            "payload_modified=excluded.payload_modified,error=excluded.error"));
        query.bindValue(QStringLiteral(":id"), entry.id);
        query.bindValue(QStringLiteral(":partition"), partition_);
        query.bindValue(QStringLiteral(":size"), qlonglong(entry.state.size));
        query.bindValue(QStringLiteral(":created"), QDateTime::currentMSecsSinceEpoch());
        query.bindValue(QStringLiteral(":metadata"), FileDatabase::encodeMetadata(entry.state.metadata));
        query.bindValue(QStringLiteral(":path"), QFileInfo(entry.state.path).fileName());
        query.bindValue(QStringLiteral(":key"), key);
        query.bindValue(QStringLiteral(":complete"), entry.state.complete);
        const auto modified = QFileInfo(entry.state.path).lastModified();
        query.bindValue(QStringLiteral(":modified"),
                        modified.isValid() ? QVariant(modified.toMSecsSinceEpoch()) : QVariant());
        query.bindValue(QStringLiteral(":error"), entry.state.error.isNull() ? QStringLiteral("") : entry.state.error);
        bool    ok = true;
        QString sqlError;
        auto    execute = [&] {
            if (ok && !query.exec()) {
                ok       = false;
                sqlError = query.lastError().text();
            }
        };
        execute();
        for (const auto &table : { QStringLiteral("file_ranges"), QStringLiteral("file_checksums") }) {
            query.prepare(QStringLiteral("DELETE FROM %1 WHERE file_id=:id").arg(table));
            query.bindValue(QStringLiteral(":id"), entry.id);
            execute();
        }
        query.prepare(QStringLiteral("INSERT INTO file_ranges(file_id,offset,length) VALUES(:id,:offset,:length)"));
        for (const auto &r : entry.state.ranges) {
            query.bindValue(QStringLiteral(":id"), entry.id);
            query.bindValue(QStringLiteral(":offset"), qlonglong(r.first));
            query.bindValue(QStringLiteral(":length"), qlonglong(r.second));
            execute();
        }
        query.prepare(QStringLiteral("INSERT INTO file_checksums(file_id,offset,length,algorithm,digest,verified) "
                                     "VALUES(:id,:offset,:length,:algorithm,:digest,:verified)"));
        for (const auto &h : entry.state.checksums) {
            query.bindValue(QStringLiteral(":id"), entry.id);
            query.bindValue(QStringLiteral(":offset"), qlonglong(h.offset));
            query.bindValue(QStringLiteral(":length"), qlonglong(h.length));
            query.bindValue(QStringLiteral(":algorithm"), h.algorithm);
            query.bindValue(QStringLiteral(":digest"), h.digest);
            query.bindValue(QStringLiteral(":verified"), entry.verified.contains(checksumId(h)));
            execute();
        }
        if (!ok || !db.commit()) {
            qWarning() << "Cannot checkpoint a cached file:" << (ok ? db.lastError().text() : sqlError);
            db.rollback();
            dirty(entry);
            return false;
        }
        entry.modified     = modified.isValid() ? std::optional<qint64>(modified.toMSecsSinceEpoch()) : std::nullopt;
        entry.checkpointed = entry.state.ranges;
        entry.dirty = entry.payloadDirty = false;
        return true;
    }
    bool checkpointAll()
    {
        bool ok = ready_;
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (!it->dirty)
                continue;
            it->state.completionError.clear();
            updateComplete(*it);
            const bool saved = save(it.key(), *it);
            if (!saved) {
                it->state.complete        = false;
                it->state.completionError = QStringLiteral("Cannot checkpoint shared file metadata");
            }
            publish(it.key());
            ok = saved && ok;
        }
        auto db = database();
        FileDatabase::drainTrash(db, partition_, directory_);
        return ok;
    }
    quint64 diskBytes() const { return bytes_; }
    void    publish(const QString &key)
    {
        auto it = entries_.find(key);
        if (it == entries_.end())
            return;
        const auto next = bytesIn(it->state.ranges);
        bytes_ -= it->accounted;
        bytes_ += next;
        it->accounted = next;
        emit changed(key, it->state, bytes_);
    }
    void finish(quint64 id, QByteArray bytes, MediaChunkError error, const QByteArray &generation = {})
    {
        emit completed(id, bytes, error, diskBytes(), generation);
    }
    void trim(const QString &keep)
    {
        while ((diskBytes() > limit_ || entries_.size() >= MaxFiles) && entries_.size() > 1) {
            auto oldest = entries_.end();
            for (auto it = entries_.begin(); it != entries_.end(); ++it)
                if (it.key() != keep && !pins_.contains(it.key()) && it->state.error.isEmpty()
                    && !it->state.metadata.value(QStringLiteral("published")).toBool()
                    && (it->state.complete || !it->state.metadata.value(QStringLiteral("retain-partial")).toBool())
                    && (oldest == entries_.end() || it->age < oldest->age))
                    oldest = it;
            if (oldest == entries_.end())
                break;
            const auto key = oldest.key();
            auto       db  = database();
            if (!FileDatabase::retireFile(db, oldest->id, partition_, QFileInfo(oldest->state.path).fileName()))
                break;
            bytes_ -= oldest->accounted;
            entries_.erase(oldest);
            hashQueue_.removeAll(key);
            queuedHashes_.remove(key);
            FileDatabase::drainTrash(db, partition_, directory_);
            emit changed(key, {}, diskBytes());
        }
    }
    QString                           partition_, directory_, databasePath_;
    quint64                           limit_, age_ = 0, bytes_ = 0;
    MediaChunkStore::Hasher           hasher_;
    QHash<QString, Entry>             entries_;
    QHash<QString, int>               pins_;
    QTimer                           *timer_ = nullptr;
    bool                              ready_ = false;
    std::optional<bool>               sparse_;
    QThreadPool                      *hashPool_ = nullptr;
    QQueue<QString>                   hashQueue_;
    QSet<QString>                     queuedHashes_;
    QString                           hashKey_;
    std::shared_ptr<std::atomic_bool> hashCancelled_;
    bool                              shuttingDown_ = false;
};
}

class MediaChunkStore::Private {
public:
    struct Pending {
        Completion done;
        QString    key;
        quint64    offset = 0, length = 0;
        bool       read = false, mutation = false, volatileWrite = false;
        quint64    revision = 0;
    };
    struct Volatile {
        quint64                  size     = 0;
        quint64                  revision = 0;
        QByteArray               generation;
        QList<MediaFileChecksum> checksums;
    };
    QHash<QString, Volatile> volatileFiles;
    Volatile                &volatileFile(const QString &key, quint64 size)
    {
        auto it = volatileFiles.find(key);
        if (it != volatileFiles.end() && it->size == size)
            return *it;
        if (volatileFiles.size() >= 2048) {
            const auto victim = volatileFiles.begin();
            memory->remove(victim->generation);
            volatileFiles.erase(victim);
        }
        return volatileFiles.insert(key, { size, 0, QUuid::createUuid().toRfc4122(), {} }).value();
    }
    QThread                           thread;
    DiskWorker                       *worker;
    quint64                           next = 0, bytes = 0;
    QHash<quint64, Pending>           pending;
    QHash<quint64, CleanupCompletion> cleanups;
    QHash<QString, int>               mutations;
    QHash<QString, MediaFileState>    states;
    QHash<QString, QString>           paths;
    std::unique_ptr<FileCacheMemory>  ownedMemory;
    FileCacheMemory                  *memory;
};
MediaChunkStore::MediaChunkStore(QString directory, quint64 limit, QObject *parent, Hasher hasher) :
    MediaChunkStore(QStringLiteral("ranges"), directory, directory + QStringLiteral("/files.sqlite"), limit, nullptr,
                    parent, std::move(hasher))
{
}
MediaChunkStore::MediaChunkStore(QString partition, QString directory, QString databasePath, quint64 limit,
                                 FileCacheMemory *memory, QObject *parent, Hasher hasher) :
    QObject(parent), d(new Private)
{
    if (!memory)
        d->ownedMemory = std::make_unique<FileCacheMemory>(16 * 1024 * 1024);
    d->memory = memory ? memory : d->ownedMemory.get();
    qRegisterMetaType<MediaFileState>();
    qRegisterMetaType<MediaChunkError>();
    d->worker
        = new DiskWorker(std::move(partition), std::move(directory), std::move(databasePath), limit, std::move(hasher));
    d->thread.setObjectName(QStringLiteral("shared-media-cache"));
    d->worker->moveToThread(&d->thread);
    connect(&d->thread, &QThread::finished, d->worker, &QObject::deleteLater);
    connect(d->worker, &DiskWorker::changed, this, [this](QString key, MediaFileState state, quint64 bytes) {
        d->bytes            = bytes;
        const auto previous = d->states.value(key);
        if (state.path.isEmpty() || !state.error.isEmpty()) {
            d->memory->remove(d->volatileFiles.value(key).generation);
            d->volatileFiles.remove(key);
        }
        if (previous.generation != state.generation || !state.error.isEmpty() || (previous.complete && !state.complete))
            d->memory->remove(previous.generation);
        d->paths.remove(d->states.value(key).path);
        if (!state.path.isEmpty()) {
            d->paths.insert(state.path, key);
            d->states.insert(key, std::move(state));
        } else
            d->states.remove(key);
        emit fileChanged(key);
        emit availabilityChanged();
    });
    connect(d->worker, &DiskWorker::completed, this,
            [this](quint64 id, QByteArray bytes, MediaChunkError error, quint64 total, QByteArray generation) {
                d->bytes     = total;
                auto request = d->pending.take(id);
                if (request.mutation && --d->mutations[request.key] == 0)
                    d->mutations.remove(request.key);
                const auto state = d->states.value(request.key);
                if (request.volatileWrite && error.isEmpty()) {
                    const auto info = d->volatileFiles.value(request.key);
                    if (info.generation == generation && info.revision == request.revision
                        && !d->mutations.contains(request.key) && state.error.isEmpty())
                        d->memory->put(generation, request.offset, bytes);
                    else
                        error = { MediaChunkError::Cancelled, QStringLiteral("Volatile file bytes were invalidated") };
                    if (!error.isEmpty())
                        bytes.clear();
                }
                if (request.read && error.isEmpty() && !bytes.isEmpty() && generation == state.generation
                    && !d->mutations.contains(request.key) && state.error.isEmpty()
                    && covers(state.ranges, request.offset, request.length))
                    d->memory->put(generation, request.offset, bytes);
                if (request.done)
                    request.done(std::move(bytes), error);
            });
    connect(d->worker, &DiskWorker::cleaned, this, [this](quint64 id, quint64 freed, const QString &error) {
        const auto done = d->cleanups.take(id);
        if (done)
            done(freed, error);
    });
    d->thread.start();
    QMetaObject::invokeMethod(d->worker, [worker = d->worker] { worker->initialize(); }, Qt::QueuedConnection);
}
MediaChunkStore::~MediaChunkStore()
{
    QMetaObject::invokeMethod(d->worker, [worker = d->worker] { worker->shutdown(); }, Qt::BlockingQueuedConnection);
    d->thread.quit();
    d->thread.wait();
}
quint64        MediaChunkStore::diskBytes() const { return d->bytes; }
MediaFileState MediaChunkStore::state(const QString &key) const { return d->states.value(key); }
bool           MediaChunkStore::contains(const QString &key, quint64 offset, quint64 length) const
{
    const auto state = d->states.value(key);
    return !state.path.isEmpty() && state.error.isEmpty() && covers(state.ranges, offset, length);
}
bool MediaChunkStore::isCompleteFile(const QString &path) const
{
    const auto state = d->states.value(d->paths.value(path));
    return state.path == path && state.complete && state.error.isEmpty()
        && !d->mutations.contains(d->paths.value(path));
}
void MediaChunkStore::setDiskLimit(quint64 limit)
{
    QMetaObject::invokeMethod(
        d->worker, [worker = d->worker, limit] { worker->setDiskLimit(limit); }, Qt::QueuedConnection);
}
void MediaChunkStore::checkpoint(Completion done)
{
    const auto id = ++d->next;
    d->pending.insert(id, { std::move(done) });
    QMetaObject::invokeMethod(d->worker, [worker = d->worker, id] { worker->checkpoint(id); }, Qt::QueuedConnection);
}
void MediaChunkStore::removeIncomplete(qint64 olderThan, CleanupCompletion done)
{
    const auto id = ++d->next;
    d->cleanups.insert(id, std::move(done));
    QMetaObject::invokeMethod(
        d->worker, [worker = d->worker, id, olderThan] { worker->removeIncomplete(id, olderThan); },
        Qt::QueuedConnection);
}
void MediaChunkStore::configure(QString key, quint64 size, QList<MediaFileChecksum> hashes, QVariantMap metadata,
                                Completion done)
{
    auto &volatileFile = d->volatileFile(key, size);
    for (const auto &h : hashes) {
        bool known = false;
        for (const auto &old : volatileFile.checksums)
            known |= checksumId(h) == checksumId(old);
        if (!known && volatileFile.checksums.size() < 4096) {
            volatileFile.checksums.append(h);
            d->memory->remove(volatileFile.generation);
            volatileFile.generation = QUuid::createUuid().toRfc4122();
        }
    }
    bool requirementsChanged = false;
    auto current             = d->states.find(key);
    if (current != d->states.end()) {
        for (const auto &h : hashes) {
            bool known = false;
            for (const auto &old : current->checksums)
                if (checksumId(h) == checksumId(old)) {
                    known = true;
                    break;
                }
            if (!known) {
                requirementsChanged = true;
                current->complete   = false;
                break;
            }
        }
    }
    const auto id = requirementsChanged ? beginMutation(key, std::move(done)) : ++d->next;
    if (!requirementsChanged)
        d->pending.insert(id, { std::move(done), key });
    QMetaObject::invokeMethod(
        d->worker,
        [worker = d->worker, id, key, size, hashes = std::move(hashes), metadata = std::move(metadata)] {
            worker->configure(id, key, size, hashes, metadata);
        },
        Qt::QueuedConnection);
}
quint64 MediaChunkStore::availableLength(const QString &key, quint64 size, quint64 offset, quint64 maximum) const
{
    if (offset >= size || d->mutations.contains(key))
        return 0;
    const auto state = d->states.value(key);
    if (!state.error.isEmpty())
        return 0;
    maximum        = std::min(maximum, size - offset);
    quint64 length = 0;
    if (state.size == size)
        for (const auto &range : state.ranges)
            if (range.first <= offset && offset - range.first < range.second) {
                length = std::min(maximum, range.second - (offset - range.first));
                break;
            }
    const auto info = d->volatileFiles.value(key);
    if (info.size == size)
        length = std::max(length, d->memory->available(info.generation, offset, maximum));
    return length;
}
bool MediaChunkStore::containsCached(const QString &key, quint64 size, quint64 offset, quint64 length) const
{
    if (size > quint64(std::numeric_limits<qint64>::max()) || offset > size || length > size - offset
        || length > MediaChunkMaxSize)
        return false;
    const auto state = d->states.value(key);
    if (d->mutations.contains(key) || !state.error.isEmpty())
        return false;
    const auto volatileFile = d->volatileFiles.value(key);
    return (state.size == size && covers(state.ranges, offset, length)
            && d->memory->contains(state.generation, offset, length))
        || (volatileFile.size == size && d->memory->contains(volatileFile.generation, offset, length));
}
quint64 MediaChunkStore::beginMutation(const QString &key, Completion done, quint64 offset, quint64 length)
{
    const auto generation   = d->states.value(key).generation;
    auto       volatileFile = d->volatileFiles.find(key);
    if (volatileFile != d->volatileFiles.end())
        ++volatileFile->revision;
    const auto volatileGeneration = d->volatileFiles.value(key).generation;
    d->memory->remove(volatileGeneration, offset, length);
    if (length == std::numeric_limits<quint64>::max())
        d->memory->remove(generation);
    else
        d->memory->remove(generation, offset, length);
    ++d->mutations[key];
    const auto id = ++d->next;
    d->pending.insert(id, { std::move(done), key, 0, 0, false, true });
    return id;
}
void MediaChunkStore::dispatchRead(quint64 id, const QString &key, quint64 size, quint64 offset, quint64 length)
{
    QMetaObject::invokeMethod(
        d->worker, [worker = d->worker, id, key, size, offset, length] { worker->read(id, key, size, offset, length); },
        Qt::QueuedConnection);
}
void MediaChunkStore::read(QString key, quint64 size, quint64 offset, quint64 length, Completion done)
{
    const auto id = ++d->next;
    d->pending.insert(id, { std::move(done), key, offset, length, true, false });
    if (!containsCached(key, size, offset, length)) {
        dispatchRead(id, key, size, offset, length);
        return;
    }
    QTimer::singleShot(0, this, [this, id, key, size, offset, length] {
        if (containsCached(key, size, offset, length)) {
            const auto generation = d->states.value(key).generation;
            auto       bytes      = d->memory->get(generation, offset, length);
            if (!bytes)
                bytes = d->memory->get(d->volatileFiles.value(key).generation, offset, length);
            if (bytes) {
                touch(key);
                auto request = d->pending.take(id);
                if (request.done)
                    request.done(std::move(*bytes), {});
                return;
            }
        }
        dispatchRead(id, key, size, offset, length);
    });
}
void MediaChunkStore::write(QString key, quint64 size, quint64 offset, QByteArray bytes, Completion done)
{
    const auto id = beginMutation(key, std::move(done), offset, quint64(bytes.size()));
    QMetaObject::invokeMethod(
        d->worker,
        [worker = d->worker, id, key, size, offset, bytes = std::move(bytes)] {
            worker->write(id, key, size, offset, bytes);
        },
        Qt::QueuedConnection);
}
void MediaChunkStore::cacheVolatile(QString key, quint64 size, quint64 offset, QByteArray bytes, Completion done)
{
    if (!validKey(key) || offset >= size || bytes.isEmpty() || quint64(bytes.size()) > size - offset
        || quint64(bytes.size()) > MediaChunkMaxSize) {
        QTimer::singleShot(0, this, [done] { done({}, QStringLiteral("Invalid volatile file range")); });
        return;
    }
    const auto info      = d->volatileFile(key, size);
    auto       checksums = info.checksums;
    for (const auto &h : d->states.value(key).checksums)
        checksums.append(h);
    const auto id = ++d->next;
    d->pending.insert(id, { std::move(done), key, offset, quint64(bytes.size()), false, false, true, info.revision });
    QMetaObject::invokeMethod(
        d->worker,
        [worker = d->worker, id, offset, bytes = std::move(bytes), checksums, generation = info.generation] {
            worker->verifyBytes(id, offset, bytes, checksums, generation);
        },
        Qt::QueuedConnection);
}
void MediaChunkStore::resume(QString key, Completion done)
{
    const auto id = beginMutation(key, std::move(done));
    QMetaObject::invokeMethod(
        d->worker, [worker = d->worker, id, key] { worker->resume(id, key); }, Qt::QueuedConnection);
}
void MediaChunkStore::touch(const QString &key)
{
    QMetaObject::invokeMethod(d->worker, [worker = d->worker, key] { worker->touch(key); }, Qt::QueuedConnection);
}
void MediaChunkStore::pin(const QString &key, bool active)
{
    QMetaObject::invokeMethod(
        d->worker, [worker = d->worker, key, active] { worker->pin(key, active); }, Qt::QueuedConnection);
}
void MediaChunkStore::invalidate(QString key, quint64 size, quint64 offset, quint64 length, QString message)
{
    const auto id = beginMutation(key, {});
    QMetaObject::invokeMethod(
        d->worker,
        [worker = d->worker, id, key, size, offset, length, message] {
            worker->invalidate(id, key, size, offset, length, message);
        },
        Qt::QueuedConnection);
}
#include "mediachunkstore.moc"
