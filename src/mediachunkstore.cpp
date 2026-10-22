#include "mediachunkstore.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QThread>
#include <algorithm>
#include <limits>
#include <utility>
#ifdef Q_OS_WIN
#include <io.h>
#include <qt_windows.h>
#include <winioctl.h>
#endif

namespace {
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
    HashRange(QFile &file, quint64 offset, quint64 length) : file(file), left(length)
    {
        if (file.seek(qint64(offset)))
            QIODevice::open(ReadOnly);
    }
    bool   isSequential() const override { return true; }
    bool   atEnd() const override { return !left; }
    qint64 readData(char *out, qint64 count) override
    {
        const auto got = file.read(out, qint64(std::min<quint64>(quint64(count), left)));
        if (got > 0)
            left -= quint64(got);
        return got;
    }
    qint64 writeData(const char *, qint64) override { return -1; }
    bool   complete() const { return !left; }

private:
    QFile  &file;
    quint64 left;
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
        return { };
    QCryptographicHash hash(*it);
    return hash.addData(device) ? hash.result() : QByteArray();
}
class DiskWorker : public QObject {
    Q_OBJECT
    struct Entry {
        MediaFileState state;
        quint64        age = 0;
        QSet<QString>  verified;
    };

public:
    DiskWorker(QString directory, quint64 limit, MediaChunkStore::Hasher hasher) :
        directory_(std::move(directory)), limit_(limit), hasher_(hasher ? std::move(hasher) : qtHash)
    {
    }
    void initialize()
    {
        QDir dir(directory_);
        dir.mkpath(QStringLiteral("."));
        for (const auto &info : dir.entryInfoList({ QStringLiteral("*.json") }, QDir::Files | QDir::NoSymLinks,
                                                  QDir::Time | QDir::Reversed)) {
            const auto key = info.completeBaseName();
            if (!validKey(key) || info.size() > 4 * 1024 * 1024)
                continue;
            QFile metadata(info.filePath());
            if (!metadata.open(QIODevice::ReadOnly))
                continue;
            const auto json = QJsonDocument::fromJson(metadata.readAll()).object();
            bool       ok   = false;
            const auto size = json.value("size").toString().toULongLong(&ok);
            if (!ok || size > quint64(std::numeric_limits<qint64>::max()) || json.value("version").toInt() != 2)
                continue;
            Entry entry;
            entry.state.size     = size;
            entry.state.path     = dataPath(key);
            entry.state.metadata = json.value("metadata").toObject().toVariantMap();
            entry.state.error    = json.value("error").toString();
            for (const auto &value : json.value("ranges").toArray()) {
                const auto range = value.toArray();
                if (range.size() != 2) {
                    ok = false;
                    break;
                }
                bool       a = false, b = false;
                const auto offset = range.at(0).toString().toULongLong(&a);
                const auto length = range.at(1).toString().toULongLong(&b);
                if (!a || !b || offset >= size || !length || length > size - offset) {
                    ok = false;
                    break;
                }
                insertRange(entry.state.ranges, offset, length);
            }
            const QFileInfo data(entry.state.path);
            if (!ok || data.isSymLink() || !data.isFile())
                continue;
            for (const auto &value : json.value("checksums").toArray()) {
                const auto        h = value.toObject();
                bool              a = false, b = false;
                MediaFileChecksum checksum { h.value("offset").toString().toULongLong(&a),
                                             h.value("length").toString().toULongLong(&b),
                                             h.value("algorithm").toString(),
                                             QByteArray::fromBase64(h.value("digest").toString().toLatin1()) };
                if (!a || !b || checksum.offset > size || (!checksum.length && size)
                    || checksum.length > size - checksum.offset || checksum.digest.isEmpty()) {
                    ok = false;
                    break;
                }
                entry.state.checksums.append(checksum);
            }
            if (!ok)
                continue;
            entry.age = ++age_;
            entries_.insert(key, entry);
            verify(entries_[key]);
            save(key, entries_[key]);
            publish(key);
        }
        trim({ });
    }
    void configure(quint64 id, QString key, quint64 size, QList<MediaFileChecksum> checksums, QVariantMap metadata)
    {
        auto            entry = get(key, size);
        MediaChunkError error;
        if (!entry) {
            finish(id, { }, QStringLiteral("Invalid shared file identity or size"));
            return;
        }
        if (!QFileInfo::exists(entry->state.path)) {
            QFile file(entry->state.path);
            QDir().mkpath(directory_);
            if (!file.open(QIODevice::WriteOnly)) {
                finish(id, { }, QStringLiteral("Cannot create shared file payload"));
                return;
            }
        }
        for (auto it = metadata.cbegin(); it != metadata.cend(); ++it)
            entry->state.metadata.insert(it.key(), it.value());
        for (const auto &h : checksums) {
            if ((!h.length && size) || h.offset > size || h.length > size - h.offset || h.digest.isEmpty()
                || h.algorithm.isEmpty()) {
                error                 = QStringLiteral("Invalid shared file checksum range");
                entry->state.error    = error.message();
                entry->state.complete = false;
                break;
            }
            bool found = false;
            for (const auto &old : entry->state.checksums) {
                if (old.offset == h.offset && old.length == h.length && old.algorithm == h.algorithm) {
                    if (old.digest == h.digest) {
                        found = true;
                    } else {
                        removeRange(entry->state.ranges, h.offset, h.length);
                        entry->state.complete = false;
                        entry->state.error    = QStringLiteral("Conflicting checksums for shared file range");
                        error                 = { MediaChunkError::IntegrityError, entry->state.error };
                    }
                }
            }
            if (!found)
                entry->state.checksums.append(h);
        }
        if (error.isEmpty())
            error = verify(*entry);
        if (!save(key, *entry) && error.isEmpty())
            error = QStringLiteral("Cannot save shared file metadata");
        publish(key);
        trim(key);
        finish(id, { }, error);
    }
    void read(quint64 id, QString key, quint64 size, quint64 offset, quint64 length)
    {
        auto entry = get(key, size);
        if (!entry || offset > size || length > size - offset) {
            finish(id, { }, QStringLiteral("Invalid shared file range"));
            return;
        }
        if (!entry->state.error.isEmpty()) {
            finish(id, { }, { MediaChunkError::IntegrityError, entry->state.error });
            return;
        }
        if (!covers(entry->state.ranges, offset, length)) {
            finish(id, { }, { });
            return;
        }
        QFile      file(entry->state.path);
        QByteArray bytes;
        if (length <= MediaChunkMaxSize && file.open(QIODevice::ReadOnly) && file.seek(qint64(offset)))
            bytes = file.read(qint64(length));
        if (quint64(bytes.size()) != length) {
            removeRange(entry->state.ranges, offset, length);
            entry->state.complete = false;
            save(key, *entry);
            publish(key);
            bytes.clear();
        }
        entry->age = ++age_;
        finish(id, bytes, { });
    }
    void write(quint64 id, QString key, quint64 size, quint64 offset, QByteArray bytes)
    {
        auto entry = get(key, size);
        if (!entry || offset >= size || bytes.isEmpty() || quint64(bytes.size()) > size - offset) {
            finish(id, { }, QStringLiteral("Invalid shared file write"));
            return;
        }
        if (!entry->state.error.isEmpty()) {
            finish(id, { }, { MediaChunkError::IntegrityError, entry->state.error });
            return;
        }
        // Save invalidation before overwriting: a crash cannot leave metadata
        // advertising a partially written span as available.
        removeRange(entry->state.ranges, offset, quint64(bytes.size()));
        for (const auto &h : entry->state.checksums)
            if (h.offset < offset + quint64(bytes.size()) && offset < h.offset + h.length)
                entry->verified.remove(checksumId(h));
        entry->state.complete = false;
        if (!save(key, *entry)) {
            finish(id, { }, QStringLiteral("Cannot save shared file metadata"));
            return;
        }
        QFile file(entry->state.path);
        if (!file.open(QIODevice::ReadWrite)) {
            publish(key);
            finish(id, { }, QStringLiteral("Cannot open shared file payload: ") + file.errorString());
            return;
        }
#ifdef Q_OS_WIN
        if (!file.size()) {
            const auto native = _get_osfhandle(file.handle());
            if (native != -1) {
                DWORD returned = 0;
                DeviceIoControl(reinterpret_cast<HANDLE>(native), FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &returned,
                                nullptr);
            }
        }
#endif
        if (!file.seek(qint64(offset)) || file.write(bytes) != bytes.size() || !file.flush()) {
            publish(key);
            finish(id, { }, QStringLiteral("Cannot write shared file payload: ") + file.errorString());
            return;
        }
        file.close();
        insertRange(entry->state.ranges, offset, quint64(bytes.size()));
        auto error = verify(*entry);
        if (!save(key, *entry)) {
            error = QStringLiteral("Cannot save shared file metadata");
            entry->state.ranges.clear();
            entry->state.complete = false;
        }
        entry->age = ++age_;
        publish(key);
        trim(key);
        finish(id, error.isEmpty() ? bytes : QByteArray(), error);
    }
    void resume(quint64 id, const QString &key)
    {
        auto it = entries_.find(key);
        if (it == entries_.end()) {
            finish(id, { }, { });
            return;
        }
        it->state.error.clear();
        auto error = verify(*it);
        if (!save(key, *it) && error.isEmpty())
            error = QStringLiteral("Cannot save shared file metadata");
        publish(key);
        finish(id, { }, error);
    }
    void touch(const QString &key)
    {
        auto it = entries_.find(key);
        if (it != entries_.end())
            it->age = ++age_;
    }
    void pin(const QString &key, bool active)
    {
        if (active)
            ++pins_[key];
        else if (pins_.value(key) <= 1) {
            pins_.remove(key);
            trim({ });
        } else
            --pins_[key];
    }
    void invalidate(QString key, quint64 size, quint64 offset, quint64 length, QString message)
    {
        auto entry = get(key, size);
        if (!entry || offset >= size || !length || length > size - offset)
            return;
        removeRange(entry->state.ranges, offset, length);
        entry->state.complete = false;
        entry->state.error    = message;
        entry->verified.clear();
        save(key, *entry);
        publish(key);
    }
signals:
    void changed(QString key, MediaFileState state, quint64 diskBytes);
    void completed(quint64 id, QByteArray bytes, MediaChunkError error, quint64 diskBytes);

private:
    Entry *get(const QString &key, quint64 size)
    {
        if (!validKey(key) || size > quint64(std::numeric_limits<qint64>::max()))
            return nullptr;
        if (QFileInfo(dataPath(key)).isSymLink())
            return nullptr;
        auto it = entries_.find(key);
        if (it != entries_.end())
            return it->state.size == size ? &*it : nullptr;
        Entry entry;
        entry.state.size = size;
        entry.state.path = dataPath(key);
        entry.age        = ++age_;
        return &*entries_.insert(key, entry);
    }
    MediaChunkError verify(Entry &entry)
    {
        auto &state           = entry.state;
        state.complete        = false;
        const auto actualSize = quint64(QFileInfo(state.path).size());
        if (!state.ranges.isEmpty() && actualSize < state.ranges.last().first + state.ranges.last().second) {
            for (const auto &h : state.checksums)
                if (h.offset + h.length > actualSize)
                    removeRange(state.ranges, h.offset, h.length);
            removeRange(state.ranges, actualSize, state.size - actualSize);
            entry.verified.clear();
            state.error = QStringLiteral("Shared file payload was truncated");
        }
        if (!state.error.isEmpty())
            return { MediaChunkError::IntegrityError, state.error };
        QFile file(state.path);
        for (const auto &h : state.checksums) {
            if (!covers(state.ranges, h.offset, h.length))
                continue;
            const auto id = checksumId(h);
            if (entry.verified.contains(id))
                continue;
            if (!file.isOpen() && !file.open(QIODevice::ReadOnly))
                return QStringLiteral("Cannot open shared file for verification");
            HashRange  range(file, h.offset, h.length);
            const auto actual = hasher_(h.algorithm, &range);
            if (actual.isEmpty() || !range.complete() || actual != h.digest) {
                removeRange(state.ranges, h.offset, h.length);
                state.error = QStringLiteral("Shared file checksum verification failed (%1, offset %2, length %3)")
                                  .arg(h.algorithm)
                                  .arg(h.offset)
                                  .arg(h.length);
                return { MediaChunkError::IntegrityError, state.error };
            }
            entry.verified.insert(id);
        }
        state.complete = covers(state.ranges, 0, state.size);
        if (state.complete && QFileInfo(state.path).size() != qint64(state.size)) {
            file.close();
            if (!file.open(QIODevice::ReadWrite) || !file.resize(qint64(state.size))) {
                state.complete = false;
                return QStringLiteral("Cannot finalize shared file size");
            }
        }
        return { };
    }
    QString dataPath(const QString &key) const { return directory_ + '/' + key + QStringLiteral(".data"); }
    bool    save(const QString &key, const Entry &entry)
    {
        QDir().mkpath(directory_);
        QJsonArray ranges, hashes;
        for (const auto &r : entry.state.ranges)
            ranges.append(QJsonArray { QString::number(r.first), QString::number(r.second) });
        for (const auto &h : entry.state.checksums)
            hashes.append(QJsonObject { { "offset", QString::number(h.offset) },
                                        { "length", QString::number(h.length) },
                                        { "algorithm", h.algorithm },
                                        { "digest", QString::fromLatin1(h.digest.toBase64()) } });
        const QJsonObject json { { "version", 2 },
                                 { "size", QString::number(entry.state.size) },
                                 { "ranges", ranges },
                                 { "checksums", hashes },
                                 { "metadata", QJsonObject::fromVariantMap(entry.state.metadata) },
                                 { "error", entry.state.error } };
        QSaveFile         file(directory_ + '/' + key + QStringLiteral(".json"));
        const auto        bytes = QJsonDocument(json).toJson(QJsonDocument::Compact);
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
    }
    quint64 diskBytes() const
    {
        quint64 total = 0;
        for (const auto &e : entries_)
            total += bytesIn(e.state.ranges);
        return total;
    }
    void publish(const QString &key) { emit changed(key, entries_.value(key).state, diskBytes()); }
    void finish(quint64 id, QByteArray bytes, MediaChunkError error) { emit completed(id, bytes, error, diskBytes()); }
    void trim(const QString &keep)
    {
        while ((diskBytes() > limit_ || entries_.size() > 2048) && entries_.size() > 1) {
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
            if (!QFile::remove(oldest->state.path) && QFileInfo::exists(oldest->state.path))
                break;
            QFile::remove(directory_ + '/' + key + QStringLiteral(".json"));
            entries_.erase(oldest);
            emit changed(key, { }, diskBytes());
        }
    }
    QString                 directory_;
    quint64                 limit_, age_ = 0;
    MediaChunkStore::Hasher hasher_;
    QHash<QString, Entry>   entries_;
    QHash<QString, int>     pins_;
};
}

class MediaChunkStore::Private {
public:
    QThread                        thread;
    DiskWorker                    *worker;
    quint64                        next = 0, bytes = 0;
    QHash<quint64, Completion>     pending;
    QHash<QString, MediaFileState> states;
};
MediaChunkStore::MediaChunkStore(QString directory, quint64 limit, QObject *parent, Hasher hasher) :
    QObject(parent), d(new Private)
{
    qRegisterMetaType<MediaFileState>();
    qRegisterMetaType<MediaChunkError>();
    d->worker = new DiskWorker(std::move(directory), limit, std::move(hasher));
    d->thread.setObjectName(QStringLiteral("shared-media-cache"));
    d->worker->moveToThread(&d->thread);
    connect(&d->thread, &QThread::finished, d->worker, &QObject::deleteLater);
    connect(d->worker, &DiskWorker::changed, this, [this](QString key, MediaFileState state, quint64 bytes) {
        d->bytes = bytes;
        if (!state.path.isEmpty())
            d->states.insert(key, std::move(state));
        else
            d->states.remove(key);
        emit fileChanged(key);
        emit availabilityChanged();
    });
    connect(d->worker, &DiskWorker::completed, this,
            [this](quint64 id, QByteArray bytes, MediaChunkError error, quint64 total) {
                d->bytes  = total;
                auto done = d->pending.take(id);
                if (done)
                    done(std::move(bytes), error);
            });
    d->thread.start();
    QMetaObject::invokeMethod(d->worker, [worker = d->worker] { worker->initialize(); }, Qt::QueuedConnection);
}
MediaChunkStore::~MediaChunkStore()
{
    QMetaObject::invokeMethod(d->worker, [] { }, Qt::BlockingQueuedConnection);
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
void MediaChunkStore::configure(QString key, quint64 size, QList<MediaFileChecksum> hashes, QVariantMap metadata,
                                Completion done)
{
    auto current = d->states.find(key);
    if (current != d->states.end()) {
        for (const auto &h : hashes) {
            bool known = false;
            for (const auto &old : current->checksums)
                if (checksumId(h) == checksumId(old)) {
                    known = true;
                    break;
                }
            if (!known) {
                current->complete = false;
                break;
            }
        }
    }
    const auto id = ++d->next;
    d->pending.insert(id, std::move(done));
    QMetaObject::invokeMethod(
        d->worker,
        [worker = d->worker, id, key, size, hashes = std::move(hashes), metadata = std::move(metadata)] {
            worker->configure(id, key, size, hashes, metadata);
        },
        Qt::QueuedConnection);
}
void MediaChunkStore::read(QString key, quint64 size, quint64 offset, quint64 length, Completion done)
{
    const auto id = ++d->next;
    d->pending.insert(id, std::move(done));
    QMetaObject::invokeMethod(
        d->worker, [worker = d->worker, id, key, size, offset, length] { worker->read(id, key, size, offset, length); },
        Qt::QueuedConnection);
}
void MediaChunkStore::write(QString key, quint64 size, quint64 offset, QByteArray bytes, Completion done)
{
    const auto id = ++d->next;
    d->pending.insert(id, std::move(done));
    QMetaObject::invokeMethod(
        d->worker,
        [worker = d->worker, id, key, size, offset, bytes = std::move(bytes)] {
            worker->write(id, key, size, offset, bytes);
        },
        Qt::QueuedConnection);
}
void MediaChunkStore::resume(QString key, Completion done)
{
    const auto id = ++d->next;
    d->pending.insert(id, std::move(done));
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
    QMetaObject::invokeMethod(
        d->worker,
        [worker = d->worker, key, size, offset, length, message] {
            worker->invalidate(key, size, offset, length, message);
        },
        Qt::QueuedConnection);
}
#include "mediachunkstore.moc"
