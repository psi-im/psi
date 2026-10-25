/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

// FileCache stores shared bytes by hash. Only JID bindings and decoded
// pixmaps need an account/entity-scoped key.
namespace AvatarIdentity {
QString accountPrefix(const QString &accountId);
QString bindingKey(const QString &accountId, const QString &normalizedJid, bool occupant);
} // namespace AvatarIdentity

// Tracks avatar metadata identities, concurrent blob retrievals and temporary
// failures. The caller owns the actual image bytes (FileCache) and performs
// I/O only when beginFetch() allows it. No timers, polling or implicit fetches.
//
// All time arguments are monotonic milliseconds, so tests can advance time
// without sleeping and production can use a single QElapsedTimer.
class AvatarBlobFetchPolicy {
public:
    explicit AvatarBlobFetchPolicy(int retryDelayMs = 60000, int capacity = 4096);

    void announce(const QString &accountId, const QString &bareJid, const QByteArray &hash,
                  const QString &metadataItemId);
    bool beginFetch(const QString &accountId, const QString &bareJid, const QByteArray &hash, qint64 nowMs);
    // Mark a response successful only after the expected bytes were validated.
    // Unsuccessful responses establish a bounded retry cooldown, not NoPhoto.
    // Returns the other contacts awaiting these same bytes after a validated
    // success. Caller must bind them from FileCache by hash; no more IQs.
    QStringList finish(const QString &accountId, const QString &bareJid, const QByteArray &hash,
                       bool validData, qint64 nowMs);
    bool expects(const QString &accountId, const QString &bareJid, const QByteArray &hash) const;
    bool retractMetadata(const QString &accountId, const QString &bareJid, const QString &itemId);
    bool retractData(const QString &accountId, const QString &bareJid, const QByteArray &hash);
    void forgetEntity(const QString &accountId, const QString &bareJid);
    void forgetAccount(const QString &accountId);

    int trackedCount() const { return entries_.size(); }

private:
    struct Entry {
        QByteArray hash;
        QByteArray inFlight;
        bool       waitingForSharedHash = false;
        QString    metadataItemId;
        qint64     retryNotBeforeMs = 0;
        quint64    touched = 0;
    };

    static QString key(const QString &accountId, const QString &bareJid);
    bool ensureCapacity();
    Entry *lookup(const QString &accountId, const QString &bareJid);

    const qint64 retryDelayMs_;
    const int capacity_;
    quint64 serial_ = 0;
    QHash<QString, Entry> entries_;
};

// The old avatars/<JID> icon API has no account context. Publish a legacy
// alias only when the JID has been observed from exactly one account.
// A collision must produce no legacy icon, never another account's image.
class AvatarLegacyAliasPolicy {
public:
    void observe(const QString &accountId, const QString &legacyJid);
    bool allows(const QString &accountId, const QString &legacyJid) const;
    bool ambiguous(const QString &legacyJid) const;
    void forgetAccount(const QString &accountId);
private:
    QHash<QString, QSet<QString>> owners_;
};
