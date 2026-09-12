#include "psimediajingle.h"

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

void commitCapabilities(XMPP::Client &client, RTP::Manager *rtpManager,
                        const PsiMediaJingleCapabilities &capabilities,
                        std::shared_ptr<RTP::MediaProvider> &provider, bool refreshClientFeatures = true)
{
    provider = makePsiMediaJingleProvider(capabilities);
    rtpManager->setMediaProvider(provider);
    if (refreshClientFeatures)
        client.setFeatures(clientFeatures(capabilities));
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
    void advertisedTransitionsStayInSync()
    {
        QCA::Initializer qca;
        XMPP::Client     client;
        client.setIdentity({ QStringLiteral("client"), QStringLiteral("pc"), QString(), QStringLiteral("Psi test") });

        auto rtpManager = client.jingleManager()->rtpManager();
        QVERIFY(rtpManager);

        std::shared_ptr<RTP::MediaProvider> provider;
        const auto                          full = fullCapabilities();
        commitCapabilities(client, rtpManager, full, provider);
        QCOMPARE(provider->mediaTypes(), (QStringList { QStringLiteral("audio"), QStringLiteral("video") }));
        verifyDisco(client, true, true);
        const auto fullHash = advertisedHash(client);
        QVERIFY(!fullHash.isEmpty());

        // A media session keeps the provider snapshot captured at construction.
        // Replacing Manager's provider below must not retroactively remove video
        // from this already-created session.
        auto oldSession = provider->createSession();
        QVERIFY(oldSession);

        auto audioOnly = full;
        audioOnly.video = false;
        commitCapabilities(client, rtpManager, audioOnly, provider);
        QCOMPARE(provider->mediaTypes(), (QStringList { QStringLiteral("audio") }));
        verifyDisco(client, true, false);
        const auto audioHash = advertisedHash(client);
        QVERIFY(audioHash != fullHash);

        auto newSession = provider->createSession();
        QVERIFY(newSession);
        QVERIFY(newSession->createEndpoint(QStringLiteral("audio"), QStringLiteral("audio")));
        QVERIFY(!newSession->createEndpoint(QStringLiteral("video"), QStringLiteral("video")));
        QVERIFY(oldSession->createEndpoint(QStringLiteral("video"), QStringLiteral("video")));

        auto unavailable = full;
        unavailable.backendAvailable = false;
        commitCapabilities(client, rtpManager, unavailable, provider);
        QVERIFY(provider->mediaTypes().isEmpty());
        verifyDisco(client, false, false);
        const auto unavailableHash = advertisedHash(client);
        QVERIFY(unavailableHash != audioHash);

        commitCapabilities(client, rtpManager, full, provider);
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

        std::shared_ptr<RTP::MediaProvider> provider;
        const auto                          full = fullCapabilities();
        commitCapabilities(client, rtpManager, full, provider);
        const auto before = advertisedHash(client);
        QVERIFY(!before.isEmpty());

        auto deviceOnly = full;
        deviceOnly.audioInput = false;
        QCOMPARE(deviceOnly.mediaTypes(), full.mediaTypes());

        // AvCallManager intentionally skips PsiAccount::updateFeatures() when
        // advertised audio/video is unchanged. Commit the new provider without
        // touching Client::features and prove that Manager discovery/hash remains
        // identical, so this path does not need a presence broadcast.
        commitCapabilities(client, rtpManager, deviceOnly, provider, false);
        QCOMPARE(provider->mediaTypes(), full.mediaTypes());
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

        std::shared_ptr<RTP::MediaProvider> provider;
        auto                                probing = fullCapabilities();
        probing.probeComplete = false;
        commitCapabilities(client, rtpManager, probing, provider);
        QVERIFY(provider->mediaTypes().isEmpty());
        verifyDisco(client, false, false);
        const auto probingHash = advertisedHash(client);

        const auto full = fullCapabilities();
        commitCapabilities(client, rtpManager, full, provider);
        QCOMPARE(provider->mediaTypes(), full.mediaTypes());
        verifyDisco(client, true, true);
        QVERIFY(advertisedHash(client) != probingHash);
    }
};

QTEST_GUILESS_MAIN(CapabilityRefreshTest)
#include "tst_capability_refresh.moc"
