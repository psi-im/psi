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
#include "avcallaudiodirection.h"

#include "../psimedia/psimedia.h"
#include "avcallpolicy.h"
#include "mediadevicewatcher.h"
#include "psiaccount.h"
#include "psimediajingle.h"
#include "psimediajinglecapabilitytransaction.h"

#include <iris/dtls.h>
#include <iris/jingle-ice.h>
#include <iris/jingle-rtp-description.h>
#include <iris/jingle-rtp.h>
#include <iris/jingle-session.h>
#include <iris/xmpp-im/xmpp_jinglemessage.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_message.h>

#include <QHash>
#include <QHostAddress>
#include <QPointer>
#include <QUuid>
#include <QtCrypto>

#include <algorithm>
#include <memory>
#include <utility>

namespace Jingle = XMPP::Jingle;
namespace RTP    = XMPP::Jingle::RTP;
namespace ICE    = XMPP::Jingle::ICE;

static MediaConfiguration *g_config = new MediaConfiguration;

static QString resolvedAudioInputDevice()
{
    if (!g_config->liveInput)
        return {};

    const auto devices = MediaDeviceWatcher::instance()->audioInputDevices();
    for (const auto &device : devices) {
        if (!g_config->audioInDeviceId.isEmpty() && device.id() == g_config->audioInDeviceId)
            return device.id();
    }
    for (const auto &device : devices) {
        if (device.isDefault())
            return device.id();
    }
    return devices.isEmpty() ? QString() : devices.first().id();
}

static QString resolvedVideoInputDevice()
{
    if (!g_config->liveInput)
        return {};
    const auto devices = MediaDeviceWatcher::instance()->videoInputDevices();
    for (const auto &device : devices)
        if (!g_config->videoInDeviceId.isEmpty() && device.id() == g_config->videoInDeviceId)
            return device.id();
    for (const auto &device : devices)
        if (device.isDefault())
            return device.id();
    return devices.isEmpty() ? QString() : devices.first().id();
}

static PsiMediaJingleCapabilities currentNativeCallCapabilities()
{
    PsiMediaJingleCapabilities result;
    auto                       watcher = MediaDeviceWatcher::instance();
    result.backendAvailable            = PsiMedia::isSupported();
    result.probeComplete               = watcher->featuresReady();
#ifdef PSI_ENABLE_AVCALL
    const auto backendProfiles = PsiMedia::RtpSession::supportedSecureRtpProfiles();
    const auto dtlsProfiles    = XMPP::Dtls::supportedSRTPProfiles();
    result.secureRtp           = std::any_of(backendProfiles.cbegin(), backendProfiles.cend(),
                                             [&dtlsProfiles](const QString &profile) { return dtlsProfiles.contains(profile); });
#else
    result.secureRtp = false;
#endif
    result.audio = AvCallPolicy::mediaTypeSupported(result.backendAvailable, result.probeComplete, result.secureRtp,
                                                    !watcher->supportedAudioModes().isEmpty());
    result.video = AvCallPolicy::mediaTypeSupported(result.backendAvailable, result.probeComplete, result.secureRtp,
                                                    !watcher->supportedVideoModes().isEmpty());
    result.audioInput  = !watcher->audioInputDevices().isEmpty();
    result.audioOutput = !watcher->audioOutputDevices().isEmpty();
    result.videoInput  = !watcher->videoInputDevices().isEmpty();
    return result;
}

class AvCallManagerPrivate : public QObject {
    Q_OBJECT

public:
    AvCallManagerPrivate(PsiAccount *account, AvCallManager *q);
    ~AvCallManagerPrivate() override;

    void unlink(AvCall *call);
    void applyNetworkConfiguration();
    void refreshCapabilities();

    AvCallManager                      *q             = nullptr;
    PsiAccount                         *pa            = nullptr;
    Jingle::Manager                    *jingleManager = nullptr;
    RTP::Manager                       *rtpManager    = nullptr;
    ICE::Manager                       *iceManager    = nullptr;
    std::shared_ptr<RTP::MediaProvider> mediaProvider;
    PsiMediaJingleCapabilities          capabilities;
    QList<AvCall *>                     sessions;
    QList<AvCall *>                     pending;
    QHash<QString, AvCall *>            jmiCalls;

private slots:
    void incomingSession(Jingle::Session *session);
    void incomingRtpProposal(const XMPP::Message &message, const QString &id, RTP::MediaSet media);
    void incomingMessageInitiation(const XMPP::Message &message, const Jingle::MessageInitiation &initiation);
};

class AvCallPrivate : public QObject {
    Q_OBJECT

public:
    explicit AvCallPrivate(AvCall *q) : QObject(q), q(q)
    {
        connect(&audioDirection, &AvCallAudioDirection::changed, this, [this] { syncActiveTransmit(); });
        connect(&cameraDirection, &AvCallAudioDirection::changed, this, [this] { syncActiveTransmit(); });
        connect(&screenDirection, &AvCallAudioDirection::changed, this, [this] { syncActiveTransmit(); });
    }
    ~AvCallPrivate() override
    {
        if (!incoming && !session && !jmiId.isEmpty() && !jmiProceedSent && !jmiClosed) {
            sendJmi(Jingle::MessageInitiation::Action::Retract, QStringLiteral("cancel"), QStringLiteral("Cancelled"));
            jmiClosed = true;
        }
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

    bool attachProposal(const XMPP::Jid &from, const QString &id, RTP::MediaSet media)
    {
        if (!manager || id.isEmpty() || media == RTP::MediaSet())
            return false;

        const bool audio = media.testFlag(RTP::Media::Audio);
        const bool video = media.testFlag(RTP::Media::Video);
        if (!audio && !video)
            return false;

        peer           = from;
        incoming       = true;
        jmiId          = id;
        requestedAudio = audio;
        requestedVideo = video;
        mode           = audio && video ? AvCall::Both : (audio ? AvCall::Audio : AvCall::Video);
        return true;
    }

    bool proposalMatches(Jingle::Session *incomingSession) const
    {
        if (jmiId.isEmpty())
            return true;
        if (!incomingSession || incomingSession->sid() != jmiId || !incomingSession->peer().compare(peer, false))
            return false;

        bool audio = false;
        bool video = false;
        for (auto app : incomingSession->contentList()) {
            const auto rtp = dynamic_cast<RTP::Application *>(app);
            if (!rtp)
                return false;
            if (rtp->media() == QLatin1String("audio"))
                audio = true;
            else if (rtp->media() == QLatin1String("video"))
                video = true;
            else
                return false;
        }
        return audio == requestedAudio && video == requestedVideo;
    }

    bool sendJmi(Jingle::MessageInitiation::Action action, const QString &condition = {}, const QString &text = {})
    {
        if (!manager || !manager->jingleManager || jmiId.isEmpty() || !peer.isValid())
            return false;
        Jingle::MessageInitiation initiation(action, jmiId);
        if (!condition.isEmpty() || !text.isEmpty())
            initiation.setReason(condition, text);
        return manager->jingleManager->sendMessageInitiation(peer, initiation);
    }

    void sendJmiFinish(const QString &condition, const QString &text)
    {
        if (!jmiProceedSent || jmiFinishSent)
            return;
        if (sendJmi(Jingle::MessageInitiation::Action::Finish, condition, text))
            jmiFinishSent = true;
    }

    void cancelJmiUi(const QString &message)
    {
        if (jmiClosed)
            return;
        jmiClosed   = true;
        errorString = message;
        peer        = {};
        emit q->cancelled();
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
        if (!jmiProceedSent)
            mode = audio && video ? AvCall::Both : (audio ? AvCall::Audio : AvCall::Video);
        return true;
    }

    void startOutgoing()
    {
        if (!manager || session || !jmiId.isEmpty())
            return;

        const bool needAudio = mode == AvCall::Audio || mode == AvCall::Both;
        const bool needVideo = mode == AvCall::Video || mode == AvCall::Both;
        if (!manager->capabilities.available()) {
            fail(manager->capabilities.unavailableReason());
            return;
        }
        if ((needAudio && !manager->capabilities.audio) || (needVideo && !manager->capabilities.video)) {
            fail(tr("The requested media type is not supported by the current media backend."));
            return;
        }

        requestedAudio      = needAudio;
        requestedVideo      = needVideo;
        captureAudioConsent = needAudio && microphoneWanted;
        captureVideoConsent = needVideo && cameraWanted;

        if (manager->jingleManager->messageInitiationEnabled()) {
            RTP::MediaSet media;
            if (needAudio)
                media |= RTP::Media::Audio;
            if (needVideo)
                media |= RTP::Media::Video;

            jmiId = manager->rtpManager->propose(peer, media);
            if (!jmiId.isEmpty()) {
                // XEP-0353 proposes to the peer's bare JID. The resource that
                // answers with <proceed/> becomes the actual Jingle peer.
                peer = XMPP::Jid(peer.bare());
                manager->jmiCalls.insert(jmiId, q);
                return;
            }
        }

        startOutgoingSession();
    }

    void startOutgoingSession()
    {
        if (!manager || session)
            return;

        manager->applyNetworkConfiguration();
        session = jmiId.isEmpty() ? manager->jingleManager->newSession(peer)
                                  : manager->jingleManager->newSession(peer, jmiId);
        if (!session) {
            if (!jmiId.isEmpty() && jmiProceedSent)
                sendJmiFinish(QStringLiteral("expired"), QString());
            fail(tr("Unable to create a Jingle call session."));
            return;
        }
        setupSession();

        const bool needAudio = requestedAudio;
        const bool needVideo = requestedVideo;

        Jingle::Origin audioSenders = Jingle::Origin::Both;
        if (needAudio) {
            audioSenders = AvCallPolicy::sendersForCaptureAvailability(Jingle::Origin::Both, session->role(),
                                                                       audioCaptureAvailable());
        }

        if ((needAudio && !manager->rtpManager->createOutgoing(session, RTP::Media::Audio, audioSenders))
            || (needVideo && !manager->rtpManager->createOutgoing(session, RTP::Media::Video))) {
            fail(tr("Unable to create the requested RTP media."));
            return;
        }
        for (auto app : session->contentList()) {
            auto rtp = dynamic_cast<RTP::Application *>(app);
            if (rtp && rtp->media() == QLatin1String("video")) {
                cameraContent = rtp;
                cameraDirection.bind(rtp, true, (!g_config->liveInput || !resolvedVideoInputDevice().isEmpty()));
                break;
            }
        }
        wireApplications();

        if (needAudio)
            audioDirection.bind(audioApplication(), captureAudioConsent, audioCaptureAvailable());

        if (!configureBackend()) {
            fail(errorString.isEmpty() ? tr("Unable to initialize the media backend.") : errorString);
            return;
        }
        session->initiate();
    }

    void accept()
    {
        if (!manager || !incoming || jmiClosed)
            return;

        const bool wantsAudio  = mode == AvCall::Audio || mode == AvCall::Both;
        const bool wantsVideo  = mode == AvCall::Video || mode == AvCall::Both;
        const bool acceptAudio = requestedAudio && wantsAudio;
        const bool acceptVideo = requestedVideo && wantsVideo;
        if (!acceptAudio && !acceptVideo) {
            errorString      = tr("No offered RTP media type was accepted.");
            localTermination = true;
            if (session)
                session->terminate(Jingle::Reason::Decline);
            else
                sendJmi(Jingle::MessageInitiation::Action::Reject, QStringLiteral("busy"), QStringLiteral("Busy"));
            jmiClosed = true;
            emit q->error();
            return;
        }

        // XEP-0353 acceptance is a user-confirmation boundary. The UI calls
        // accept(), so proceed is never emitted merely because a proposal was
        // received.
        if (!jmiId.isEmpty() && !jmiProceedSent) {
            if (!sendJmi(Jingle::MessageInitiation::Action::Proceed)) {
                fail(tr("Unable to accept the incoming call proposal."));
                return;
            }
            jmiProceedSent = true;
            if (!session)
                return;
        }

        if (!session || session->state() != Jingle::State::Created)
            return;

        manager->applyNetworkConfiguration();
        if (!configureBackend()) {
            fail(errorString.isEmpty() ? tr("Unable to initialize the media backend.") : errorString);
            return;
        }

        captureAudioConsent = acceptAudio && microphoneWanted;
        captureVideoConsent = acceptVideo && cameraWanted;
        for (auto app : session->contentList()) {
            auto rtp = dynamic_cast<RTP::Application *>(app);
            if (!rtp)
                continue;
            const bool accepted = (rtp->media() == QLatin1String("audio") && acceptAudio)
                || (rtp->media() == QLatin1String("video") && acceptVideo);
            if (!accepted) {
                rtp->remove(Jingle::Reason::Decline, QStringLiteral("Media type declined locally"));
            } else if (rtp->media() == QLatin1String("video")) {
                cameraContent = rtp;
                cameraDirection.bind(rtp, captureVideoConsent,
                                     (!g_config->liveInput || !resolvedVideoInputDevice().isEmpty()));
            } else if (rtp->media() == QLatin1String("audio")) {
                audioDirection.bind(rtp,
                                    captureAudioConsent && AvCallPolicy::allowsSender(rtp->senders(), session->role()),
                                    audioCaptureAvailable());
            }
        }
        syncAudioDirection();
        session->accept();
    }

    void reject()
    {
        localTermination = true;
        jmiClosed        = true;

        if (!jmiId.isEmpty() && !jmiProceedSent)
            sendJmi(Jingle::MessageInitiation::Action::Reject, QStringLiteral("busy"), QStringLiteral("Busy"));

        if (!session) {
            if (!jmiId.isEmpty() && jmiProceedSent)
                sendJmiFinish(QStringLiteral("expired"), QStringLiteral("Cancelled before session start"));
            return;
        }

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

    RTP::Application *audioApplication() const
    {
        if (!session)
            return nullptr;
        for (auto app : session->contentList()) {
            auto rtp = dynamic_cast<RTP::Application *>(app);
            if (rtp && rtp->media() == QLatin1String("audio") && rtp->state() < Jingle::State::Finishing)
                return rtp;
        }
        return nullptr;
    }

    RTP::Application *cameraApplication() const
    {
        if (cameraContent && cameraContent->state() < Jingle::State::Finishing)
            return cameraContent;
        return nullptr;
    }

    void setMicrophoneEnabled(bool enabled)
    {
        microphoneWanted    = enabled;
        captureAudioConsent = enabled;
        audioDirection.setLocalSending(enabled);
        syncActiveTransmit();
    }

    void setCameraEnabled(bool enabled)
    {
        cameraWanted        = enabled;
        captureVideoConsent = enabled;
        if (!active || !session)
            return;
        if (enabled && !cameraApplication()) {
            if (!manager || !manager->capabilities.video || resolvedVideoInputDevice().isEmpty()) {
                captureVideoConsent = false;
                emit q->cameraEnabledChanged(false);
                emit q->mediaControlError(tr("No camera is available."));
                return;
            }
            if (screenContent && screenContent->state() < Jingle::State::Active) {
                captureVideoConsent = false;
                emit q->cameraEnabledChanged(false);
                emit q->mediaControlError(tr("Wait for screen sharing to connect before enabling the camera."));
                return;
            }
            cameraContent = manager->rtpManager->createOutgoing(session, RTP::Media::Video, session->role(),
                                                                QStringLiteral("camera-")
                                                                    + QUuid::createUuid().toString(QUuid::Id128));
            if (!cameraContent) {
                captureVideoConsent = false;
                emit q->cameraEnabledChanged(false);
                emit q->mediaControlError(tr("Unable to add a camera to this call."));
                return;
            }
            wireApplications();
            cameraDirection.bind(cameraContent, true, (!g_config->liveInput || !resolvedVideoInputDevice().isEmpty()));
        } else {
            cameraDirection.setLocalSending(enabled);
        }
        syncActiveTransmit();
    }

    bool startScreenSharing(const QString &source, std::shared_ptr<const void> lease)
    {
        if (!session || !active || !manager || !manager->capabilities.video || source.isEmpty() || screenContent
            || !psiMediaJingleSupportsGroupedCapture(session))
            return false;
        if (cameraContent && cameraContent->state() < Jingle::State::Active) {
            emit q->mediaControlError(tr("Wait for the camera to connect before sharing your screen."));
            return false;
        }
        screenSource  = source;
        screenContent = manager->rtpManager->createOutgoing(session, RTP::Media::Video, session->role(),
                                                            QStringLiteral("screen-")
                                                                + QUuid::createUuid().toString(QUuid::Id128));
        if (!screenContent) {
            screenSource.clear();
            emit q->mediaControlError(tr("The media backend cannot add screen sharing to this call."));
            return false;
        }
        wireApplications();
        setPsiMediaJingleContentCaptureLease(session, screenContent->contentName(), std::move(lease));
        setPsiMediaJingleContentVideoProfile(session, screenContent->contentName(), QSize(1920, 1080), 15);
        screenDirection.bind(screenContent, true, true);
        emit q->screenSharingChanged(true);
        syncActiveTransmit();
        return true;
    }

    void stopScreenSharing()
    {
        const auto content = screenContent;
        screenSource.clear();
        screenDirection.setLocalSending(false);
        if (session && content)
            setPsiMediaJingleContentCapture(session, content->contentName(), false, QString());
        screenContent.clear();
        if (content && content->state() < Jingle::State::Finishing)
            content->remove(Jingle::Reason::Success, tr("Screen sharing stopped"));
        emit q->screenSharingChanged(false);
    }

    bool audioCaptureAvailable() const { return !g_config->liveInput || !resolvedAudioInputDevice().isEmpty(); }

    void syncAudioDirection() { audioDirection.setCaptureAvailable(audioCaptureAvailable()); }

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
        const auto contents = session->contentList().values();
        for (auto app : contents) {
            auto rtp = dynamic_cast<RTP::Application *>(app);
            if (!rtp)
                continue;
            connect(rtp, &Jingle::Application::stateChanged, this, &AvCallPrivate::applicationStateChanged,
                    Qt::UniqueConnection);
            connect(rtp, &Jingle::Application::sendersChanged, this, &AvCallPrivate::applicationSendersChanged,
                    Qt::UniqueConnection);
            const QPointer<RTP::Application> guard(rtp);
            AvCallPolicy::prepareIncomingContent(rtp, active);
            if (!guard)
                continue;
            if (rtp->media() == QLatin1String("video") && rtp->state() < Jingle::State::Finishing) {
                const bool receive = AvCallPolicy::allowsSender(rtp->senders(), AvCallPolicy::peerRole(session->role()));
                const bool presentation = rtp != cameraContent
                    && (rtp->contentName().startsWith(QStringLiteral("screen-"))
                        || (cameraContent && !rtp->contentName().startsWith(QStringLiteral("camera-"))));
                auto widget = receive ? (presentation ? presentationWidget : videoWidget) : nullptr;
                setPsiMediaJingleContentVideoOutput(session, rtp->contentName(), widget);
                if (widget) {
                    if (presentation) displayedScreen = rtp;
                    else displayedCamera = rtp;
                }
            }
        }
    }

    void syncActiveTransmit()
    {
        if (!session || !active)
            return;
        const bool audioAvailable = audioCaptureAvailable();
        for (auto app : session->contentList()) {
            auto rtp = dynamic_cast<RTP::Application *>(app);
            if (!rtp || rtp->state() >= Jingle::State::Finishing)
                continue;
            bool    enabled = false;
            QString source;
            if (rtp->media() == QLatin1String("audio")) {
                enabled = rtp->state() == Jingle::State::Active && captureAudioConsent && audioAvailable
                    && audioDirection.allowsCapture(rtp);
                source = resolvedAudioInputDevice();
            } else if (rtp == screenContent) {
                enabled = rtp->state() == Jingle::State::Active && !screenSource.isEmpty()
                    && screenDirection.allowsCapture(rtp);
                source = screenSource;
            } else if (rtp == cameraContent) {
                enabled = rtp->state() == Jingle::State::Active && captureVideoConsent
                    && cameraDirection.allowsCapture(rtp);
                source = resolvedVideoInputDevice();
            }
            if (g_config->liveInput)
                setPsiMediaJingleContentCapture(session, rtp->contentName(), enabled, source);
        }
        if (!g_config->liveInput)
            startPsiMediaJingleTransmit(session, false, captureAudioConsent, QString(), captureVideoConsent, QString());
    }

    void maybeActivateMedia()
    {
        if (!session || !signalingActive || active)
            return;

        acceptedAudio   = false;
        acceptedVideo   = false;
        bool audioReady = true;
        bool videoReady = true;

        for (auto app : session->contentList()) {
            auto rtp = dynamic_cast<RTP::Application *>(app);
            if (!rtp || rtp->state() >= Jingle::State::Finishing)
                continue;

            if (rtp->media() == QLatin1String("audio")) {
                acceptedAudio = true;
                audioReady    = audioReady && rtp->state() == Jingle::State::Active;
            } else if (rtp->media() == QLatin1String("video")) {
                acceptedVideo = true;
                videoReady    = videoReady && rtp->state() == Jingle::State::Active;
            }
        }

        if (!acceptedAudio && !acceptedVideo) {
            fail(tr("The peer did not accept any usable RTP media."));
            return;
        }
        if ((acceptedAudio && !audioReady) || (acceptedVideo && !videoReady))
            return;

        active = true;
        // A receive-only call is active even though this leaves capture paused.
        // sendersChanged/device hotplug reuse the same backend session below.
        syncActiveTransmit();
        emit q->activated();
    }

    void mediaCapabilitiesChanged()
    {
        QPointer<AvCallPrivate> guard(this);
        syncAudioDirection();
        if (!guard)
            return;
        cameraDirection.setCaptureAvailable((!g_config->liveInput || !resolvedVideoInputDevice().isEmpty()));
        if (!guard)
            return;
        if (active)
            syncActiveTransmit();
        else
            maybeActivateMedia();
    }

    void fail(const QString &message)
    {
        errorString      = message;
        localTermination = true;
        if (!session && !jmiId.isEmpty())
            jmiClosed = true;
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
        QPointer<AvCallPrivate> guard(this);
        syncAudioDirection();
        if (guard)
            maybeActivateMedia();
    }

    void applicationStateChanged(Jingle::State state)
    {
        QPointer<AvCallPrivate>    guard(this);
        QPointer<RTP::Application> changed = qobject_cast<RTP::Application *>(sender());
        if (changed && state >= Jingle::State::Finishing
            && (changed == displayedCamera || changed == displayedScreen)) {
            const bool presentation = changed == displayedScreen;
            if (presentation) displayedScreen.clear();
            else displayedCamera.clear();
            emit q->incomingVideoRemoved(presentation);
            if (!guard) return;
        }
        if (changed && changed == screenContent && state == Jingle::State::Active)
            emit q->screenSharingStarted();
        if (!guard)
            return;
        if (changed && changed == cameraContent && state >= Jingle::State::Finishing) {
            cameraContent.clear();
            captureVideoConsent = false;
            emit q->cameraEnabledChanged(false);
            if (!guard)
                return;
        }
        if (changed && changed == screenContent && state >= Jingle::State::Finishing) {
            screenContent.clear();
            screenSource.clear();
            const auto reason = changed->lastReason().condition();
            emit       q->screenSharingChanged(false);
            if (!guard)
                return;
            if (reason != Jingle::Reason::Success)
                emit q->mediaControlError(tr("Screen sharing was declined or could not connect."));
        }
        if (!guard)
            return;
        if (active)
            syncActiveTransmit();
        else
            maybeActivateMedia();
    }

    void applicationSendersChanged(Jingle::Origin)
    {
        QPointer<AvCallPrivate> guard(this);
        auto changed = qobject_cast<RTP::Application *>(sender());
        if (changed && session && !AvCallPolicy::allowsSender(changed->senders(), AvCallPolicy::peerRole(session->role()))
            && (changed == displayedCamera || changed == displayedScreen)) {
            const bool presentation = changed == displayedScreen;
            if (presentation) displayedScreen.clear();
            else displayedCamera.clear();
            setPsiMediaJingleContentVideoOutput(session, changed->contentName(), nullptr);
            emit q->incomingVideoRemoved(presentation);
            if (!guard) return;
        }
        wireApplications();
        if (!guard) return;
        if (active)
            syncActiveTransmit();
        else
            maybeActivateMedia();
    }

    void sessionTerminated()
    {
        if (!session)
            return;
        const bool wasActive = active;
        stopPsiMediaJingleTransmit(session);
        if (!jmiId.isEmpty() && jmiProceedSent)
            sendJmiFinish(wasActive ? QStringLiteral("success") : QStringLiteral("expired"),
                          wasActive ? QStringLiteral("Success") : QStringLiteral("Session ended before activation"));
        if (!localTermination) {
            errorString = wasActive ? tr("Call was terminated.") : tr("Call was rejected or negotiation failed.");
            emit q->error();
        }
        signalingActive = false;
        active          = false;
        jmiClosed       = true;
    }

public:
    AvCall                   *q       = nullptr;
    AvCallManagerPrivate     *manager = nullptr;
    QPointer<Jingle::Session> session;
    XMPP::Jid                 peer;
    AvCall::Mode              mode    = AvCall::Audio;
    int                       bitrate = -1;
    QString                   errorString;
    PsiMedia::VideoWidget    *videoWidget         = nullptr;
    PsiMedia::VideoWidget     *presentationWidget  = nullptr;
    QPointer<RTP::Application> cameraContent;
    QPointer<RTP::Application> screenContent;
    QPointer<RTP::Application> displayedCamera;
    QPointer<RTP::Application> displayedScreen;
    QString                    screenSource;
    bool                      incoming            = false;
    bool                      signalingActive     = false;
    bool                      active              = false;
    bool                      localTermination    = false;
    bool                      requestedAudio      = false;
    bool                      requestedVideo      = false;
    bool                      acceptedAudio       = false;
    bool                      acceptedVideo       = false;
    bool                       microphoneWanted    = true;
    bool                       cameraWanted        = true;
    bool                      captureAudioConsent = false;
    bool                      captureVideoConsent = false;
    QString                   jmiId;
    bool                      jmiProceedSent = false;
    bool                      jmiFinishSent  = false;
    bool                      jmiClosed      = false;
    AvCallAudioDirection      audioDirection;
    AvCallAudioDirection       cameraDirection;
    AvCallAudioDirection       screenDirection;
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
    d->wireApplications();
}

void AvCall::setIncomingPresentation(PsiMedia::VideoWidget *widget)
{
    d->presentationWidget = widget;
    d->wireApplications();
}
void AvCall::setMicrophoneEnabled(bool enabled) { d->setMicrophoneEnabled(enabled); }
void AvCall::setCameraEnabled(bool enabled) { d->setCameraEnabled(enabled); }
bool AvCall::startScreenSharing(const QString &source, std::shared_ptr<const void> lease)
{
    return d->startScreenSharing(source, std::move(lease));
}
bool AvCall::screenSharingSupported() const
{
    return d->active && d->session && psiMediaJingleSupportsGroupedCapture(d->session);
}
void AvCall::stopScreenSharing() { d->stopScreenSharing(); }
bool AvCall::screenSharing() const { return d->screenContent && d->screenContent->state() < Jingle::State::Finishing; }

QString AvCall::errorString() const { return d->errorString; }

void AvCall::unlink() { d->unlink(); }

AvCallManagerPrivate::AvCallManagerPrivate(PsiAccount *account, AvCallManager *q) : QObject(q), q(q), pa(account)
{
    jingleManager = pa->client()->jingleManager();
    rtpManager    = jingleManager->rtpManager();
    iceManager    = pa->client()->jingleICEManager();

    // JMI support is a signalling-policy opt-in, not a snapshot of the
    // currently available media devices/codecs. Keep it enabled while the
    // native call manager exists; RTP::Manager and the handlers below validate
    // concrete media/transport capability separately.
    jingleManager->setMessageInitiationEnabled(true);
    rtpManager->setTransportNamespaces({ ICE::NS, ICE::NS_ICE_UDP });

    auto watcher = MediaDeviceWatcher::instance();
    // PsiAccount used to listen to the watcher independently. That made caps
    // advertisement race the provider refresh because the two slots depended on
    // QObject connection order. AvCallManager owns the transaction now.
    QObject::disconnect(watcher, &MediaDeviceWatcher::capabilitiesChanged, pa, &PsiAccount::updateFeatures);
    connect(watcher, &MediaDeviceWatcher::capabilitiesChanged, this, &AvCallManagerPrivate::refreshCapabilities);
    refreshCapabilities();

    connect(jingleManager, &Jingle::Manager::incomingSession, this, &AvCallManagerPrivate::incomingSession);
    connect(jingleManager, &Jingle::Manager::incomingMessageInitiation, this,
            &AvCallManagerPrivate::incomingMessageInitiation);
    connect(rtpManager, &RTP::Manager::incomingProposal, this, &AvCallManagerPrivate::incomingRtpProposal);
}

AvCallManagerPrivate::~AvCallManagerPrivate()
{
    if (jingleManager)
        jingleManager->setMessageInitiationEnabled(false);
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
    for (auto it = jmiCalls.begin(); it != jmiCalls.end();) {
        if (it.value() == call)
            it = jmiCalls.erase(it);
        else
            ++it;
    }
}

void AvCallManagerPrivate::applyNetworkConfiguration()
{
    iceManager->setBasePort(g_config->basePort);
    iceManager->setExternalAddress(g_config->extHost);
}

void AvCallManagerPrivate::refreshCapabilities()
{
    const auto next = currentNativeCallCapabilities();
    commitPsiMediaJingleCapabilities(rtpManager, capabilities, mediaProvider, next, [this] { pa->updateFeatures(); });

    // Input-device changes do not necessarily alter advertised audio/video
    // support, but they do alter the legal Jingle direction and capture state of
    // an existing call.
    const auto calls = sessions;
    for (auto call : calls) {
        if (call && call->d && call->d->manager == this)
            call->d->mediaCapabilitiesChanged();
    }
}

void AvCallManagerPrivate::incomingSession(Jingle::Session *incoming)
{
    if (!incoming || incoming->role() != Jingle::Origin::Responder)
        return;
    const auto types = incoming->allApplicationTypes();
    if (types.size() != 1 || types.first() != RTP::Description::ns())
        return;

    if (!capabilities.available()) {
        incoming->terminate(Jingle::Reason::UnsupportedApplications);
        return;
    }

    if (auto proposed = jmiCalls.value(incoming->sid())) {
        if (!proposed->d || proposed->d->manager != this || proposed->d->jmiClosed
            || !proposed->d->proposalMatches(incoming)) {
            incoming->terminate(Jingle::Reason::UnsupportedApplications);
            return;
        }
        if (!proposed->d->attachIncoming(incoming)) {
            incoming->terminate(Jingle::Reason::UnsupportedApplications);
            proposed->d->cancelJmiUi(tr("The Jingle offer did not match the incoming call proposal."));
            return;
        }
        if (proposed->d->jmiProceedSent)
            proposed->d->accept();
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

void AvCallManagerPrivate::incomingRtpProposal(const XMPP::Message &message, const QString &id, RTP::MediaSet media)
{
    const bool ownMessage = message.from().compare(pa->client()->jid(), false);
    auto       call       = jmiCalls.value(id, nullptr);

    if (ownMessage || message.spooled() || !jingleManager->messageInitiationEnabled())
        return;

    if (call) {
        if (call->d && !call->d->jmiClosed && message.from().compare(call->d->peer, false)) {
            Jingle::MessageInitiation ringing(Jingle::MessageInitiation::Action::Ringing, id);
            jingleManager->sendMessageInitiation(message.from(), ringing);
        }
        return;
    }

    auto proposed        = new AvCall;
    proposed->d->manager = this;
    if (!proposed->d->attachProposal(message.from(), id, media)) {
        delete proposed;
        return;
    }

    const bool usableAudio = proposed->d->requestedAudio && capabilities.audio;
    const bool usableVideo = proposed->d->requestedVideo && capabilities.video;
    if (!usableAudio && !usableVideo) {
        Jingle::MessageInitiation reject(Jingle::MessageInitiation::Action::Reject, id);
        reject.setReason(QStringLiteral("busy"), QStringLiteral("Busy"));
        jingleManager->sendMessageInitiation(message.from(), reject);
        delete proposed;
        return;
    }

    sessions.append(proposed);
    pending.append(proposed);
    jmiCalls.insert(id, proposed);

    Jingle::MessageInitiation ringing(Jingle::MessageInitiation::Action::Ringing, id);
    jingleManager->sendMessageInitiation(message.from(), ringing);
    emit q->incomingReady();
}

void AvCallManagerPrivate::incomingMessageInitiation(const XMPP::Message             &message,
                                                     const Jingle::MessageInitiation &initiation)
{
    const bool ownMessage = message.from().compare(pa->client()->jid(), false);
    auto       call       = jmiCalls.value(initiation.id(), nullptr);

    switch (initiation.action()) {
    case Jingle::MessageInitiation::Action::Propose:
        // Pure RTP proposals are surfaced through RTP::Manager::incomingProposal.
        // Mixed/application-composite proposals deliberately remain generic.
        return;

    case Jingle::MessageInitiation::Action::Proceed:
        if (!call || !call->d || call->d->jmiClosed || call->d->session)
            return;
        // A carbon from another local resource means that device accepted the
        // incoming call. Stop this resource from ringing; never auto-proceed.
        if (ownMessage) {
            if (call->d->incoming && !call->d->jmiProceedSent)
                call->d->cancelJmiUi(tr("Call answered on another device."));
            return;
        }
        // For an outgoing proposal, the resource that sends <proceed/> is the
        // resource selected for the actual Jingle session.
        if (!call->d->incoming && !call->d->jmiProceedSent && message.from().compare(call->d->peer, false)) {
            call->d->peer           = message.from();
            call->d->jmiProceedSent = true;
            call->d->startOutgoingSession();
        }
        return;

    case Jingle::MessageInitiation::Action::Reject:
        if (!call || !call->d || call->d->jmiClosed || call->d->session)
            return;
        if (ownMessage) {
            if (call->d->incoming)
                call->d->cancelJmiUi(tr("Call declined on another device."));
            return;
        }
        if (!call->d->incoming && message.from().compare(call->d->peer, false))
            call->d->cancelJmiUi(tr("Call was declined."));
        return;

    case Jingle::MessageInitiation::Action::Retract:
        if (!ownMessage && call && call->d && message.from().compare(call->d->peer, false) && !call->d->session) {
            call->d->cancelJmiUi(tr("Call was cancelled."));
        }
        return;

    case Jingle::MessageInitiation::Action::Finish:
        if (call && call->d && (ownMessage || message.from().compare(call->d->peer, false))) {
            if (!call->d->session) {
                if (!ownMessage && call->d->jmiProceedSent)
                    call->d->sendJmiFinish(QStringLiteral("success"), QStringLiteral("Success"));
                call->d->cancelJmiUi(tr("Call was finished."));
            }
        }
        return;

    case Jingle::MessageInitiation::Action::Ringing:
    case Jingle::MessageInitiation::Action::None:
        return;
    }
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

bool AvCallManager::isSupported() { return currentNativeCallCapabilities().available(); }

bool AvCallManager::isAudioSupported() { return currentNativeCallCapabilities().audio; }

bool AvCallManager::isVideoSupported() { return currentNativeCallCapabilities().video; }

QString AvCallManager::unsupportedReason() { return currentNativeCallCapabilities().unavailableReason(); }

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
