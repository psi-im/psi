#include "mediachunkcache.h"
#include "fileidentity.h"
#include "mediachunkcallback.h"
#include "mediachunkstore.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <QLoggingCategory>
#include <QTimer>
#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

namespace {
Q_LOGGING_CATEGORY(lcMediaChunks, "psi.filesharing.chunks", QtWarningMsg)
QByteArray digest(const QByteArray &bytes) { return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256); }
}

MediaChunkCache::MediaChunkCache(QString directory, QObject *parent, quint64 chunkSize, quint64 diskLimit,
                                 MediaChunkStore::Hasher hasher) :
    MediaChunkCache(new MediaChunkStore(std::move(directory), diskLimit, nullptr, std::move(hasher)), parent, chunkSize)
{
    store_->setParent(this);
}
MediaChunkCache::MediaChunkCache(MediaChunkStore *store, QObject *parent, quint64 chunkSize) :
    QObject(parent), chunkSize_(std::clamp<quint64>(chunkSize, 1, MaxChunkSize)), store_(store)
{
    scheduler_.setMaxActive(MaxActiveFetches);
    scheduler_.setBackgroundLimit(MaxActiveFetches - 1);
    connect(store_, &MediaChunkStore::fileChanged, this, [this](const QString &key) {
        const auto file = files_.value(key);
        if (file.first.isEmpty())
            return;
        const auto     state = store_->state(key);
        QList<quint64> ready;
        for (auto it = requests_.cbegin(); it != requests_.cend(); ++it) {
            if (!it->payload || it->file != key || it->storageReadPending || !state.error.isEmpty())
                continue;
            const auto offset = it->start + it->inside;
            for (const auto &range : state.ranges)
                if (range.first <= offset && offset - range.first <= range.second
                    && it->count <= range.second - (offset - range.first)) {
                    ready.append(it.key());
                    break;
                }
        }
        for (const auto ticket : ready)
            readAvailable(ticket);
        if (!state.error.isEmpty())
            emit integrityError(file.first, file.second, state.error);
        if (state.path.isEmpty())
            files_.remove(key);
        notifyRanges(file.first, file.second);
    });
}
MediaChunkCache::~MediaChunkCache() { shutdown(); }
quint64 MediaChunkCache::diskBytes() const { return store_->diskBytes(); }

QString MediaChunkCache::fileKey(const QString &source, quint64 size) const
{
    return FileIdentity::rangeKey(source, size);
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
quint64 MediaChunkCache::availableLength(const QString &source, quint64 size, quint64 offset, quint64 maximum) const
{
    return store_->availableLength(fileKey(source, size), size, offset, maximum);
}
QString MediaChunkCache::resolveSource(const QString &source, const QString &account, quint64 size,
                                       const QList<MediaFileChecksum> &checksums) const
{
    return store_->resolveSource(source, account, size, checksums);
}
QString MediaChunkCache::localFile(const QString &source, quint64 size) const
{
    return store_->localFile(fileKey(source, size), size);
}
bool MediaChunkCache::isCompleteFile(const QString &path) const { return store_->isCompleteFile(path); }
void MediaChunkCache::resume(const QString &source, quint64 size)
{
    const auto key = fileKey(source, size);
    resuming_.insert(key);
    const QPointer<MediaChunkCache> guard(this);
    store_->resume(key, [guard, key](QByteArray, MediaChunkError) {
        if (!guard)
            return;
        guard->resuming_.remove(key);
        const auto tickets = guard->requests_.keys();
        for (const auto ticket : tickets) {
            const auto request = guard->requests_.value(ticket);
            if (request.file == key && !request.schedulerTicket && !request.storageReadPending)
                guard->schedule(ticket);
        }
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

QString MediaChunkCache::recordKey(const QString &source, quint64 size, quint64 start, quint64 length) const
{
    QByteArray  identity;
    QDataStream stream(&identity, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_10);
    stream << source << size << start << length;
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

void    MediaChunkCache::checkpoint(Completion done) { store_->checkpoint(std::move(done)); }
quint64 MediaChunkCache::read(QString source, quint64 size, quint64 offset, quint64 length, QString lane, Fetch fetch,
                              Completion done, bool urgent, quint64 quantum)
{
    return request(std::move(source), size, offset, length, std::move(lane), std::move(fetch), std::move(done), true,
                   urgent, quantum);
}
quint64 MediaChunkCache::ensure(QString source, quint64 size, quint64 offset, quint64 length, QString lane, Fetch fetch,
                                Completion done, quint64 quantum)
{
    return request(std::move(source), size, offset, length, std::move(lane), std::move(fetch), std::move(done), false,
                   false, quantum);
}
quint64 MediaChunkCache::request(QString source, quint64 fileSize, quint64 offset, quint64 length, QString lane,
                                 Fetch fetch, Completion done, bool payload, bool urgent, quint64 quantum)
{
    if (!done || requests_.size() >= MaxRequests)
        return 0;
    const auto ticket = ++nextTicket_;
    requests_.insert(ticket, { 0, std::move(done) });
    auto fail = [this, ticket](MediaChunkError error) {
        QTimer::singleShot(0, this, [this, ticket, error] { deliver(ticket, {}, error); });
    };
    if (stopped_ || source.isEmpty() || lane.isEmpty() || !fetch || !length || offset >= fileSize
        || fileSize > quint64(std::numeric_limits<qint64>::max())) {
        fail(QStringLiteral("Invalid or unavailable media chunk request"));
        return ticket;
    }
    quantum           = quantum ? std::clamp<quint64>(quantum, 1, MaxChunkSize) : chunkSize_;
    const auto file   = fileKey(source, fileSize);
    quint64    start  = payload ? offset / quantum * quantum : offset;
    quint64    end    = payload ? start + std::min(quantum, fileSize - start)
                                : offset + std::min({ length, MaxChunkSize, fileSize - offset });
    bool       joined = false;
    for (const auto &active : requests_) {
        if (active.file == file && active.start <= offset && offset - active.start < active.expected) {
            start  = active.start;
            end    = active.start + active.expected;
            joined = true;
            break;
        }
    }
    if (!joined) {
        for (const auto &active : requests_) {
            if (active.file != file || !active.expected)
                continue;
            if (active.start + active.expected <= offset)
                start = std::max(start, active.start + active.expected);
            else if (active.start > offset)
                end = std::min(end, active.start);
        }
    }
    const quint64 expected = end - start;
    const quint64 inside   = offset - start;
    const quint64 count    = std::min(length, end - offset);
    const auto    key      = recordKey(source, fileSize, start, expected);
    files_.insert(file, { source, fileSize });
    const auto fileError = store_->state(file).error;
    if (!fileError.isEmpty() && !resuming_.contains(file)) {
        fail({ MediaChunkError::IntegrityError, fileError });
        return ticket;
    }
    auto &request    = requests_[ticket];
    request.key      = key;
    request.file     = file;
    request.source   = source;
    request.lane     = std::move(lane);
    request.size     = fileSize;
    request.start    = start;
    request.expected = expected;
    request.inside   = inside;
    request.count    = count;
    request.fetch    = std::move(fetch);
    request.urgent   = urgent;
    request.payload  = payload;
    if (payload)
        readAvailable(ticket);
    else
        schedule(ticket);
    return ticket;
}

void MediaChunkCache::schedule(quint64 ticket)
{
    const auto request = requests_.value(ticket);
    if (!requests_.contains(ticket) || resuming_.contains(request.file))
        return;
    files_.insert(request.file, { request.source, request.size });
    scheduler_.setLaneMaxActive(request.lane, MaxActiveFetches);
    const QPointer<MediaChunkCache> guard(this);
    const auto                      scheduled = scheduler_.requestCancelable(
        request.key, 0, request.lane,
        request.payload ? (request.urgent ? 100 : 50) : MediaChunkScheduler::BackgroundPriority,
        [guard, file = request.file, size = request.size, start = request.start, expected = request.expected,
         fetch = request.fetch](Completion complete) {
            return guard->fetchChunk(file, size, start, expected, fetch, std::move(complete));
        },
        [guard, ticket](QByteArray, MediaChunkError error) {
            if (!guard || !guard->requests_.contains(ticket))
                return;
            const auto request = guard->requests_.value(ticket);
            if (request.storageReadPending)
                return;
            if ((!error.isEmpty() && error.code() != MediaChunkError::StorageUnavailable) || !request.payload)
                guard->deliver(ticket, {}, error);
            else
                guard->readAvailable(ticket);
        });
    auto it = requests_.find(ticket);
    if (it != requests_.end())
        it->schedulerTicket = scheduled;
}

void MediaChunkCache::readAvailable(quint64 ticket)
{
    auto it = requests_.find(ticket);
    if (it == requests_.end() || it->storageReadPending)
        return;
    it->storageReadPending = true;
    if (const auto available = store_->availableLength(it->file, it->size, it->start + it->inside, it->count))
        it->count = available;
    const auto request = *it;
    if (request.schedulerTicket)
        scheduler_.cancel(request.schedulerTicket);
    const QPointer<MediaChunkCache> guard(this);
    store_->read(request.file, request.size, request.start + request.inside, request.count,
                 [guard, ticket](QByteArray bytes, MediaChunkError error) {
                     if (!guard)
                         return;
                     auto it = guard->requests_.find(ticket);
                     if (it == guard->requests_.end())
                         return;
                     it->storageReadPending = false;
                     it->schedulerTicket    = 0;
                     if (error.isEmpty() && bytes.isEmpty())
                         guard->schedule(ticket);
                     else
                         guard->deliver(ticket, std::move(bytes), error);
                 });
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

MediaChunkCache::Ranges MediaChunkCache::cachedRanges(const QString &source, quint64 size) const
{
    const auto state = store_->state(fileKey(source, size));
    return state.error.isEmpty() ? state.ranges : Ranges();
}

MediaChunkCache::Abort MediaChunkCache::fetchChunk(const QString &file, quint64 size, quint64 start, quint64 expected,
                                                   Fetch fetch, Completion complete)
{
    struct State {
        bool                  cancelled = false, completed = false;
        Abort                 abort;
        Completion            complete;
        MediaFileRanges       missing;
        MediaChunkError       storageError;
        int                   nextRange = 0;
        quint64               serial    = 0;
        bool                  waiting   = false;
        std::function<void()> next;
        void                  finish(QByteArray bytes, MediaChunkError error)
        {
            if (completed)
                return;
            completed     = true;
            abort         = {};
            auto callback = std::exchange(complete, {});
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
    state->next = [weak, guard, file, size, fetch] {
        auto state = weak.lock();
        if (!state || !guard || guard->stopped_ || state->completed)
            return;
        if (state->cancelled) {
            state->finish({}, { MediaChunkError::Cancelled, QStringLiteral("Media read cancelled") });
            return;
        }
        while (state->nextRange < state->missing.size()) {
            auto      &range     = state->missing[state->nextRange];
            const auto available = guard->store_->availableLength(file, size, range.first, range.second);
            if (!available)
                break;
            if (!guard->store_->contains(file, range.first, available))
                state->storageError
                    = { MediaChunkError::StorageUnavailable, QStringLiteral("File data is cached only in memory") };
            range.first += available;
            range.second -= available;
            if (!range.second)
                ++state->nextRange;
        }
        if (state->nextRange == state->missing.size()) {
            state->finish({}, state->storageError);
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
                    state->abort   = {};
                    if (state->cancelled) {
                        state->finish({}, { MediaChunkError::Cancelled, QStringLiteral("Media read cancelled") });
                        return;
                    }
                    if (error.isEmpty() && quint64(bytes.size()) != range.second)
                        error = QStringLiteral("Incomplete shared file range");
                    if (!error.isEmpty()) {
                        state->finish({}, error);
                        return;
                    }
                    if (guard->store_->contains(file, range.first, range.second)) {
                        state->next(); // A sequential fallback may already have committed this span.
                        return;
                    }
                    guard->store_->write(file, size, range.first, bytes,
                                         [guard, state, file, size, range, bytes](QByteArray, MediaChunkError error) {
                                             if (!guard || guard->stopped_)
                                                 return;
                                             if (error.code() == MediaChunkError::StorageUnavailable) {
                                                 guard->store_->cacheVolatile(
                                                     file, size, range.first, bytes,
                                                     [state, error](QByteArray, MediaChunkError verification) {
                                                         if (!verification.isEmpty())
                                                             state->finish({}, verification);
                                                         else {
                                                             state->storageError = error;
                                                             state->next();
                                                         }
                                                     });
                                             } else if (!error.isEmpty())
                                                 state->finish({}, error);
                                             else
                                                 state->next();
                                         });
                }));
    };
    const auto stored = store_->state(file);
    if (!stored.error.isEmpty()) {
        QTimer::singleShot(0, this, [state, message = stored.error] {
            state->finish({}, { MediaChunkError::IntegrityError, message });
        });
    } else {
        quint64 cursor = start;
        for (const auto &range : stored.ranges) {
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
        QTimer::singleShot(0, this, [state] { state->next(); });
    }
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
    stopped_ = true;
    store_->checkpoint();
    auto pending = std::exchange(requests_, {});
    scheduler_.shutdown();
    for (const auto &request : pending)
        if (request.done)
            request.done({}, QStringLiteral("Media cache closed"));
}
