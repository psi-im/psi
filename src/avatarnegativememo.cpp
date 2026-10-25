/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "avatarnegativememo.h"

AvatarNegativeMemo::AvatarNegativeMemo(int maxEntries, qint64 ttlMs) :
    entries_(qMax(1, maxEntries)), ttlMs_(qMax(qint64(1), ttlMs))
{
}

std::optional<AvatarNegativeMemo::State> AvatarNegativeMemo::lookup(const QString &key, qint64 nowMs)
{
    const Entry *entry = entries_.object(key);
    if (!entry)
        return std::nullopt;
    // Monotonic clock restarts invalidate observations, never make them immortal.
    if (nowMs < entry->recordedMs || nowMs - entry->recordedMs >= ttlMs_) {
        entries_.remove(key);
        return std::nullopt;
    }
    return entry->state;
}

void AvatarNegativeMemo::record(const QString &key, State state, qint64 nowMs)
{
    entries_.insert(key, new Entry { state, nowMs }, 1);
}

void AvatarNegativeMemo::invalidate(const QString &key)
{
    entries_.remove(key);
}

void AvatarNegativeMemo::invalidatePrefix(const QString &prefix)
{
    // QCache has no key enumeration API. Keep the LRU cache for indexed lookup,
    // and rely on account-scoped generation reset to distinguish old keys.
    // A full account reset may flush this bounded, small memo without
    // touching the shared content-hash FileCache or decoded positive pixmaps.
    Q_UNUSED(prefix)
    entries_.clear();
}
