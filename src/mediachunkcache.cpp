#include "mediachunkcache.h"
#include "mediachunkcallback.h"
#include "mediachunkstore.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <QFileInfo>
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

MediaChunkCache::MediaChunkCache(QString directory, QObject *parent, quint64 chunkSize, quint64 diskLimit,
                                 MediaChunkStore::Hasher hasher) :
    QObject(parent), chunkSize_(std::clamp<quint64>(chunkSize, 1, MaxChunkSize)), memory_(16 * 1024 * 1024),
    store_(new MediaChunkStore(std::move(directory), diskLimit, nullptr, std::move(hasher)))
{
    scheduler_.setMaxActive(MaxActiveFetches);
    connect(store_.get(), &MediaChunkStore::availabilityChanged, this, &MediaChunkCache::refreshAvailability);
    connect(store_.get(), &MediaChunkStore::fileChanged, this, [this](const QString &key) {
        const auto file = files_.value(key);
        if (file.first.isEmpty())
            return;
        const auto state = store_->state(key);
        if (!state.error.isEmpty() || state.path.isEmpty()) {
            for (auto it = records_.begin(); it != records_.end(); ++it)
                if (it->source == file.first && it->size == file.second)
                    memory_.remove(it.key());
            if (!state.error.isEmpty())
                emit integrityError(file.first, file.second, state.error);
        }
        notifyRanges(file.first, file.second);
    });
}
MediaChunkCache::~MediaChunkCache() { shutdown(); }
quint64 MediaChunkCache::diskBytes() const { return store_->diskBytes(); }

QString MediaChunkCache::fileKey(const QString &source, quint64 size) const
{
    QByteArray  identity;
    QDataStream stream(&identity, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_10);
    stream << source << size;
    return QString::fromLatin1(digest(identity).toHex());
}
void MediaChunkCache::configure(const QString &source, quint64 size, QList<MediaFileChecksum> checksums,
                                QVariantMap metadata)
{
    const auto key = fileKey(source, size);
    files_.insert(key, { source, size });
    store_->configure(key, size, std::move(checksums), std::move(metadata));
}
MediaFileState MediaChunkCache::fileState(const QString &source, quint64 size) const
{
    return store_->state(fileKey(source, size));
}
bool MediaChunkCache::isCompleteFile(const QString &path) const
{
    const auto state = store_->state(QFileInfo(path).completeBaseName());
    return state.complete && state.error.isEmpty() && state.path == path;
}
void MediaChunkCache::resume(const QString &source, quint64 size)
{
    const auto key = fileKey(source, size);
    resuming_.insert(key);
    const QPointer<MediaChunkCache> guard(this);
    store_->resume(key, [guard, key](QByteArray, MediaChunkError) {
        if (guard)
            guard->resuming_.remove(key);
    });
}
void MediaChunkCache::invalidate(const QString &source, quint64 size, quint64 offset, quint64 length,
                                 const QString &message)
{
    store_->invalidate(fileKey(source, size), size, offset, length, message);
}
void MediaChunkCache::pin(const QString &source, quint64 size, bool active)
{
    store_->pin(fileKey(source, size), active);
}

QString MediaChunkCache::recordKey(const QString &source, quint64 size, quint64 index) const
{
    QByteArray  identity;
    QDataStream stream(&identity, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_10);
    stream << source << size << chunkSize_ << index;
    return QString::fromLatin1(digest(identity).toHex());
}

void MediaChunkCache::storeBytes(const QString &source, quint64 size, quint64 offset, QByteArray bytes, Completion done)
{
    const auto key = fileKey(source, size);
    files_.insert(key, { source, size });
    store_->write(key, size, offset, std::move(bytes), std::move(done));
}
void MediaChunkCache::readStored(const QString &source, quint64 size, quint64 offset, quint64 length, Completion done)
{
    store_->read(fileKey(source, size), size, offset, length, std::move(done));
}

quint64 MediaChunkCache::read(QString source, quint64 fileSize, quint64 offset, quint64 length, QString lane,
                              Fetch fetch, Completion done, bool urgent)
{
    if (!done || requests_.size() >= MaxRequests)
        return 0;
    const auto ticket = ++nextTicket_;
    requests_.insert(ticket, { 0, std::move(done), { } });
    auto fail = [this, ticket](MediaChunkError error) {
        QTimer::singleShot(0, this, [this, ticket, error] { deliver(ticket, { }, error); });
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
    const auto    file     = fileKey(source, fileSize);
    files_.insert(file, { source, fileSize });
    const auto fileError = store_->state(file).error;
    if (!fileError.isEmpty() && !resuming_.contains(file)) {
        fail({ MediaChunkError::IntegrityError, fileError });
        return ticket;
    }
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
        store_->touch(file);
        requests_[ticket].cachedSlice = bytes->mid(int(inside), int(count));
        QTimer::singleShot(0, this, [this, ticket] {
            auto it = requests_.find(ticket);
            if (it != requests_.end())
                deliver(ticket, std::move(it->cachedSlice), { });
        });
        return ticket;
    }
    scheduler_.setLaneMaxActive(lane, MaxActiveFetches);
    const QPointer<MediaChunkCache> guard(this);
    const auto                      scheduled = scheduler_.requestCancelable(
        key, 0, lane, urgent ? 100 : 50,
        [guard, key, file, fileSize, expected, start, fetch](Completion complete) {
            return guard->fetchChunk(key, file, fileSize, start, expected, fetch, std::move(complete));
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
        const auto                      pending = std::exchange(pendingRanges_, { });
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
        const auto offset = it->index * chunkSize_;
        if (!memory_.contains(it.key())
            && !store_->contains(fileKey(it->source, it->size), offset, std::min(chunkSize_, it->size - offset))) {
            notifyRanges(it->source, it->size);
            it = records_.erase(it);
        } else {
            ++it;
        }
    }
}

MediaChunkCache::Ranges MediaChunkCache::cachedRanges(const QString &source, quint64 size) const
{
    if (!store_->state(fileKey(source, size)).error.isEmpty())
        return { };
    Ranges ranges = store_->state(fileKey(source, size)).ranges;
    for (const auto &record : records_) {
        if (record.source == source && record.size == size) {
            const auto offset = record.index * chunkSize_;
            ranges.append({ offset, std::min(chunkSize_, size - offset) });
        }
    }
    std::sort(ranges.begin(), ranges.end());
    Ranges merged;
    for (const auto &range : ranges) {
        if (!merged.isEmpty() && merged.last().first + merged.last().second >= range.first)
            merged.last().second = std::max(merged.last().first + merged.last().second, range.first + range.second)
                - merged.last().first;
        else
            merged.append(range);
    }
    return merged;
}

MediaChunkCache::Abort MediaChunkCache::fetchChunk(const QString &key, const QString &file, quint64 size, quint64 start,
                                                   quint64 expected, Fetch fetch, Completion complete)
{
    struct State {
        bool                  cancelled = false, completed = false;
        Abort                 abort;
        Completion            complete;
        MediaFileRanges       missing;
        int                   nextRange = 0;
        quint64               serial    = 0;
        bool                  waiting   = false;
        std::function<void()> next;
        void                  finish(QByteArray bytes, MediaChunkError error)
        {
            if (completed)
                return;
            completed     = true;
            abort         = { };
            auto callback = std::exchange(complete, { });
            if (callback)
                callback(std::move(bytes), error);
        }
    };
    auto                            state = std::make_shared<State>();
    const QPointer<MediaChunkCache> guard(this);
    store_->pin(file, true);
    state->complete
        = [guard, file, start, expected, complete = std::move(complete)](QByteArray bytes, MediaChunkError error) {
              qCDebug(lcMediaChunks) << "complete" << file.left(12) << "offset" << start << "length" << expected
                                     << "bytes" << bytes.size() << "error" << error.message();
              if (guard)
                  guard->store_->pin(file, false);
              complete(std::move(bytes), error);
          };
    const std::weak_ptr<State> weak(state);
    state->next = [weak, guard, key, file, size, start, expected, fetch] {
        auto state = weak.lock();
        if (!state || !guard || guard->stopped_ || state->completed)
            return;
        if (state->cancelled) {
            state->finish({ }, { MediaChunkError::Cancelled, QStringLiteral("Media read cancelled") });
            return;
        }
        while (state->nextRange < state->missing.size()) {
            const auto &range = state->missing.at(state->nextRange);
            if (!guard->store_->contains(file, range.first, range.second))
                break;
            ++state->nextRange;
        }
        if (state->nextRange == state->missing.size()) {
            guard->store_->read(file, size, start, expected,
                                [guard, key, state, expected](QByteArray bytes, MediaChunkError error) {
                                    if (!guard || guard->stopped_)
                                        return;
                                    if (error.isEmpty() && quint64(bytes.size()) != expected)
                                        error = QStringLiteral("Shared file range no longer available");
                                    if (error.isEmpty())
                                        guard->memory_.insert(key, new QByteArray(bytes), bytes.size());
                                    state->finish(std::move(bytes), error);
                                });
            return;
        }
        const auto range  = state->missing.at(state->nextRange++);
        const auto serial = ++state->serial;
        state->waiting    = true;
        qCDebug(lcMediaChunks) << "fetch" << file.left(12) << "offset" << range.first << "length" << range.second;
        state->abort = fetch(
            range.first, range.second,
            queuedMediaChunkCompletion(
                guard, [guard, state, file, size, range, serial](QByteArray bytes, MediaChunkError error) {
                    if (!guard || guard->stopped_ || state->completed || !state->waiting || serial != state->serial)
                        return;
                    state->waiting = false;
                    state->abort   = { };
                    if (state->cancelled) {
                        state->finish({ }, { MediaChunkError::Cancelled, QStringLiteral("Media read cancelled") });
                        return;
                    }
                    if (error.isEmpty() && quint64(bytes.size()) != range.second)
                        error = QStringLiteral("Incomplete shared file range");
                    if (!error.isEmpty()) {
                        state->finish({ }, error);
                        return;
                    }
                    if (guard->store_->contains(file, range.first, range.second)) {
                        state->next(); // A sequential fallback may already have committed this span.
                        return;
                    }
                    guard->store_->write(file, size, range.first, bytes,
                                         [guard, state](QByteArray, MediaChunkError error) {
                                             if (!guard || guard->stopped_)
                                                 return;
                                             if (!error.isEmpty())
                                                 state->finish({ }, error);
                                             else
                                                 state->next();
                                         });
                }));
    };
    store_->read(file, size, start, expected,
                 [guard, key, file, start, expected, state](QByteArray cached, MediaChunkError error) {
                     if (!guard || guard->stopped_ || state->completed)
                         return;
                     if (!error.isEmpty()) {
                         state->finish({ }, error);
                         return;
                     }
                     if (!cached.isEmpty()) {
                         guard->memory_.insert(key, new QByteArray(cached), cached.size());
                         state->finish(std::move(cached), { });
                         return;
                     }
                     quint64 cursor = start;
                     for (const auto &range : guard->store_->state(file).ranges) {
                         if (range.first >= start + expected)
                             break;
                         if (range.first + range.second <= cursor)
                             continue;
                         if (range.first > cursor)
                             state->missing.append({ cursor, range.first - cursor });
                         cursor = std::min(start + expected, range.first + range.second);
                     }
                     if (cursor < start + expected)
                         state->missing.append({ cursor, start + expected - cursor });
                     state->next();
                 });
    return [state] {
        state->cancelled = true;
        auto abort       = std::exchange(state->abort, { });
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
        done({ }, { MediaChunkError::Cancelled, QStringLiteral("Media read cancelled") });
    return true;
}

void MediaChunkCache::shutdown()
{
    if (stopped_)
        return;
    stopped_     = true;
    auto pending = std::exchange(requests_, { });
    scheduler_.shutdown();
    for (const auto &request : pending)
        if (request.done)
            request.done({ }, QStringLiteral("Media cache closed"));
}

MediaChunkReader::MediaChunkReader(MediaChunkCache *cache, QString source, quint64 fileSize, quint64 offset,
                                   quint64 length, QString lane, MediaChunkCache::Fetch fetch, QObject *parent) :
    QIODevice(parent), cache_(cache), source_(std::move(source)), lane_(std::move(lane)), size_(fileSize),
    offset_(offset), left_(length), fetch_(std::move(fetch))
{
    if (cache)
        connect(cache, &MediaChunkCache::integrityError, this,
                [this](const QString &source, quint64 size, const QString &message) {
                    if (source != source_ || size != size_ || stopped_)
                        return;
                    abort();
                    errorCode_ = MediaChunkError::IntegrityError;
                    setErrorString(message);
                    emit failed();
                });
}

MediaChunkReader::~MediaChunkReader() { abort(); }

bool MediaChunkReader::open(OpenMode mode)
{
    if (isOpen())
        return mode == openMode();
    if (mode != ReadOnly || stopped_ || !cache_ || offset_ > size_ || left_ > size_ - offset_)
        return false;
    QIODevice::open(mode);
    cache_->pin(source_, size_, true);
    pinned_ = true;
    schedule();
    return true;
}

void MediaChunkReader::abort()
{
    stopped_ = true;
    if (cache_ && std::exchange(pinned_, false))
        cache_->pin(source_, size_, false);
    const auto ticket = std::exchange(ticket_, 0);
    if (cache_ && ticket)
        cache_->cancel(ticket);
    const auto prefetched = std::exchange(prefetchTickets_, { });
    if (cache_)
        for (auto ticket : prefetched)
            if (ticket)
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
    for (auto it = prefetchTickets_.begin(); it != prefetchTickets_.end();) {
        if (it.key() <= offset_ || it.key() >= offset_ + left_) {
            if (it.value())
                cache_->cancel(it.value());
            it = prefetchTickets_.erase(it);
        } else
            ++it;
    }
    const auto                       distance = cache_->chunkSize() - offset_ % cache_->chunkSize();
    const QPointer<MediaChunkReader> guard(this);
    auto                             next = offset_ + distance;
    for (int i = 1; i < MediaChunkCache::MaxActiveFetches && next < offset_ + left_; ++i, next += cache_->chunkSize()) {
        if (prefetchTickets_.contains(next))
            continue;
        prefetchTickets_.insert(next, 0);
        prefetchTickets_[next] = cache_->read(
            source_, size_, next, 1, lane_, fetch_,
            [guard, next](QByteArray, MediaChunkError) {
                if (guard && guard->prefetchTickets_.contains(next))
                    guard->prefetchTickets_[next] = 0;
            },
            false);
    }
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
