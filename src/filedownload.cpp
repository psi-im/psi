/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filedownload.h"
#include "mediachunkcache.h"
#include "mediachunkcallback.h"
#include <QElapsedTimer>
#include <QTimer>
#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

FileDownload::FileDownload(MediaChunkCache *cache, QString source, quint64 size, QString lane, Fetch fetch,
                           QObject *parent) :
    QObject(parent), cache_(cache), source_(std::move(source)), lane_(std::move(lane)), size_(size),
    fetch_(std::move(fetch))
{
    if (cache) {
        connect(cache, &MediaChunkCache::rangesChanged, this, [this](const QString &source, quint64 size) {
            if (source == source_ && size == size_)
                schedule();
        });
        connect(cache, &MediaChunkCache::integrityError, this,
                [this](const QString &source, quint64 size, const QString &message) {
                    if (source != source_ || size != size_ || stopped_)
                        return;
                    const MediaChunkError        error { MediaChunkError::IntegrityError, message };
                    const QPointer<FileDownload> guard(this);
                    finish(error);
                    if (guard)
                        emit failed(error);
                });
        connect(cache, &QObject::destroyed, this, [this] {
            const QPointer<FileDownload> guard(this);
            finish(QStringLiteral("File storage closed"));
            if (guard)
                emit failed(QStringLiteral("File storage closed"));
        });
    }
}
FileDownload::~FileDownload() { shutdown(); }

MediaFileState FileDownload::state() const
{
    auto       result = cache_ ? cache_->fileState(source_, size_) : MediaFileState();
    const auto local  = localFile_ ? localFile_() : QString();
    if (!local.isEmpty()) {
        result.size     = size_;
        result.path     = local;
        result.complete = true;
        result.ranges   = { { 0, size_ } };
        result.error.clear();
        result.completionError.clear();
    }
    return result;
}

quint64 FileDownload::quantum(bool background) const
{
    if (!bytesPerMs_)
        return 64 * 1024;
    const double targetMs = background ? 400 : 150;
    const auto   wanted   = quint64(std::clamp(bytesPerMs_ * targetMs, double(16 * 1024), double(1024 * 1024)));
    // A stable power-of-two geometry avoids needless changes for noisy samples.
    quint64 result = 16 * 1024;
    while (result < wanted && result < 1024 * 1024)
        result *= 2;
    return result;
}

std::function<MediaChunkAbort(quint64, quint64, FileDownload::Completion)> FileDownload::provider()
{
    const QPointer<FileDownload> guard(this);
    return [guard](quint64 offset, quint64 length, Completion done) -> MediaChunkAbort {
        if (!guard || guard->stopped_ || !guard->cache_) {
            done({}, { MediaChunkError::Cancelled, QStringLiteral("File download closed") });
            return {};
        }
        auto timer = std::make_shared<QElapsedTimer>();
        timer->start();
        auto complete = queuedMediaChunkCompletion(
            guard->cache_, [guard, timer, done = std::move(done)](QByteArray bytes, MediaChunkError error) {
                if (guard && error.isEmpty() && !bytes.isEmpty()) {
                    const double rate  = double(bytes.size()) / std::max<qint64>(1, timer->elapsed());
                    guard->bytesPerMs_ = guard->bytesPerMs_ ? 0.75 * guard->bytesPerMs_ + 0.25 * rate : rate;
                }
                done(std::move(bytes), std::move(error));
            });
        const auto allowed = [guard] { return guard && !guard->stopped_ && !guard->goals_.isEmpty(); };
        return guard->fetch_(offset, length, allowed, std::move(complete));
    };
}

void FileDownload::setPlayhead(quint64 offset)
{
    playhead_ = std::min(offset, size_);
    schedule();
}

quint64 FileDownload::readBuffered(quint64 offset, quint64 length, Completion done)
{
    const auto resident = cache_ ? cache_->availableLength(source_, size_, offset, 64 * 1024) : 0;
    return read(offset, std::max(length, resident), std::move(done));
}
quint64 FileDownload::read(quint64 offset, quint64 length, Completion done)
{
    if (!cache_ || stopped_ || !fetch_)
        return 0;
    setPlayhead(offset);
    const QPointer<FileDownload> guard(this);
    auto                         ticket = std::make_shared<quint64>(0);
    *ticket                             = cache_->read(
        source_, size_, offset, length, lane_, provider(),
        [guard, ticket, done = std::move(done)](QByteArray bytes, MediaChunkError error) {
            if (guard) {
                guard->reads_.remove(*ticket);
                guard->schedule();
            }
            done(std::move(bytes), std::move(error));
        },
        true, quantum(false));
    if (*ticket)
        reads_.insert(*ticket);
    schedule();
    return *ticket;
}
bool FileDownload::cancelRead(quint64 ticket)
{
    reads_.remove(ticket);
    schedule();
    return cache_ && cache_->cancel(ticket);
}
void FileDownload::pin(bool active)
{
    if (cache_)
        cache_->pin(source_, size_, active);
}

quint64 FileDownload::downloadAll(GoalCompletion done, Progress progress)
{
    if (stopped_ || !cache_ || !fetch_ || !size_ || !done)
        return 0;
    const auto id = ++next_;
    goals_.insert(id, { std::move(done), std::move(progress) });
    if (!goalPinned_) {
        goalPinned_ = true;
        pin(true);
    }
    cache_->configure(source_, size_, {}, { { QStringLiteral("retain-partial"), true } });
    cache_->resume(source_, size_);
    schedule();
    if (goals_.size() == 1)
        emit fullDownloadChanged(true);
    return id;
}
bool FileDownload::cancelGoal(quint64 goal)
{
    auto it = goals_.find(goal);
    if (it == goals_.end())
        return false;
    auto done = std::move(it->done);
    goals_.erase(it);
    if (goals_.isEmpty()) {
        const auto pending = std::exchange(pending_, {});
        if (cache_)
            for (const auto &p : pending)
                cache_->cancel(p.ticket);
        if (std::exchange(goalPinned_, false))
            pin(false);
    }
    if (goals_.isEmpty())
        emit fullDownloadChanged(false);
    done({}, { MediaChunkError::Cancelled, QStringLiteral("File download cancelled") });
    return true;
}
void FileDownload::schedule()
{
    if (scheduled_ || stopped_ || goals_.isEmpty())
        return;
    scheduled_ = true;
    QTimer::singleShot(0, this, [this] {
        scheduled_ = false;
        pump();
    });
}
void FileDownload::pump()
{
    if (stopped_ || goals_.isEmpty() || checkpointing_)
        return;
    if (!cache_) {
        finish(QStringLiteral("File storage closed"));
        return;
    }
    if (localFile_) {
        const auto path = localFile_();
        if (!path.isEmpty()) {
            finish({}, path);
            return;
        }
    }
    const auto current = state();
    if (!current.error.isEmpty()) {
        finish({ MediaChunkError::IntegrityError, current.error });
        return;
    }
    if (!current.completionError.isEmpty()) {
        finish({ MediaChunkError::StorageUnavailable, current.completionError });
        return;
    }
    quint64 available = 0;
    for (const auto &range : current.ranges)
        available += range.second;
    const QPointer<FileDownload> guard(this);
    const auto                   goals = goals_;
    for (const auto &goal : goals) {
        if (goal.progress)
            goal.progress(qreal(available) / size_);
        if (!guard || stopped_ || goals_.isEmpty())
            return;
    }
    if (current.complete && pending_.isEmpty()) {
        checkpointing_ = true;
        cache_->checkpoint([guard](QByteArray, MediaChunkError error) {
            if (guard) {
                guard->checkpointing_ = false;
                guard->finish(error);
            }
        });
        return;
    }
    if (!reads_.isEmpty())
        return; // Let interactive demand consume bandwidth before admitting more background work.
    while (pending_.size() < MediaChunkCache::MaxActiveFetches - 1) {
        auto covered = current.ranges;
        for (auto it = pending_.cbegin(); it != pending_.cend(); ++it)
            covered.append({ it.key(), it->length });
        std::sort(covered.begin(), covered.end());
        const auto hole = [&](quint64 begin, quint64 end) {
            quint64 cursor = begin;
            for (const auto &range : covered) {
                if (range.first > cursor)
                    break;
                cursor = std::max(cursor, std::min(end, range.first + range.second));
            }
            return cursor < end ? std::optional<quint64>(cursor) : std::nullopt;
        };
        auto next = hole(playhead_, size_);
        if (!next) {
            // Admission of the tail is not completion of the tail. Do not wrap
            // while its final requests are still in flight.
            for (auto it = pending_.cbegin(); it != pending_.cend(); ++it)
                if (it.key() + it->length > playhead_)
                    return;
            next = hole(0, playhead_);
        }
        if (!next)
            return;
        const auto q      = quantum(true);
        const auto start  = *next;
        const auto length = std::min(q - start % q, size_ - start);
        const auto ticket = cache_->ensure(
            source_, size_, start, length, lane_, provider(),
            [guard, start](QByteArray, MediaChunkError error) {
                if (!guard || !guard->pending_.contains(start))
                    return;
                guard->pending_.remove(start);
                if (!error.isEmpty())
                    guard->finish(error);
                else
                    guard->schedule();
            },
            q);
        if (!ticket) {
            QTimer::singleShot(50, this, [this] { schedule(); });
            return;
        }
        pending_.insert(start, { ticket, length });
    }
}
void FileDownload::finish(MediaChunkError error, QString localPath)
{
    const auto pending = std::exchange(pending_, {});
    const auto goals   = std::exchange(goals_, {});
    if (cache_)
        for (const auto &p : pending)
            cache_->cancel(p.ticket);
    if (std::exchange(goalPinned_, false))
        pin(false);
    const auto path = error.isEmpty() ? (localPath.isEmpty() ? state().path : localPath) : QString();
    if (!goals.isEmpty())
        emit fullDownloadChanged(false);
    for (const auto &goal : goals) {
        if (error.isEmpty() && goal.progress)
            goal.progress(1);
        goal.done(path, error);
    }
}
void FileDownload::shutdown()
{
    if (stopped_)
        return;
    stopped_           = true;
    const auto reads   = std::exchange(reads_, {});
    const auto storage = cache_;
    finish({ MediaChunkError::Cancelled, QStringLiteral("File download closed") });
    // A completion may destroy this controller. Only local snapshots follow.
    for (const auto ticket : reads)
        if (storage)
            storage->cancel(ticket);
}
