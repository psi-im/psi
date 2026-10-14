#include "mediachunkcache.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>
#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

namespace {
Q_LOGGING_CATEGORY(lcMediaChunks, "psi.filesharing.chunks", QtWarningMsg)
const QByteArray magic("PSICHNK1");
constexpr int    headerSize = 8 + 32 + 32;
QByteArray       digest(const QByteArray &bytes) { return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256); }
}

MediaChunkCache::MediaChunkCache(QString directory, QObject *parent, quint64 chunkSize, quint64 diskLimit) :
    QObject(parent), directory_(std::move(directory)), chunkSize_(std::clamp<quint64>(chunkSize, 1, 4 * 1024 * 1024)),
    diskLimit_(diskLimit), memory_(16 * 1024 * 1024)
{
    scheduler_.setMaxActive(2);
    QDir dir(directory_);
    dir.mkpath(QStringLiteral("."));
    const QRegularExpression name(QStringLiteral("^[0-9a-f]{64}\\.chunk$"));
    const auto               files
        = dir.entryInfoList({ QStringLiteral("*.chunk") }, QDir::Files | QDir::NoSymLinks, QDir::Time | QDir::Reversed);
    for (const auto &file : files) {
        if (!name.match(file.fileName()).hasMatch())
            continue;
        const auto key = file.completeBaseName();
        entries_.insert(key, { quint64(file.size()), ++age_ });
        diskBytes_ += quint64(file.size());
    }
    trim(0);
}

MediaChunkCache::~MediaChunkCache() { shutdown(); }

QString MediaChunkCache::recordKey(const QString &source, quint64 size, quint64 index) const
{
    QByteArray  identity;
    QDataStream stream(&identity, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_10);
    stream << source << size << chunkSize_ << index;
    return QString::fromLatin1(digest(identity).toHex());
}

void MediaChunkCache::discard(const QString &key)
{
    memory_.remove(key);
    const auto it = entries_.find(key);
    if (it == entries_.end())
        return;
    const QString path = directory_ + '/' + key + QStringLiteral(".chunk");
    if (QFile::remove(path) || !QFileInfo::exists(path)) {
        diskBytes_ -= it->size;
        entries_.erase(it);
    }
}

bool MediaChunkCache::trim(quint64 incoming)
{
    if (incoming > diskLimit_)
        return false;
    while ((diskBytes_ > diskLimit_ - incoming || entries_.size() + (incoming ? 1 : 0) > 2048) && !entries_.isEmpty()) {
        auto oldest = entries_.begin();
        for (auto it = entries_.begin(); it != entries_.end(); ++it)
            if (it->age < oldest->age)
                oldest = it;
        const auto before = diskBytes_;
        discard(oldest.key());
        if (diskBytes_ == before)
            return false; // A read-only cache must not exceed its disk budget.
    }
    return true;
}

QByteArray MediaChunkCache::load(const QString &key, quint64 expected)
{
    if (const auto bytes = memory_.object(key)) {
        if (entries_.contains(key))
            entries_[key].age = ++age_;
        return *bytes;
    }
    if (!entries_.contains(key))
        return {};
    QFile file(directory_ + '/' + key + QStringLiteral(".chunk"));
    if (file.open(QIODevice::ReadOnly) && quint64(file.size()) == headerSize + expected) {
        const auto header = file.read(headerSize);
        const auto bytes  = file.read(qint64(expected));
        if (header.size() == headerSize && header.startsWith(magic)
            && header.mid(8, 32) == QByteArray::fromHex(key.toLatin1()) && quint64(bytes.size()) == expected
            && header.mid(40, 32) == digest(bytes)) {
            entries_[key].age = ++age_;
            memory_.insert(key, new QByteArray(bytes), bytes.size());
            return bytes;
        }
    }
    file.close();
    discard(key); // Truncation or local corruption is a cache miss, never valid data.
    return {};
}

void MediaChunkCache::store(const QString &key, const QByteArray &bytes)
{
    discard(key);
    if (trim(headerSize + quint64(bytes.size()))) {
        QSaveFile  file(directory_ + '/' + key + QStringLiteral(".chunk"));
        const auto header = magic + QByteArray::fromHex(key.toLatin1()) + digest(bytes);
        if (file.open(QIODevice::WriteOnly) && file.write(header) == header.size() && file.write(bytes) == bytes.size()
            && file.commit()) {
            const auto size = quint64(headerSize + bytes.size());
            entries_.insert(key, { size, ++age_ });
            diskBytes_ += size;
        }
    }
    // Cache persistence is best-effort; a full/read-only disk must not prevent playback.
    memory_.insert(key, new QByteArray(bytes), bytes.size());
}

quint64 MediaChunkCache::read(QString source, quint64 fileSize, quint64 offset, quint64 length, QString lane,
                              Fetch fetch, Completion done, bool urgent)
{
    if (!done)
        return 0;
    const auto ticket = ++nextTicket_;
    requests_.insert(ticket, { 0, std::move(done) });
    auto fail = [this, ticket](QString error) {
        QTimer::singleShot(0, this, [this, ticket, error] { deliver(ticket, {}, error); });
    };
    if (stopped_ || sourceDisabled(source) || requests_.size() > 256 || source.isEmpty() || lane.isEmpty() || !fetch
        || !length || offset >= fileSize || fileSize > quint64(std::numeric_limits<qint64>::max())) {
        fail(QStringLiteral("Invalid or unavailable media chunk request"));
        return ticket;
    }
    const quint64 index    = offset / chunkSize_;
    const quint64 start    = index * chunkSize_;
    const quint64 expected = std::min(chunkSize_, fileSize - start);
    const quint64 inside   = offset - start;
    const quint64 count    = std::min(length, expected - inside);
    const auto    key      = recordKey(source, fileSize, index);
    if (auto bytes = load(key, expected); !bytes.isEmpty()) {
        auto slice = bytes.mid(int(inside), int(count));
        QTimer::singleShot(0, this, [this, ticket, slice] { deliver(ticket, slice, {}); });
        return ticket;
    }
    scheduler_.setLaneMaxActive(lane, 2);
    const QPointer<MediaChunkCache> guard(this);
    const auto                      scheduled = scheduler_.requestCancelable(
        key, 0, lane, urgent ? 100 : 50,
        [guard, key, expected, start, fetch](Completion complete) {
            qCDebug(lcMediaChunks) << "fetch" << key.left(16) << "offset" << start << "length" << expected;
            auto completed = std::make_shared<bool>(false);
            return fetch(start, expected, [guard, key, expected, complete, completed](QByteArray bytes, QString error) {
                if (!guard)
                    return;
                QMetaObject::invokeMethod(
                    guard,
                    [guard, key, expected, complete, completed, bytes = std::move(bytes),
                     error = std::move(error)]() mutable {
                        if (!guard || *completed)
                            return;
                        *completed = true;
                        if (error.isEmpty() && quint64(bytes.size()) != expected)
                            error = QStringLiteral("Incomplete media chunk");
                        if (error.isEmpty() && !guard->stopped_)
                            guard->store(key, bytes);
                        qCDebug(lcMediaChunks)
                            << "complete" << key.left(16) << "bytes" << bytes.size() << "error" << error;
                        complete(std::move(bytes), std::move(error));
                    },
                    Qt::QueuedConnection);
            });
        },
        [guard, ticket, inside, count](QByteArray bytes, QString error) {
            if (guard)
                guard->deliver(ticket, error.isEmpty() ? bytes.mid(int(inside), int(count)) : QByteArray(), error);
        },
        this);
    auto it = requests_.find(ticket);
    if (it != requests_.end())
        it->schedulerTicket = scheduled;
    return ticket;
}

void MediaChunkCache::deliver(quint64 ticket, QByteArray bytes, QString error)
{
    auto it = requests_.find(ticket);
    if (it == requests_.end())
        return;
    auto done = std::move(it->done);
    requests_.erase(it);
    if (done)
        done(std::move(bytes), std::move(error));
}

bool MediaChunkCache::cancel(quint64 ticket)
{
    auto it = requests_.find(ticket);
    if (it == requests_.end())
        return false;
    const auto scheduled = it->schedulerTicket;
    auto       done      = std::move(it->done);
    requests_.erase(it);
    if (scheduled)
        scheduler_.cancel(scheduled);
    if (done)
        done({}, QStringLiteral("Media read cancelled"));
    return true;
}

void MediaChunkCache::shutdown()
{
    if (stopped_)
        return;
    stopped_     = true;
    auto pending = std::exchange(requests_, {});
    scheduler_.shutdown();
    for (const auto &request : pending)
        if (request.done)
            request.done({}, QStringLiteral("Media cache closed"));
}

MediaChunkReader::MediaChunkReader(MediaChunkCache *cache, QString source, quint64 fileSize, quint64 offset,
                                   quint64 length, QString lane, MediaChunkCache::Fetch fetch, QObject *parent) :
    QIODevice(parent), cache_(cache), source_(std::move(source)), lane_(std::move(lane)), size_(fileSize),
    offset_(offset), left_(length), fetch_(std::move(fetch))
{
}

MediaChunkReader::~MediaChunkReader() { abort(); }

bool MediaChunkReader::open(OpenMode mode)
{
    if (mode != ReadOnly || stopped_ || !cache_ || offset_ > size_ || left_ > size_ - offset_)
        return false;
    QIODevice::open(mode);
    schedule();
    return true;
}

void MediaChunkReader::abort()
{
    stopped_          = true;
    const auto ticket = std::exchange(ticket_, 0);
    if (cache_ && ticket)
        cache_->cancel(ticket);
    buffer_.clear();
    QIODevice::close();
}

bool   MediaChunkReader::isConnected() const { return !stopped_ && (left_ || bytesAvailable()); }
qint64 MediaChunkReader::bytesAvailable() const { return buffer_.size() + QIODevice::bytesAvailable(); }

void MediaChunkReader::schedule()
{
    if (scheduled_ || stopped_ || ticket_ || !buffer_.isEmpty() || !left_)
        return;
    scheduled_ = true;
    QTimer::singleShot(0, this, [this] {
        scheduled_ = false;
        if (stopped_)
            return;
        if (!cache_) {
            stopped_ = true;
            setErrorString(QStringLiteral("Media cache unavailable"));
            emit failed();
            return;
        }
        const QPointer<MediaChunkReader> guard(this);
        ticket_ = cache_->read(
            source_, size_, offset_, std::min<quint64>(left_, 65536), lane_, fetch_,
            [guard](QByteArray bytes, QString error) {
                if (!guard || guard->stopped_)
                    return;
                guard->ticket_ = 0;
                if (!error.isEmpty()) {
                    guard->stopped_ = true;
                    guard->setErrorString(error);
                    emit guard->failed();
                    return;
                }
                guard->buffer_ = std::move(bytes);
                emit guard->readyRead();
            },
            first_);
        first_ = false;
    });
}

qint64 MediaChunkReader::readData(char *data, qint64 maxSize)
{
    if (stopped_)
        return -1;
    const auto count = std::min(maxSize, qint64(buffer_.size()));
    if (count > 0) {
        std::memcpy(data, buffer_.constData(), size_t(count));
        buffer_.remove(0, int(count));
        offset_ += quint64(count);
        left_ -= quint64(count);
    }
    if (!left_)
        QTimer::singleShot(0, this, [this] { emit disconnected(); });
    else
        schedule();
    return count ? count : left_ ? 0 : -1;
}
