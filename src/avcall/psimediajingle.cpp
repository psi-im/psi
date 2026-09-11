/*
 * Copyright (C) 2026  Psi Project
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include "psimediajingle.h"

#include "../psimedia/psimedia.h"

#include <iris/jingle-rtp.h>
#include <iris/jingle-session.h>

#include <QMetaObject>
#include <QPointer>
#include <QSet>
#include <QTimer>

#include <optional>
#include <utility>

namespace {
namespace RTP = XMPP::Jingle::RTP;

RTP::MediaError backendError(const QString &text)
{
    return { RTP::MediaError::Code::Backend, text };
}

RTP::MediaError unsupportedError(const QString &text)
{
    return { RTP::MediaError::Code::Unsupported, text };
}

std::optional<RTP::PayloadType> toRtpPayload(const PsiMedia::PayloadInfo &input)
{
    if (input.id() < 0 || input.id() > 127 || input.name().isEmpty())
        return {};

    RTP::PayloadType result;
    result.id   = quint8(input.id());
    result.name = input.name();
    if (input.clockrate() > 0)
        result.clockrate = quint32(input.clockrate());
    if (input.channels() > 0)
        result.channels = quint8(input.channels());
    if (input.ptime() > 0)
        result.ptime = quint32(input.ptime());
    if (input.maxptime() > 0)
        result.maxptime = quint32(input.maxptime());
    for (const auto &parameter : input.parameters())
        result.parameters.insert(parameter.name, parameter.value);
    return result;
}

std::optional<QList<PsiMedia::PayloadInfo>> toPsiPayloads(const RTP::Description &description)
{
    QList<PsiMedia::PayloadInfo> result;
    for (const auto &payload : description.payloads) {
        if (payload.id > 127 || payload.name.isEmpty())
            return {};
        PsiMedia::PayloadInfo out;
        out.setId(payload.id);
        out.setName(payload.name);
        out.setClockrate(payload.clockrate ? int(*payload.clockrate) : -1);
        out.setChannels(payload.channels ? int(*payload.channels) : -1);
        out.setPtime(payload.ptime ? int(*payload.ptime) : -1);
        out.setMaxptime(payload.maxptime ? int(*payload.maxptime) : -1);
        QList<PsiMedia::PayloadInfo::Parameter> parameters;
        for (auto it = payload.parameters.cbegin(); it != payload.parameters.cend(); ++it) {
            PsiMedia::PayloadInfo::Parameter parameter;
            parameter.name  = it.key();
            parameter.value = it.value();
            parameters.append(parameter);
        }
        out.setParameters(parameters);
        result.append(out);
    }
    if (result.isEmpty())
        return {};
    return result;
}

bool hasUnsupportedAnswerFeatures(const RTP::Description &answer)
{
    if (!answer.feedback.isEmpty() || answer.feedbackTrrInt || !answer.headerExtensions.isEmpty()
        || answer.extmapAllowMixed || !answer.extensions.isEmpty())
        return true;
    for (const auto &payload : answer.payloads) {
        if (!payload.feedback.isEmpty() || payload.feedbackTrrInt || !payload.extensions.isEmpty())
            return true;
    }
    return false;
}

class BackendSession;

class Endpoint final : public RTP::MediaEndpoint {
public:
    Endpoint(BackendSession *session, QString media);
    ~Endpoint() override;

    bool supportsPacketIo() const override { return true; }
    bool attachPacketIo(PacketWriter writer) override;
    void receivePacket(const QByteArray &data, RTP::SrtpContext::Packet kind) override;

    RTP::Description localOffer() const override { return prepared_.value_or(RTP::Description {}); }
    std::optional<RTP::Description> makeAnswer(const RTP::Description &) const override { return {}; }
    bool acceptsAnswer(const RTP::Description &, const RTP::Description &answer) const override
    {
        return !hasUnsupportedAnswerFeatures(answer);
    }
    bool configure(const RTP::Description &, const RTP::Description &) override { return false; }
    void stop() override;

    const QString  &media() const { return media_; }
    BackendSession *session() const { return session_; }
    void            setPrepared(RTP::Description description) { prepared_ = std::move(description); }

private:
    friend class BackendSession;
    void detachPacketIo();
    void backendUnavailable();
    void invalidateSession();
    void drainOutgoing();

    BackendSession                  *session_ = nullptr;
    QString                          media_;
    std::optional<RTP::Description> prepared_;
    PacketWriter                     writer_;
    QMetaObject::Connection          readyReadConnection_;
    bool                             stopped_ = false;
};

class BackendSession final : public RTP::MediaSession {
public:
    BackendSession()
    {
        // Negotiation is intentionally device-independent. Capture gets enabled
        // only by AvCall after the Jingle session has been accepted.
        rtp_.setAudioInputDevice(QString());
        rtp_.setVideoInputDevice(QString());
        connect(&rtp_, &PsiMedia::RtpSession::started, this, [this] {
            if (state_ != State::Starting || !running_ || await_ != Await::Started)
                return;
            state_ = State::Running;
            finishRunning({});
        });
        connect(&rtp_, &PsiMedia::RtpSession::preferencesUpdated, this, [this] {
            if (state_ != State::Running || !running_ || await_ != Await::Preferences)
                return;
            finishRunning({});
        });
        connect(&rtp_, &PsiMedia::RtpSession::stopped, this, [this] {
            if (state_ == State::Stopped || state_ == State::Failed)
                return;
            const bool expected = state_ == State::Stopping;
            state_              = State::Stopped;
            deferred_.reset();
            revokeEndpoints();
            if (expected) {
                running_.reset();
                await_ = Await::None;
                return;
            }
            failCurrentAndCall(backendError(QStringLiteral("psimedia RTP session stopped unexpectedly")));
        });
        connect(&rtp_, &PsiMedia::RtpSession::error, this, [this] {
            // GstRtpSessionContext has already destroyed its live control before
            // this signal. Capture the provider error while its context still
            // carries lastStatus, then make the adapter terminal before any
            // completion/runtime callback can synchronously tear the call down.
            const auto code = rtp_.errorCode();
            const auto error
                = backendError(QStringLiteral("psimedia RTP session error (%1)").arg(int(code)));
            if (state_ == State::Failed || state_ == State::Stopped)
                return;
            state_ = State::Failed;
            deferred_.reset();
            revokeEndpoints();
            failCurrentAndCall(error);
        });
    }

    ~BackendSession() override
    {
        cancelAll();
        invalidateEndpoints();
        deferred_.reset();
        running_.reset();
        await_ = Await::None;

        // The real psimedia provider requires a live control for stop(). It has
        // no idempotent stop contract and destroys that control before error()/
        // stopped(). Disconnect first, and request stop only while our state
        // proves that the control created by start() is still live. The provider
        // context destructor performs its own synchronous cleanup afterwards.
        rtp_.disconnect(this);
        if (state_ == State::Starting || state_ == State::Running) {
            state_ = State::Stopping;
            rtp_.stop();
        }
    }

    std::unique_ptr<RTP::MediaEndpoint> createEndpoint(const QString &, const QString &media) override
    {
        if (media != QLatin1String("audio") && media != QLatin1String("video"))
            return {};
        return std::make_unique<Endpoint>(this, media);
    }

    PsiMedia::RtpChannel *channel(const QString &media)
    {
        if (state_ != State::Running)
            return nullptr;
        if (media == QLatin1String("audio"))
            return rtp_.audioRtpChannel();
        if (media == QLatin1String("video"))
            return rtp_.videoRtpChannel();
        return nullptr;
    }

    void pause(const QString &media)
    {
        if (state_ != State::Running)
            return;
        if (media == QLatin1String("audio"))
            rtp_.pauseAudio();
        else if (media == QLatin1String("video"))
            rtp_.pauseVideo();
    }

    void configurePolicy(const QString &audioOutputDevice, const QString &fileInput, bool loopFile,
                         int maximumSendingBitrate)
    {
        if (isTerminalOrStopping())
            return;
        if (!audioOutputDevice.isEmpty())
            rtp_.setAudioOutputDevice(audioOutputDevice);
        if (!fileInput.isEmpty()) {
            rtp_.setFileInput(fileInput);
            rtp_.setFileLoopEnabled(loopFile);
        }
        if (maximumSendingBitrate >= 0)
            rtp_.setMaximumSendingBitrate(maximumSendingBitrate);
    }

    void setVideoOutput(PsiMedia::VideoWidget *widget)
    {
        if (isTerminalOrStopping())
            return;
#ifdef QT_GUI_LIB
        rtp_.setVideoOutputWidget(widget);
#else
        Q_UNUSED(widget)
#endif
    }

    bool startTransmit(bool liveInput, bool audio, const QString &audioInputDevice, bool video,
                       const QString &videoInputDevice)
    {
        if (state_ != State::Running)
            return false;

        bool transmitting = false;
        if (liveInput) {
            rtp_.setAudioInputDevice(audio ? audioInputDevice : QString());
            rtp_.setVideoInputDevice(video ? videoInputDevice : QString());
        }

        if (audio && (liveInput ? !audioInputDevice.isEmpty() : rtp_.canTransmitAudio())) {
            rtp_.transmitAudio();
            transmitting = true;
        } else {
            rtp_.pauseAudio();
        }
        if (video && (liveInput ? !videoInputDevice.isEmpty() : rtp_.canTransmitVideo())) {
            rtp_.transmitVideo();
            transmitting = true;
        } else {
            rtp_.pauseVideo();
        }
        return transmitting;
    }

    void stopTransmit()
    {
        if (state_ != State::Running)
            return;
        rtp_.pauseAudio();
        rtp_.pauseVideo();
        rtp_.setAudioInputDevice(QString());
        rtp_.setVideoInputDevice(QString());
    }

    void registerEndpoint(Endpoint *endpoint) { endpoints_.insert(endpoint); }

    void unregisterEndpoint(Endpoint *endpoint)
    {
        endpoints_.remove(endpoint);
        if (deferred_ && deferred_->endpoint == endpoint)
            deferred_.reset();
        if (running_ && running_->endpoint == endpoint)
            running_->cancelled = true;
    }

protected:
    void beginPrepareLocalOffer(RTP::MediaOperation::Id id, RTP::MediaEndpoint *base,
                                PrepareCompletion completion) override
    {
        Pending operation;
        operation.id                = id;
        operation.kind              = Kind::PrepareOffer;
        operation.endpoint          = checkedEndpoint(base);
        operation.prepareCompletion = std::move(completion);
        submit(std::move(operation));
    }

    void beginPrepareAnswer(RTP::MediaOperation::Id id, RTP::MediaEndpoint *base, const RTP::Description &remote,
                            PrepareCompletion completion) override
    {
        Pending operation;
        operation.id                = id;
        operation.kind              = Kind::PrepareAnswer;
        operation.endpoint          = checkedEndpoint(base);
        operation.remote            = remote;
        operation.prepareCompletion = std::move(completion);
        submit(std::move(operation));
    }

    void beginApplyNegotiation(RTP::MediaOperation::Id id, RTP::MediaEndpoint *base, const RTP::Description &local,
                               const RTP::Description &remote, ApplyCompletion completion) override
    {
        Pending operation;
        operation.id              = id;
        operation.kind            = Kind::Apply;
        operation.endpoint        = checkedEndpoint(base);
        operation.local           = local;
        operation.remote          = remote;
        operation.applyCompletion = std::move(completion);
        submit(std::move(operation));
    }

    void cancelMediaOperation(RTP::MediaOperation::Id id) override
    {
        if (deferred_ && deferred_->id == id) {
            deferred_.reset();
            return;
        }
        if (running_ && running_->id == id)
            running_->cancelled = true;
    }

private:
    enum class Kind { PrepareOffer, PrepareAnswer, Apply };
    enum class Await { None, Started, Preferences };
    enum class State { Unstarted, Starting, Running, Stopping, Stopped, Failed };

    struct Pending {
        RTP::MediaOperation::Id         id = 0;
        Kind                            kind = Kind::PrepareOffer;
        Endpoint                       *endpoint = nullptr;
        std::optional<RTP::Description> local;
        std::optional<RTP::Description> remote;
        PrepareCompletion               prepareCompletion;
        ApplyCompletion                 applyCompletion;
        bool                            cancelled = false;
    };

    bool isTerminalOrStopping() const
    {
        return state_ == State::Stopping || state_ == State::Stopped || state_ == State::Failed;
    }

    Endpoint *checkedEndpoint(RTP::MediaEndpoint *base)
    {
        auto endpoint = dynamic_cast<Endpoint *>(base);
        return endpoint && endpoint->session() == this && endpoints_.contains(endpoint) ? endpoint : nullptr;
    }

    void submit(Pending operation)
    {
        if (!operation.endpoint || !endpoints_.contains(operation.endpoint)) {
            complete(std::move(operation), backendError(QStringLiteral("Invalid psimedia RTP endpoint")));
            return;
        }
        if (isTerminalOrStopping()) {
            complete(std::move(operation), backendError(QStringLiteral("psimedia RTP backend is no longer available")));
            return;
        }
        if (running_) {
            // A cancelled psimedia start/update cannot be interrupted. Iris has
            // already cancelled its operation, but this signal-without-id must be
            // drained before the next operation can be mapped safely.
            deferred_ = std::move(operation);
            return;
        }
        start(std::move(operation));
    }

    void start(Pending operation)
    {
        if (!operation.endpoint || !endpoints_.contains(operation.endpoint)) {
            complete(std::move(operation), backendError(QStringLiteral("Invalid psimedia RTP endpoint")));
            return;
        }
        if (isTerminalOrStopping()) {
            complete(std::move(operation), backendError(QStringLiteral("psimedia RTP backend is no longer available")));
            return;
        }

        bool changed = false;
        if (operation.kind == Kind::PrepareOffer || operation.kind == Kind::PrepareAnswer)
            changed |= enableLocal(operation.endpoint->media());

        if (operation.kind == Kind::PrepareAnswer || operation.kind == Kind::Apply) {
            if (!operation.remote) {
                complete(std::move(operation), unsupportedError(QStringLiteral("Missing remote RTP description")));
                return;
            }
            auto remote = toPsiPayloads(*operation.remote);
            if (!remote) {
                complete(std::move(operation), unsupportedError(QStringLiteral("Unsupported remote RTP payloads")));
                return;
            }
            changed |= setRemote(operation.endpoint->media(), *remote);
        }

        if (state_ == State::Unstarted) {
            running_ = std::move(operation);
            await_   = Await::Started;
            state_   = State::Starting;
            rtp_.start();
            return;
        }
        if (state_ != State::Running) {
            complete(std::move(operation), backendError(QStringLiteral("psimedia RTP backend is not runnable")));
            return;
        }
        if (changed) {
            running_ = std::move(operation);
            await_   = Await::Preferences;
            rtp_.updatePreferences();
            return;
        }
        complete(std::move(operation), {});
    }

    bool enableLocal(const QString &media)
    {
        if (media == QLatin1String("audio")) {
            if (audioEnabled_)
                return false;
            QList<PsiMedia::AudioParams> preferences;
            preferences.append(PsiMedia::AudioParams());
            rtp_.setLocalAudioPreferences(preferences);
            audioEnabled_ = true;
            return true;
        }
        if (media == QLatin1String("video")) {
            if (videoEnabled_)
                return false;
            QList<PsiMedia::VideoParams> preferences;
            preferences.append(PsiMedia::VideoParams());
            rtp_.setLocalVideoPreferences(preferences);
            videoEnabled_ = true;
            return true;
        }
        return false;
    }

    bool setRemote(const QString &media, const QList<PsiMedia::PayloadInfo> &payloads)
    {
        if (media == QLatin1String("audio")) {
            if (audioRemote_ == payloads)
                return false;
            audioRemote_ = payloads;
            rtp_.setRemoteAudioPreferences(payloads);
            return true;
        }
        if (media == QLatin1String("video")) {
            if (videoRemote_ == payloads)
                return false;
            videoRemote_ = payloads;
            rtp_.setRemoteVideoPreferences(payloads);
            return true;
        }
        return false;
    }

    std::optional<RTP::Description> preparedDescription(Endpoint *endpoint)
    {
        if (!endpoint || !endpoints_.contains(endpoint) || state_ != State::Running)
            return {};
        const auto payloads = endpoint->media() == QLatin1String("audio") ? rtp_.localAudioPayloadInfo()
                                                                          : rtp_.localVideoPayloadInfo();
        RTP::Description result;
        result.media   = endpoint->media();
        result.rtcpMux = true;
        for (const auto &payload : payloads) {
            auto converted = toRtpPayload(payload);
            if (!converted)
                return {};
            result.payloads.append(*converted);
        }
        if (result.payloads.isEmpty())
            return {};
        endpoint->setPrepared(result);
        return result;
    }

    void finishRunning(RTP::MediaError error)
    {
        if (!running_)
            return;
        auto operation = std::move(*running_);
        running_.reset();
        await_ = Await::None;
        if (operation.cancelled) {
            startDeferred();
            return;
        }
        complete(std::move(operation), std::move(error));
    }

    void complete(Pending operation, RTP::MediaError error)
    {
        QPointer<BackendSession> guard(this);
        if (operation.kind == Kind::Apply) {
            auto completion = std::move(operation.applyCompletion);
            if (completion)
                completion(std::move(error));
        } else {
            std::optional<RTP::Description> description;
            if (!error)
                description = preparedDescription(operation.endpoint);
            if (!error && !description)
                error = unsupportedError(QStringLiteral("psimedia produced no usable RTP payloads"));
            auto completion = std::move(operation.prepareCompletion);
            if (completion)
                completion(std::move(description), std::move(error));
        }
        if (guard)
            guard->startDeferred();
    }

    void startDeferred()
    {
        if (running_ || !deferred_)
            return;
        if (isTerminalOrStopping()) {
            deferred_.reset();
            return;
        }
        auto operation = std::move(*deferred_);
        deferred_.reset();
        start(std::move(operation));
    }

    void failCurrentAndCall(const RTP::MediaError &error)
    {
        const bool hadRunning = running_.has_value();
        QPointer<BackendSession> guard(this);
        if (hadRunning)
            finishRunning(error);
        if (!guard)
            return;

        if (!hadRunning) {
            emit runtimeError(error);
            return;
        }

        QTimer::singleShot(0, this, [guard, error] {
            if (guard)
                emit guard->runtimeError(error);
        });
    }

    void revokeEndpoints()
    {
        const auto endpoints = endpoints_;
        for (auto endpoint : endpoints) {
            if (endpoint)
                endpoint->backendUnavailable();
        }
    }

    void invalidateEndpoints()
    {
        const auto endpoints = endpoints_;
        endpoints_.clear();
        for (auto endpoint : endpoints) {
            if (endpoint)
                endpoint->invalidateSession();
        }
    }

    PsiMedia::RtpSession         rtp_;
    State                        state_ = State::Unstarted;
    bool                         audioEnabled_ = false;
    bool                         videoEnabled_ = false;
    QList<PsiMedia::PayloadInfo> audioRemote_;
    QList<PsiMedia::PayloadInfo> videoRemote_;
    Await                        await_ = Await::None;
    std::optional<Pending>       running_;
    std::optional<Pending>       deferred_;
    QSet<Endpoint *>             endpoints_;
};

Endpoint::Endpoint(BackendSession *session, QString media) : session_(session), media_(std::move(media))
{
    if (session_)
        session_->registerEndpoint(this);
}

Endpoint::~Endpoint()
{
    stop();
    if (session_)
        session_->unregisterEndpoint(this);
}

void Endpoint::detachPacketIo()
{
    QObject::disconnect(readyReadConnection_);
    readyReadConnection_ = {};
    writer_               = {};
}

void Endpoint::backendUnavailable()
{
    detachPacketIo();
    stopped_ = true;
}

void Endpoint::invalidateSession()
{
    backendUnavailable();
    session_ = nullptr;
}

bool Endpoint::attachPacketIo(PacketWriter writer)
{
    detachPacketIo();
    if (!session_ || stopped_ || !writer)
        return false;
    auto channel = session_->channel(media_);
    if (!channel)
        return false;
    writer_ = std::move(writer);
    readyReadConnection_
        = QObject::connect(channel, &PsiMedia::RtpChannel::readyRead, session_, [this] { drainOutgoing(); });
    drainOutgoing();
    return true;
}

void Endpoint::receivePacket(const QByteArray &data, RTP::SrtpContext::Packet kind)
{
    if (!session_ || stopped_)
        return;
    auto channel = session_->channel(media_);
    if (!channel)
        return;
    channel->write(PsiMedia::RtpPacket(data, kind == RTP::SrtpContext::Packet::Rtp ? 0 : 1));
}

void Endpoint::stop()
{
    detachPacketIo();
    if (stopped_)
        return;
    stopped_ = true;
    if (session_)
        session_->pause(media_);
}

void Endpoint::drainOutgoing()
{
    if (!session_ || stopped_ || !writer_)
        return;
    auto channel = session_->channel(media_);
    if (!channel)
        return;

    struct OutgoingPacket {
        QByteArray                data;
        RTP::SrtpContext::Packet kind;
    };
    QList<OutgoingPacket> batch;
    while (channel->packetsAvailable() > 0) {
        const auto packet = channel->read();
        if (packet.isNull())
            continue;
        if (packet.portOffset() == 0)
            batch.append({ packet.rawValue(), RTP::SrtpContext::Packet::Rtp });
        else if (packet.portOffset() == 1)
            batch.append({ packet.rawValue(), RTP::SrtpContext::Packet::Rtcp });
    }

    // PacketWriter is external code and may synchronously tear down the Jingle
    // application, this endpoint and its backend session. Keep everything used
    // after the first callback in local values only.
    const auto writer = writer_;
    for (auto &packet : batch) {
        if (!writer(std::move(packet.data), packet.kind))
            break;
    }
}

class Provider final : public RTP::MediaProvider {
public:
    std::unique_ptr<RTP::MediaSession> createSession() override { return std::make_unique<BackendSession>(); }
    QStringList mediaTypes() const override { return { QStringLiteral("audio"), QStringLiteral("video") }; }
};

BackendSession *backendSession(XMPP::Jingle::Session *session)
{
    if (!session)
        return nullptr;
    auto pad = qSharedPointerDynamicCast<RTP::Pad>(session->applicationPad(RTP::Description::ns()));
    return pad ? dynamic_cast<BackendSession *>(pad->mediaSession()) : nullptr;
}
}

std::shared_ptr<XMPP::Jingle::RTP::MediaProvider> makePsiMediaJingleProvider()
{
    return std::make_shared<Provider>();
}

bool configurePsiMediaJingleSession(XMPP::Jingle::Session *session, const QString &audioOutputDevice,
                                    const QString &fileInput, bool loopFile, int maximumSendingBitrate)
{
    auto backend = backendSession(session);
    if (!backend)
        return false;
    backend->configurePolicy(audioOutputDevice, fileInput, loopFile, maximumSendingBitrate);
    return true;
}

bool setPsiMediaJingleVideoOutput(XMPP::Jingle::Session *session, PsiMedia::VideoWidget *widget)
{
    auto backend = backendSession(session);
    if (!backend)
        return false;
    backend->setVideoOutput(widget);
    return true;
}

bool startPsiMediaJingleTransmit(XMPP::Jingle::Session *session, bool liveInput, bool audio,
                                 const QString &audioInputDevice, bool video, const QString &videoInputDevice)
{
    auto backend = backendSession(session);
    return backend && backend->startTransmit(liveInput, audio, audioInputDevice, video, videoInputDevice);
}

void stopPsiMediaJingleTransmit(XMPP::Jingle::Session *session)
{
    if (auto backend = backendSession(session))
        backend->stopTransmit();
}
