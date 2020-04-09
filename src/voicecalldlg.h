/*
 * voicecalldlg.h
 * Copyright (C) 2006  Remko Troncon
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef VOICECALLDLG_H
#define VOICECALLDLG_H

#include "ui_voicecall.h"
#include "xmpp_jid.h"

#include <QDialog>

class VoiceCaller;

using namespace XMPP;

class VoiceCallDlg : public QDialog {
    Q_OBJECT

public:
    enum class CallStatus : char {
        Default,
        Calling,
        Accepting,
        Rejecting,
        Terminating,
        Accepted,
        Rejected,
        InProgress,
        Terminated,
        Incoming
    };

    VoiceCallDlg(const Jid &, VoiceCaller *);

public slots:
    void incoming();
    void call();

    void terminate_call();
    void accept_call();
    void reject_call();

    void accepted(const Jid &);
    void rejected(const Jid &);
    void in_progress(const Jid &);
    void terminated(const Jid &);

protected slots:
    void reject();

protected:
    void finalize();
    void setStatus(CallStatus);

private:
    Jid           jid_;
    CallStatus    status_;
    VoiceCaller * voiceCaller_;
    Ui::VoiceCall ui_;
};

#endif // VOICECALLDLG_H
