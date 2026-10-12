// SPDX-License-Identifier: GPL-2.0-or-later
// Repeats the ring tone of an incoming call or whiteboard invitation, or the ringback of an outgoing one.
#ifndef CALLRINGER_H
#define CALLRINGER_H

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>

class PsiAccount;

class CallRinger : public QObject {
    Q_OBJECT
public:
    enum Kind { Incoming, Outgoing };

    // Rings until destroyed or stopped, at most for a minute (incoming) or 90 seconds (outgoing). With
    // playNow false the first sound comes after one interval, e.g. when taking over from another ringer.
    CallRinger(PsiAccount *account, Kind kind, bool playNow = true, QObject *parent = nullptr);

    void stop();
    bool isRinging() const;

private:
    void ring();

    QPointer<PsiAccount> account_;
    Kind                 kind_;
    QTimer               timer_;
    QElapsedTimer        elapsed_;
};

#endif // CALLRINGER_H
