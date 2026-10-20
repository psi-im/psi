/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "MamHistorySource.h"
#include <QtMath>
#include <algorithm>
#include <limits>

int MamHistorySource::logicalIndex(int knownIndex) const
{
    return knownIndex >= 0 && knownIndex < positions_.size() ? positions_.at(knownIndex) : -1;
}
int     MamHistorySource::knownIndex(int logicalIndex) const { return knownAt_.value(logicalIndex, -1); }
int     MamHistorySource::indexOfKey(const QString &key) const { return indexes_.value(key, -1); }
QString MamHistorySource::keyAt(int index) const
{
    const int at = knownIndex(index);
    return at < 0 ? QString() : rows_.at(at).key;
}
qreal MamHistorySource::visibleRatio() const
{
    qint64 events = 0;
    for (const auto &window : windows_)
        events += qint64(window.last) - window.first + 1;
    if (events == 0)
        events = sampledEventCount_;
    // A cache bootstrap can replay only receipts while real bodies already
    // arrived through EDB. Zero sampled bodies is insufficient evidence that
    // the entire unseen archive has zero visible rows. Keep the initial prior.
    return events > 0 && !sampledRows_.isEmpty() ? qBound(0.0, qreal(sampledRows_.size()) / events, 1.0) : 1.0 / 3.0;
}

void MamHistorySource::restoreSnapshot(int count, const QString &lastUid)
{
    if (count < 0)
        return;
    // Reuse the retention invalidation path; an empty observation certifies no range.
    observePage({}, {}, count, -1);
    if (count > 0 && !lastUid.isEmpty()) {
        uidRanks_.insert(lastUid, count - 1);
        rankUids_.insert(count - 1, lastUid);
    }
}

void MamHistorySource::observePage(const QStringList &uids, const QStringList &keys, int count, int firstIndex,
                                   const QString &before, const QString &after, bool tailChain, bool stable)
{
    if (count >= 0) {
        if (count_ >= 0 && count < count_) {
            // Retention invalidates ordinal hints, not resident message data.
            uidRanks_.clear();
            rankUids_.clear();
            keyRanks_.clear();
            keyUids_.clear();
            windows_.clear();
            tailFirst_ = -1;
        }
        count_ = count;
    }
    const bool serverReported = firstIndex >= 0;
    lastPageServerReported_   = serverReported;
    if (firstIndex < 0 && !uids.isEmpty()) {
        for (int at = 0; at < uids.size(); ++at) {
            const int known = rankOfUid(uids.at(at));
            if (known >= at) {
                firstIndex = known - at;
                break;
            }
        }
        const int boundary = rankOfUid(before);
        const int previous = rankOfUid(after);
        if (firstIndex >= 0) { /* An actual UID in this page anchors its position. */
        } else if (boundary >= uids.size())
            firstIndex = boundary - uids.size();
        else if (previous >= 0)
            firstIndex = previous + 1;
        else if (tailChain && count_ >= 0) {
            // Count-only servers can still support index requests. Seed an
            // explicitly estimated tail placement; an indexed response must
            // supply its own index before it is accepted as a seek result.
            int laterRows = 0;
            if (!before.isEmpty()) {
                for (int at = 0; at < rows_.size(); ++at)
                    if (rows_.at(at).message.archiveId() == before) {
                        laterRows = rows_.size() - at;
                        break;
                    }
            }
            firstIndex = qMax(0, count_ - int(uids.size()) - qCeil(laterRows / qMax(.001, visibleRatio())));
        }
    }
    if (tailChain && firstIndex >= 0 && !uids.isEmpty()) {
        const int last = firstIndex + uids.size() - 1;
        tailFirst_     = tailFirst_ >= 0 && last >= tailFirst_ - 1 ? qMin(tailFirst_, firstIndex) : firstIndex;
        if (!before.isEmpty()) {
            uidRanks_.insert(before, last + 1);
            rankUids_.insert(last + 1, before);
            for (const auto &row : rows_)
                if (row.message.archiveId() == before)
                    keyRanks_.insert(row.key, last + 1);
        }
    }
    QSet<QString> messageKeys;
    for (const auto &row : rows_)
        if (row.message.type() == MessageView::Message)
            messageKeys.insert(row.key);
    lastPageUids_       = uids;
    lastPageFirstIndex_ = firstIndex;
    lastPageHasMessage_ = false;
    lastPageMessageKeys_.clear();

    sampledEventCount_ += uids.size();
    for (int i = 0; i < uids.size(); ++i) {
        const auto key = keys.value(i);
        if (messageKeys.contains(key))
            sampledRows_.insert(key);
        if (firstIndex < 0)
            continue;
        const int rank = firstIndex + i;
        // Protocol-only interior events need no resident identity. Keep real
        // rows and page edges; SQLite owns the full verified event stream.
        if (!key.isEmpty() || i == 0 || i + 1 == uids.size()) {
            uidRanks_.insert(uids.at(i), rank);
            rankUids_.insert(rank, uids.at(i));
        }
        if (!key.isEmpty()) {
            const int oldRank = keyRanks_.value(key, rank);
            keyRanks_.insert(key, qMin(oldRank, rank));
            if (rank <= oldRank)
                keyUids_.insert(key, uids.at(i));
        }
    }
    for (const auto &key : keys) {
        const int rank = keyRanks_.value(key, -1);
        if (messageKeys.contains(key) && rank >= firstIndex && rank < firstIndex + uids.size()) {
            lastPageHasMessage_ = true;
            lastPageMessageKeys_.insert(key);
        }
    }
    if (firstIndex >= 0 && !uids.isEmpty()) {
        windows_.append({ firstIndex, firstIndex + int(uids.size()) - 1, serverReported, stable });
        std::sort(windows_.begin(), windows_.end(), [](const Window &a, const Window &b) { return a.first < b.first; });
        QVector<Window> merged;
        for (const auto &window : windows_) {
            if (!merged.isEmpty() && window.first <= merged.last().last + 1) {
                merged.last().last = qMax(merged.last().last, window.last);
                merged.last().serverReported &= window.serverReported;
                merged.last().stable &= window.stable;
            } else
                merged.append(window);
        }
        windows_ = merged;
    }
}
void MamHistorySource::enableVirtual()
{
    virtual_ = canSeek() && !windows_.isEmpty();
    rebuild();
}
void MamHistorySource::rejectIndex()
{
    indexRejected_ = true;
    virtual_       = false;
    rebuild();
}
void MamHistorySource::clear() { *this = MamHistorySource(); }

void MamHistorySource::rebuild()
{
    positions_.clear();
    knownAt_.clear();
    indexes_.clear();
    QHash<QString, int> projected;
    qint64              position = 0;
    int                 event    = 0;
    const auto          gapSlots = [this](int events) -> qint64 {
        // A known non-empty unresolved interval must stay reachable even if
        // recent pages happened to contain almost entirely receipts.
        return events > 0 ? qMax<qint64>(1, qCeil(events * visibleRatio())) : 0;
    };
    if (virtual_) {
        auto projectionWindows = windows_;
        // The ordinary before-cursor chain is adjacent to delivered MUC/live
        // history. Keep that presentation island compact without certifying
        // the intervening protocol events in the persistent archive cache.
        if (tailFirst_ >= 0 && count_ > tailFirst_)
            projectionWindows.append({ tailFirst_, count_ - 1 });
        std::sort(projectionWindows.begin(), projectionWindows.end(),
                  [](const Window &a, const Window &b) { return a.first < b.first; });
        QVector<Window> merged;
        for (const auto &window : projectionWindows) {
            if (!merged.isEmpty() && window.first <= merged.last().last + 1)
                merged.last().last = qMax(merged.last().last, window.last);
            else
                merged.append(window);
        }
        for (const auto &window : merged) {
            position += gapSlots(window.first - event);
            QVector<QPair<int, QString>> keys;
            for (const auto &row : rows_) {
                const int rank = keyRanks_.value(row.key, -1);
                if (rank >= window.first && rank <= window.last)
                    keys.append(qMakePair(rank, row.key));
            }
            std::sort(keys.begin(), keys.end());
            for (const auto &key : keys)
                projected.insert(key.second, int(qMin<qint64>(position++, std::numeric_limits<int>::max() - 1)));
            event = window.last + 1;
        }
        position += gapSlots(count_ - event);
    }
    estimatedTotal_ = virtual_ ? qMax<qint64>(position, rows_.size()) : rows_.size();
    // LongListWidget's height index is dense, but payloads are sparse. Bound
    // geometry memory independently of the server's possibly approximate count.
    // Larger archives retain proportional event addressing in virtual slots.
    const int    budget      = qMax(1000000, int(rows_.size()));
    const qreal  compression = estimatedTotal_ > budget ? qreal(budget) / estimatedTotal_ : 1.0;
    QVector<int> nextProjected(rows_.size(), -1);
    int          nextKnown = -1;
    for (int i = rows_.size() - 1; i >= 0; --i) {
        nextProjected[i] = nextKnown;
        if (projected.contains(rows_.at(i).key))
            nextKnown = i;
    }
    int previous = -1;
    for (int i = 0; i < rows_.size(); ++i) {
        const auto &key = rows_.at(i).key;
        int         at  = i;
        if (virtual_) {
            if (projected.contains(key))
                at = int(projected.value(key) * compression);
            else {
                // Local-only and live rows are real provisional identities.
                // Place them next to their known chronological neighbours.
                at             = previous + 1;
                const int next = nextProjected.at(i);
                if (next >= 0)
                    at = qMax(at, int(projected.value(rows_.at(next).key) * compression) - (next - i));
            }
            at = qMax(at, previous + 1);
        }
        positions_.append(at);
        knownAt_.insert(at, i);
        indexes_.insert(key, at);
        previous = at;
    }
    logicalCount_ = qMax(previous + 1, virtual_ ? int(qMin<qint64>(estimatedTotal_, budget)) : int(rows_.size()));
}

MamHistorySource::Request MamHistorySource::requestForRange(int first, int last, int limit, quint64 generation,
                                                            bool preferIndex) const
{
    Request request;
    request.first         = first;
    request.last          = last;
    request.limit         = qBound(1, limit, 40);
    request.snapshotCount = count_;
    request.generation    = generation;
    if (!virtual_ || !canSeek() || first < 0 || last < first || first >= logicalCount_)
        return request;
    // Prefer an adjacent proven UID edge. Only a disconnected gap needs RSM index.
    int left = -1, right = -1;
    for (int i = 0; i < positions_.size(); ++i) {
        if (positions_.at(i) < first)
            left = i;
        if (positions_.at(i) > last) {
            right = i;
            break;
        }
    }
    if (!preferIndex && right >= 0 && positions_.at(right) - last <= 2) {
        const int rank = keyRanks_.value(rows_.at(right).key, -1);
        if (rank >= 0 && rankUids_.contains(rank)) {
            request.kind   = Request::Before;
            request.cursor = keyUids_.value(rows_.at(right).key, rows_.at(right).message.archiveId());
            return request;
        }
    }
    if (!preferIndex && left >= 0 && first - positions_.at(left) <= 2) {
        const int rank = keyRanks_.value(rows_.at(left).key, -1);
        if (rank >= 0 && rankUids_.contains(rank)) {
            request.kind   = Request::After;
            request.cursor = keyUids_.value(rows_.at(left).key, rows_.at(left).message.archiveId());
            return request;
        }
    }
    request.kind  = Request::Index;
    request.index = qBound(0, int(qreal(first + (last - first) / 2) * count_ / qMax(1, logicalCount_)) - limit / 2,
                           qMax(0, count_ - 1));
    return request;
}
int MamHistorySource::logicalPositionForEvent(int eventIndex) const
{
    int leftEvent = 0, leftPosition = 0;
    int rightEvent = qMax(1, count_ - 1), rightPosition = qMax(0, logicalCount_ - 1);
    for (int at = 0; at < rows_.size(); ++at) {
        const int rank = keyRanks_.value(rows_.at(at).key, -1);
        if (rank < 0)
            continue;
        if (rank == eventIndex)
            return positions_.at(at);
        if (rank < eventIndex && rank >= leftEvent) {
            leftEvent    = rank;
            leftPosition = positions_.at(at);
        } else if (rank > eventIndex && rank <= rightEvent) {
            rightEvent    = rank;
            rightPosition = positions_.at(at);
        }
    }
    const qreal ratio = qreal(eventIndex - leftEvent) / qMax(1, rightEvent - leftEvent);
    return qBound(0, leftPosition + qRound(ratio * (rightPosition - leftPosition)), qMax(0, logicalCount_ - 1));
}

QString MamHistorySource::closestKnownKey(int eventIndex) const
{
    QString closest;
    qint64  distance = std::numeric_limits<qint64>::max();
    for (const auto &row : rows_) {
        const int rank = keyRanks_.value(row.key, -1);
        if (rank >= 0 && qAbs(qint64(rank) - eventIndex) < distance) {
            distance = qAbs(qint64(rank) - eventIndex);
            closest  = row.key;
        }
    }
    return closest;
}

void MamHistorySource::beginNavigation(const Request &request)
{
    navigationWasIndex_ = request.kind == Request::Index;
    navigationTarget_   = navigationWasIndex_ ? request.index + request.limit / 2
                                              : rankOfUid(request.cursor) + (request.kind == Request::Before ? -1 : 1);
    navigationRightCursor_.clear();
    navigationCursors_.clear();
    navigationStalled_ = false;
    if (!request.cursor.isEmpty())
        navigationCursors_.insert(QString::number(request.kind) + QLatin1Char(':') + request.cursor);
}
MamHistorySource::Request MamHistorySource::continueNavigation(const Request &request, bool complete, bool wantMore)
{
    Request next       = request;
    next.index         = -1;
    next.snapshotCount = count_;
    if (lastPageHasMessage_ && !wantMore)
        return {};
    if (request.kind == Request::Index && !lastPageUids_.isEmpty())
        navigationRightCursor_ = lastPageUids_.last();
    if (request.kind == Request::After) {
        if (complete || lastPageUids_.isEmpty())
            return {};
        next.kind   = Request::After;
        next.cursor = lastPageUids_.last();
    } else if (!lastPageUids_.isEmpty() && lastPageFirstIndex_ != 0 && !(request.kind == Request::Before && complete)) {
        next.kind   = Request::Before;
        next.cursor = lastPageUids_.first();
    } else if (navigationWasIndex_ && !navigationRightCursor_.isEmpty()) {
        next.kind   = Request::After;
        next.cursor = navigationRightCursor_;
    } else
        return {};
    const QString identity = QString::number(next.kind) + QLatin1Char(':') + next.cursor;
    if (navigationCursors_.contains(identity)) {
        navigationStalled_ = true;
        return {};
    }
    navigationCursors_.insert(identity);
    return next;
}
