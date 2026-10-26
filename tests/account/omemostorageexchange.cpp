// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise real SQLite persistence and Double Ratchet sessions over Iris's
// in-memory PEP fixture. No real account or network service is used.
#include "psiomemostorage.h"
#define main fixture_main
#include "../../iris/tests/jingle/omemoelement.cpp"
#undef main
#include <QTemporaryDir>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCA::Initializer qca;
    QTemporaryDir    dir;
    auto             storage = std::make_unique<PsiOmemoStorage>(dir.path(), QStringLiteral("alice"));
    Endpoint         alice(QStringLiteral("alice")), bob(QStringLiteral("bob"));
    delete alice.omemo;
    alice.omemo          = new OmemoEncryption(&alice.client, storage.get(), storage.get());
    const auto configure = [&] {
        alice.omemo->setNewIdentityTrustLevel(EncryptionTrustLevel::Undecided);
        alice.omemo->setAcceptedSessionBuildingTrustLevels(EncryptionTrustLevel::ManuallyTrusted);
    };
    configure();
    for (auto endpoint : { &alice, &bob })
        check(wait(endpoint->omemo->setUp(QStringLiteral("Psi")), alice, bob)->success(), "setup failed");
    check(wait(alice.omemo->refreshDevices(bob.jid, false), alice, bob)->success(), "discovery failed");
    for (const auto &d : alice.omemo->devices(bob.jid))
        check(wait(alice.omemo->refreshBundle(bob.jid, d.id, d.protocol, false), alice, bob)->success(),
              "bundle discovery failed");
    for (const auto &d : alice.omemo->devices(bob.jid))
        if (!d.identityKey.isEmpty())
            check(alice.omemo->setTrustLevel(bob.jid, d.identityKey, EncryptionTrustLevel::ManuallyTrusted),
                  "trust write failed");
    QDomDocument doc;
    auto         payload = fingerprint(doc);
    for (int iteration = 0; iteration < 10; ++iteration) {
        for (auto protocol : { OmemoProtocol::Omemo2, OmemoProtocol::Legacy }) {
            if (!alice.omemo->supportedProtocols().testFlag(protocol))
                continue;
            auto outbound = wait(alice.omemo->encryptElement(bob.jid, protocol, payload, { bob.omemo->ownDeviceId() }),
                                 alice, bob);
            check(outbound->success(), "send failed");
            auto read = wait(bob.omemo->decryptElement(alice.jid, outbound->stanza()), alice, bob);
            check(read->success(), "receive failed");
            auto inbound = wait(bob.omemo->encryptElement(alice.jid, protocol, payload, { alice.omemo->ownDeviceId() }),
                                alice, bob);
            check(inbound->success(), "answer failed");
            auto answer = wait(alice.omemo->decryptElement(bob.jid, inbound->stanza()), alice, bob);
            check(answer->success(), "answer receive failed");
        }
        delete alice.omemo;
        storage.reset();
        storage     = std::make_unique<PsiOmemoStorage>(dir.path(), QStringLiteral("alice"));
        alice.omemo = new OmemoEncryption(&alice.client, storage.get(), storage.get());
        configure();
        check(wait(alice.omemo->refreshDevices(bob.jid, false), alice, bob)->success(), "rediscovery failed");
        for (const auto &d : alice.omemo->devices(bob.jid))
            check(d.trust == EncryptionTrustLevel::ManuallyTrusted && d.hasSession, "restart lost trust or session");
    }
    qInfo("Ten SQLite/Iris restart cycles with bidirectional OMEMO 2 and legacy encryption passed");
    pep.clear();
}
