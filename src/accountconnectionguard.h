// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef ACCOUNTCONNECTIONGUARD_H
#define ACCOUNTCONNECTIONGUARD_H

#include <QPointer>

// The receiver owns the generation counter. Use its QObject as the connection
// context so the guard cannot outlive that counter. Disconnecting a signal alone
// does not cancel deliveries already queued for a previous connection attempt.
template <typename Source> class AccountConnectionGuard {
public:
    AccountConnectionGuard(Source *source, const quint64 &generation) :
        source_(source), currentGeneration_(generation), generation_(generation)
    {
    }

    bool isCurrent() const { return source_ && currentGeneration_ == generation_; }

    template <typename Receiver, typename... Args>
    auto callback(Receiver *receiver, void (Receiver::*slot)(Args...)) const
    {
        return [guard = *this, receiver, slot](Args... args) {
            if (guard.isCurrent())
                (receiver->*slot)(args...);
        };
    }

private:
    QPointer<Source> source_;
    const quint64   &currentGeneration_;
    quint64          generation_;
};

#endif
