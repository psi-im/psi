#include "mediachunkcache.h"
#include "mediachunkcallback.h"
#include "mediachunkstore.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <QLoggingCategory>
#include <QTimer>
#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

namespace {
Q_LOGGING_CATEGORY(lcMediaChunks, "psi.filesharing.chunks", QtWarningMsg)
QByteArray digest(const QByteArray &bytes) { return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256); }
}

MediaChunkCache::MediaChunkCache(QString directory, QObject *parent, quint64 chunkSize, quint64 diskLimit) :
    QObject(parent), chunkSize_(std::clamp<quint64>(chunkSize, 1, MaxChunkSize)), memory_(16 * 1024 * 1024),
    store_(new MediaChunkStore(std::move(directory), diskLimit))
{
    scheduler_.setMaxActive(2);
    connect(store_.get(), &MediaChunkStore::availabilityChanged, this, &MediaChunkCache::refreshAvailability);
}
MediaChunkCache::~MediaChunkCache() { shutdown(); }
quint64 MediaChunkCache::diskBytes() const { return store_->diskBytes(); }

QString MediaChunkCache::recordKey(const QString &source, quint64 size, quint64 index) const
{
    QByteArray  identity;
    QDataStream stream(&identity, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_10);
    stream << source << size << chunkSize_ << index;
    return QString::fromLatin1(digest(identity).toHex());
}

quint64 MediaChunkCache::read(QString source, quint64 fileSize, quint64 offset, quint64 length, QString lane,
                              Fetch fetch, Completion done, bool urgent)
{
    if (!done || requests_.size() >= MaxRequests)
        return 0;
    const auto ticket = ++nextTicket_;
    requests_.insert(ticket, { 0, std::move(done), {} });
    auto fail = [this, ticket](MediaChunkError error) {
        QTimer::singleShot(0, this, [this, ticket, error] { deliver(ticket, {}, error); });
    };
    if (stopped_ || source.isEmpty() || lane.isEmpty() || !fetch || !length || offset >= fileSize
        || fileSize > quint64(std::numeric_limits<qint64>::max())) {
        fail(QStringLiteral("Invalid or unavailable media chunk request"));
        return ticket;
    }
    const quint64 index    = offset / chunkSize_;
    const quint64 start    = index * chunkSize_;
    const quint64 expected = std::min(chunkSize_, fileSize - start);
    const quint64 inside   = offset - start;
    const quint64 count    = std::min(length, expected - inside);
    const auto    key      = recordKey(source, fileSize, index);
    // Store only identities of verified, available chunks; do not accumulate a
    // history of every seek or retain payloads for the progress UI.
    auto &request  = requests_[ticket];
    auto  original = std::move(request.done);
    request.done   = [this, key, source, fileSize, index, original = std::move(original)](QByteArray      bytes,
                                                                                        MediaChunkError error) {
        if (error.isEmpty() && !bytes.isEmpty())
            remember(key, source, fileSize, index);
        original(std::move(bytes), std::move(error));
    };
    if (const auto bytes = memory_.object(key)) {
        store_->touch(key);
        requests_[ticket].cachedSlice = bytes->mid(int(inside), int(count));
        QTimer::singleShot(0, this, [this, ticket] {
            auto it = requests_.find(ticket);
            if (it != requests_.end())
                deliver(ticket, std::move(it->cachedSlice), {});
        });
        return ticket;
    }
    scheduler_.setLaneMaxActive(lane, 2);
    const QPointer<MediaChunkCache> guard(this);
    const auto                      scheduled = scheduler_.requestCancelable(
        key, 0, lane, urgent ? 100 : 50,
        [guard, key, expected, start, fetch](Completion complete) {
            return guard->fetchChunk(key, start, expected, fetch, std::move(complete));
        },
        [guard, ticket, inside, count](QByteArray bytes, MediaChunkError error) {
            if (guard)
                guard->deliver(ticket, error.isEmpty() ? bytes.mid(int(inside), int(count)) : QByteArray(), error);
        });
    auto it = requests_.find(ticket);
    if (it != requests_.end())
        it->schedulerTicket = scheduled;
    return ticket;
}

void MediaChunkCache::notifyRanges(const QString &source, quint64 size)
{
    pendingRanges_.insert({ source, size });
    if (rangesPending_)
        return;
    rangesPending_ = true;
    QTimer::singleShot(0, this, [this] {
        rangesPending_                          = false;
        const auto                      pending = std::exchange(pendingRanges_, {});
        const QPointer<MediaChunkCache> guard(this);
        for (const auto &id : pending) {
            emit rangesChanged(id.first, id.second);
            if (!guard)
                return;
        }
    });
}

void MediaChunkCache::remember(const QString &key, const QString &source, quint64 size, quint64 index)
{
    const bool added = !records_.contains(key);
    records_.insert(key, { source, size, index });
    refreshAvailability();
    if (added)
        notifyRanges(source, size);
}

void MediaChunkCache::refreshAvailability()
{
    for (auto it = records_.begin(); it != records_.end();) {
        if (!memory_.contains(it.key()) && !store_->contains(it.key())) {
            notifyRanges(it->source, it->size);
            it = records_.erase(it);
        } else {
            ++it;
        }
    }
}

MediaChunkCache::Ranges MediaChunkCache::cachedRanges(const QString &source, quint64 size) const
{
    Ranges ranges;
    for (const auto &record : records_) {
        if (record.source == source && record.size == size) {
            const auto offset = record.index * chunkSize_;
            ranges.append({ offset, std::min(chunkSize_, size - offset) });
        }
    }
    std::sort(ranges.begin(), ranges.end());
    Ranges merged;
    for (const auto &range : ranges) {
        if (!merged.isEmpty() && merged.last().first + merged.last().second == range.first)
            merged.last().second += range.second;
        else
            merged.append(range);
    }
    return merged;
}

MediaChunkCache::Abort MediaChunkCache::fetchChunk(const QString &key, quint64 start, quint64 expected, Fetch fetch,
                                                   Completion complete)
{
    struct State {
        bool  cancelled = false, completed = false;
        Abort abort;
    };
    auto                            state = std::make_shared<State>();
    const QPointer<MediaChunkCache> guard(this);
    store_->read(key, expected, [guard, key, start, expected, fetch, complete, state](QByteArray cached) {
        if (!guard || guard->stopped_)
            return;
        if (!cached.isEmpty()) {
            guard->memory_.insert(key, new QByteArray(cached), cached.size());
            complete(std::move(cached), {});
            return;
        }
        if (state->cancelled) {
            complete({}, { MediaChunkError::Cancelled, QStringLiteral("Media read cancelled") });
            return;
        }
        qCDebug(lcMediaChunks) << "fetch" << key.left(16) << "offset" << start << "length" << expected;
        state->abort = fetch(
            start, expected,
            queuedMediaChunkCompletion(
                guard, [guard, key, expected, complete, state](QByteArray bytes, MediaChunkError error) mutable {
                    if (!guard || state->completed || guard->stopped_)
                        return;
                    state->completed = true;
                    state->abort     = {}; // Providers may capture their completion in the abort callable.
                    if (error.isEmpty() && quint64(bytes.size()) != expected)
                        error = QStringLiteral("Incomplete media chunk");
                    qCDebug(lcMediaChunks)
                        << "complete" << key.left(16) << "bytes" << bytes.size() << "error" << error.message();
                    if (!error.isEmpty()) {
                        complete({}, std::move(error));
                        return;
                    }
                    guard->memory_.insert(key, new QByteArray(bytes), bytes.size());
                    guard->store_->write(key, bytes, [guard, complete, bytes](QByteArray) {
                        if (guard && !guard->stopped_)
                            complete(bytes, {});
                    });
                }));
        if (state->cancelled) {
            auto abort = std::exchange(state->abort, {});
            if (abort)
                abort();
        }
    });
    return [state] {
        state->cancelled = true;
        auto abort       = std::exchange(state->abort, {});
        if (abort)
            abort();
    };
}

void MediaChunkCache::deliver(quint64 ticket, QByteArray bytes, MediaChunkError error)
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
        done({}, { MediaChunkError::Cancelled, QStringLiteral("Media read cancelled") });
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
    if (isOpen())
        return mode == openMode();
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
    if (cache_ && prefetchTicket_)
        cache_->cancel(std::exchange(prefetchTicket_, 0));
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
            stopped_   = true;
            errorCode_ = MediaChunkError::Failed;
            setErrorString(QStringLiteral("Media cache unavailable"));
            emit failed();
            return;
        }
        const QPointer<MediaChunkReader> guard(this);
        ticket_ = cache_->read(
            source_, size_, offset_, std::min<quint64>(left_, 65536), lane_, fetch_,
            [guard](QByteArray bytes, MediaChunkError error) {
                if (!guard || guard->stopped_)
                    return;
                guard->ticket_ = 0;
                if (!error.isEmpty()) {
                    guard->stopped_   = true;
                    guard->errorCode_ = error.code();
                    guard->setErrorString(error.message());
                    emit guard->failed();
                    return;
                }
                guard->buffer_ = std::move(bytes);
                emit guard->readyRead();
            },
            first_);
        if (!ticket_) {
            stopped_ = true;
            setErrorString(QStringLiteral("Too many pending media reads"));
            errorCode_ = MediaChunkError::Failed;
            emit failed();
            return;
        }
        first_ = false;
        prefetch();
    });
}

void MediaChunkReader::prefetch()
{
    // Queue a real successor demand before the current fetch verifies its
    // payload. The Jingle provider can then add it before releasing <received>.
    // This never runs beyond the HTTP request and stops when the reader stalls.
    const auto distance = cache_->chunkSize() - offset_ % cache_->chunkSize();
    if (left_ <= distance)
        return;
    const auto next = offset_ + distance;
    if (prefetchOffset_ == next)
        return;
    if (prefetchTicket_)
        cache_->cancel(std::exchange(prefetchTicket_, 0));
    prefetchOffset_ = next;
    const QPointer<MediaChunkReader> guard(this);
    prefetchTicket_ = cache_->read(
        source_, size_, next, 1, lane_, fetch_,
        [guard, next](QByteArray, MediaChunkError) {
            if (guard && guard->prefetchOffset_ == next)
                guard->prefetchTicket_ = 0;
        },
        false);
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
    if (!left_ && !ended_) {
        ended_ = true;
        QTimer::singleShot(0, this, [this] {
            if (!stopped_)
                emit disconnected();
        });
    } else
        schedule();
    return count ? count : left_ ? 0 : -1;
}
