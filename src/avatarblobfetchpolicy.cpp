/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "avatarblobfetchpolicy.h"

#include <limits>

AvatarBlobFetchPolicy::AvatarBlobFetchPolicy(int retryDelayMs, int capacity) :
    retryDelayMs_(qMax(0, retryDelayMs)), capacity_(qMax(1, capacity))
{
}

QString AvatarBlobFetchPolicy::key(const QString &accountId, const QString &bareJid)
{
    return accountId + QChar(0x1f) + bareJid;
}

bool AvatarBlobFetchPolicy::ensureCapacity()
{
    if (entries_.size() < capacity_)
        return true;

    // Never evict a pending download: a second metadata event would then
    // launch a duplicate blob request, and its late result could be accepted.
    auto oldest = entries_.end();
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (!it->inFlight.isEmpty())
            continue;
        if (oldest == entries_.end() || it->touched < oldest->touched)
            oldest = it;
    }
    if (oldest == entries_.end())
        return false;
    entries_.erase(oldest);
    return true;
}

AvatarBlobFetchPolicy::Entry *AvatarBlobFetchPolicy::lookup(const QString &accountId, const QString &bareJid)
{
    const auto it = entries_.find(key(accountId, bareJid));
    return it == entries_.end() ? nullptr : &it.value();
}

void AvatarBlobFetchPolicy::announce(const QString &accountId, const QString &bareJid,
                                     const QByteArray &hash, const QString &metadataItemId)
{
    const QString scoped = key(accountId, bareJid);
    auto it = entries_.find(scoped);
    if (it == entries_.end()) {
        if (!ensureCapacity())
            return;
        it = entries_.insert(scoped, Entry {});
    }
    Entry &entry = it.value();
    if (entry.hash != hash) {
        entry.hash = hash;
        entry.inFlight.clear(); // The old reply must not revive a new avatar.
        entry.retryNotBeforeMs = 0;
    }
    entry.metadataItemId = metadataItemId;
    entry.touched = ++serial_;
}

bool AvatarBlobFetchPolicy::beginFetch(const QString &accountId, const QString &bareJid,
                                      const QByteArray &hash, qint64 nowMs)
{
    Entry *entry = lookup(accountId, bareJid);
    if (!entry || hash.size() != 20 || entry->hash != hash
        || !entry->inFlight.isEmpty() || nowMs < entry->retryNotBeforeMs)
        return false;
    entry->inFlight = hash;
    entry->touched = ++serial_;
    return true;
}

void AvatarBlobFetchPolicy::finish(const QString &accountId, const QString &bareJid,
                                   const QByteArray &hash, bool validData, qint64 nowMs)
{
    Entry *entry = lookup(accountId, bareJid);
    if (!entry || entry->hash != hash || entry->inFlight != hash)
        return; // A response to an obsolete metadata generation.
    entry->inFlight.clear();
    entry->retryNotBeforeMs = validData ? 0 : nowMs + retryDelayMs_;
    entry->touched = ++serial_;
}

bool AvatarBlobFetchPolicy::expects(const QString &accountId, const QString &bareJid,
                                    const QByteArray &hash) const
{
    const auto it = entries_.constFind(key(accountId, bareJid));
    return it != entries_.cend() && !hash.isEmpty() && it->hash == hash;
}

bool AvatarBlobFetchPolicy::retractMetadata(const QString &accountId, const QString &bareJid,
                                            const QString &itemId)
{
    Entry *entry = lookup(accountId, bareJid);
    if (!entry || itemId.isEmpty() || entry->metadataItemId != itemId)
        return false;
    entry->metadataItemId.clear();
    entry->hash.clear();
    entry->inFlight.clear();
    entry->retryNotBeforeMs = 0;
    entry->touched = ++serial_;
    return true;
}

bool AvatarBlobFetchPolicy::retractData(const QString &accountId, const QString &bareJid,
                                        const QByteArray &hash)
{
    Entry *entry = lookup(accountId, bareJid);
    if (!entry || entry->hash != hash || hash.isEmpty())
        return false;
    entry->hash.clear();
    entry->inFlight.clear();
    entry->retryNotBeforeMs = 0;
    entry->touched = ++serial_;
    return true;
}

void AvatarBlobFetchPolicy::forgetEntity(const QString &accountId, const QString &bareJid)
{
    entries_.remove(key(accountId, bareJid));
}

void AvatarBlobFetchPolicy::forgetAccount(const QString &accountId)
{
    const QString prefix = accountId + QChar(0x1f);
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (it.key().startsWith(prefix))
            it = entries_.erase(it);
        else
            ++it;
    }
}
