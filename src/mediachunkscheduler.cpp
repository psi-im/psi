// Adapted from Ri0n/AnyKeep PR #156, db77d42ef791225839d03ecb0c62cd06462b31e3.
// Original implementation: Sergei Ilinykh (AnyKeep).
#include "mediachunkscheduler.h"
#include "mediachunkcallback.h"

#include <QPointer>
#include <QTimer>
#include <algorithm>
#include <limits>
#include <utility>

MediaChunkScheduler::MediaChunkScheduler(QObject *parent) : QObject(parent) { }
MediaChunkScheduler::~MediaChunkScheduler() { shutdown(); }

QString MediaChunkScheduler::makeKey(const QString &source, quint64 index)
{
    // Prefix the source length so distinct user-supplied identities cannot
    // accidentally collide with the numeric chunk suffix.
    return QString::number(source.size()) + QLatin1Char(':') + source + QLatin1Char(':') + QString::number(index);
}

quint64 MediaChunkScheduler::request(QString immutableSource, quint64 index, QString lane, int priority, Fetch fetch,
                                     Completion done)
{
    return addWaiter(std::move(immutableSource), index, std::move(lane), priority, std::move(fetch), {},
                     std::move(done));
}

quint64 MediaChunkScheduler::requestCancelable(QString immutableSource, quint64 index, QString lane, int priority,
                                               CancelableFetch fetch, Completion done)
{
    return addWaiter(std::move(immutableSource), index, std::move(lane), priority, {}, std::move(fetch),
                     std::move(done));
}

quint64 MediaChunkScheduler::addWaiter(QString immutableSource, quint64 index, QString lane, int priority, Fetch fetch,
                                       CancelableFetch cancelableFetch, Completion done)
{
    if (stopped_ || immutableSource.isEmpty() || lane.isEmpty() || (!fetch && !cancelableFetch) || !done) {
        if (done)
            done({}, QStringLiteral("Media chunk scheduler unavailable or invalid request"));
        return 0;
    }

    const auto key = makeKey(immutableSource, index);
    auto       job = jobs_.value(key);
    if (job && job->lane != lane) {
        done({}, QStringLiteral("Conflicting transport lane for one immutable media chunk"));
        return 0;
    }
    if (!job) {
        job                  = std::make_shared<Job>();
        job->key             = key;
        job->lane            = std::move(lane);
        job->sequence        = ++nextSequence_;
        job->fetch           = std::move(fetch);
        job->cancelableFetch = std::move(cancelableFetch);
        jobs_.insert(key, job);
    }
    const quint64 ticket = ++nextTicket_;
    job->waiters.append({ ticket, priority, std::move(done) });
    ticketKeys_.insert(ticket, key);
    schedule();
    return ticket;
}

bool MediaChunkScheduler::cancel(quint64 ticket)
{
    const auto key = ticketKeys_.take(ticket);
    if (key.isEmpty())
        return false;
    const auto job = jobs_.value(key);
    if (!job)
        return false;
    Completion done;
    for (auto it = job->waiters.begin(); it != job->waiters.end(); ++it) {
        if (it->ticket == ticket) {
            done = std::move(it->done);
            job->waiters.erase(it);
            break;
        }
    }
    const QPointer<MediaChunkScheduler> guard(this);
    if (job->waiters.isEmpty()) {
        if (!job->active)
            jobs_.remove(key);
        else if (job->abort && !job->aborting) {
            // Keep admission occupied until the provider reports completion.
            // Protocol teardown below that provider may outlive its loader. A new waiter arriving during abort is
            // rescheduled if the aborted transfer does not return valid data.
            job->aborting = true;
            job->abort();
        }
    }
    if (guard)
        schedule();
    if (done)
        done({}, { MediaChunkError::Cancelled, QStringLiteral("Media chunk request cancelled") });
    return true;
}

bool MediaChunkScheduler::reprioritize(quint64 ticket, int priority)
{
    const auto job = jobs_.value(ticketKeys_.value(ticket));
    if (!job)
        return false;
    for (auto &waiter : job->waiters) {
        if (waiter.ticket == ticket) {
            waiter.priority = priority;
            schedule();
            return true;
        }
    }
    return false;
}

void MediaChunkScheduler::setMaxActive(int count)
{
    maxActive_ = std::max(1, count);
    schedule();
}

void MediaChunkScheduler::setLaneMaxActive(const QString &lane, int count)
{
    if (!lane.isEmpty()) {
        laneLimits_.insert(lane, std::max(1, count));
        schedule();
    }
}

int MediaChunkScheduler::pendingCount() const
{
    int result = 0;
    for (const auto &job : jobs_) {
        if (!job->active && !job->waiters.isEmpty())
            ++result;
    }
    return result;
}

void MediaChunkScheduler::schedule()
{
    if (stopped_ || scheduled_)
        return;
    scheduled_ = true;
    QTimer::singleShot(0, this, [this] {
        scheduled_ = false;
        pump();
    });
}

void MediaChunkScheduler::pump()
{
    if (stopped_)
        return;
    while (active_ < maxActive_) {
        std::shared_ptr<Job> candidate;
        int                  bestPriority = std::numeric_limits<int>::min();
        for (const auto &job : jobs_) {
            if (job->active || job->waiters.isEmpty()
                || laneActive_.value(job->lane) >= laneLimits_.value(job->lane, 1))
                continue;
            int priority = std::numeric_limits<int>::min();
            for (const auto &waiter : job->waiters)
                priority = std::max(priority, waiter.priority);
            if (!candidate || priority > bestPriority
                || (priority == bestPriority && job->sequence < candidate->sequence)) {
                candidate    = job;
                bestPriority = priority;
            }
        }
        if (!candidate)
            break;
        candidate->active = true;
        ++active_;
        ++laneActive_[candidate->lane];

        const QString                       key      = candidate->key;
        const quint64                       sequence = candidate->sequence;
        const QPointer<MediaChunkScheduler> guard(this);
        auto                                complete
            = queuedMediaChunkCompletion(this, [guard, key, sequence](QByteArray bytes, MediaChunkError error) {
                  if (guard)
                      guard->finish(key, sequence, std::move(bytes), std::move(error));
              });
        if (candidate->cancelableFetch) {
            candidate->abort = candidate->cancelableFetch(std::move(complete));
            if (!guard || guard->stopped_) {
                if (candidate->abort && !candidate->aborting)
                    candidate->abort();
                return;
            }
            if (candidate->waiters.isEmpty() && candidate->abort && !candidate->aborting) {
                candidate->aborting = true;
                candidate->abort();
            }
        } else {
            candidate->fetch(std::move(complete));
        }
        if (!guard || guard->stopped_)
            return;
    }
}

void MediaChunkScheduler::finish(const QString &key, quint64 sequence, QByteArray bytes, MediaChunkError error)
{
    auto job = jobs_.value(key);
    if (!job || job->sequence != sequence || !job->active)
        return; // Late or duplicated completion after shutdown/retry.
    --active_;
    if (--laneActive_[job->lane] == 0)
        laneActive_.remove(job->lane);
    if (job->aborting && !job->waiters.isEmpty() && (!error.isEmpty() || bytes.isEmpty())) {
        // Consumers may have joined while a now-unneeded transfer was being
        // aborted. Restart it as a new admitted job rather than reporting a
        // cancellation that belonged to a previous consumer.
        job->active   = false;
        job->aborting = false;
        job->abort    = {};
        job->sequence = ++nextSequence_;
        schedule();
        return;
    }
    jobs_.remove(key);
    for (const auto &waiter : std::as_const(job->waiters))
        ticketKeys_.remove(waiter.ticket);
    auto waiters = std::move(job->waiters);
    schedule();
    for (auto &waiter : waiters) {
        if (waiter.done)
            waiter.done(bytes, error);
    }
}

void MediaChunkScheduler::shutdown()
{
    if (stopped_)
        return;
    stopped_  = true;
    auto jobs = std::move(jobs_);
    jobs_.clear();
    ticketKeys_.clear();
    laneActive_.clear();
    active_ = 0;
    for (const auto &job : std::as_const(jobs)) {
        if (job->abort && !job->aborting)
            job->abort();
        for (const auto &waiter : std::as_const(job->waiters)) {
            if (waiter.done)
                waiter.done({}, QStringLiteral("Media chunk scheduler stopped"));
        }
    }
}
