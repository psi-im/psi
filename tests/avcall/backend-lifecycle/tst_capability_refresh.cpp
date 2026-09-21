#include "psimediajinglecapabilitytransaction.h"

#include "psimedia.h"
#include "psimediaprovider.h"

#include <iris/jingle-ice.h>
#include <iris/jingle-rtp.h>
#include <iris/jingle.h>
#include <iris/xmpp_client.h>

#include <QCryptographicHash>
#include <QTest>
#include <QtCrypto>

#include <memory>

namespace {
namespace Jingle = XMPP::Jingle;
namespace RTP    = XMPP::Jingle::RTP;

const QString GenericRtp = RTP::Description::ns();
const QString AudioRtp   = QStringLiteral("urn:xmpp:jingle:apps:rtp:audio");
const QString VideoRtp   = QStringLiteral("urn:xmpp:jingle:apps:rtp:video");
const QString Baseline   = QStringLiteral("urn:psi:test:baseline");

class CapabilityRtpSessionContext final : public QObject,
                                          public PsiMedia::RtpSessionContext,
                                          public PsiMedia::SecureRtpSessionContext {
    Q_OBJECT
    Q_INTERFACES(PsiMedia::RtpSessionContext PsiMedia::SecureRtpSessionContext)
public:
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
    void                          setRecorder(QIODevice *) override { }
    void                          stopRecording() override { }
    void                          setLocalAudioPreferences(const QList<PsiMedia::PAudioParams> &) override { }
    void                          setLocalVideoPreferences(const QList<PsiMedia::PVideoParams> &) override { }
    void                          setMaximumSendingBitrate(int) override { }
    void                          setRemoteAudioPreferences(const QList<PsiMedia::PPayloadInfo> &) override { }
    void                          setRemoteVideoPreferences(const QList<PsiMedia::PPayloadInfo> &) override { }
    void                          start() override { }
    void                          updatePreferences() override { }
    void                          transmitAudio() override { }
    void                          transmitVideo() override { }
    void                          pauseAudio() override { }
    void                          pauseVideo() override { }
    void                          stop() override { }
    QList<PsiMedia::PPayloadInfo> localAudioPayloadInfo() const override { return {}; }
    QList<PsiMedia::PPayloadInfo> localVideoPayloadInfo() const override { return {}; }
    QList<PsiMedia::PPayloadInfo> remoteAudioPayloadInfo() const override { return {}; }
    QList<PsiMedia::PPayloadInfo> remoteVideoPayloadInfo() const override { return {}; }
    QList<PsiMedia::PAudioParams> audioParams() const override { return {}; }
    QList<PsiMedia::PVideoParams> videoParams() const override { return {}; }
    bool                          canTransmitAudio() const override { return false; }
    bool                          canTransmitVideo() const override { return false; }
    int                           outputVolume() const override { return 100; }
    void                          setOutputVolume(int) override { }
    int                           inputVolume() const override { return 100; }
    void                          setInputVolume(int) override { }
    Error                         errorCode() const override { return ErrorGeneric; }
    PsiMedia::RtpChannelContext  *audioRtpChannel() override { return nullptr; }
    PsiMedia::RtpChannelContext  *videoRtpChannel() override { return nullptr; }
    void dumpPipeline(std::function<void(const QStringList &)> callback) override { callback({}); }

    bool configureEndpoints(const QList<PsiMedia::PSecureRtpEndpoint> &) override { return true; }
    bool configureAssociation(const QByteArray &associationId, quint64 epoch, const QString &profile,
                              const QByteArray &, const QByteArray &, const QByteArray &, const QByteArray &) override
    {
        if (associationId.isEmpty() || !epoch || profile.isEmpty())
            return false;
        epochs_.insert(associationId, epoch);
        return true;
    }
    void invalidateAssociation(const QByteArray &associationId, quint64 epoch) override
    {
        if (epochs_.value(associationId) == epoch)
            epochs_.remove(associationId);
    }
    bool associationReady(const QByteArray &associationId) const override { return epochs_.contains(associationId); }
    quint64 associationEpoch(const QByteArray &associationId) const override { return epochs_.value(associationId); }
    PsiMedia::SecureRtpSessionContext::Error lastError(const QByteArray &) const override
    {
        return PsiMedia::SecureRtpSessionContext::Error::None;
    }
    void setProtectedPacketHandler(ProtectedPacketHandler handler) override { protectedHandler_ = std::move(handler); }
    void setRuntimeErrorHandler(RuntimeErrorHandler handler) override { runtimeHandler_ = std::move(handler); }
    bool receiveProtectedPacket(const PsiMedia::PSecureRtpPacket &packet) override
    {
        return epochs_.value(packet.associationId) == packet.epoch;
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
    QHash<QByteArray, quint64> epochs_;
    ProtectedPacketHandler     protectedHandler_;
    RuntimeErrorHandler        runtimeHandler_;
};

class CapabilityProvider final : public QObject,
                                 public PsiMedia::Provider,
                                 public PsiMedia::SecureRtpProvider {
    Q_OBJECT
    Q_INTERFACES(PsiMedia::Provider PsiMedia::SecureRtpProvider)
public:
    QObject                        *qobject() override { return this; }
    bool                            isInitialized() const override { return true; }
    QString                         creditName() const override { return QStringLiteral("capability-test"); }
    QString                         creditText() const override { return {}; }
    PsiMedia::FeaturesContext      *createFeatures() override { return nullptr; }
    PsiMedia::RtpSessionContext    *createRtpSession() override { return new CapabilityRtpSessionContext; }
    QStringList supportedSecureRtpProfiles() const override
    {
        return { QStringLiteral("SRTP_AES128_CM_HMAC_SHA1_80") };
    }
    PsiMedia::SecureRtpSessionContext *createSecureRtpSession() override
    {
        return new CapabilityRtpSessionContext;
    }
    PsiMedia::AudioRecorderContext *createAudioRecorder() override { return nullptr; }

signals:
    void initialized();
};

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

XMPP::Features clientFeatures(const PsiMediaJingleCapabilities &capabilities)
{
    XMPP::Features features;
    features << Baseline;
    if (capabilities.available()) {
        features << GenericRtp;
        if (capabilities.audio)
            features << AudioRtp;
        if (capabilities.video)
            features << VideoRtp;
    }
    return features;
}

void verifyJingleDisco(const XMPP::Client &client, bool audio, bool video)
{
    const auto features = client.jingleManager()->discoFeatures();
    QCOMPARE(features.contains(GenericRtp), audio || video);
    QCOMPARE(features.contains(AudioRtp), audio);
    QCOMPARE(features.contains(VideoRtp), video);
}

void commitCapabilities(XMPP::Client &client, RTP::Manager *rtpManager, PsiMediaJingleCapabilities &current,
                        const PsiMediaJingleCapabilities &next, std::shared_ptr<RTP::MediaProvider> &provider,
                        int &featureRefreshes)
{
    const auto oldMedia    = current.mediaTypes();
    const auto newMedia    = next.mediaTypes();
    bool       callbackRan = false;

    const bool committed = commitPsiMediaJingleCapabilities(rtpManager, current, provider, next, [&] {
        callbackRan = true;
        ++featureRefreshes;

        // This is deliberately checked before Client::setFeatures(). If the
        // transaction ever advertises first and updates RTP::Manager second,
        // the callback observes the stale Jingle feature set and fails here.
        verifyJingleDisco(client, newMedia.contains(QStringLiteral("audio")),
                          newMedia.contains(QStringLiteral("video")));
        client.setFeatures(clientFeatures(current));
    });

    QVERIFY(committed);
    QCOMPARE(callbackRan, oldMedia != newMedia);
    QCOMPARE(current, next);
}

void verifyDisco(const XMPP::Client &client, bool audio, bool video)
{
    const auto features = client.makeDiscoResult().features();
    QCOMPARE(features.test(Baseline), true);
    QCOMPARE(features.test(GenericRtp), audio || video);
    QCOMPARE(features.test(AudioRtp), audio);
    QCOMPARE(features.test(VideoRtp), video);
}

QString advertisedHash(const XMPP::Client &client)
{
    return client.makeDiscoResult().capsHash(QCryptographicHash::Sha256);
}
}

class CapabilityRefreshTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { PsiMedia::setProvider(&provider_); }

    void advertisedTransitionsStayInSync()
    {
        QCA::Initializer qca;
        XMPP::Client     client;
        client.setIdentity({ QStringLiteral("client"), QStringLiteral("pc"), QString(), QStringLiteral("Psi test") });

        auto rtpManager = client.jingleManager()->rtpManager();
        QVERIFY(rtpManager);
        rtpManager->setTransportNamespaces({ Jingle::ICE::NS, Jingle::ICE::NS_ICE_UDP });

        PsiMediaJingleCapabilities          current;
        std::shared_ptr<RTP::MediaProvider> provider;
        int                                 featureRefreshes = 0;
        const auto                          full             = fullCapabilities();
        commitCapabilities(client, rtpManager, current, full, provider, featureRefreshes);
        QCOMPARE(featureRefreshes, 1);
        QCOMPARE(provider->mediaTypes(), (QStringList { QStringLiteral("audio"), QStringLiteral("video") }));
        verifyDisco(client, true, true);
        const auto fullHash = advertisedHash(client);
        QVERIFY(!fullHash.isEmpty());

        // An identical authoritative snapshot is a strict no-op: no provider
        // replacement and no disco/caps refresh.
        const auto sameProvider     = provider;
        bool       duplicateRefresh = false;
        QVERIFY(
            !commitPsiMediaJingleCapabilities(rtpManager, current, provider, full, [&] { duplicateRefresh = true; }));
        QVERIFY(!duplicateRefresh);
        QVERIFY(provider == sameProvider);
        QCOMPARE(featureRefreshes, 1);

        // A media session keeps the provider snapshot captured at construction.
        // Replacing Manager's provider below must not retroactively remove video
        // from this already-created session.
        auto oldSession = provider->createSession();
        QVERIFY(oldSession);

        auto audioOnly  = full;
        audioOnly.video = false;
        commitCapabilities(client, rtpManager, current, audioOnly, provider, featureRefreshes);
        QCOMPARE(featureRefreshes, 2);
        QCOMPARE(provider->mediaTypes(), (QStringList { QStringLiteral("audio") }));
        verifyDisco(client, true, false);
        const auto audioHash = advertisedHash(client);
        QVERIFY(audioHash != fullHash);

        auto newSession = provider->createSession();
        QVERIFY(newSession);
        QVERIFY(newSession->createEndpoint(QStringLiteral("audio"), QStringLiteral("audio")));
        QVERIFY(!newSession->createEndpoint(QStringLiteral("video"), QStringLiteral("video")));
        QVERIFY(oldSession->createEndpoint(QStringLiteral("video"), QStringLiteral("video")));

        auto unavailable             = full;
        unavailable.backendAvailable = false;
        commitCapabilities(client, rtpManager, current, unavailable, provider, featureRefreshes);
        QCOMPARE(featureRefreshes, 3);
        QVERIFY(provider->mediaTypes().isEmpty());
        verifyDisco(client, false, false);
        const auto unavailableHash = advertisedHash(client);
        QVERIFY(unavailableHash != audioHash);

        commitCapabilities(client, rtpManager, current, full, provider, featureRefreshes);
        QCOMPARE(featureRefreshes, 4);
        verifyDisco(client, true, true);
        QCOMPARE(advertisedHash(client), fullHash);
    }

    void deviceOnlyChangeNeedsNoReadvertisement()
    {
        QCA::Initializer qca;
        XMPP::Client     client;
        client.setIdentity({ QStringLiteral("client"), QStringLiteral("pc"), QString(), QStringLiteral("Psi test") });

        auto rtpManager = client.jingleManager()->rtpManager();
        QVERIFY(rtpManager);
        rtpManager->setTransportNamespaces({ Jingle::ICE::NS, Jingle::ICE::NS_ICE_UDP });

        PsiMediaJingleCapabilities          current;
        std::shared_ptr<RTP::MediaProvider> provider;
        int                                 featureRefreshes = 0;
        const auto                          full             = fullCapabilities();
        commitCapabilities(client, rtpManager, current, full, provider, featureRefreshes);
        QCOMPARE(featureRefreshes, 1);
        const auto before = advertisedHash(client);
        QVERIFY(!before.isEmpty());

        auto deviceOnly       = full;
        deviceOnly.audioInput = false;
        QCOMPARE(deviceOnly.mediaTypes(), full.mediaTypes());

        // The authoritative snapshot/provider still changes because capture
        // availability changed, but the advertised media set did not. The shared
        // production transaction therefore must not call the feature refresh.
        commitCapabilities(client, rtpManager, current, deviceOnly, provider, featureRefreshes);
        QCOMPARE(featureRefreshes, 1);
        QCOMPARE(current, deviceOnly);
        QCOMPARE(provider->mediaTypes(), full.mediaTypes());
        verifyJingleDisco(client, true, true);
        verifyDisco(client, true, true);
        QCOMPARE(advertisedHash(client), before);
    }

    void initialAsyncProbeIsFailClosed()
    {
        QCA::Initializer qca;
        XMPP::Client     client;
        client.setIdentity({ QStringLiteral("client"), QStringLiteral("pc"), QString(), QStringLiteral("Psi test") });

        auto rtpManager = client.jingleManager()->rtpManager();
        QVERIFY(rtpManager);
        rtpManager->setTransportNamespaces({ Jingle::ICE::NS, Jingle::ICE::NS_ICE_UDP });

        PsiMediaJingleCapabilities          current;
        std::shared_ptr<RTP::MediaProvider> provider;
        int                                 featureRefreshes = 0;
        client.setFeatures(clientFeatures(current));

        auto probing          = fullCapabilities();
        probing.probeComplete = false;
        commitCapabilities(client, rtpManager, current, probing, provider, featureRefreshes);
        QCOMPARE(featureRefreshes, 0);
        QVERIFY(provider->mediaTypes().isEmpty());
        verifyJingleDisco(client, false, false);
        verifyDisco(client, false, false);
        const auto probingHash = advertisedHash(client);

        const auto full = fullCapabilities();
        commitCapabilities(client, rtpManager, current, full, provider, featureRefreshes);
        QCOMPARE(featureRefreshes, 1);
        QCOMPARE(provider->mediaTypes(), full.mediaTypes());
        verifyDisco(client, true, true);
        QVERIFY(advertisedHash(client) != probingHash);
    }

private:
    CapabilityProvider provider_;
};

QTEST_GUILESS_MAIN(CapabilityRefreshTest)
#include "tst_capability_refresh.moc"
