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
