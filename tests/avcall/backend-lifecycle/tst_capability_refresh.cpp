#include "psimediajinglecapabilitytransaction.h"

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
    const auto oldMedia = current.mediaTypes();
    const auto newMedia = next.mediaTypes();
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
    void advertisedTransitionsStayInSync()
    {
        QCA::Initializer qca;
        XMPP::Client     client;
        client.setIdentity({ QStringLiteral("client"), QStringLiteral("pc"), QString(), QStringLiteral("Psi test") });

        auto rtpManager = client.jingleManager()->rtpManager();
        QVERIFY(rtpManager);

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
        const auto sameProvider = provider;
        bool       duplicateRefresh = false;
        QVERIFY(!commitPsiMediaJingleCapabilities(rtpManager, current, provider, full,
                                                   [&] { duplicateRefresh = true; }));
        QVERIFY(!duplicateRefresh);
        QVERIFY(provider == sameProvider);
        QCOMPARE(featureRefreshes, 1);

        // A media session keeps the provider snapshot captured at construction.
        // Replacing Manager's provider below must not retroactively remove video
        // from this already-created session.
        auto oldSession = provider->createSession();
        QVERIFY(oldSession);

        auto audioOnly = full;
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

        auto unavailable = full;
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

        PsiMediaJingleCapabilities          current;
        std::shared_ptr<RTP::MediaProvider> provider;
        int                                 featureRefreshes = 0;
        const auto                          full             = fullCapabilities();
        commitCapabilities(client, rtpManager, current, full, provider, featureRefreshes);
        QCOMPARE(featureRefreshes, 1);
        const auto before = advertisedHash(client);
        QVERIFY(!before.isEmpty());

        auto deviceOnly = full;
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

        PsiMediaJingleCapabilities          current;
        std::shared_ptr<RTP::MediaProvider> provider;
        int                                 featureRefreshes = 0;
        client.setFeatures(clientFeatures(current));

        auto probing = fullCapabilities();
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
};

QTEST_GUILESS_MAIN(CapabilityRefreshTest)
#include "tst_capability_refresh.moc"
