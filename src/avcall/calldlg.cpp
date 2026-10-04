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
#include "../psimedia/psimedia.h"
#include "avcall.h"
#include "callview.h"
#include "iconset.h"
#include "mediadevicewatcher.h"
#include "psiaccount.h"
#include "psioptions.h"
#include "screensharecapture.h"
#include "xmpp_caps.h"
#include "xmpp_client.h"
#include <QElapsedTimer>
#include <QIcon>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>

class CallDlg::Private : public QObject {
public:
    explicit Private(CallDlg *dialog) : QObject(dialog), q(dialog), capture(this)
    {
        q->setWindowTitle(tr("Call"));
        q->setWindowIcon(IconsetFactory::icon("psi/avcall").icon());
        q->resize(860, 640);
        auto layout = new QVBoxLayout(q);
        layout->setContentsMargins(0, 0, 0, 0);
        view = new CallView(q);
        layout->addWidget(view);
        view->setVideoSupported(AvCallManager::isVideoSupported());
        if (AvCallManager::isSupported()) {
            const auto config = MediaDeviceWatcher::instance()->configuration();
            AvCallManager::setAudioOutDevice(config.audioOutDeviceId);
            AvCallManager::setAudioInDevice(config.audioInDeviceId);
            AvCallManager::setVideoInDevice(config.videoInDeviceId);
            AvCallManager::setBasePort(
                PsiOptions::instance()->getOption("options.p2p.bytestreams.listen-port").toInt());
            AvCallManager::setExternalAddress(
                PsiOptions::instance()->getOption("options.p2p.bytestreams.external-address").toString());
        }
        connect(view, &CallView::accepted, this, [this] { accept(); });
        connect(view, &CallView::ended, q, &QDialog::close);
        connect(view, &CallView::microphoneChanged, this, [this](bool enabled) {
            if (session)
                session->setMicrophoneEnabled(enabled);
        });
        connect(view, &CallView::cameraChanged, this, [this](bool enabled) {
            if (session)
                session->setCameraEnabled(enabled);
        });
        connect(view, &CallView::shareRequested, this, [this] {
            if (!session || !active)
                return;
            if (capture.selecting() || session->screenSharing())
                capture.stop();
            else
                capture.select(q);
        });
        connect(&capture, &ScreenShareCapture::ready, this, [this](const QString &source, const QString &label) {
            if (!session || !active || !session->startScreenSharing(source, capture.sourceLease())) {
                capture.stop();
                return;
            }
            shareLabel = label;
            view->setSharing(true, label, true);
        });
        connect(&capture, &ScreenShareCapture::stopped, this, [this] {
            if (session)
                session->stopScreenSharing();
            view->setSharing(false);
        });
        connect(&capture, &ScreenShareCapture::selectingChanged, view, &CallView::setShareSelecting);
        connect(&capture, &ScreenShareCapture::failed, view, &CallView::setStatus);
        connect(&timer, &QTimer::timeout, this, [this] { view->setDuration(elapsed.elapsed() / 1000); });
    }

    ~Private() override
    {
        capture.stop();
        if (session) {
            session->disconnect(this);
            if (!finished)
                session->reject();
            session->setIncomingVideo(nullptr);
            session->setIncomingPresentation(nullptr);
            session->unlink();
            session->deleteLater();
        }
    }

    void bind(AvCall *call)
    {
        session = call;
        connect(call, &AvCall::activated, this, [this] {
            active = true;
            view->setConnecting(false);
            view->setActive(true);
            view->setSharingSupported(session->screenSharingSupported());
            view->setStatus(tr("Connected"));
            session->setIncomingVideo(view->remoteVideo());
            session->setIncomingPresentation(view->remotePresentation());
            elapsed.start();
            timer.start(1000);
        });
        connect(call, &AvCall::error, this, [this] {
            finished = true;
            active   = false;
            capture.stop();
            timer.stop();
            view->setConnecting(false);
            view->setFinished();
            view->setStatus(session ? session->errorString() : tr("Call ended"));
        });
        connect(call, &AvCall::cancelled, this, [this] {
            finished = true;
            active   = false;
            capture.stop();
            timer.stop();
            view->setFinished();
            view->setStatus(tr("Call cancelled"));
        });
        connect(call, &AvCall::screenSharingChanged, this, [this](bool sharing) {
            if (!sharing) {
                capture.stop();
                view->setSharing(false);
            }
        });
        connect(call, &AvCall::mediaControlError, view, &CallView::setStatus);
        connect(call, &AvCall::incomingVideoRemoved, view, &CallView::hideRemoteVideo);
        connect(call, &AvCall::cameraEnabledChanged, view, &CallView::setCameraEnabled);
        connect(call, &AvCall::screenSharingStarted, this, [this] { view->setSharing(true, shareLabel); });
    }

    void accept()
    {
        if (finished || active)
            return;
        view->setConnecting(true);
        if (!incoming) {
            view->setStatus(tr("Calling…"));
            bind(account->avCallManager()->createOutgoing());
            session->setMicrophoneEnabled(view->microphoneEnabled());
            session->setCameraEnabled(view->cameraEnabled());
            const auto           caps = account->client()->capsManager()->features(view->peer());
            AvCall::PeerFeatures features;
            if (caps.hasJingleIce())
                features |= AvCall::IceTransport;
            if (caps.hasJingleIceUdp())
                features |= AvCall::IceUdpTransport;
            session->connectToJid(view->peer(), view->cameraEnabled() ? AvCall::Both : AvCall::Audio, view->bitrate(),
                                  features);
        } else if (session) {
            view->setStatus(tr("Connecting…"));
            session->setMicrophoneEnabled(view->microphoneEnabled());
            session->setCameraEnabled(view->cameraEnabled());
            session->accept(session->mode(), view->bitrate());
        }
    }

    CallDlg           *q;
    PsiAccount        *account = nullptr;
    CallView          *view    = nullptr;
    QPointer<AvCall>   session;
    ScreenShareCapture capture;
    QTimer             timer;
    QElapsedTimer      elapsed;
    bool               incoming = false;
    bool               active   = false;
    bool               finished = false;
    QString            shareLabel;
};

CallDlg::CallDlg(PsiAccount *account, QWidget *parent) : QDialog(parent), d(new Private(this))
{
    d->account = account;
    account->dialogRegister(this);
}
CallDlg::~CallDlg()
{
    d->account->dialogUnregister(this);
    delete d;
}
void CallDlg::setOutgoing(const XMPP::Jid &jid)
{
    d->view->setPeer(jid.full(), true);
    d->view->setIncoming(false);
}
void CallDlg::setIncoming(AvCall *session)
{
    d->incoming = true;
    d->bind(session);
    d->view->setPeer(session->jid().full(), false);
    d->view->setIncoming(true);
    d->view->setStatus(tr("Incoming call"));
}
