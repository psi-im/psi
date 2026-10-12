// SPDX-License-Identifier: GPL-2.0-or-later
#include "callringer.h"

#include "psiaccount.h"

namespace {
constexpr int RingInterval        = 3500;
constexpr int IncomingRingTimeout = 60000;
constexpr int OutgoingRingTimeout = 90000;
}

CallRinger::CallRinger(PsiAccount *account, Kind kind, bool playNow, QObject *parent) :
    QObject(parent), account_(account), kind_(kind)
{
    timer_.setInterval(RingInterval);
    connect(&timer_, &QTimer::timeout, this, &CallRinger::ring);
    elapsed_.start();
    timer_.start();
    if (playNow)
        ring();
}

void CallRinger::stop() { timer_.stop(); }

bool CallRinger::isRinging() const { return timer_.isActive(); }

void CallRinger::ring()
{
    if (!account_ || elapsed_.elapsed() > (kind_ == Incoming ? IncomingRingTimeout : OutgoingRingTimeout)) {
        stop();
        return;
    }
    // The account checks the sound switch, Do Not Disturb and "silent while away" on every ring.
    account_->playSound(kind_ == Incoming ? PsiAccount::eIncomingCall : PsiAccount::eOutgoingCall);
}
