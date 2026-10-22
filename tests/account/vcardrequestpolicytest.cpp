/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "vcardrequestpolicy.h"

#include <QtTest/QtTest>

class VCardRequestPolicyTest final : public QObject {
    Q_OBJECT
private slots:
    void fullRequestsQueryBothStoresRegardlessOfClientCaps()
    {
        using namespace VCardRequestPolicy;
        const auto full = plan(false, false, false);
        QVERIFY(full.fetchModern);
        QVERIFY(full.fetchLegacy);
        // The plan is intentionally independent of online-resource disco:
        // resources may advertise only vcard-temp, only vCard4, or nothing.
    }

    void legacyPhotoRequestsSkipPep()
    {
        using namespace VCardRequestPolicy;
        const auto photo = plan(true, false, false);
        QVERIFY(!photo.fetchModern);
        QVERIFY(photo.fetchLegacy);
    }

    void explicitLegacyAndMucFlags()
    {
        using namespace VCardRequestPolicy;
        for (auto request : { plan(false, true, false),
                              plan(false, false, true),
                              plan(true, true, true) }) {
            QVERIFY(!request.fetchModern);
            QVERIFY(request.fetchLegacy);
        }
    }

    void selfCardAlsoQueriesBoth()
    {
        // The self-card is handled like any other full request. The account
        // may hold additional data in the old server store.
        const auto result = VCardRequestPolicy::plan(false, false, false);
        QVERIFY(result.fetchModern);
        QVERIFY(result.fetchLegacy);
    }
};

QTEST_GUILESS_MAIN(VCardRequestPolicyTest)
#include "vcardrequestpolicytest.moc"
