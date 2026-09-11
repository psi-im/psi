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

#include <QMetaObject>
#include <QPointer>
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

class Session;

class Endpoint final : public RTP::MediaEndpoint {
public:
    Endpoint(Session *session, QString media);
    ~Endpoint() override { stop(); }

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

    const QString &media() const { return media_; }
    Session       *session() const { return session_; }
    void           setPrepared(RTP::Description description) { prepared_ = std::move(description); }

private:
    void drainOutgoing();

    Session                        *session_ = nullptr;
    QString                         media_;
    std::optional<RTP::Description> prepared_;
    PacketWriter                    writer_;
    QMetaObject::Connection         readyReadConnection_;
};

class Session final : public RTP::MediaSession {
public:
    Session()
    {
        rtp_.setAudioInputDevice(QString());
        rtp_.setVideoInputDevice(QString());
        connect(&rtp_, &PsiMedia::RtpSession::started, this, [this] {
            if (!running_ || await_ != Await::Started)
                return;
            started_ = true;
            finishRunning({});
        });
        connect(&rtp_, &PsiMedia::RtpSession::preferencesUpdated, this, [this] {
            if (!running_ || await_ != Await::Preferences)
                return;
            finishRunning({});
        });
        connect(&rtp_, &PsiMedia::RtpSession::error, this, [this] {
            const auto error = backendError(QStringLiteral("psimedia RTP session error (%1)").arg(int(rtp_.errorCode())));
            if (!running_) {
                emit runtimeError(error);
                return;
            }

            // RtpSession::error() is call-fatal in the existing Psi call path.
            // Complete the current Iris operation first so an initial incoming
            // prepare can become ContentReject rather than a generic remove.
            // Then fail the rest of the call on the next event-loop turn.
            deferred_.reset();
            finishRunning(error);
            QPointer<Session> guard(this);
            QTimer::singleShot(0, this, [guard, error] {
                if (guard)
                    emit guard->runtimeError(error);
            });
        });
    }

    ~Session() override
    {
        rtp_.disconnect(this);
        rtp_.stop();
    }

    std::unique_ptr<RTP::MediaEndpoint> createEndpoint(const QString &, const QString &media) override
    {
        if (media != QLatin1String("audio") && media != QLatin1String("video"))
            return {};
        return std::make_unique<Endpoint>(this, media);
    }

    PsiMedia::RtpChannel *channel(const QString &media)
    {
        if (media == QLatin1String("audio"))
            return rtp_.audioRtpChannel();
        if (media == QLatin1String("video"))
            return rtp_.videoRtpChannel();
        return nullptr;
    }

    void pause(const QString &media)
    {
        if (!started_)
            return;
        if (media == QLatin1String("audio"))
            rtp_.pauseAudio();
        else if (media == QLatin1String("video"))
            rtp_.pauseVideo();
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

    void beginPrepareAnswer(RTP::MediaOperation::Id id, RTP::MediaEndpoint *base,
                            const RTP::Description &remote, PrepareCompletion completion) override
    {
        Pending operation;
        operation.id                = id;
        operation.kind              = Kind::PrepareAnswer;
        operation.endpoint          = checkedEndpoint(base);
        operation.remote            = remote;
        operation.prepareCompletion = std::move(completion);
        submit(std::move(operation));
    }

    void beginApplyNegotiation(RTP::MediaOperation::Id id, RTP::MediaEndpoint *base,
                               const RTP::Description &local, const RTP::Description &remote,
                               ApplyCompletion completion) override
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

    Endpoint *checkedEndpoint(RTP::MediaEndpoint *base)
    {
        auto endpoint = dynamic_cast<Endpoint *>(base);
        return endpoint && endpoint->session() == this ? endpoint : nullptr;
    }

    void submit(Pending operation)
    {
        if (!operation.endpoint) {
            complete(std::move(operation), backendError(QStringLiteral("Invalid psimedia RTP endpoint")));
            return;
        }
        if (running_) {
            // A cancelled psimedia start/update cannot be interrupted. The Iris
            // operation is already cancelled, but its backend signal still has to
            // be drained before another operation can be mapped to signal-without-id.
            deferred_ = std::move(operation);
            return;
        }
        start(std::move(operation));
    }

    void start(Pending operation)
    {
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

        if (!started_) {
            running_ = std::move(operation);
            await_   = Await::Started;
            rtp_.start();
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
        startDeferred();
    }

    void startDeferred()
    {
        if (running_ || !deferred_)
            return;
        auto operation = std::move(*deferred_);
        deferred_.reset();
        start(std::move(operation));
    }

    PsiMedia::RtpSession         rtp_;
    bool                         started_ = false;
    bool                         audioEnabled_ = false;
    bool                         videoEnabled_ = false;
    QList<PsiMedia::PayloadInfo> audioRemote_;
    QList<PsiMedia::PayloadInfo> videoRemote_;
    Await                        await_ = Await::None;
    std::optional<Pending>       running_;
    std::optional<Pending>       deferred_;
};

Endpoint::Endpoint(Session *session, QString media) : session_(session), media_(std::move(media)) { }

bool Endpoint::attachPacketIo(PacketWriter writer)
{
    stop();
    if (!session_ || !writer)
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
    if (!session_)
        return;
    auto channel = session_->channel(media_);
    if (!channel)
        return;
    channel->write(PsiMedia::RtpPacket(data, kind == RTP::SrtpContext::Packet::Rtp ? 0 : 1));
}

void Endpoint::stop()
{
    QObject::disconnect(readyReadConnection_);
    readyReadConnection_ = {};
    writer_               = {};
    if (session_)
        session_->pause(media_);
}

void Endpoint::drainOutgoing()
{
    if (!session_ || !writer_)
        return;
    auto channel = session_->channel(media_);
    if (!channel)
        return;
    while (channel->packetsAvailable() > 0) {
        const auto packet = channel->read();
        if (packet.isNull())
            continue;
        if (packet.portOffset() == 0)
            writer_(packet.rawValue(), RTP::SrtpContext::Packet::Rtp);
        else if (packet.portOffset() == 1)
            writer_(packet.rawValue(), RTP::SrtpContext::Packet::Rtcp);
    }
}

class Provider final : public RTP::MediaProvider {
public:
    std::unique_ptr<RTP::MediaSession> createSession() override { return std::make_unique<Session>(); }
};
}

std::shared_ptr<XMPP::Jingle::RTP::MediaProvider> makePsiMediaJingleProvider()
{
    return std::make_shared<Provider>();
}
