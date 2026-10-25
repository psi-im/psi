/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>

#include <utility>

// Only tracks unfinished XEP-0084 blob IQs. Data is still cached globally in
// the existing FileCache by SHA-1, and JID bindings remain account-independent.
// An account here is merely an available *transport* for a pending IQ.
class AvatarBlobInFlight {
public:
    struct Decision {
        bool request = false;
        quint64 token = 0;
    };
    struct Retry {
        bool request = false;
        quint64 token = 0;
        QString jid;
        QString itemId;
        QPointer<QObject> transport;
    };

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

    Decision join(const QString &jid, const QByteArray &hash, QObject *transport = nullptr,
                  const QString &itemId = {})
    {
        if (hash.size() != 20)
            return {};
        supersede(jid, hash);
        auto flight = pending_.find(hash);
        if (flight != pending_.end()) {
            flight->waiters.insert(jid);
            waitingFor_.insert(jid, hash);
            addTransport(*flight, jid, transport, itemId);
            return { false, flight->token };
        }
        Flight created;
        created.token = ++nextToken_;
        created.waiters.insert(jid);
        addTransport(created, jid, transport, itemId);
        created.nextSource = created.sources.size(); // first source already started
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

    QStringList complete(const QByteArray &hash)
    {
        const auto it = pending_.constFind(hash);
        if (it == pending_.cend())
            return {};
        const QStringList result = it->waiters.values();
        finish(hash, it->token);
        return result;
    }

    // A failed IQ might be due to per-contact authorization. Sequentially
    // try another *already waiting* contact/account before abandoning the
    // shared download. Never create duplicate concurrent blob transfers.
    Retry fail(const QByteArray &hash, quint64 token)
    {
        auto it = pending_.find(hash);
        if (it == pending_.end() || it->token != token)
            return {};
        while (it->nextSource < it->sources.size()) {
            const Source source = it->sources.at(it->nextSource++);
            if (source.transport.isNull() || !it->waiters.contains(source.jid))
                continue;
            it->token = ++nextToken_;
            return { true, it->token, source.jid, source.itemId, source.transport };
        }
        finish(hash, token);
        return {};
    }

    // Successful or canceled requests release only the matching generation.
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
    struct Source {
        QString jid;
        QString itemId;
        QPointer<QObject> transport;
    };
    struct Flight {
        quint64 token = 0;
        QSet<QString> waiters;
        QList<Source> sources;
        int nextSource = 0;
    };
    static void addTransport(Flight &flight, const QString &jid, QObject *transport, const QString &itemId)
    {
        if (!transport)
            return;
        for (const Source &source : std::as_const(flight.sources))
            if (source.jid == jid && source.transport.data() == transport)
                return;
        flight.sources.append({ jid, itemId, QPointer<QObject>(transport) });
    }

    quint64 nextToken_ = 0;
    QHash<QByteArray, Flight> pending_;
    QHash<QString, QByteArray> waitingFor_;
};
