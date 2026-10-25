/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "avatarepochpolicy.h"
#include "avatarblobfetchpolicy.h"

#include <QtTest/QtTest>

class AvatarEpochPolicyTest final : public QObject {
    Q_OBJECT
private slots:
    void lateRepliesAreRejectedAfterOccupantLeave()
    {
        AvatarEpochPolicy epochs;
        const auto key = AvatarIdentity::bindingKey(
            QStringLiteral("account-a"), QStringLiteral("room@example.org/nick"), true);
        const auto requested = epochs.generation(key);
        QCOMPARE(requested, quint64(0));
        epochs.advance(key);
        QVERIFY(epochs.generation(key) != requested);
    }
    void sameNickReusedAfterRejoinDoesNotInheritEpoch()
    {
        AvatarEpochPolicy epochs;
        const auto key = AvatarIdentity::bindingKey(
            QStringLiteral("account-a"), QStringLiteral("room@example.org/nick"), true);
        epochs.advance(key);
        const auto previous = epochs.generation(key);
        epochs.advance(key); // New occupant reuses the same nickname.
        QVERIFY(epochs.generation(key) > previous);
    }
    void disconnectedAccountInvalidatesQueuedContactGeneration()
    {
        AvatarEpochPolicy policy;
        const QString key(QStringLiteral("account-1/session/first-instance"));
        const quint64 requestGeneration = policy.generation(key);
        policy.advance(key); // disconnected; a new connection starts.
        QVERIFY(policy.generation(key) != requestGeneration);
        QVERIFY(policy.generation(key) > requestGeneration);
    }
    void sameBareJidOnOtherAccountIsNotInvalidated()
    {
        AvatarEpochPolicy policy;
        const QString a(QStringLiteral("account-1/session"));
        const QString b(QStringLiteral("account-2/session"));
        policy.advance(a);
        const quint64 otherRequest = policy.generation(b);
        policy.advance(a);
        QCOMPARE(policy.generation(b), otherRequest);
    }
    void roomRejoinFencesUncachedOccupant()
    {
        AvatarEpochPolicy policy;
        const QString room = AvatarIdentity::accountPrefix("a")
            + QStringLiteral("occupant/room@example.org/");
        const QString occupant = room + QStringLiteral("new-nick");
        // No occupant entry was ever created, yet its queued request must
        // become stale after the room generation advances.
        const quint64 requestRoomGeneration = policy.generation(room);
        QCOMPARE(policy.generation(occupant), quint64(0));
        policy.advance(room);
        QVERIFY(policy.generation(room) != requestRoomGeneration);
    }
    void roomAndAccountKeysRemainIndependent()
    {
        AvatarEpochPolicy epochs;
        const auto one = AvatarIdentity::bindingKey("a", "room@example.org/nick", true);
        const auto two = AvatarIdentity::bindingKey("b", "room@example.org/nick", true);
        epochs.advance(one);
        QCOMPARE(epochs.generation(two), quint64(0));
        const auto before = epochs.generation(one);
        epochs.forgetPrefix(AvatarIdentity::accountPrefix("a"));
        QVERIFY(epochs.generation(one) > before);
        QCOMPARE(epochs.generation(two), quint64(0));
    }
    void resetOfOneRoomDoesNotInvalidateOther()
    {
        AvatarEpochPolicy epochs;
        const auto one = AvatarIdentity::bindingKey("a", "room1@example.org/nick", true);
        const auto two = AvatarIdentity::bindingKey("a", "room2@example.org/nick", true);
        const auto requested = epochs.generation(one);
        epochs.advance(one);
        QCOMPARE(epochs.generation(two), quint64(0));
        QVERIFY(epochs.generation(one) != requested);
    }
};
QTEST_GUILESS_MAIN(AvatarEpochPolicyTest)
#include "avatarepochpolicytest.moc"
