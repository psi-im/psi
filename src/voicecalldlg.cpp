/*
 * voicecalldlg.cpp
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <QLabel>
#include <QPushButton>

#include "voicecalldlg.h"
#include "voicecaller.h"

VoiceCallDlg::VoiceCallDlg(const Jid& jid, VoiceCaller* voiceCaller)
    : QDialog(0), jid_(jid), voiceCaller_(voiceCaller)
{
    setAttribute(Qt::WA_DeleteOnClose);
    ui_.setupUi(this);
    setModal(false);

    setWindowTitle(QString(tr("Voice Call (%1)")).arg(jid.full()));

    // Voice Caller signals
    connect(voiceCaller_,SIGNAL(accepted(const Jid&)),SLOT(accepted(const Jid&)));
    connect(voiceCaller_,SIGNAL(rejected(const Jid&)),SLOT(rejected(const Jid&)));
    connect(voiceCaller_,SIGNAL(in_progress(const Jid&)),SLOT(in_progress(const Jid&)));
    connect(voiceCaller_,SIGNAL(terminated(const Jid&)),SLOT(terminated(const Jid&)));

    // Buttons
    ui_.pb_hangup->setEnabled(false);
    ui_.pb_accept->setEnabled(false);
    ui_.pb_reject->setEnabled(false);

    connect(ui_.pb_hangup,SIGNAL(clicked()),SLOT(terminate_call()));
    connect(ui_.pb_accept,SIGNAL(clicked()),SLOT(accept_call()));
    connect(ui_.pb_reject,SIGNAL(clicked()),SLOT(reject_call()));

}

void VoiceCallDlg::call()
{
    setStatus(CallStatus::Calling);
    voiceCaller_->call(jid_);
}

void VoiceCallDlg::accept_call()
{
    setStatus(CallStatus::Accepting);
    voiceCaller_->accept(jid_);
}

void VoiceCallDlg::reject_call()
{
    setStatus(CallStatus::Rejecting);
    voiceCaller_->reject(jid_);
    finalize();
    close();
}

void VoiceCallDlg::terminate_call()
{
    setStatus(CallStatus::Terminating);
    voiceCaller_->terminate(jid_);
    finalize();
    close();
}

void VoiceCallDlg::accepted(const Jid& j)
{
    if (jid_.compare(j)) {
        setStatus(CallStatus::Accepted);
    }
}

void VoiceCallDlg::rejected(const Jid& j)
{
    if (jid_.compare(j)) {
        setStatus(CallStatus::Rejected);
        finalize();
    }
}

void VoiceCallDlg::in_progress(const Jid& j)
{
    if (jid_.compare(j)) {
        setStatus(CallStatus::InProgress);
    }
}

void VoiceCallDlg::terminated(const Jid& j)
{
    if (jid_.compare(j)) {
        setStatus(CallStatus::Terminated);
        finalize();
    }
}


void VoiceCallDlg::incoming()
{
    setStatus(CallStatus::Incoming);
}

void VoiceCallDlg::setStatus(CallStatus s)
{
    status_ = s;
    switch (s) {
        case CallStatus::Calling:
            ui_.lb_status->setText(tr("Calling"));
            ui_.pb_accept->setEnabled(false);
            ui_.pb_reject->setEnabled(false);
            ui_.pb_hangup->setEnabled(true);
            break;

        case CallStatus::Accepting:
            ui_.lb_status->setText(tr("Accepting"));
            ui_.pb_accept->setEnabled(false);
            ui_.pb_reject->setEnabled(false);
            ui_.pb_hangup->setEnabled(true);
            break;

        case CallStatus::Rejecting:
            ui_.lb_status->setText(tr("Rejecting"));
            ui_.pb_accept->setEnabled(false);
            ui_.pb_reject->setEnabled(false);
            ui_.pb_hangup->setEnabled(false);
            break;

        case CallStatus::Terminating:
            ui_.lb_status->setText(tr("Hanging up"));
            ui_.pb_accept->setEnabled(false);
            ui_.pb_reject->setEnabled(false);
            ui_.pb_hangup->setEnabled(false);
            break;

        case CallStatus::Accepted:
            ui_.lb_status->setText(tr("Accepted"));
            ui_.pb_accept->setEnabled(false);
            ui_.pb_reject->setEnabled(false);
            ui_.pb_hangup->setEnabled(true);
            break;

        case CallStatus::Rejected:
            ui_.lb_status->setText(tr("Rejected"));
            ui_.pb_accept->setEnabled(false);
            ui_.pb_reject->setEnabled(false);
            ui_.pb_hangup->setEnabled(false);
            break;

        case CallStatus::InProgress:
            ui_.lb_status->setText(tr("In progress"));
            ui_.pb_accept->setEnabled(false);
            ui_.pb_reject->setEnabled(false);
            ui_.pb_hangup->setEnabled(true);
            break;

        case CallStatus::Terminated:
            ui_.lb_status->setText(tr("Terminated"));
            ui_.pb_accept->setEnabled(false);
            ui_.pb_reject->setEnabled(false);
            ui_.pb_hangup->setEnabled(false);
            break;

        case CallStatus::Incoming:
            ui_.lb_status->setText(tr("Incoming Call"));
            ui_.pb_accept->setEnabled(true);
            ui_.pb_reject->setEnabled(true);
            ui_.pb_hangup->setEnabled(false);
            break;

        default:
            break;
    }
}

void VoiceCallDlg::reject()
{
    finalize();
    QDialog::reject();
}

void VoiceCallDlg::finalize()
{
    // Close connection
    if (status_ == CallStatus::Incoming) {
        reject_call();
    }
    else if (status_ == CallStatus::InProgress ||
             status_ == CallStatus::Calling ||
             status_ == CallStatus::Accepting ||
             status_ == CallStatus::Accepted) {
        terminate_call();
    }

    // Disconnect signals
    disconnect(voiceCaller_,SIGNAL(accepted(const Jid&)),this,SLOT(accepted(const Jid&)));
    disconnect(voiceCaller_,SIGNAL(rejected(const Jid&)),this,SLOT(rejected(const Jid&)));
    disconnect(voiceCaller_,SIGNAL(in_progress(const Jid&)),this,SLOT(in_progress(const Jid&)));
    disconnect(voiceCaller_,SIGNAL(terminated(const Jid&)),this,SLOT(terminated(const Jid&)));
}
