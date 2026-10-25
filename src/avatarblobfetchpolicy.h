/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include "iris/xmpp_jid.h"

// FileCache stores shared bytes by hash. Only JID bindings and decoded
// pixmaps need an account/entity-scoped key.
namespace AvatarIdentity {
QString accountPrefix(const QString &accountId);
QString bindingKey(const QString &accountId, const XMPP::Jid &jid, bool occupant);
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
    void finish(const QString &accountId, const QString &bareJid, const QByteArray &hash,
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
