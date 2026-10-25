/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QCache>
#include <QHash>
#include <QString>

#include <optional>

// Local, non-authoritative negative lookup memo. Null pixmaps are NOT proof
// that a contact has no avatar. Invalidation is event-driven; TTL only bounds
// repeated expensive local resolution, and never schedules network traffic.
class AvatarNegativeMemo {
public:
    enum class State { Unknown, KnownAbsent, Fetching, Unavailable, InvalidImage, UriOnly };
    explicit AvatarNegativeMemo(int maxEntries = 4096, qint64 ttlMs = 30000);
    std::optional<State> lookup(const QString &key, qint64 nowMs);
    void record(const QString &key, State state, qint64 nowMs);
    void invalidate(const QString &key);
    void invalidatePrefix(const QString &prefix);
    int size() const { return entries_.size(); }
    int capacity() const { return entries_.maxCost(); }
private:
    struct Entry {
        State state;
        qint64 recordedMs;
        quint64 accountEpoch;
        quint64 roomEpoch;
    };
    static QString accountPrefix(const QString &key);
    static QString roomPrefix(const QString &key);
    QCache<QString, Entry> entries_;
    QHash<QString, quint64> prefixEpochs_;
    const qint64 ttlMs_;
};
