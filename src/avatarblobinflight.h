/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

// Only tracks unfinished XEP-0084 blob requests. FileCache remains the
// authoritative content-addressed store; there is no extra blob/negative cache.
// A SHA-1 identifies the same bytes regardless of who announced them.
class AvatarBlobInFlight {
public:
    struct Decision {
        bool request = false;
        quint64 token = 0;
    };

    // A new announcement invalidates this JID's interest in an older download.
    // This does not cancel the old IQ: other contacts may still be waiting.
    void supersede(const QString &jid, const QByteArray &hash)
    {
        auto old = waitingFor_.find(jid);
        if (old == waitingFor_.end() || old.value() == hash)
            return;
        auto flight = pending_.find(old.value());
        if (flight != pending_.end())
            flight->waiters.remove(jid);
        waitingFor_.erase(old);
    }

    Decision join(const QString &jid, const QByteArray &hash)
    {
        if (hash.size() != 20)
            return {};
        supersede(jid, hash);
        auto flight = pending_.find(hash);
        if (flight != pending_.end()) {
            flight->waiters.insert(jid);
            waitingFor_.insert(jid, hash);
            return { false, flight->token };
        }
        Flight created;
        created.token = ++nextToken_;
        created.waiters.insert(jid);
        pending_.insert(hash, created);
        waitingFor_.insert(jid, hash);
        return { true, created.token };
    }

    bool hasFlight(const QByteArray &hash) const { return pending_.contains(hash); }

    QStringList waiters(const QByteArray &hash) const
    {
        const auto it = pending_.constFind(hash);
        return it == pending_.cend() ? QStringList() : it->waiters.values();
    }

    // A successful download has already populated FileCache. The caller
    // binds these interested JIDs to the cached bytes, without further IQs.
    QStringList complete(const QByteArray &hash)
    {
        const auto it = pending_.constFind(hash);
        if (it == pending_.cend())
            return {};
        const QStringList result = it->waiters.values();
        finish(hash, it->token);
        return result;
    }

    // An obsolete task's completion must not release a newer request.
    void finish(const QByteArray &hash, quint64 token)
    {
        auto it = pending_.find(hash);
        if (it == pending_.end() || it->token != token)
            return;
        for (const auto &jid : std::as_const(it->waiters)) {
            auto waiting = waitingFor_.find(jid);
            if (waiting != waitingFor_.end() && waiting.value() == hash)
                waitingFor_.erase(waiting);
        }
        pending_.erase(it);
    }

    int count() const { return pending_.size(); }

private:
    struct Flight {
        quint64 token = 0;
        QSet<QString> waiters;
    };
    quint64 nextToken_ = 0;
    QHash<QByteArray, Flight> pending_;
    QHash<QString, QByteArray> waitingFor_;
};
