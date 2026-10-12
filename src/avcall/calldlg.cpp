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
#include <iris/xmpp-im/jingle-rtp.h>
#include <iris/xmpp-im/jingle-sxe.h>
#include <QElapsedTimer>
#include <functional>
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
            if (!session && enabled && !joinMedia()) {
                view->setMicrophoneChecked(false);
                return;
            }
            if (session)
                session->setMicrophoneEnabled(enabled);
        });
        connect(view, &CallView::cameraChanged, this, [this](bool enabled) {
            if (!session && enabled && !joinMedia()) {
                view->setCameraEnabled(false);
                return;
            }
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
            if (!proposalId.isEmpty())
                account->wbManager()->finish(proposalId, active);
            if (wbSession->state() < Jingle::State::Finishing)
                wbSession->terminate(active || !incoming ? Jingle::Reason::Success : Jingle::Reason::Decline);
        } else if (!proposalId.isEmpty() && !finished && account->wbManager()) {
            // A proposal without a session yet: decline it, or withdraw our own.
            if (incoming)
                account->wbManager()->reject(proposalId);
            else
                account->wbManager()->retract(proposalId);
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
            // Media joining a running whiteboard session keeps the session's duration.
            if (!timer.isActive()) {
                elapsed.start();
                timer.start(1000);
            }
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
        connect(call, &AvCall::microphoneEnabledChanged, view, &CallView::setMicrophoneChecked);
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
            const auto target
                = outgoingTarget(XMPP::Jid(view->peer()), [this](const XMPP::Jid &full) { return callCapable(full); });
            if (!target.isValid()) {
                view->setConnecting(false);
                view->setStatus(tr("The contact cannot be called."));
                return;
            }
            view->setStatus(tr("Calling…"));
            bind(account->avCallManager()->createOutgoing());
            session->setMicrophoneEnabled(view->microphoneEnabled());
            session->setCameraEnabled(view->cameraEnabled());
            const auto           caps = account->client()->capsManager()->features(target);
            AvCall::PeerFeatures features;
            if (caps.hasJingleIce())
                features |= AvCall::IceTransport;
            if (caps.hasJingleIceUdp())
                features |= AvCall::IceUdpTransport;
            session->connectToJid(target, view->cameraEnabled() ? AvCall::Both : AvCall::Audio, view->bitrate(),
                                  features);
        } else if (session) {
            view->setStatus(tr("Connecting…"));
            session->setMicrophoneEnabled(view->microphoneEnabled());
            session->setCameraEnabled(view->cameraEnabled());
            session->accept(session->mode(), view->bitrate());
        }
    }

    // Where an outgoing session goes. A full JID (a group chat occupant or a resource chosen explicitly) is used as
    // is. A bare JID stays bare when a proposal (XEP-0353) can ring the contact's devices: some capable device
    // advertises proposals, or no device is known. Otherwise the capable resource with the highest priority.
    XMPP::Jid outgoingTarget(const XMPP::Jid &peer, const std::function<bool(const XMPP::Jid &)> &capable) const
    {
        if (!peer.isValid() || !peer.resource().isEmpty())
            return peer;
        const bool          proposals = account->client()->jingleManager()->messageInitiationEnabled();
        const UserListItem *item      = account->find(peer);
        if (!item || item->userResourceList().isEmpty())
            return proposals ? peer : XMPP::Jid();
        const UserResource *best      = nullptr;
        bool                ringable = false;
        for (const auto &resource : item->userResourceList()) {
            const auto full = peer.withResource(resource.name());
            if (!capable(full))
                continue;
            ringable = ringable
                || account->client()->capsManager()->features(full).test(Jingle::MessageInitiation::ns());
            if (!best || resource.priority() > best->priority())
                best = &resource;
        }
        if (proposals && ringable)
            return peer;
        return best ? peer.withResource(best->name()) : XMPP::Jid();
    }

    bool whiteboardCapable(const XMPP::Jid &full) const
    {
        return account->wbManager() && account->wbManager()->checkSupport(full);
    }

    bool callCapable(const XMPP::Jid &full) const
    {
        // Resources whose capabilities are not known yet are tried, as before.
        return account->canCall(full) || account->client()->capsManager()->features(full).isEmpty();
    }

    void updateWhiteboardSupport()
    {
        Jingle::Session *jingle = jingleSession();
        const bool       supported
            = jingle ? account->wbManager() && account->wbManager()->checkSupport(jingle->peer())
                     : account->canWhiteboard(XMPP::Jid(view->peer()));
        // In a whiteboard-only session the whiteboard is the session; it cannot be chosen away before it starts.
        view->setWhiteboardSupported(supported && (!view->whiteboardOnly() || active));
    }

    Jingle::Session *jingleSession() const { return session ? session->jingleSession() : wbSession.data(); }

    void acceptWhiteboard()
    {
        if (finished || active)
            return;
        if (incoming && !proposalId.isEmpty() && !wbSession) {
            // Accepting a proposal: the session follows from the proposing side.
            if (!account->wbManager()->proceed(proposalId)) {
                view->setStatus(tr("Unable to accept the whiteboard invitation."));
                return;
            }
            joined = true;
            view->setConnecting(true);
            view->setStatus(tr("Connecting…"));
            return;
        }
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

        const auto target = outgoingTarget(XMPP::Jid(view->peer()), [this](const XMPP::Jid &full) {
            return whiteboardCapable(full);
        });
        if (!target.isValid() || (!target.resource().isEmpty() && !whiteboardCapable(target))) {
            view->setStatus(tr("The contact does not support whiteboards."));
            return;
        }
        if (target.resource().isEmpty()) {
            // The invitation rings all devices; the first one that accepts gets the session.
            proposalId = account->wbManager()->propose(target);
            if (proposalId.isEmpty()) {
                view->setStatus(tr("Unable to send the whiteboard invitation."));
                return;
            }
            view->setConnecting(true);
            view->setStatus(tr("Inviting…"));
            return;
        }
        startWhiteboardSession(target, QString());
    }

    void startWhiteboardSession(const XMPP::Jid &target, const QString &sid)
    {
        auto jingleManager = account->client()->jingleManager();
        auto jingle = sid.isEmpty() ? jingleManager->newSession(target) : jingleManager->newSession(target, sid);
        auto app = jingle ? qobject_cast<SXE::Application *>(jingle->newContent(SXE::NS_SVG_WHITEBOARD)) : nullptr;
        if (!app) {
            delete jingle;
            if (!sid.isEmpty())
                account->wbManager()->finish(sid, false);
            view->setStatus(tr("Unable to create a whiteboard."));
            return;
        }
        view->setConnecting(true);
        view->setStatus(sid.isEmpty() ? tr("Inviting…") : tr("Connecting…"));
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
            if (wbSession)
                view->setMediaAvailable(mediaAvailable(wbSession->peer()));
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
            if (!proposalId.isEmpty())
                account->wbManager()->finish(proposalId, wasActive);
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
                    } else if (!app && jingle == wbSession && !session) {
                        // Media added by the peer to a whiteboard session: AvCall accepts it for receiving.
                        if (!dynamic_cast<XMPP::Jingle::RTP::Application *>(content) || !joinMedia())
                            content->remove(Jingle::Reason::UnsupportedApplications);
                    }
                }
            });
    }

    bool mediaAvailable(const XMPP::Jid &peer) const
    {
        return AvCallManager::isSupported() && account->avCallManager() && account->canCall(peer);
    }

    // Media for a session that started as a whiteboard: AvCall joins the session and adds or accepts RTP
    // contents.
    bool joinMedia()
    {
        if (session)
            return true;
        if (!wbSession || !active || finished || !mediaAvailable(wbSession->peer()))
            return false;
        auto call = account->avCallManager()->joinSession(wbSession);
        if (!call) {
            view->setStatus(tr("Audio and video are not available."));
            return false;
        }
        bind(call);
        session->setIncomingVideo(view->remoteVideo());
        session->setIncomingPresentation(view->remotePresentation());
        return true;
    }

    // Answers to the proposal of this dialog.
    void watchProposals()
    {
        auto wb = account->wbManager();
        connect(wb, &WbManager::proposalRinging, this, [this](const QString &id) {
            if (id == proposalId && !wbSession)
                view->setStatus(tr("Ringing…"));
        });
        connect(wb, &WbManager::proposalAccepted, this, [this](const QString &id, const XMPP::Jid &peer) {
            if (id != proposalId || wbSession || finished)
                return;
            view->setPeer(peer.full(), false);
            view->setStatus(tr("Connecting…"));
            startWhiteboardSession(peer, id);
        });
        connect(wb, &WbManager::proposalSession, this, [this](const QString &id, SXE::Application *app) {
            if (id != proposalId || wbSession || finished)
                return;
            auto jingle = app->pad()->session();
            bindWhiteboardSession(jingle);
            bindWhiteboard(app);
            jingle->accept();
        });
        connect(wb, &WbManager::proposalClosed, this, [this](const QString &id, const QString &message) {
            if (id != proposalId || wbSession || finished)
                return;
            finished = true;
            view->setConnecting(false);
            view->setFinished();
            view->setStatus(message);
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
    QString                    proposalId;
    WbView                    *wbView           = nullptr;
    bool                       whiteboardWanted = false;
    bool                       joined           = false;
};

CallDlg::CallDlg(PsiAccount *account, QWidget *parent) : QDialog(parent), d(new Private(this))
{
    d->account = account;
    if (account->wbManager())
        d->watchProposals();
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
    d->view->setMediaAvailable(d->mediaAvailable(jid));
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
    d->view->setMediaAvailable(d->mediaAvailable(jingle->peer()));
    d->view->setPeer(jingle->peer().full(), false);
    d->view->setIncoming(true);
    d->view->setStatus(tr("Whiteboard invitation"));
    d->bindWhiteboardSession(jingle);
    d->bindWhiteboard(whiteboard);
    d->updateWhiteboardSupport();
}
void CallDlg::setIncomingWhiteboardProposal(const XMPP::Jid &peer, const QString &proposalId)
{
    setWindowTitle(tr("Whiteboard"));
    setWindowIcon(IconsetFactory::icon("psi/whiteboard").icon());
    d->incoming   = true;
    d->proposalId = proposalId;
    d->view->setWhiteboardOnly(true);
    d->view->setMediaAvailable(d->mediaAvailable(peer));
    d->view->setWhiteboardEnabled(true);
    d->view->setPeer(peer.full(), false);
    d->view->setIncoming(true);
    d->view->setStatus(tr("Whiteboard invitation"));
    d->updateWhiteboardSupport();
}
