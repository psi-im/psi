/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "avatarblobinflight.h"

#include <QtTest/QtTest>

class AvatarBlobInFlightTest final : public QObject {
    Q_OBJECT
    static QByteArray sha(char c) { return QByteArray(20, c); }

private slots:
    void oneIqPerHashAcrossDifferentContacts()
    {
        AvatarBlobInFlight flights;
        const auto hash = sha('a');
        const auto a = flights.join("a@example.org", hash);
        const auto b = flights.join("b@example.org", hash);
        const auto c = flights.join("a@example.org", hash);
        QVERIFY(a.request);
        QVERIFY(!b.request);
        QVERIFY(!c.request);
        QCOMPARE(a.token, b.token);
        QCOMPARE(flights.count(), 1);
        const auto waiting = flights.complete(hash);
        QCOMPARE(waiting.size(), 2);
        QVERIFY(waiting.contains("a@example.org"));
        QVERIFY(waiting.contains("b@example.org"));
        QCOMPARE(flights.count(), 0);
    }

    void supersededContactCannotReceiveOldBlob()
    {
        AvatarBlobInFlight flights;
        const auto oldHash = sha('a'), newHash = sha('b');
        const auto first = flights.join("alice@example.org", oldHash);
        QVERIFY(!flights.join("bob@example.org", oldHash).request);
        flights.supersede("alice@example.org", newHash);
        QCOMPARE(flights.waiters(oldHash), QStringList { "bob@example.org" });
        QVERIFY(flights.join("alice@example.org", newHash).request);
        QCOMPARE(flights.complete(oldHash), QStringList { "bob@example.org" });
        QVERIFY(flights.waiters(newHash).contains("alice@example.org"));
        flights.finish(oldHash, first.token); // delayed finish from the original IQ
        QVERIFY(flights.waiters(newHash).contains("alice@example.org"));
    }

    void staleErrorCannotReleaseNewFlight()
    {
        AvatarBlobInFlight flights;
        const auto hash = sha('a');
        const auto old = flights.join("one@example.org", hash);
        flights.finish(hash, old.token);
        const auto fresh = flights.join("two@example.org", hash);
        QVERIFY(fresh.request);
        QVERIFY(fresh.token != old.token);
        flights.finish(hash, old.token);
        QCOMPARE(flights.count(), 1);
        QCOMPARE(flights.complete(hash), QStringList { "two@example.org" });
    }

    void removalAndFailurePermitRetry()
    {
        AvatarBlobInFlight flights;
        const auto hash = sha('a');
        const auto request = flights.join("contact@example.org", hash);
        flights.supersede("contact@example.org", {});
        QVERIFY(flights.waiters(hash).isEmpty());
        flights.finish(hash, request.token);
        QVERIFY(flights.join("contact@example.org", hash).request);
    }

    void identicalShaAcrossAccountsNeedsNoSeparateBlob()
    {
        AvatarBlobInFlight flights;
        const auto hash = sha('a');
        // JID identity is global; the announced bytes are keyed by SHA-1.
        QVERIFY(flights.join("same@example.org", hash).request);
        QVERIFY(!flights.join("same@example.org", hash).request);
        QVERIFY(!flights.join("other@example.org", hash).request);
        QCOMPARE(flights.complete(hash).size(), 2);
    }

    void unavailableFirstContactFallsBackToSecond()
    {
        AvatarBlobInFlight flights;
        QObject aliceAccount, bobAccount;
        const auto hash = sha('a');
        const auto first = flights.join("alice@example.org", hash, &aliceAccount, "sha-a");
        QVERIFY(first.request);
        QVERIFY(!flights.join("bob@example.org", hash, &bobAccount, "sha-a").request);
        const auto retry = flights.fail(hash, first.token);
        QVERIFY(retry.request);
        QCOMPARE(retry.jid, QStringLiteral("bob@example.org"));
        QCOMPARE(retry.itemId, QStringLiteral("sha-a"));
        QCOMPARE(retry.transport.data(), static_cast<QObject *>(&bobAccount));
        QVERIFY(retry.token != first.token);
        QCOMPARE(flights.count(), 1);
        QVERIFY(!flights.fail(hash, first.token).request);
        QCOMPARE(flights.complete(hash).size(), 2);
        QVERIFY(!flights.fail(hash, retry.token).request);
    }

    void sameContactOnTwoAccountsMayUseOtherTransport()
    {
        AvatarBlobInFlight flights;
        QObject a, b;
        const auto hash = sha('x');
        const auto first = flights.join("same@example.org", hash, &a, "hash");
        QVERIFY(first.request);
        QVERIFY(!flights.join("same@example.org", hash, &b, "hash").request);
        const auto retry = flights.fail(hash, first.token);
        QVERIFY(retry.request);
        QCOMPARE(retry.jid, QStringLiteral("same@example.org"));
        QCOMPARE(retry.transport.data(), static_cast<QObject *>(&b));
        QCOMPARE(flights.waiters(hash).size(), 1);
    }

    void destroyedTransportIsNotUsedForRetry()
    {
        AvatarBlobInFlight flights;
        QObject firstAccount;
        const auto hash = sha('a');
        const auto first = flights.join("one@example.org", hash, &firstAccount, "a");
        auto *secondAccount = new QObject;
        flights.join("two@example.org", hash, secondAccount, "a");
        delete secondAccount;
        QVERIFY(!flights.fail(hash, first.token).request);
        QCOMPARE(flights.count(), 0);
    }

    void invalidHashDoesNotCreateFlight()
    {
        AvatarBlobInFlight flights;
        QVERIFY(!flights.join("a@example.org", "not-a-sha1").request);
        QCOMPARE(flights.count(), 0);
    }
};

QTEST_GUILESS_MAIN(AvatarBlobInFlightTest)
#include "avatarblobinflighttest.moc"
