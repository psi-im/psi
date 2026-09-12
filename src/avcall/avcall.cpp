/*
 * avcall.cpp - Psi call UI facade for Iris-native Jingle RTP
 * Copyright (C) 2009  Barracuda Networks, Inc.
 * Copyright (C) 2026  Psi Project
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "avcall.h"

#include "../psimedia/psimedia.h"
#include "mediadevicewatcher.h"
#include "psimediajingle.h"
#include "psiaccount.h"

#include <iris/jingle-ice.h>
#include <iris/jingle-rtp.h>
#include <iris/jingle-session.h>
#include <iris/xmpp_client.h>

#include <QHostAddress>
#include <QPointer>
#include <QtCrypto>

#include <memory>
#include <utility>

namespace Jingle = XMPP::Jingle;
namespace RTP    = XMPP::Jingle::RTP;
namespace ICE    = XMPP::Jingle::ICE;

static MediaConfiguration *g_config = new MediaConfiguration;

class AvCallManagerPrivate : public QObject {
    Q_OBJECT

public:
    AvCallManagerPrivate(PsiAccount *account, AvCallManager *q);
    ~AvCallManagerPrivate() override;

    void unlink(AvCall *call);
    void applyNetworkConfiguration();

    AvCallManager                       *q             = nullptr;
    PsiAccount                          *pa            = nullptr;
    Jingle::Manager                     *jingleManager = nullptr;
    RTP::Manager                        *rtpManager    = nullptr;
    ICE::Manager                        *iceManager    = nullptr;
    std::shared_ptr<RTP::MediaProvider>  mediaProvider;
    QList<AvCall *>                      sessions;
    QList<AvCall *>                      pending;

private slots:
    void incomingSession(Jingle::Session *session);
};

class AvCallPrivate : public QObject {
    Q_OBJECT

public:
    explicit AvCallPrivate(AvCall *q) : QObject(q), q(q) { }
    ~AvCallPrivate() override
    {
        if (session)
            stopPsiMediaJingleTransmit(session);
        unlink();
    }

    void unlink()
    {
        if (manager) {
            manager->unlink(q);
            manager = nullptr;
        }
    }

    bool attachIncoming(Jingle::Session *incomingSession)
    {
        if (!incomingSession || incomingSession->role() != Jingle::Origin::Responder)
            return false;
        session  = incomingSession;
        peer     = session->peer();
        incoming = true;
        setupSession();

        bool audio = false;
        bool video = false;
        for (auto app : session->contentList()) {
            auto rtp = dynamic_cast<RTP::Application *>(app);
            if (!rtp)
                return false;
            if (rtp->media() == QLatin1String("audio"))
                audio = true;
            else if (rtp->media() == QLatin1String("video"))
                video = true;
            else
                return false;
        }
        if (!audio && !video)
            return false;
        requestedAudio      = audio;
        requestedVideo      = video;
        captureAudioConsent = false;
        captureVideoConsent = false;
        mode                = audio && video ? AvCall::Both : (audio ? AvCall::Audio : AvCall::Video);
        return true;
    }

    void startOutgoing()
    {
        if (!manager || session)
            return;

        manager->applyNetworkConfiguration();
        session = manager->jingleManager->newSession(peer);
        if (!session) {
            fail(tr("Unable to create a Jingle call session."));
            return;
        }
        setupSession();

        const bool needAudio = mode == AvCall::Audio || mode == AvCall::Both;
        const bool needVideo = mode == AvCall::Video || mode == AvCall::Both;
        requestedAudio      = needAudio;
        requestedVideo      = needVideo;
        captureAudioConsent = needAudio;
        captureVideoConsent = needVideo;
        if ((needAudio && !manager->rtpManager->createOutgoing(session, QStringLiteral("audio")))
            || (needVideo && !manager->rtpManager->createOutgoing(session, QStringLiteral("video")))) {
            fail(tr("Unable to create the requested RTP media."));
            return;
        }
        wireApplications();

        if (!configureBackend()) {
            fail(errorString.isEmpty() ? tr("Unable to initialize the media backend.") : errorString);
            return;
        }
        session->initiate();
    }

    void accept()
    {
        if (!manager || !session || !incoming || session->state() != Jingle::State::Created)
            return;

        manager->applyNetworkConfiguration();
        if (!configureBackend()) {
            fail(errorString.isEmpty() ? tr("Unable to initialize the media backend.") : errorString);
            return;
        }

        const bool wantsAudio = mode == AvCall::Audio || mode == AvCall::Both;
        const bool wantsVideo = mode == AvCall::Video || mode == AvCall::Both;
        const bool acceptAudio = requestedAudio && wantsAudio;
        const bool acceptVideo = requestedVideo && wantsVideo;
        captureAudioConsent = acceptAudio;
        captureVideoConsent = acceptVideo;
        if (!acceptAudio && !acceptVideo) {
            errorString      = tr("No offered RTP media type was accepted.");
            localTermination = true;
            session->terminate(Jingle::Reason::Decline);
            emit q->error();
            return;
        }
        for (auto app : session->contentList()) {
            auto rtp = dynamic_cast<RTP::Application *>(app);
            if (!rtp)
                continue;
            const bool accepted = (rtp->media() == QLatin1String("audio") && acceptAudio)
                || (rtp->media() == QLatin1String("video") && acceptVideo);
            if (!accepted)
                rtp->remove(Jingle::Reason::Decline, QStringLiteral("Media type declined locally"));
        }
        session->accept();
    }

    void reject()
    {
        if (!session)
            return;
        localTermination = true;
        stopPsiMediaJingleTransmit(session);
        session->terminate(Jingle::Reason::Decline);
    }

    bool configureBackend()
    {
        if (!session)
            return false;
        const QString file = g_config->liveInput ? QString() : g_config->file;
        if (!g_config->liveInput && file.isEmpty()) {
            errorString = tr("A media file was selected as call input but no file is configured.");
            return false;
        }
        if (!configurePsiMediaJingleSession(session, g_config->audioOutDeviceId, file, g_config->loopFile, bitrate))
            return false;
        if (videoWidget)
            setPsiMediaJingleVideoOutput(session, videoWidget);
        return true;
    }

    void setupSession()
    {
        Q_ASSERT(session);
        connect(session, &Jingle::Session::activated, this, &AvCallPrivate::sessionActivated);
        connect(session, &Jingle::Session::terminated, this, &AvCallPrivate::sessionTerminated);
        connect(session, &Jingle::Session::newContentReceived, this, &AvCallPrivate::wireApplications);
        connect(session, &QObject::destroyed, this, [this] {
            session         = nullptr;
            signalingActive = false;
            active          = false;
            acceptedAudio   = false;
            acceptedVideo   = false;
        });
        wireApplications();
    }

    void wireApplications()
    {
        if (!session)
            return;
        for (auto app : session->contentList()) {
            auto rtp = dynamic_cast<RTP::Application *>(app);
            if (!rtp)
                continue;
            connect(rtp, &Jingle::Application::stateChanged, this, &AvCallPrivate::applicationStateChanged,
                    Qt::UniqueConnection);
        }
    }

    void maybeActivateMedia()
    {
        if (!session || !signalingActive || active)
            return;

        acceptedAudio       = false;
        acceptedVideo       = false;
        bool audioReady     = true;
        bool videoReady     = true;
        bool audioMaySend   = false;
        bool videoMaySend   = false;

        for (auto app : session->contentList()) {
            auto rtp = dynamic_cast<RTP::Application *>(app);
            if (!rtp || rtp->state() >= Jingle::State::Finishing)
                continue;

            const bool localMaySend
                = rtp->senders() == Jingle::Origin::Both || rtp->senders() == session->role();
            if (rtp->media() == QLatin1String("audio")) {
                acceptedAudio = true;
                audioReady    = audioReady && rtp->state() == Jingle::State::Active;
                audioMaySend  = audioMaySend || localMaySend;
            } else if (rtp->media() == QLatin1String("video")) {
                acceptedVideo = true;
                videoReady    = videoReady && rtp->state() == Jingle::State::Active;
                videoMaySend  = videoMaySend || localMaySend;
            }
        }

        if (!acceptedAudio && !acceptedVideo) {
            fail(tr("The peer did not accept any usable RTP media."));
            return;
        }
        if ((acceptedAudio && !audioReady) || (acceptedVideo && !videoReady))
            return;

        const bool transmitAudio = acceptedAudio && captureAudioConsent && audioMaySend;
        const bool transmitVideo = acceptedVideo && captureVideoConsent && videoMaySend;

        active = true;
        // A false return is valid for receive-only calls: activation is driven
        // by negotiated media readiness, while this call controls local capture.
        startPsiMediaJingleTransmit(session, g_config->liveInput, transmitAudio, g_config->audioInDeviceId,
                                    transmitVideo, g_config->videoInDeviceId);
        emit q->activated();
    }

    void fail(const QString &message)
    {
        errorString       = message;
        localTermination  = true;
        if (session) {
            if (session->state() == Jingle::State::Created && session->role() == Jingle::Origin::Initiator) {
                delete session;
                session = nullptr;
            } else {
                session->terminate(Jingle::Reason::FailedApplication, message);
            }
        }
        emit q->error();
    }

private slots:
    void sessionActivated()
    {
        signalingActive = true;
        maybeActivateMedia();
    }

    void applicationStateChanged(Jingle::State) { maybeActivateMedia(); }

    void sessionTerminated()
    {
        if (!session)
            return;
        stopPsiMediaJingleTransmit(session);
        if (!localTermination) {
            errorString = active ? tr("Call was terminated.") : tr("Call was rejected or negotiation failed.");
            emit q->error();
        }
        signalingActive = false;
        active          = false;
    }

public:
    AvCall                        *q       = nullptr;
    AvCallManagerPrivate          *manager = nullptr;
    QPointer<Jingle::Session>      session;
    XMPP::Jid                      peer;
    AvCall::Mode                   mode = AvCall::Audio;
    int                            bitrate = -1;
    QString                        errorString;
    PsiMedia::VideoWidget         *videoWidget = nullptr;
    bool                           incoming = false;
    bool                           signalingActive = false;
    bool                           active = false;
    bool                           localTermination = false;
    bool                           requestedAudio = false;
    bool                           requestedVideo = false;
    bool                           acceptedAudio = false;
    bool                           acceptedVideo = false;
    bool                           captureAudioConsent = false;
    bool                           captureVideoConsent = false;
};

AvCall::AvCall() : d(new AvCallPrivate(this)) { }

AvCall::AvCall(const AvCall &from) : QObject(nullptr)
{
    Q_UNUSED(from)
    qFatal("AvCall copy is not supported");
}

AvCall::~AvCall() { delete d; }

XMPP::Jid AvCall::jid() const { return d->session ? d->session->peer() : d->peer; }

AvCall::Mode AvCall::mode() const { return d->mode; }

void AvCall::connectToJid(const XMPP::Jid &jid, Mode mode, int kbps, PeerFeatures features)
{
    Q_UNUSED(features)
    d->peer    = jid;
    d->mode    = mode;
    d->bitrate = kbps;
    d->startOutgoing();
}

void AvCall::accept(Mode mode, int kbps)
{
    d->mode    = mode;
    d->bitrate = kbps;
    d->accept();
}

void AvCall::reject() { d->reject(); }

void AvCall::setIncomingVideo(PsiMedia::VideoWidget *widget)
{
    d->videoWidget = widget;
    if (d->session)
        setPsiMediaJingleVideoOutput(d->session, widget);
}

QString AvCall::errorString() const { return d->errorString; }

void AvCall::unlink() { d->unlink(); }

AvCallManagerPrivate::AvCallManagerPrivate(PsiAccount *account, AvCallManager *q) : QObject(q), q(q), pa(account)
{
    jingleManager = pa->client()->jingleManager();
    rtpManager    = jingleManager->rtpManager();
    iceManager    = pa->client()->jingleICEManager();
    mediaProvider = makePsiMediaJingleProvider();

    rtpManager->setMediaProvider(mediaProvider);
    rtpManager->setTransportNamespaces({ ICE::NS, ICE::NS_ICE_UDP });
    connect(jingleManager, &Jingle::Manager::incomingSession, this, &AvCallManagerPrivate::incomingSession);
}

AvCallManagerPrivate::~AvCallManagerPrivate()
{
    if (rtpManager) {
        rtpManager->closeAll();
        rtpManager->setTransportNamespaces({});
        rtpManager->setMediaProvider({});
    }
    for (auto call : std::as_const(sessions)) {
        if (call && call->d)
            call->d->manager = nullptr;
    }
}

void AvCallManagerPrivate::unlink(AvCall *call)
{
    sessions.removeAll(call);
    pending.removeAll(call);
}

void AvCallManagerPrivate::applyNetworkConfiguration()
{
    iceManager->setBasePort(g_config->basePort);
    iceManager->setExternalAddress(g_config->extHost);
}

void AvCallManagerPrivate::incomingSession(Jingle::Session *incoming)
{
    if (!incoming || incoming->role() != Jingle::Origin::Responder)
        return;
    const auto types = incoming->allApplicationTypes();
    if (types.size() != 1 || types.first() != RTP::Description::ns())
        return;

    if (!PsiMedia::isSupported()) {
        incoming->terminate(Jingle::Reason::UnsupportedApplications);
        return;
    }

    auto call        = new AvCall;
    call->d->manager = this;
    if (!call->d->attachIncoming(incoming)) {
        incoming->terminate(Jingle::Reason::UnsupportedApplications);
        delete call;
        return;
    }
    sessions.append(call);
    pending.append(call);
    emit q->incomingReady();
}

AvCallManager::AvCallManager(PsiAccount *pa) : QObject(nullptr), d(new AvCallManagerPrivate(pa, this)) { }

AvCallManager::~AvCallManager() { delete d; }

AvCall *AvCallManager::createOutgoing()
{
    auto call        = new AvCall;
    call->d->manager = d;
    d->sessions.append(call);
    return call;
}

AvCall *AvCallManager::takeIncoming() { return d->pending.isEmpty() ? nullptr : d->pending.takeFirst(); }

void AvCallManager::config() { }

bool AvCallManager::isSupported()
{
    if (!QCA::isSupported("hmac(sha1)")) {
        qWarning("hmac support missing for calls, install qca-ossl");
        return false;
    }
    return PsiMedia::isSupported();
}

void AvCallManager::setSelfAddress(const QHostAddress &addr) { d->iceManager->setSelfAddress(addr); }

void AvCallManager::setStunBindService(const QString &host, int port) { d->iceManager->setStunBindService(host, port); }

void AvCallManager::setStunRelayUdpService(const QString &host, int port, const QString &user, const QString &pass)
{
    d->iceManager->setStunRelayUdpService(host, port, user, pass);
}

void AvCallManager::setStunRelayTcpService(const QString &host, int port, const XMPP::AdvancedConnector::Proxy &proxy,
                                           const QString &user, const QString &pass)
{
    d->iceManager->setStunRelayTcpService(host, port, proxy, user, pass);
}

void AvCallManager::setAllowIpExposure(bool allow) { d->iceManager->setAllowIpExposure(allow); }

void AvCallManager::setBasePort(int port)
{
    if (port == 0)
        port = -1;
    g_config->basePort = port;
}

void AvCallManager::setExternalAddress(const QString &host) { g_config->extHost = host; }

void AvCallManager::setAudioOutDevice(const QString &id) { g_config->audioOutDeviceId = id; }

void AvCallManager::setAudioInDevice(const QString &id) { g_config->audioInDeviceId = id; }

void AvCallManager::setVideoInDevice(const QString &id) { g_config->videoInDeviceId = id; }

#include "avcall.moc"
