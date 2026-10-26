// SPDX-License-Identifier: GPL-2.0-or-later
#include "omemodevicedecision.h"
#include "psiomemostorage.h"

#include <iris/xmpp_client.h>
#include <iris/xmpp_omemo.h>

#include <QTemporaryDir>
#include <QtCrypto>
#include <QtTest>

using namespace XMPP;

namespace {
const Jid          Peer(QStringLiteral("contact@example.test"));
const QString      Account(QStringLiteral("test-account"));
constexpr uint32_t DeviceId = 42;

// Montgomery basepoint, and its canonical Edwards encoding used for trust.
const QByteArray StoredKey = QByteArray::fromHex("0509") + QByteArray(31, '\0');
const QByteArray Identity  = QByteArray::fromHex("5866666666666666666666666666666666666666666666666666666666666666");

OmemoStorage::Device device()
{
    OmemoStorage::Device result;
    for (auto protocol : { OmemoProtocol::Legacy, OmemoProtocol::Omemo2 }) {
        auto &state   = result.protocols[protocol];
        state.keyId   = StoredKey;
        state.session = protocol == OmemoProtocol::Legacy ? QByteArray("legacy-session") : QByteArray("modern-session");
    }
    return result;
}

void checkDeviceTrust(PsiOmemoStorage &storage, EncryptionTrustLevel expected)
{
    Client          client;
    OmemoEncryption omemo(&client, &storage, &storage);
    const auto      devices = omemo.devices(Peer);
    QCOMPARE(devices.size(), 2);
    for (const auto &info : devices) {
        QCOMPARE(info.identityKey, Identity);
        QCOMPARE(info.trust, expected);
    }
}
}

class OmemoStorageTest : public QObject {
    Q_OBJECT
private slots:
    void verifiedIncomingSenderDoesNotPromptAboutOtherDevices()
    {
        OmemoDeviceInfo sender;
        sender.owner       = Peer;
        sender.id          = DeviceId;
        sender.protocol    = OmemoProtocol::Omemo2;
        sender.protocols   = sender.protocol;
        sender.active      = true;
        sender.identityKey = Identity;
        sender.trust       = EncryptionTrustLevel::ManuallyTrusted;
        auto legacy        = sender;
        legacy.protocol    = OmemoProtocol::Legacy;
        legacy.protocols   = legacy.protocol;
        legacy.identityKey = QByteArray(32, 'l');
        legacy.trust       = EncryptionTrustLevel::Undecided;
        auto other         = sender;
        other.id           = DeviceId + 1;
        other.identityKey  = QByteArray(32, 'm');
        other.trust        = EncryptionTrustLevel::AutomaticallyTrusted;
        const QList<OmemoDeviceInfo> known { sender, legacy, other };

        // Outgoing sends still ask about every pending recipient. Receiving
        // from this verified device must not reopen that unrelated decision.
        QCOMPARE(PsiOmemo::devicesForDecision(known, 0, {}, PsiOmemo::DeviceDecision::VerifyIdentity).size(), 2);
        QVERIFY(PsiOmemo::devicesForDecision(known, DeviceId, OmemoProtocol::Omemo2,
                                             PsiOmemo::DeviceDecision::VerifyIdentity)
                    .isEmpty());
        auto unknown        = sender;
        unknown.trust       = EncryptionTrustLevel::Undecided;
        const auto incoming = PsiOmemo::devicesForDecision({ unknown, legacy, other }, DeviceId, OmemoProtocol::Omemo2,
                                                           PsiOmemo::DeviceDecision::VerifyIdentity);
        QCOMPARE(incoming.size(), 1);
        QCOMPARE(incoming.first().identityKey, sender.identityKey);
        QCOMPARE(incoming.first().protocol, OmemoProtocol::Omemo2);
    }

    void sessionRepairIsIndependentOfIdentityVerification()
    {
        OmemoDeviceInfo sender;
        sender.owner       = Peer;
        sender.id          = DeviceId;
        sender.protocol    = OmemoProtocol::Omemo2;
        sender.protocols   = sender.protocol;
        sender.active      = true;
        sender.identityKey = Identity;
        sender.trust       = EncryptionTrustLevel::Authenticated;
        QVERIFY(PsiOmemo::devicesForDecision({ sender }, DeviceId, sender.protocol,
                                             PsiOmemo::DeviceDecision::VerifyIdentity)
                    .isEmpty());
        QCOMPARE(
            PsiOmemo::devicesForDecision({ sender }, DeviceId, sender.protocol, PsiOmemo::DeviceDecision::RepairSession)
                .size(),
            1);
        sender.trust = EncryptionTrustLevel::Distrusted;
        QVERIFY(
            PsiOmemo::devicesForDecision({ sender }, DeviceId, sender.protocol, PsiOmemo::DeviceDecision::RepairSession)
                .isEmpty());
        sender.trust  = EncryptionTrustLevel::Undecided;
        sender.active = false;
        QVERIFY(
            PsiOmemo::devicesForDecision({ sender }, DeviceId, sender.protocol, PsiOmemo::DeviceDecision::RepairSession)
                .isEmpty());
    }

    void trustSurvivesDeviceListUpdates_data()
    {
        QTest::addColumn<int>("trust");
        QTest::newRow("trusted") << int(EncryptionTrustLevel::ManuallyTrusted);
        QTest::newRow("authenticated") << int(EncryptionTrustLevel::Authenticated);
        QTest::newRow("distrusted") << int(EncryptionTrustLevel::Distrusted);
    }

    void trustSurvivesDeviceListUpdates()
    {
        QFETCH(int, trust);
        const auto    level = static_cast<EncryptionTrustLevel>(trust);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        {
            PsiOmemoStorage storage(dir.path(), Account);
            QVERIFY2(storage.isOpen(), qPrintable(storage.errorString()));
            auto state = device();
            QVERIFY(storage.addDevice(Peer.full(), DeviceId, state));
            QVERIFY(storage.setTrustLevel(OmemoEncryption::methodId(), Peer, Identity, level));

            // The same installation can disappear from PEP, return, and lose a
            // wire profile without losing the user's fingerprint decision.
            for (auto protocol : { OmemoProtocol::Legacy, OmemoProtocol::Omemo2 }) {
                state.protocols[protocol].removalFromDeviceListDate = QDateTime::currentDateTimeUtc();
                QVERIFY(storage.addDevice(Peer.full(), DeviceId, state));
                checkDeviceTrust(storage, level);
            }
            state.protocols.remove(OmemoProtocol::Legacy);
            QVERIFY(storage.addDevice(Peer.full(), DeviceId, state));
            QVERIFY(storage.addDevice(Peer.full(), DeviceId, device()));
            checkDeviceTrust(storage, level);
        }
        PsiOmemoStorage reopened(dir.path(), Account);
        QVERIFY2(reopened.isOpen(), qPrintable(reopened.errorString()));
        checkDeviceTrust(reopened, level);
    }

    void trustSurvivesLocalDeviceRemoval()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        {
            PsiOmemoStorage storage(dir.path(), Account);
            QVERIFY(storage.addDevice(Peer.full(), DeviceId, device()));
            QVERIFY(storage.setTrustLevel(OmemoEncryption::methodId(), Peer, Identity,
                                          EncryptionTrustLevel::ManuallyTrusted));
            QVERIFY(storage.removeDevice(Peer.full(), DeviceId));
            QVERIFY(storage.allData().devices.isEmpty());
            QCOMPARE(storage.trustLevel(OmemoEncryption::methodId(), Peer, Identity),
                     EncryptionTrustLevel::ManuallyTrusted);
        }
        PsiOmemoStorage reopened(dir.path(), Account);
        QVERIFY(reopened.addDevice(Peer.full(), DeviceId, device()));
        checkDeviceTrust(reopened, EncryptionTrustLevel::ManuallyTrusted);
        QVERIFY(reopened.removeDevices(Peer.full()));
        QCOMPARE(reopened.trustLevel(OmemoEncryption::methodId(), Peer, Identity),
                 EncryptionTrustLevel::ManuallyTrusted);
    }

    void decisionsAreBoundToAccountOwnerAndFingerprint()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PsiOmemoStorage storage(dir.path(), Account);
        QVERIFY(storage.setTrustLevel(OmemoEncryption::methodId(), Peer, Identity, EncryptionTrustLevel::Distrusted));
        QCOMPARE(storage.trustLevel(OmemoEncryption::methodId(), Peer, Identity), EncryptionTrustLevel::Distrusted);
        const Jid resource(Peer.bare() + QStringLiteral("/another-resource"));
        QCOMPARE(storage.trustLevel(OmemoEncryption::methodId(), resource, Identity), EncryptionTrustLevel::Distrusted);
        QCOMPARE(storage.trustLevel(OmemoEncryption::methodId(), Jid(QStringLiteral("another@example.test")), Identity),
                 EncryptionTrustLevel::Undecided);
        QCOMPARE(storage.trustLevel(OmemoEncryption::methodId(), Peer, QByteArray(32, 'x')),
                 EncryptionTrustLevel::Undecided);
        PsiOmemoStorage other(dir.path(), QStringLiteral("another-account"));
        QCOMPARE(other.trustLevel(OmemoEncryption::methodId(), Peer, Identity), EncryptionTrustLevel::Undecided);
    }
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCA::Initializer qca;
    OmemoStorageTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "omemostoragetest.moc"
