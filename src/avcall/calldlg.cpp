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
#include "callstage.h"
#include "callview.h"
#include "iconset.h"
#include "mediadevicewatcher.h"
#include "psiaccount.h"
#include "psioptions.h"
#include "screensharecapture.h"
#include "userlist.h"
#include "../whiteboarding/wbmanager.h"
#include "../whiteboarding/wbview.h"
#include "xmpp_caps.h"
#include "xmpp_client.h"
#include <iris/jingle-session.h>
#include <iris/xmpp-im/jingle-sxe.h>
#include <QElapsedTimer>
#include <QIcon>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>

namespace Jingle = XMPP::Jingle;
namespace SXE    = XMPP::Jingle::SXE;

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
        connect(view, &CallView::whiteboardChanged, this, &Private::toggleWhiteboard);
        connect(view->stage(), &CallStage::mainTileChanged, this, [this](QWidget *tile) {
            if (wbView)
                wbView->setPreview(tile != wbView);
        });
        connect(&capture, &ScreenShareCapture::selectingChanged, view, &CallView::setShareSelecting);
        connect(&capture, &ScreenShareCapture::failed, view, &CallView::setStatus);
        connect(&timer, &QTimer::timeout, this, [this] { view->setDuration(elapsed.elapsed() / 1000); });
    }

    ~Private() override
    {
        if (whiteboard)
            whiteboard->disconnect(this);
        if (wbSession) {
            wbSession->disconnect(this);
            if (wbSession->state() < Jingle::State::Finishing)
                wbSession->terminate(active || !incoming ? Jingle::Reason::Success : Jingle::Reason::Decline);
        }
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
            if (auto jingle = session->jingleSession()) {
                watchSession(jingle);
                updateWhiteboardSupport();
                if (whiteboardWanted && !whiteboard)
                    addWhiteboard(jingle);
            }
        });
        connect(call, &AvCall::error, this, [this] {
            finished = true;
            active   = false;
            capture.stop();
            timer.stop();
            view->setConnecting(false);
            view->setFinished();
            view->setStatus(session ? session->errorString() : tr("Call ended"));
            updateWhiteboard();
        });
        connect(call, &AvCall::cancelled, this, [this] {
            finished = true;
            active   = false;
            capture.stop();
            timer.stop();
            view->setFinished();
            view->setStatus(tr("Call cancelled"));
            updateWhiteboard();
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
        joined = true;
        if (view->whiteboardOnly()) {
            acceptWhiteboard();
            return;
        }
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

    // The resource to invite to a whiteboard: the given one, or the capable resource with the highest priority.
    XMPP::Jid whiteboardTarget(const XMPP::Jid &jid) const
    {
        auto wb = account->wbManager();
        if (!wb || !jid.isValid())
            return {};
        if (!jid.resource().isEmpty())
            return wb->checkSupport(jid) ? jid : XMPP::Jid();
        const UserListItem *best = account->find(jid);
        if (!best)
            return {};
        const UserResource *bestResource = nullptr;
        for (const auto &resource : best->userResourceList()) {
            if (wb->checkSupport(jid.withResource(resource.name()))
                && (!bestResource || resource.priority() > bestResource->priority()))
                bestResource = &resource;
        }
        return bestResource ? jid.withResource(bestResource->name()) : XMPP::Jid();
    }

    void updateWhiteboardSupport()
    {
        Jingle::Session *jingle = jingleSession();
        const bool       supported
            = jingle ? account->wbManager() && account->wbManager()->checkSupport(jingle->peer())
                     : whiteboardTarget(XMPP::Jid(view->peer())).isValid();
        // In a whiteboard-only session the whiteboard is the session; it cannot be chosen away before it starts.
        view->setWhiteboardSupported(supported && (!view->whiteboardOnly() || active));
    }

    Jingle::Session *jingleSession() const { return session ? session->jingleSession() : wbSession.data(); }

    void acceptWhiteboard()
    {
        if (finished || active)
            return;
        if (incoming) {
            if (!wbSession || !whiteboard || wbSession->state() >= Jingle::State::Finishing)
                return;
            joined = true;
            view->setConnecting(true);
            view->setStatus(tr("Connecting…"));
            wbSession->accept();
            updateWhiteboard();
            return;
        }

        const auto target = whiteboardTarget(XMPP::Jid(view->peer()));
        if (!target.isValid()) {
            view->setStatus(tr("The contact does not support whiteboards."));
            return;
        }
        auto jingle = account->client()->jingleManager()->newSession(target);
        auto app    = qobject_cast<SXE::Application *>(jingle->newContent(SXE::NS_SVG_WHITEBOARD));
        if (!app) {
            delete jingle;
            view->setStatus(tr("Unable to create a whiteboard."));
            return;
        }
        view->setConnecting(true);
        view->setStatus(tr("Inviting…"));
        jingle->addContent(app);
        bindWhiteboardSession(jingle);
        bindWhiteboard(app);
        jingle->initiate();
    }

    // A session that carries only a whiteboard: its lifetime is the dialog's call state.
    void bindWhiteboardSession(Jingle::Session *jingle)
    {
        wbSession = jingle;
        connect(jingle, &Jingle::Session::activated, this, [this] {
            active = true;
            view->setConnecting(false);
            view->setActive(true);
            view->setStatus(tr("Connected"));
            elapsed.start();
            timer.start(1000);
            updateWhiteboardSupport();
            updateWhiteboard();
        });
        connect(jingle, &Jingle::Session::terminated, this, [this] {
            const bool wasActive = active;
            finished             = true;
            active               = false;
            timer.stop();
            view->setConnecting(false);
            view->setFinished();
            view->setStatus(wasActive ? tr("Session ended") : tr("The invitation was declined or failed"));
            updateWhiteboard();
        });
        watchSession(jingle);
    }

    // Contents added by the peer: a whiteboard is accepted like added media. A session that started
    // as a whiteboard cannot carry media yet.
    void watchSession(Jingle::Session *jingle)
    {
        // Qt::UniqueConnection does not work with lambdas: remember the session instead.
        if (watchedSession == jingle)
            return;
        watchedSession = jingle;
        connect(
            jingle, &Jingle::Session::newContentReceived, this,
            [this, jingle] {
                const auto contents = jingle->contentList().values();
                for (auto content : contents) {
                    if (!content->isRemote() || content->state() != Jingle::State::Created)
                        continue;
                    auto app = qobject_cast<SXE::Application *>(content);
                    if (app && app != whiteboard) {
                        app->prepare();
                        bindWhiteboard(app);
                    } else if (!app && jingle == wbSession) {
                        content->remove(Jingle::Reason::UnsupportedApplications);
                    }
                }
            });
    }

    void addWhiteboard(Jingle::Session *jingle)
    {
        auto app = qobject_cast<SXE::Application *>(jingle->newContent(SXE::NS_SVG_WHITEBOARD));
        if (!app) {
            view->setWhiteboardEnabled(false);
            view->setStatus(tr("Unable to add a whiteboard to this call."));
            return;
        }
        jingle->addContent(app);
        bindWhiteboard(app);
    }

    void bindWhiteboard(SXE::Application *app)
    {
        if (wbView) {
            view->stage()->removeTile(wbView);
            wbView = nullptr;
            delete wbDocument;
        }
        whiteboard = app;
        // The dialog keeps the document, so the drawing can be saved after the session. It is a child
        // created after the view, so the dialog deletes it after the whiteboard tile.
        wbDocument = app->document();
        wbDocument->setParent(q);
        wbView = new WbView(wbDocument);
        view->stage()->addTile(wbView, tr("Whiteboard"), CallStage::WhiteboardPriority);
        connect(app, &SXE::Application::syncStateChanged, this, &Private::updateWhiteboard);
        connect(app, &Jingle::Application::sendersChanged, this, &Private::updateWhiteboard);
        connect(app, &Jingle::Application::stateChanged, this, &Private::updateWhiteboard);
        connect(app, &QObject::destroyed, this, &Private::updateWhiteboard, Qt::QueuedConnection);
        updateWhiteboard();
    }

    void updateWhiteboard()
    {
        if (!wbView)
            return;
        const bool live = whiteboard && whiteboard->state() < Jingle::State::Finishing && !finished;
        const bool on   = live && whiteboard->senders() != Jingle::Origin::None;
        // Senders 'none' hides the board for both participants; after the session it stays for saving.
        const bool shown = live ? on && (joined || whiteboard->isHost()) : finished;
        view->stage()->setTileVisible(wbView, shown);
        const bool synchronized = live
            && (whiteboard->isHost() || whiteboard->syncState() == SXE::Application::SyncState::Synchronized);
        wbView->setEditable(synchronized && whiteboard->mayEdit(whiteboard->pad()->session()->role()));
        view->setWhiteboardEnabled(on);
    }

    void toggleWhiteboard(bool enabled)
    {
        if (finished)
            return;
        if (!active) {
            whiteboardWanted = enabled;
            return;
        }
        if (!whiteboard || whiteboard->state() >= Jingle::State::Finishing) {
            if (enabled && jingleSession())
                addWhiteboard(jingleSession());
            return;
        }
        if (!whiteboard->requestSenders(enabled ? Jingle::Origin::Both : Jingle::Origin::None))
            updateWhiteboard();
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

    QPointer<SXE::Application> whiteboard;
    QPointer<SXE::Document>    wbDocument;
    QPointer<Jingle::Session>  wbSession;
    QPointer<Jingle::Session>  watchedSession;
    WbView                    *wbView           = nullptr;
    bool                       whiteboardWanted = false;
    bool                       joined           = false;
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
    d->updateWhiteboardSupport();
}
void CallDlg::setOutgoingWhiteboard(const XMPP::Jid &jid)
{
    setWindowTitle(tr("Whiteboard"));
    setWindowIcon(IconsetFactory::icon("psi/whiteboard").icon());
    d->view->setWhiteboardOnly(true);
    d->view->setWhiteboardEnabled(true);
    d->view->setPeer(jid.full(), true);
    d->view->setIncoming(false);
    d->updateWhiteboardSupport();
}
void CallDlg::setIncoming(AvCall *session)
{
    d->incoming = true;
    d->bind(session);
    d->view->setPeer(session->jid().full(), false);
    d->view->setIncoming(true);
    d->view->setStatus(tr("Incoming call"));
    d->updateWhiteboardSupport();
}
void CallDlg::setIncomingWhiteboard(XMPP::Jingle::SXE::Application *whiteboard)
{
    auto jingle = whiteboard->pad()->session();
    setWindowTitle(tr("Whiteboard"));
    setWindowIcon(IconsetFactory::icon("psi/whiteboard").icon());
    d->incoming = true;
    d->view->setWhiteboardOnly(true);
    d->view->setPeer(jingle->peer().full(), false);
    d->view->setIncoming(true);
    d->view->setStatus(tr("Whiteboard invitation"));
    d->bindWhiteboardSession(jingle);
    d->bindWhiteboard(whiteboard);
    d->updateWhiteboardSupport();
}
