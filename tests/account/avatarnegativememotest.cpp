/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "avatarnegativememo.h"

#include <QtTest/QtTest>

class AvatarNegativeMemoTest final : public QObject {
    Q_OBJECT
private slots:
    void unknownIsNotConfirmedAbsence()
    {
        AvatarNegativeMemo memo;
        const QString key = QStringLiteral("account/a/contact/bob");
        QVERIFY(!memo.lookup(key, 0).has_value());
        memo.record(key, AvatarNegativeMemo::State::Unknown, 0);
        const auto state = memo.lookup(key, 1);
        QVERIFY(state.has_value());
        QCOMPARE(*state, AvatarNegativeMemo::State::Unknown);
        QVERIFY(*state != AvatarNegativeMemo::State::KnownAbsent);
    }
    void lazyTtlDoesNotPerformRefresh()
    {
        AvatarNegativeMemo memo;
        const QString key = QStringLiteral("account/a/contact/bob");
        memo.record(key, AvatarNegativeMemo::State::InvalidImage, 100);
        QCOMPARE(*memo.lookup(key, 30099), AvatarNegativeMemo::State::InvalidImage);
        QVERIFY(!memo.lookup(key, 30100).has_value());
        QCOMPARE(memo.size(), 0);
        QVERIFY(!memo.lookup(key, 60100).has_value());
    }
    void eventInvalidationWorksForNeverPositiveEntry()
    {
        AvatarNegativeMemo memo;
        const QString key = QStringLiteral("account/a/occupant/room/nick");
        memo.record(key, AvatarNegativeMemo::State::Unknown, 0);
        QVERIFY(memo.lookup(key, 1));
        memo.invalidate(key);
        QVERIFY(!memo.lookup(key, 1));
    }
    void capacityAndLruEvictionAreBounded()
    {
        AvatarNegativeMemo memo(2, 30000);
        memo.record("a", AvatarNegativeMemo::State::Unknown, 0);
        memo.record("b", AvatarNegativeMemo::State::UriOnly, 0);
        QVERIFY(memo.lookup("a", 1));
        memo.record("c", AvatarNegativeMemo::State::Unavailable, 1);
        QCOMPARE(memo.size(), 2);
        QVERIFY(!memo.lookup("b", 1));
        QVERIFY(memo.lookup("a", 1));
        QVERIFY(memo.lookup("c", 1));
    }
    void malformedAndTransientStatesNeverTurnIntoNoPhoto()
    {
        AvatarNegativeMemo memo;
        memo.record("bad", AvatarNegativeMemo::State::InvalidImage, 1);
        memo.record("offline", AvatarNegativeMemo::State::Unavailable, 1);
        memo.record("uri", AvatarNegativeMemo::State::UriOnly, 1);
        QCOMPARE(*memo.lookup("bad", 2), AvatarNegativeMemo::State::InvalidImage);
        QCOMPARE(*memo.lookup("offline", 2), AvatarNegativeMemo::State::Unavailable);
        QCOMPARE(*memo.lookup("uri", 2), AvatarNegativeMemo::State::UriOnly);
    }
    void resetInvalidatesAllNegativeStatesButNoPositiveFiles()
    {
        AvatarNegativeMemo memo;
        memo.record("account/a/contact/a", AvatarNegativeMemo::State::Unknown, 0);
        memo.record("account/b/contact/b", AvatarNegativeMemo::State::Unknown, 0);
        memo.invalidatePrefix("account/a/");
        QVERIFY(!memo.lookup("account/a/contact/a", 1));
        QVERIFY(!memo.lookup("account/b/contact/b", 1));
    }
    void timerRollbackExpiresEntry()
    {
        AvatarNegativeMemo memo;
        memo.record("key", AvatarNegativeMemo::State::Unknown, 1234);
        QVERIFY(!memo.lookup("key", 0));
    }
};
QTEST_GUILESS_MAIN(AvatarNegativeMemoTest)
#include "avatarnegativememotest.moc"
