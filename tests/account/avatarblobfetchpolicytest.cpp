/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "avatarblobfetchpolicy.h"

#include <QtTest/QtTest>

class AvatarBlobFetchPolicyTest final : public QObject {
    Q_OBJECT
private:
    static QByteArray hash(char c) { return QByteArray(20, c); }
private slots:
    void onlyOneRequestWhileBlobIsPending()
    {
        AvatarBlobFetchPolicy policy;
        const auto sha = hash('a');
        policy.announce("account", "contact@example.net", sha, "metadata1");
        QVERIFY(policy.beginFetch("account", "contact@example.net", sha, 100));
        QVERIFY(!policy.beginFetch("account", "contact@example.net", sha, 100));
        QVERIFY(!policy.beginFetch("account", "contact@example.net", sha, 20000));
        policy.finish("account", "contact@example.net", sha, true, 20001);
        QVERIFY(policy.beginFetch("account", "contact@example.net", sha, 20002));
        // Production only attempts a new fetch if FileCache lookup missed.
    }
    void failingBlobIsNotRepeatedUntilNewEventAfterCooldown()
    {
        AvatarBlobFetchPolicy policy;
        const auto sha = hash('b');
        policy.announce("account", "contact@example.net", sha, "metadata1");
        QVERIFY(policy.beginFetch("account", "contact@example.net", sha, 100));
        policy.finish("account", "contact@example.net", sha, false, 100);
        QVERIFY(!policy.beginFetch("account", "contact@example.net", sha, 60099));
        // Expiry never issues a request by itself: only explicit beginFetch.
        QVERIFY(policy.beginFetch("account", "contact@example.net", sha, 60100));
    }
    void newerMetadataRejectsObsoleteBlobResponse()
    {
        AvatarBlobFetchPolicy policy;
        const auto old = hash('a');
        const auto current = hash('b');
        policy.announce("account", "contact@example.net", old, "oldMetadata");
        QVERIFY(policy.beginFetch("account", "contact@example.net", old, 0));
        policy.announce("account", "contact@example.net", current, "newMetadata");
        QVERIFY(!policy.expects("account", "contact@example.net", old));
        QVERIFY(policy.expects("account", "contact@example.net", current));
        policy.finish("account", "contact@example.net", old, true, 1);
        QVERIFY(policy.beginFetch("account", "contact@example.net", current, 1));
    }
    void currentRetractionCancelsPendingBlobAndDoesNotAffectNewMetadata()
    {
        AvatarBlobFetchPolicy policy;
        const auto old = hash('a'), current = hash('b');
        policy.announce("account", "contact@example.net", old, "item-a");
        QVERIFY(policy.beginFetch("account", "contact@example.net", old, 0));
        policy.announce("account", "contact@example.net", current, "item-b");
        QVERIFY(!policy.retractMetadata("account", "contact@example.net", "item-a"));
        QVERIFY(policy.expects("account", "contact@example.net", current));
        QVERIFY(policy.retractMetadata("account", "contact@example.net", "item-b"));
        QVERIFY(!policy.expects("account", "contact@example.net", current));
        QVERIFY(!policy.beginFetch("account", "contact@example.net", current, 100));
    }
    void sameJidOnTwoAccountsIsNotCoalesced()
    {
        AvatarBlobFetchPolicy policy;
        const auto a = hash('a'), b = hash('b');
        policy.announce("alice", "contact@example.net", a, "item-a");
        policy.announce("bob", "contact@example.net", b, "item-b");
        QVERIFY(policy.beginFetch("alice", "contact@example.net", a, 0));
        QVERIFY(policy.beginFetch("bob", "contact@example.net", b, 0));
        policy.forgetAccount("alice");
        QVERIFY(policy.expects("bob", "contact@example.net", b));
        QVERIFY(!policy.expects("alice", "contact@example.net", a));
    }
    void memoryBoundAndInFlightEntriesAreNeverEvicted()
    {
        AvatarBlobFetchPolicy policy(60000, 2);
        const auto sha = hash('x');
        policy.announce("account", "a@example.net", sha, "a");
        QVERIFY(policy.beginFetch("account", "a@example.net", sha, 1));
        policy.announce("account", "b@example.net", sha, "b");
        policy.announce("account", "c@example.net", sha, "c");
        QCOMPARE(policy.trackedCount(), 2);
        QVERIFY(policy.expects("account", "a@example.net", sha));
        QVERIFY(!policy.expects("account", "b@example.net", sha));
        QVERIFY(policy.expects("account", "c@example.net", sha));
    }
    void whenAllSlotsAreInflightExtraFetchIsSuppressed()
    {
        AvatarBlobFetchPolicy policy(60000, 1);
        const auto sha = hash('x');
        policy.announce("account", "a@example.net", sha, "a");
        QVERIFY(policy.beginFetch("account", "a@example.net", sha, 1));
        policy.announce("account", "b@example.net", sha, "b");
        QVERIFY(!policy.beginFetch("account", "b@example.net", sha, 1));
        QCOMPARE(policy.trackedCount(), 1);
    }
    void dataRetractionOnlyAffectsMatchingHash()
    {
        AvatarBlobFetchPolicy policy;
        const auto a = hash('a'), b = hash('b');
        policy.announce("account", "a@example.net", a, "metadata-a");
        QVERIFY(!policy.retractData("account", "a@example.net", b));
        QVERIFY(policy.expects("account", "a@example.net", a));
        QVERIFY(policy.retractData("account", "a@example.net", a));
        QVERIFY(!policy.expects("account", "a@example.net", a));
    }
};
QTEST_GUILESS_MAIN(AvatarBlobFetchPolicyTest)
#include "avatarblobfetchpolicytest.moc"
