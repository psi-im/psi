#include "psimediajingle.h"

#include "psimedia.h"
#include "psimediaprovider.h"

#include <iris/jingle-rtp.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QPointer>
#include <QQueue>
#include <QTest>

#include <memory>
#include <optional>

namespace {
namespace RTP = XMPP::Jingle::RTP;

struct BackendStats {
    int startCalls         = 0;
    int updateCalls        = 0;
    int stopCalls          = 0;
    int pauseAudioCalls    = 0;
    int pauseVideoCalls    = 0;
    int transmitAudioCalls = 0;
    int transmitVideoCalls = 0;
    int invalidCalls       = 0;
    int cleanups           = 0;
};

class FakeRtpChannel final : public QObject, public PsiMedia::RtpChannelContext {
    Q_OBJECT
    Q_INTERFACES(PsiMedia::RtpChannelContext)
public:
    QObject *qobject() override { return this; }

    void setEnabled(bool enabled) override { enabled_ = enabled; }
    int  packetsAvailable() const override { return packets_.size(); }

    PsiMedia::PRtpPacket read() override
    {
        if (packets_.isEmpty())
            return {};
        return packets_.dequeue();
    }

    void write(const PsiMedia::PRtpPacket &packet) override
    {
        written_.enqueue(packet);
        emit packetsWritten(1);
    }

    void queueOutgoing(const PsiMedia::PRtpPacket &packet) { packets_.enqueue(packet); }
    void notifyReadyRead() { emit readyRead(); }

signals:
    void readyRead();
    void packetsWritten(int count);

private:
    bool                         enabled_ = false;
    QQueue<PsiMedia::PRtpPacket> packets_;
    QQueue<PsiMedia::PRtpPacket> written_;
};

class FakeRtpSessionContext final : public QObject, public PsiMedia::RtpSessionContext {
    Q_OBJECT
    Q_INTERFACES(PsiMedia::RtpSessionContext)
public:
    explicit FakeRtpSessionContext(BackendStats *stats) : stats_(stats) { }

    QObject *qobject() override { return this; }

    void setAudioOutputDevice(const QString &) override { }
    void setAudioInputDevice(const QString &) override { }
    void setVideoInputDevice(const QString &) override { }
    void setFileInput(const QString &) override { }
    void setFileDataInput(const QByteArray &) override { }
    void setFileLoopEnabled(bool) override { }
#ifdef QT_GUI_LIB
    void setVideoOutputWidget(PsiMedia::VideoWidgetContext *) override { }
    void setVideoPreviewWidget(PsiMedia::VideoWidgetContext *) override { }
#endif
    void setRecorder(QIODevice *) override { }
    void stopRecording() override { }
    void setLocalAudioPreferences(const QList<PsiMedia::PAudioParams> &) override { }
    void setLocalVideoPreferences(const QList<PsiMedia::PVideoParams> &) override { }
    void setMaximumSendingBitrate(int) override { }
    void setRemoteAudioPreferences(const QList<PsiMedia::PPayloadInfo> &) override { }
    void setRemoteVideoPreferences(const QList<PsiMedia::PPayloadInfo> &) override { }

    void start() override
    {
        if (controlAlive_) {
            ++stats_->invalidCalls;
            return;
        }
        controlAlive_ = true;
        ++stats_->startCalls;
    }

    void updatePreferences() override
    {
        if (!requireControl())
            return;
        ++stats_->updateCalls;
    }

    void transmitAudio() override
    {
        if (!requireControl())
            return;
        ++stats_->transmitAudioCalls;
    }

    void transmitVideo() override
    {
        if (!requireControl())
            return;
        ++stats_->transmitVideoCalls;
    }

    void pauseAudio() override
    {
        if (!requireControl())
            return;
        ++stats_->pauseAudioCalls;
    }

    void pauseVideo() override
    {
        if (!requireControl())
            return;
        ++stats_->pauseVideoCalls;
    }

    void stop() override
    {
        if (!requireControl())
            return;
        ++stats_->stopCalls;
    }

    QList<PsiMedia::PPayloadInfo> localAudioPayloadInfo() const override
    {
        PsiMedia::PPayloadInfo payload;
        payload.id        = 111;
        payload.name      = QStringLiteral("opus");
        payload.clockrate = 48000;
        payload.channels  = 2;
        return { payload };
    }

    QList<PsiMedia::PPayloadInfo> localVideoPayloadInfo() const override
    {
        PsiMedia::PPayloadInfo payload;
        payload.id        = 96;
        payload.name      = QStringLiteral("VP8");
        payload.clockrate = 90000;
        return { payload };
    }

    QList<PsiMedia::PPayloadInfo> remoteAudioPayloadInfo() const override { return {}; }
    QList<PsiMedia::PPayloadInfo> remoteVideoPayloadInfo() const override { return {}; }
    QList<PsiMedia::PAudioParams> audioParams() const override { return {}; }
    QList<PsiMedia::PVideoParams> videoParams() const override { return {}; }
    bool                          canTransmitAudio() const override { return controlAlive_; }
    bool                          canTransmitVideo() const override { return controlAlive_; }
    int                           outputVolume() const override { return 100; }
    void                          setOutputVolume(int) override { }
    int                           inputVolume() const override { return 100; }
    void                          setInputVolume(int) override { }
    Error                         errorCode() const override { return error_; }
    PsiMedia::RtpChannelContext  *audioRtpChannel() override { return &audio_; }
    PsiMedia::RtpChannelContext  *videoRtpChannel() override { return &video_; }
    void dumpPipeline(std::function<void(const QStringList &)> callback) override { callback({}); }

    FakeRtpChannel *audioChannel() { return &audio_; }

    void completeStart()
    {
        Q_ASSERT(controlAlive_);
        emit started();
    }

    void completePreferences()
    {
        Q_ASSERT(controlAlive_);
        emit preferencesUpdated();
    }

    void failAfterCleanup(Error code)
    {
        error_        = code;
        controlAlive_ = false;
        ++stats_->cleanups;
        emit error();
    }

    void stopAfterCleanup()
    {
        controlAlive_ = false;
        ++stats_->cleanups;
        emit stopped();
    }

signals:
    void started();
    void preferencesUpdated();
    void audioOutputIntensityChanged(int intensity);
    void audioInputIntensityChanged(int intensity);
    void stoppedRecording();
    void stopped();
    void finished();
    void error();

private:
    bool requireControl()
    {
        if (controlAlive_)
            return true;
        ++stats_->invalidCalls;
        return false;
    }

    BackendStats  *stats_        = nullptr;
    bool           controlAlive_ = false;
    Error          error_        = ErrorGeneric;
    FakeRtpChannel audio_;
    FakeRtpChannel video_;
};

class FakeProvider final : public QObject, public PsiMedia::Provider {
    Q_OBJECT
    Q_INTERFACES(PsiMedia::Provider)
public:
    QObject *qobject() override { return this; }
    bool     isInitialized() const override { return true; }
    QString  creditName() const override { return QStringLiteral("fake"); }
    QString  creditText() const override { return {}; }

    PsiMedia::FeaturesContext *createFeatures() override { return nullptr; }

    PsiMedia::RtpSessionContext *createRtpSession() override
    {
        auto context = new FakeRtpSessionContext(&stats_);
        context_     = context;
        return context;
    }

    PsiMedia::AudioRecorderContext *createAudioRecorder() override { return nullptr; }

    void                   resetStats() { stats_ = {}; }
    BackendStats          &stats() { return stats_; }
    FakeRtpSessionContext *context() const { return context_; }

signals:
    void initialized();

private:
    BackendStats                    stats_;
    QPointer<FakeRtpSessionContext> context_;
};

RTP::Description audioDescription()
{
    RTP::Description description;
    description.media   = QStringLiteral("audio");
    description.rtcpMux = true;
    RTP::PayloadType payload;
    payload.id        = 111;
    payload.name      = QStringLiteral("opus");
    payload.clockrate = 48000;
    payload.channels  = 2;
    description.payloads.append(payload);
    return description;
}

PsiMediaJingleCapabilities fullCapabilities()
{
    PsiMediaJingleCapabilities result;
    result.backendAvailable = true;
    result.probeComplete    = true;
    result.secureRtp        = true;
    result.audio            = true;
    result.video            = true;
    result.audioInput       = true;
    result.audioOutput      = true;
    result.videoInput       = true;
    return result;
}

RTP::MediaOperationPolicy shortOperationPolicy()
{
    RTP::MediaOperationPolicy policy;
    policy.prepareDeadlineMs    = 5;
    policy.applyDeadlineMs      = 5;
    policy.maxPendingOperations = 4;
    return policy;
}

struct Harness {
    std::shared_ptr<RTP::MediaProvider> provider = makePsiMediaJingleProvider(fullCapabilities());
    std::unique_ptr<RTP::MediaSession>  session  = provider->createSession();
    std::unique_ptr<RTP::MediaEndpoint> endpoint
        = session->createEndpoint(QStringLiteral("audio"), QStringLiteral("audio"));
};
}

class BackendLifecycleTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { PsiMedia::setProvider(&provider_); }

    void init()
    {
        QVERIFY(provider_.context() == nullptr);
        provider_.resetStats();
    }

    void capabilitySnapshotLimitsMediaTypes()
    {
        auto audioOnly     = fullCapabilities();
        audioOnly.video    = false;
        auto audioProvider = makePsiMediaJingleProvider(audioOnly);
        QCOMPARE(audioProvider->mediaTypes(), QStringList { QStringLiteral("audio") });

        auto receiveOnlyVideo       = fullCapabilities();
        receiveOnlyVideo.audio      = false;
        receiveOnlyVideo.videoInput = false;
        auto videoProvider          = makePsiMediaJingleProvider(receiveOnlyVideo);
        QCOMPARE(videoProvider->mediaTypes(), QStringList { QStringLiteral("video") });

        auto unavailable             = fullCapabilities();
        unavailable.backendAvailable = false;
        QVERIFY(makePsiMediaJingleProvider(unavailable)->mediaTypes().isEmpty());
        unavailable               = fullCapabilities();
        unavailable.probeComplete = false;
        QVERIFY(makePsiMediaJingleProvider(unavailable)->mediaTypes().isEmpty());
        unavailable           = fullCapabilities();
        unavailable.secureRtp = false;
        QVERIFY(makePsiMediaJingleProvider(unavailable)->mediaTypes().isEmpty());

        auto disabled         = fullCapabilities();
        disabled.audio        = false;
        disabled.video        = false;
        auto disabledProvider = makePsiMediaJingleProvider(disabled);
        QVERIFY(disabledProvider->mediaTypes().isEmpty());
        QVERIFY(!disabledProvider->createSession());
    }

    void endpointReservationTracksLifetime()
    {
        auto provider = makePsiMediaJingleProvider(fullCapabilities());
        auto session  = provider->createSession();
        QVERIFY(session);

        auto audio = session->createEndpoint(QStringLiteral("audio-1"), QStringLiteral("audio"));
        QVERIFY(audio);
        QVERIFY(!session->createEndpoint(QStringLiteral("audio-2"), QStringLiteral("audio")));

        auto video = session->createEndpoint(QStringLiteral("video-1"), QStringLiteral("video"));
        QVERIFY(video);
        QVERIFY(!session->createEndpoint(QStringLiteral("video-2"), QStringLiteral("video")));

        audio->stop();
        QVERIFY(!session->createEndpoint(QStringLiteral("audio-after-stop"), QStringLiteral("audio")));

        audio.reset();
        audio = session->createEndpoint(QStringLiteral("audio-recreated"), QStringLiteral("audio"));
        QVERIFY(audio);

        video.reset();
        video = session->createEndpoint(QStringLiteral("video-recreated"), QStringLiteral("video"));
        QVERIFY(video);

        audio.reset();
        video.reset();
        session.reset();
        QCOMPARE(provider_.stats().startCalls, 0);
        QCOMPARE(provider_.stats().invalidCalls, 0);
    }

    void silentBackendTimeoutIsFailClosed()
    {
        Harness harness;
        QVERIFY(harness.session->setOperationPolicy(shortOperationPolicy()));
        QStringList     events;
        int             callbacks     = 0;
        int             runtimeErrors = 0;
        RTP::MediaError operationError;
        RTP::MediaError runtimeError;
        connect(harness.session.get(), &RTP::MediaSession::runtimeError, this, [&](const RTP::MediaError &error) {
            events.append(QStringLiteral("runtime"));
            runtimeError = error;
            ++runtimeErrors;
        });

        auto operation = harness.session->prepareLocalOffer(
            harness.endpoint.get(),
            [&](RTP::MediaOperation::Id, std::optional<RTP::Description> result, RTP::MediaError error) {
                QVERIFY(!result);
                events.append(QStringLiteral("operation"));
                operationError = error;
                ++callbacks;
            });
        QVERIFY(operation);
        QTRY_COMPARE(provider_.stats().startCalls, 1);
        auto context = QPointer<FakeRtpSessionContext>(provider_.context());
        QVERIFY(context);

        QTRY_COMPARE(callbacks, 1);
        QCOMPARE(operationError.code, RTP::MediaError::Code::Timeout);
        QTRY_COMPARE(runtimeErrors, 1);
        QCOMPARE(runtimeError.code, RTP::MediaError::Code::Timeout);
        QCOMPARE(events, (QStringList { QStringLiteral("operation"), QStringLiteral("runtime") }));

        // The provider may still deliver the signal which missed the deadline.
        // It must not revive this backend or become a completion for newer work.
        context->completeStart();
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCOMPARE(callbacks, 1);
        QCOMPARE(runtimeErrors, 1);
        QCOMPARE(provider_.stats().updateCalls, 0);

        harness.endpoint->stop();
        operation.reset();
        harness.endpoint.reset();
        harness.session.reset();
        QCOMPARE(provider_.stats().pauseAudioCalls, 0);
        QCOMPARE(provider_.stats().stopCalls, 0);
        QCOMPARE(provider_.stats().invalidCalls, 0);
    }

    void cancelledAudioDrainsBeforeQueuedVideo()
    {
        auto provider = makePsiMediaJingleProvider(fullCapabilities());
        auto session  = provider->createSession();
        auto audio    = session->createEndpoint(QStringLiteral("audio"), QStringLiteral("audio"));
        auto video    = session->createEndpoint(QStringLiteral("video"), QStringLiteral("video"));
        QVERIFY(audio);
        QVERIFY(video);

        int  audioCallbacks = 0;
        auto audioOperation = session->prepareLocalOffer(
            audio.get(),
            [&](RTP::MediaOperation::Id, std::optional<RTP::Description>, RTP::MediaError) { ++audioCallbacks; });
        QTRY_COMPARE(provider_.stats().startCalls, 1);
        audioOperation->cancel();

        bool videoPrepared  = false;
        auto videoOperation = session->prepareLocalOffer(
            video.get(),
            [&](RTP::MediaOperation::Id, std::optional<RTP::Description> description, RTP::MediaError error) {
                QVERIFY(!error);
                QVERIFY(description);
                QCOMPARE(description->media, QStringLiteral("video"));
                videoPrepared = true;
            });
        QVERIFY(videoOperation);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QVERIFY(!videoPrepared);
        QCOMPARE(provider_.stats().updateCalls, 0);

        provider_.context()->completeStart();
        QTRY_COMPARE(provider_.stats().updateCalls, 1);
        provider_.context()->completePreferences();
        QTRY_VERIFY(videoPrepared);
        QCOMPARE(audioCallbacks, 0);

        audio.reset();
        video.reset();
        videoOperation.reset();
        session.reset();
        QCOMPARE(provider_.stats().invalidCalls, 0);
    }

    void runtimeErrorWhileDrainingFailsDeferred()
    {
        auto provider = makePsiMediaJingleProvider(fullCapabilities());
        auto session  = provider->createSession();
        auto audio    = session->createEndpoint(QStringLiteral("audio"), QStringLiteral("audio"));
        auto video    = session->createEndpoint(QStringLiteral("video"), QStringLiteral("video"));
        QVERIFY(audio);
        QVERIFY(video);

        int  audioCallbacks = 0;
        auto audioOperation = session->prepareLocalOffer(
            audio.get(),
            [&](RTP::MediaOperation::Id, std::optional<RTP::Description>, RTP::MediaError) { ++audioCallbacks; });
        QTRY_COMPARE(provider_.stats().startCalls, 1);
        audioOperation->cancel();

        QStringList     events;
        int             videoCallbacks = 0;
        int             runtimeErrors  = 0;
        RTP::MediaError videoError;
        auto            videoOperation = session->prepareLocalOffer(
            video.get(),
            [&](RTP::MediaOperation::Id, std::optional<RTP::Description> description, RTP::MediaError error) {
                QVERIFY(!description);
                events.append(QStringLiteral("video"));
                videoError = error;
                ++videoCallbacks;
            });
        connect(session.get(), &RTP::MediaSession::runtimeError, this, [&](const RTP::MediaError &) {
            events.append(QStringLiteral("runtime"));
            ++runtimeErrors;
        });
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QVERIFY(!videoCallbacks);

        provider_.context()->failAfterCleanup(PsiMedia::RtpSessionContext::ErrorSystem);
        QTRY_COMPARE(videoCallbacks, 1);
        QCOMPARE(videoError.code, RTP::MediaError::Code::Backend);
        QTRY_COMPARE(runtimeErrors, 1);
        QCOMPARE(events, (QStringList { QStringLiteral("video"), QStringLiteral("runtime") }));
        QCOMPARE(audioCallbacks, 0);

        audio.reset();
        video.reset();
        videoOperation.reset();
        session.reset();
        QCOMPARE(provider_.stats().stopCalls, 0);
        QCOMPARE(provider_.stats().invalidCalls, 0);
    }

    void destroyBeforeStart()
    {
        {
            Harness harness;
            QVERIFY(harness.endpoint);
            QVERIFY(provider_.context());
        }
        QCOMPARE(provider_.stats().startCalls, 0);
        QCOMPARE(provider_.stats().stopCalls, 0);
        QCOMPARE(provider_.stats().invalidCalls, 0);
    }

    void cancelDuringStarting()
    {
        Harness harness;
        bool    callbackCalled = false;
        auto    operation      = harness.session->prepareLocalOffer(
            harness.endpoint.get(),
            [&](RTP::MediaOperation::Id, std::optional<RTP::Description>, RTP::MediaError) { callbackCalled = true; });
        QTRY_COMPARE(provider_.stats().startCalls, 1);

        operation->cancel();
        operation.reset();
        harness.endpoint.reset();
        harness.session.reset();

        QVERIFY(!callbackCalled);
        QCOMPARE(provider_.stats().stopCalls, 1);
        QCOMPARE(provider_.stats().invalidCalls, 0);
    }

    void startErrorCleansUpBeforeSignal()
    {
        Harness         harness;
        bool            callbackCalled = false;
        RTP::MediaError operationError;
        int             runtimeErrors = 0;
        connect(harness.session.get(), &RTP::MediaSession::runtimeError, this,
                [&](const RTP::MediaError &) { ++runtimeErrors; });

        auto operation = harness.session->prepareLocalOffer(
            harness.endpoint.get(),
            [&](RTP::MediaOperation::Id, std::optional<RTP::Description>, RTP::MediaError error) {
                callbackCalled = true;
                operationError = std::move(error);
            });
        QTRY_COMPARE(provider_.stats().startCalls, 1);
        auto context = QPointer<FakeRtpSessionContext>(provider_.context());
        QVERIFY(context);
        context->failAfterCleanup(PsiMedia::RtpSessionContext::ErrorCodec);

        QTRY_VERIFY(callbackCalled);
        QTRY_COMPARE(runtimeErrors, 1);
        QCOMPARE(operationError.code, RTP::MediaError::Code::Backend);

        harness.endpoint->stop();
        harness.endpoint->stop();
        operation.reset();
        harness.endpoint.reset();
        harness.session.reset();

        QCOMPARE(provider_.stats().stopCalls, 0);
        QCOMPARE(provider_.stats().pauseAudioCalls, 0);
        QCOMPARE(provider_.stats().invalidCalls, 0);
    }

    void applyErrorDoesNotResumeBackend()
    {
        Harness                         harness;
        std::optional<RTP::Description> local;
        bool                            prepared = false;
        auto                            prepare  = harness.session->prepareLocalOffer(
            harness.endpoint.get(),
            [&](RTP::MediaOperation::Id, std::optional<RTP::Description> description, RTP::MediaError error) {
                QVERIFY(!error);
                local    = std::move(description);
                prepared = true;
            });
        QTRY_COMPARE(provider_.stats().startCalls, 1);
        provider_.context()->completeStart();
        QTRY_VERIFY(prepared);
        QVERIFY(local);
        prepare.reset();

        bool            applied = false;
        RTP::MediaError applyError;
        int             runtimeErrors = 0;
        connect(harness.session.get(), &RTP::MediaSession::runtimeError, this,
                [&](const RTP::MediaError &) { ++runtimeErrors; });
        auto apply = harness.session->applyNegotiation(harness.endpoint.get(), *local, audioDescription(),
                                                       [&](RTP::MediaOperation::Id, RTP::MediaError error) {
                                                           applied    = true;
                                                           applyError = std::move(error);
                                                       });
        QTRY_COMPARE(provider_.stats().updateCalls, 1);
        provider_.context()->failAfterCleanup(PsiMedia::RtpSessionContext::ErrorSystem);

        QTRY_VERIFY(applied);
        QTRY_COMPARE(runtimeErrors, 1);
        QCOMPARE(applyError.code, RTP::MediaError::Code::Backend);
        apply.reset();
        harness.endpoint.reset();
        harness.session.reset();

        QCOMPARE(provider_.stats().updateCalls, 1);
        QCOMPARE(provider_.stats().stopCalls, 0);
        QCOMPARE(provider_.stats().invalidCalls, 0);
    }

    void applyRejectsLocalPayloadTypeNotCommittedByBackend()
    {
        Harness                         harness;
        std::optional<RTP::Description> local;
        bool                            prepared = false;
        auto                            prepare  = harness.session->prepareLocalOffer(
            harness.endpoint.get(),
            [&](RTP::MediaOperation::Id, std::optional<RTP::Description> description, RTP::MediaError error) {
                QVERIFY(!error);
                local    = std::move(description);
                prepared = true;
            });
        QTRY_COMPARE(provider_.stats().startCalls, 1);
        provider_.context()->completeStart();
        QTRY_VERIFY(prepared);
        QVERIFY(local);
        prepare.reset();

        auto accepted           = *local;
        accepted.payloads[0].id = 112;

        bool            applied = false;
        RTP::MediaError applyError;
        auto            apply = harness.session->applyNegotiation(harness.endpoint.get(), accepted, audioDescription(),
                                                                  [&](RTP::MediaOperation::Id, RTP::MediaError error) {
                                                           applied    = true;
                                                           applyError = std::move(error);
                                                       });
        QTRY_COMPARE(provider_.stats().updateCalls, 1);
        provider_.context()->completePreferences();
        QTRY_VERIFY(applied);
        QCOMPARE(applyError.code, RTP::MediaError::Code::Unsupported);

        apply.reset();
        harness.endpoint.reset();
        harness.session.reset();
        QCOMPARE(provider_.stats().invalidCalls, 0);
    }

    void applyRejectsLocalFmtpNotConfirmedByBackend()
    {
        Harness                         harness;
        std::optional<RTP::Description> local;
        bool                            prepared = false;
        auto                            prepare  = harness.session->prepareLocalOffer(
            harness.endpoint.get(),
            [&](RTP::MediaOperation::Id, std::optional<RTP::Description> description, RTP::MediaError error) {
                QVERIFY(!error);
                local    = std::move(description);
                prepared = true;
            });
        QTRY_COMPARE(provider_.stats().startCalls, 1);
        provider_.context()->completeStart();
        QTRY_VERIFY(prepared);
        QVERIFY(local);
        prepare.reset();

        auto accepted = *local;
        accepted.payloads[0].parameters.insert(QStringLiteral("stereo"), QStringLiteral("1"));

        bool            applied = false;
        RTP::MediaError applyError;
        auto            apply = harness.session->applyNegotiation(harness.endpoint.get(), accepted, audioDescription(),
                                                                  [&](RTP::MediaOperation::Id, RTP::MediaError error) {
                                                           applied    = true;
                                                           applyError = std::move(error);
                                                       });
        QTRY_COMPARE(provider_.stats().updateCalls, 1);
        provider_.context()->completePreferences();
        QTRY_VERIFY(applied);
        QCOMPARE(applyError.code, RTP::MediaError::Code::Unsupported);

        apply.reset();
        harness.endpoint.reset();
        harness.session.reset();
        QCOMPARE(provider_.stats().invalidCalls, 0);
    }

    void activeErrorAfterCleanupIsTerminal()
    {
        Harness                         harness;
        std::optional<RTP::Description> local;
        bool                            prepared = false;
        auto                            prepare  = harness.session->prepareLocalOffer(
            harness.endpoint.get(),
            [&](RTP::MediaOperation::Id, std::optional<RTP::Description> description, RTP::MediaError error) {
                QVERIFY(!error);
                local    = std::move(description);
                prepared = true;
            });
        QTRY_COMPARE(provider_.stats().startCalls, 1);
        provider_.context()->completeStart();
        QTRY_VERIFY(prepared);
        QVERIFY(local);
        prepare.reset();

        bool applied = false;
        auto apply   = harness.session->applyNegotiation(harness.endpoint.get(), *local, audioDescription(),
                                                         [&](RTP::MediaOperation::Id, RTP::MediaError error) {
                                                           QVERIFY(!error);
                                                           applied = true;
                                                       });
        QTRY_COMPARE(provider_.stats().updateCalls, 1);
        provider_.context()->completePreferences();
        QTRY_VERIFY(applied);
        apply.reset();

        int runtimeErrors = 0;
        connect(harness.session.get(), &RTP::MediaSession::runtimeError, this,
                [&](const RTP::MediaError &) { ++runtimeErrors; });
        provider_.context()->failAfterCleanup(PsiMedia::RtpSessionContext::ErrorCodec);
        QTRY_COMPARE(runtimeErrors, 1);

        harness.endpoint->stop();
        harness.endpoint.reset();
        harness.session.reset();
        QCOMPARE(provider_.stats().pauseAudioCalls, 0);
        QCOMPARE(provider_.stats().stopCalls, 0);
        QCOMPARE(provider_.stats().invalidCalls, 0);
    }

    void packetWriterMayDeleteBackendSynchronously()
    {
        Harness harness;
        bool    prepared  = false;
        auto    operation = harness.session->prepareLocalOffer(
            harness.endpoint.get(),
            [&](RTP::MediaOperation::Id, std::optional<RTP::Description>, RTP::MediaError error) {
                QVERIFY(!error);
                prepared = true;
            });
        QTRY_COMPARE(provider_.stats().startCalls, 1);
        provider_.context()->completeStart();
        QTRY_VERIFY(prepared);
        operation.reset();

        auto context = QPointer<FakeRtpSessionContext>(provider_.context());
        QVERIFY(context);
        PsiMedia::PRtpPacket first;
        first.rawValue = QByteArray(12, 'a');
        first.type     = PsiMedia::PRtpPacket::Type::Rtp;
        PsiMedia::PRtpPacket second;
        second.rawValue = QByteArray(12, 'b');
        second.type     = PsiMedia::PRtpPacket::Type::Rtcp;
        context->audioChannel()->queueOutgoing(first);
        context->audioChannel()->queueOutgoing(second);

        int writes = 0;
        QVERIFY(harness.endpoint->attachPacketIo([&](QByteArray, RTP::SrtpContext::Packet) {
            ++writes;
            harness.session.reset();
            return false;
        }));

        QCOMPARE(writes, 1);
        QVERIFY(!harness.session);
        QVERIFY(!context);
        harness.endpoint.reset();
        QCOMPARE(provider_.stats().stopCalls, 1);
        QCOMPARE(provider_.stats().invalidCalls, 0);
    }

    void repeatedEndpointStopIsIdempotent()
    {
        Harness harness;
        bool    prepared  = false;
        auto    operation = harness.session->prepareLocalOffer(
            harness.endpoint.get(),
            [&](RTP::MediaOperation::Id, std::optional<RTP::Description>, RTP::MediaError error) {
                QVERIFY(!error);
                prepared = true;
            });
        QTRY_COMPARE(provider_.stats().startCalls, 1);
        provider_.context()->completeStart();
        QTRY_VERIFY(prepared);
        operation.reset();

        harness.endpoint->stop();
        harness.endpoint->stop();
        QCOMPARE(provider_.stats().pauseAudioCalls, 1);
        QCOMPARE(provider_.stats().invalidCalls, 0);

        harness.endpoint.reset();
        harness.session.reset();
        QCOMPARE(provider_.stats().stopCalls, 1);
        QCOMPARE(provider_.stats().invalidCalls, 0);
    }

    void providerStoppedAfterCleanupIsTerminal()
    {
        Harness harness;
        bool    prepared  = false;
        auto    operation = harness.session->prepareLocalOffer(
            harness.endpoint.get(),
            [&](RTP::MediaOperation::Id, std::optional<RTP::Description>, RTP::MediaError error) {
                QVERIFY(!error);
                prepared = true;
            });
        QTRY_COMPARE(provider_.stats().startCalls, 1);
        provider_.context()->completeStart();
        QTRY_VERIFY(prepared);
        operation.reset();

        int runtimeErrors = 0;
        connect(harness.session.get(), &RTP::MediaSession::runtimeError, this,
                [&](const RTP::MediaError &) { ++runtimeErrors; });
        provider_.context()->stopAfterCleanup();
        QTRY_COMPARE(runtimeErrors, 1);

        harness.endpoint->stop();
        harness.endpoint.reset();
        harness.session.reset();
        QCOMPARE(provider_.stats().pauseAudioCalls, 0);
        QCOMPARE(provider_.stats().stopCalls, 0);
        QCOMPARE(provider_.stats().invalidCalls, 0);
    }

    void runtimeErrorMayDeleteBackendSynchronously()
    {
        Harness harness;
        bool    prepared  = false;
        auto    operation = harness.session->prepareLocalOffer(
            harness.endpoint.get(),
            [&](RTP::MediaOperation::Id, std::optional<RTP::Description>, RTP::MediaError error) {
                QVERIFY(!error);
                prepared = true;
            });
        QTRY_COMPARE(provider_.stats().startCalls, 1);
        provider_.context()->completeStart();
        QTRY_VERIFY(prepared);
        operation.reset();

        auto context = QPointer<FakeRtpSessionContext>(provider_.context());
        connect(harness.session.get(), &RTP::MediaSession::runtimeError, this,
                [&](const RTP::MediaError &) { harness.session.reset(); });
        context->failAfterCleanup(PsiMedia::RtpSessionContext::ErrorGeneric);

        QVERIFY(!harness.session);
        QVERIFY(!context);
        harness.endpoint->stop();
        harness.endpoint.reset();
        QCOMPARE(provider_.stats().stopCalls, 0);
        QCOMPARE(provider_.stats().invalidCalls, 0);
    }

private:
    FakeProvider provider_;
};

QTEST_GUILESS_MAIN(BackendLifecycleTest)

#include "tst_backend_lifecycle.moc"
