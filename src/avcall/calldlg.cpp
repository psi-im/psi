/*
 * Copyright (C) 2009  Barracuda Networks, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
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

#include "calldlg.h"

#include "../avcall/mediadevicewatcher.h"
#include "../psimedia/psimedia.h"
#include "avcall.h"
#include "common.h"
#include "iconset.h"
#include "psiaccount.h"
#include "psioptions.h"
#include "ui_call.h"
#include "xmpp_caps.h"
#include "xmpp_client.h"

#include <QMessageBox>
#include <QTime>
#include <QTimer>

class CallDlg::Private : public QObject {
    Q_OBJECT

public:
    CallDlg *              q;
    Ui::Call               ui;
    PsiAccount *           pa;
    bool                   incoming;
    bool                   active;
    bool                   activated;
    AvCall *               sess;
    PsiMedia::VideoWidget *vw_remote;
    QTimer *               timer;
    QTime                  call_duration;

    explicit Private(CallDlg *_q) : QObject(_q), q(_q), active(false), activated(false), sess(nullptr), timer(nullptr)
    {
        ui.setupUi(q);
        q->setWindowTitle(tr("Voice Call"));
        q->setWindowIcon(IconsetFactory::icon("psi/avcall").icon());

        if (AvCallManager::isSupported()) {
            auto config = MediaDeviceWatcher::instance()->configuration();
            AvCallManager::setAudioOutDevice(config.audioOutDeviceId);
            AvCallManager::setAudioInDevice(config.audioInDeviceId);
            AvCallManager::setVideoInDevice(config.videoInDeviceId);
            AvCallManager::setBasePort(
                PsiOptions::instance()->getOption("options.p2p.bytestreams.listen-port").toInt());
            AvCallManager::setExternalAddress(
                PsiOptions::instance()->getOption("options.p2p.bytestreams.external-address").toString());
        }

        ui.lb_bandwidth->setEnabled(false);
        ui.cb_bandwidth->setEnabled(false);
        connect(ui.ck_useVideo, SIGNAL(toggled(bool)), ui.lb_bandwidth, SLOT(setEnabled(bool)));
        connect(ui.ck_useVideo, SIGNAL(toggled(bool)), ui.cb_bandwidth, SLOT(setEnabled(bool)));

        ui.cb_bandwidth->addItem(tr("High (1Mbps)"), 1000);
        ui.cb_bandwidth->addItem(tr("Average (400Kbps)"), 400);
        ui.cb_bandwidth->addItem(tr("Low (160Kbps)"), 160);
        ui.cb_bandwidth->setCurrentIndex(1);

        connect(ui.pb_accept, SIGNAL(clicked()), SLOT(ok_clicked()));
        connect(ui.pb_reject, SIGNAL(clicked()), SLOT(cancel_clicked()));

        ui.pb_accept->setDefault(true);
        ui.pb_accept->setFocus();

        timer = new QTimer(q);
        connect(timer, SIGNAL(timeout()), SLOT(update_call_duration()));

        q->resize(q->minimumSizeHint());
    }

    ~Private() override
    {
        if (sess) {
            if (active)
                sess->reject();

            sess->setIncomingVideo(nullptr);
            sess->disconnect(this);
            sess->unlink();
            sess->deleteLater();
        }
    }

    void setOutgoing(const XMPP::Jid &jid)
    {
        incoming = false;

        ui.le_to->setText(jid.full());

        ui.pb_reject->setText(tr("&Close"));
        ui.pb_accept->setText(tr("C&all"));
        ui.lb_status->setText(tr("Ready"));
    }

    void setIncoming(AvCall *_sess)
    {
        incoming = true;
        sess     = _sess;
        connect(sess, SIGNAL(activated()), SLOT(sess_activated()));
        connect(sess, SIGNAL(error()), SLOT(sess_error()));

        ui.lb_to->setText(tr("From:"));
        ui.le_to->setText(sess->jid().full());
        ui.le_to->setReadOnly(true);

        if (sess->mode() == AvCall::Video || sess->mode() == AvCall::Both) {
            ui.ck_useVideo->setChecked(true);

            // video-only session, don't allow deselecting video
            if (sess->mode() == AvCall::Video)
                ui.ck_useVideo->setEnabled(false);
        }

        ui.lb_status->setText(tr("Accept call?"));
    }

private slots:
    void ok_clicked()
    {
        AvCall::Mode mode = AvCall::Audio;
        int          kbps = -1;
        if (ui.ck_useVideo->isChecked()) {
            mode = AvCall::Both;
            kbps = ui.cb_bandwidth->itemData(ui.cb_bandwidth->currentIndex()).toInt();
        }

        if (!incoming) {
            ui.le_to->setReadOnly(true);
            ui.le_to->setEnabled(false);
            ui.ck_useVideo->setEnabled(false);
            ui.cb_bandwidth->setEnabled(false);

            ui.pb_accept->setEnabled(false);
            ui.pb_reject->setText(tr("&Cancel"));
            ui.pb_reject->setFocus();
            ui.busy->start();
            ui.lb_status->setText(tr("Calling..."));

            sess = pa->avCallManager()->createOutgoing();
            connect(sess, SIGNAL(activated()), SLOT(sess_activated()));
            connect(sess, SIGNAL(error()), SLOT(sess_error()));

            active                    = true;
            auto                 caps = pa->client()->capsManager()->features(ui.le_to->text());
            AvCall::PeerFeatures features;
            if (caps.hasJingleIce())
                features |= AvCall::IceTransport;
            if (caps.hasJingleIceUdp())
                features |= AvCall::IceUdpTransport;
            sess->connectToJid(ui.le_to->text(), mode, kbps, features);
        } else {
            ui.le_to->setEnabled(false);
            ui.ck_useVideo->setEnabled(false);
            ui.cb_bandwidth->setEnabled(false);

            ui.pb_accept->setEnabled(false);
            ui.pb_reject->setText(tr("&Cancel"));
            ui.pb_reject->setFocus();
            ui.busy->start();
            ui.lb_status->setText(tr("Accepting..."));

            active = true;
            sess->accept(mode, kbps);
        }
    }

    void cancel_clicked()
    {
        if (sess && incoming && !active)
            sess->reject();
        q->close();
    }

    void sess_activated()
    {
        ui.le_to->setEnabled(true);
        ui.lb_bandwidth->hide();
        ui.cb_bandwidth->hide();

        if (sess->mode() == AvCall::Video || sess->mode() == AvCall::Both) {
            vw_remote = new PsiMedia::VideoWidget(q);
            replaceWidget(ui.ck_useVideo, vw_remote);
            sess->setIncomingVideo(vw_remote);
            vw_remote->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            vw_remote->setMinimumSize(320, 240);
            ui.fake_spacer->hide();
        } else
            ui.ck_useVideo->hide();

        ui.busy->stop();
        ui.busy->hide();
        ui.pb_accept->hide();
        ui.pb_reject->setText(tr("&Hang up"));
        ui.lb_status->setText(tr("Call active"));

        call_duration = QTime(0, 0, 0, 0);
        timer->start(1000);
        activated = true;
    }

    void sess_error()
    {
        if (!activated)
            ui.busy->stop();

        if (timer->isActive())
            timer->stop();

        QMessageBox::information(q, tr("Call is ended"), sess->errorString());
        q->close();
    }

    void update_call_duration()
    {
        call_duration = call_duration.addSecs(1);
        ui.lb_status->setText(tr("Call duration: %1").arg(call_duration.toString("mm:ss")));
    }
};

CallDlg::CallDlg(PsiAccount *pa, QWidget *parent) : QDialog(parent)
{
    d     = new Private(this);
    d->pa = pa;
    d->pa->dialogRegister(this);
}

CallDlg::~CallDlg()
{
    d->pa->dialogUnregister(this);
    delete d;
}

void CallDlg::setOutgoing(const XMPP::Jid &jid) { d->setOutgoing(jid); }

void CallDlg::setIncoming(AvCall *sess) { d->setIncoming(sess); }

#include "calldlg.moc"
