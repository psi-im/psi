/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "vcardrequestpolicy.h"

#include <QtTest/QtTest>

class VCardRequestPolicyTest final : public QObject {
    Q_OBJECT
private slots:
    void protocolFromContactResourceCaps()
    {
        using namespace VCardRequestPolicy;
        const auto temp = Protocol::VCardTemp;
        const auto modern = Protocol::VCard4;

        // The legacy feature is on the resource's full JID, while requests
        // target the bare JID.
        QCOMPARE(select(false, false, { { true, false } }), temp);
        QCOMPARE(select(false, false, { { true, true } }), modern);
        QCOMPARE(select(false, false, { { true, false }, { true, true } }), modern);
        QCOMPARE(select(false, false, { { true, true }, { true, false } }), modern);
        // Unknown caps are not evidence that vCard4 is unsupported.
        QCOMPARE(select(false, false, { { false, false } }), modern);
        QCOMPARE(select(false, false, { }), modern);
        // However, they must not negate known legacy-only capabilities.
        QCOMPARE(select(false, false, { { false, false }, { true, false } }), temp);
        QCOMPARE(select(false, false, { { true, false }, { false, false } }), temp);
    }

    void explicitLegacyAndMucFlagsOverrideCapabilities()
    {
        using namespace VCardRequestPolicy;
        const QList<ResourceCapabilities> modern { { true, true } };
        QCOMPARE(select(true, false, modern), Protocol::VCardTemp);
        QCOMPARE(select(true, true, modern), Protocol::VCardTemp);
    }

    void selfCardRetainsPepFirstWithOrWithoutContactCaps()
    {
        using namespace VCardRequestPolicy;
        QCOMPARE(select(false, true, QList<ResourceCapabilities> { { true, false } }), Protocol::VCard4);
        QCOMPARE(select(false, true, QList<ResourceCapabilities> {}), Protocol::VCard4);
    }
};

QTEST_GUILESS_MAIN(VCardRequestPolicyTest)
#include "vcardrequestpolicytest.moc"
