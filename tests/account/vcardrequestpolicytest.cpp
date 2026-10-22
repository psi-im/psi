/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "vcardrequestpolicy.h"

#include <QtTest/QtTest>

class VCardRequestPolicyTest final : public QObject {
    Q_OBJECT
private slots:
    void protocolFromContactResourceCaps_data()
    {
        using Caps = VCardRequestPolicy::ResourceCapabilities;
        QTest::addColumn<QList<Caps>>("resources");
        QTest::addColumn<bool>("temp");

        // In particular, bare JID capabilities are unknown even when a
        // contact's full JID has advertised the legacy vcard-temp feature.
        QTest::newRow("roster contact with vcard-temp only") << QList<Caps> { { true, false } } << true;
        QTest::newRow("known unrelated features") << QList<Caps> { { true, false } } << true;
        QTest::newRow("vcard4 announced") << QList<Caps> { { true, true } } << false;
        QTest::newRow("vcard4 on second resource")
            << QList<Caps> { { true, false }, { true, true } } << false;
        QTest::newRow("unknown resource cannot override known legacy")
            << QList<Caps> { { false, false }, { true, false } } << true;
        QTest::newRow("unresolved capabilities") << QList<Caps> { { false, false } } << false;
        QTest::newRow("no online resources") << QList<Caps> {} << false;
    }

    void protocolFromContactResourceCaps()
    {
        QFETCH(QList<VCardRequestPolicy::ResourceCapabilities>, resources);
        QFETCH(bool, temp);
        QCOMPARE(VCardRequestPolicy::select(false, false, resources)
                     == VCardRequestPolicy::Protocol::VCardTemp, temp);
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
