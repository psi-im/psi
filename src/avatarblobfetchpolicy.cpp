/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "avatarblobfetchpolicy.h"

#include <QUrl>

QString AvatarIdentity::accountPrefix(const QString &accountId)
{
    return QStringLiteral("account/")
        + QString::fromLatin1(QUrl::toPercentEncoding(accountId)) + QLatin1Char('/');
}

QString AvatarIdentity::bindingKey(const QString &accountId, const QString &normalizedJid, bool occupant)
{
    return accountPrefix(accountId)
        + (occupant ? QStringLiteral("occupant/") : QStringLiteral("contact/"))
        + normalizedJid;
}

AvatarBlobFetchPolicy::AvatarBlobFetchPolicy(int retryDelayMs, int capacity) :
    retryDelayMs_(qMax(0, retryDelayMs)), capacity_(qMax(1, capacity))
{
}

QString AvatarBlobFetchPolicy::key(const QString &accountId, const QString &bareJid)
{
    return accountId + QChar(0x1f) + bareJid;
}

QString AvatarBlobFetchPolicy::flightKey(const QString &accountId, const QByteArray &hash)
{
    return accountId + QChar(0x1f) + QString::fromLatin1(hash.toHex());
}

void AvatarBlobFetchPolicy::releaseFlight(const QString &accountId, const QString &bareJid,
                                          const QByteArray &hash)
{
    if (hash.isEmpty())
        return;
    const QString flight = flightKey(accountId, hash);
    if (inFlightByHash_.value(flight) == bareJid)
        inFlightByHash_.remove(flight);
}

bool AvatarBlobFetchPolicy::ensureCapacity()
{
    if (entries_.size() < capacity_)
        return true;

    // Never evict a pending download: a second metadata event would then
    // launch a duplicate blob request, and its late result could be accepted.
    auto oldest = entries_.end();
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (!it->inFlight.isEmpty() || it->waitingForSharedHash)
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
        releaseFlight(accountId, bareJid, entry.inFlight);
        entry.hash = hash;
        entry.inFlight.clear(); // Old IQ cannot restore a superseded avatar.
        entry.waitingForSharedHash = false;
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
    // FileCache already deduplicates completed blobs. Only outstanding IQ
    // requests need a second (small) index to avoid simultaneous transfers.
    const QString flight = flightKey(accountId, hash);
    if (inFlightByHash_.contains(flight)) {
        entry->waitingForSharedHash = true;
        entry->touched = ++serial_;
        return false;
    }
    inFlightByHash_.insert(flight, bareJid);
    entry->waitingForSharedHash = false;
    entry->inFlight = hash;
    entry->touched = ++serial_;
    return true;
}

QStringList AvatarBlobFetchPolicy::finish(const QString &accountId, const QString &bareJid,
                                         const QByteArray &hash, bool validData, qint64 nowMs)
{
    Entry *entry = lookup(accountId, bareJid);
    if (!entry || entry->hash != hash || entry->inFlight != hash)
        return {}; // An obsolete reply must not revive any waiter.
    releaseFlight(accountId, bareJid, hash);
    entry->inFlight.clear();
    entry->retryNotBeforeMs = validData ? 0 : nowMs + retryDelayMs_;
    entry->touched = ++serial_;

    QStringList waiters;
    const QString prefix = accountId + QChar(0x1f);
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (!it.key().startsWith(prefix) || !it->waitingForSharedHash || it->hash != hash)
            continue;
        it->waitingForSharedHash = false;
        it->retryNotBeforeMs = validData ? 0 : nowMs + retryDelayMs_;
        it->touched = ++serial_;
        if (validData)
            waiters.append(it.key().mid(prefix.size()));
    }
    return waiters;
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
    releaseFlight(accountId, bareJid, entry->inFlight);
    entry->metadataItemId.clear();
    entry->hash.clear();
    entry->inFlight.clear();
    entry->waitingForSharedHash = false;
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
    releaseFlight(accountId, bareJid, entry->inFlight);
    entry->hash.clear();
    entry->inFlight.clear();
    entry->waitingForSharedHash = false;
    entry->retryNotBeforeMs = 0;
    entry->touched = ++serial_;
    return true;
}

void AvatarBlobFetchPolicy::forgetEntity(const QString &accountId, const QString &bareJid)
{
    if (const Entry *entry = lookup(accountId, bareJid))
        releaseFlight(accountId, bareJid, entry->inFlight);
    entries_.remove(key(accountId, bareJid));
}

void AvatarBlobFetchPolicy::forgetAccount(const QString &accountId)
{
    const QString prefix = accountId + QChar(0x1f);
    for (auto it = inFlightByHash_.begin(); it != inFlightByHash_.end();) {
        if (it.key().startsWith(prefix))
            it = inFlightByHash_.erase(it);
        else
            ++it;
    }
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (it.key().startsWith(prefix))
            it = entries_.erase(it);
        else
            ++it;
    }
}

void AvatarLegacyAliasPolicy::observe(const QString &accountId, const QString &legacyJid)
{
    owners_[legacyJid].insert(accountId);
}

bool AvatarLegacyAliasPolicy::allows(const QString &accountId, const QString &legacyJid) const
{
    const auto it = owners_.constFind(legacyJid);
    return it != owners_.cend() && it->size() == 1 && it->contains(accountId);
}

bool AvatarLegacyAliasPolicy::ambiguous(const QString &legacyJid) const
{
    return owners_.value(legacyJid).size() > 1;
}

void AvatarLegacyAliasPolicy::forgetAccount(const QString &accountId)
{
    for (auto it = owners_.begin(); it != owners_.end();) {
        it->remove(accountId);
        if (it->isEmpty())
            it = owners_.erase(it);
        else
            ++it;
    }
}
