/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "avatarnegativememo.h"

AvatarNegativeMemo::AvatarNegativeMemo(int maxEntries, qint64 ttlMs) :
    entries_(qMax(1, maxEntries)), ttlMs_(qMax(qint64(1), ttlMs))
{
}

QString AvatarNegativeMemo::accountPrefix(const QString &key)
{
    if (!key.startsWith(QLatin1String("account/")))
        return {};
    const int end = key.indexOf(QLatin1Char('/'), 8);
    return end < 0 ? QString() : key.left(end + 1);
}

QString AvatarNegativeMemo::roomPrefix(const QString &key)
{
    const QString account = accountPrefix(key);
    if (account.isEmpty() || !key.mid(account.size()).startsWith(QLatin1String("occupant/")))
        return {};
    const int end = key.lastIndexOf(QLatin1Char('/'));
    return end < account.size() + 9 ? QString() : key.left(end + 1);
}

std::optional<AvatarNegativeMemo::State> AvatarNegativeMemo::lookup(const QString &key, qint64 nowMs)
{
    const Entry *entry = entries_.object(key);
    if (!entry)
        return std::nullopt;
    // Monotonic clock restarts invalidate observations, never make them immortal.
    if (nowMs < entry->recordedMs || nowMs - entry->recordedMs >= ttlMs_
        || entry->accountEpoch != prefixEpochs_.value(accountPrefix(key))
        || entry->roomEpoch != prefixEpochs_.value(roomPrefix(key))) {
        entries_.remove(key);
        return std::nullopt;
    }
    return entry->state;
}

void AvatarNegativeMemo::record(const QString &key, State state, qint64 nowMs)
{
    entries_.insert(key, new Entry { state, nowMs,
        prefixEpochs_.value(accountPrefix(key)), prefixEpochs_.value(roomPrefix(key)) }, 1);
}

void AvatarNegativeMemo::invalidate(const QString &key)
{
    entries_.remove(key);
}

void AvatarNegativeMemo::invalidatePrefix(const QString &prefix)
{
    if (prefix.isEmpty())
        return;
    // A generation makes scoped invalidation O(1), even for negative entries
    // that never had a FileCache binding. It also preserves other accounts.
    // Cap auxiliary generations; clearing only negatives is always safe.
    if (prefixEpochs_.size() >= entries_.maxCost() && !prefixEpochs_.contains(prefix)) {
        entries_.clear();
        prefixEpochs_.clear();
    }
    ++prefixEpochs_[prefix];
}
