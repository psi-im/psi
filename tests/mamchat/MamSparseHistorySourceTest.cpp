/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/MamHistorySource.h"
#include <QtTest>

class MamSparseHistorySourceTest : public QObject {
    Q_OBJECT
    void add(MamHistorySource &source, const QString &key, const QString &uid = {})
    {
        MessageView message(MessageView::Message);
        message.setMessageId(key);
        message.setArchiveId(uid);
        message.setPlainText("body");
        source.insertRow(source.knownCount(), { key, message, {} });
    }
private slots:
    void persistedSnapshotAnchorsCachedTailWithoutPageMetadata()
    {
        MamHistorySource source;
        add(source, "old", "uid-old");
        add(source, "new", "uid-last");
        source.restoreSnapshot(5000, "uid-last");
        QVERIFY(source.windows().isEmpty()); // A count is not cached coverage.
        source.observePage({ "uid-old", "uid-last" }, { "old", "new" }, -1, -1);
        source.enableVirtual();
        QVERIFY(source.isVirtual());
        QCOMPARE(source.rankOfUid("uid-old"), 4998);
        QCOMPARE(source.logicalIndex(0), 4998);
        QCOMPARE(source.itemCount(), 5000);
    }

    void millionEventArchiveDoesNotAllocateMessagePlaceholders()
    {
        MamHistorySource source;
        QStringList      uids, keys;
        for (int i = 0; i < 10; ++i) {
            const auto id = QString("uid-%1").arg(i);
            add(source, id, id);
            uids << id;
            keys << id;
        }
        source.observePage(uids, keys, 1000000, 999990, {}, {}, true);
        source.enableVirtual();
        QCOMPARE(source.knownCount(), 10);
        QCOMPARE(source.itemCount(), 1000000);
        QVERIFY(!source.isAvailable(500000));
        QCOMPARE(source.logicalIndex(0), 999990);
        QCOMPARE(source.knownIndex(999999), 9);
        const auto seek = source.requestForRange(499995, 500005, 15, 3);
        QCOMPARE(seek.kind, MamHistorySource::Request::Index);
        QVERIFY(qAbs(seek.index - 500000) <= 15);
        QCOMPARE(seek.limit, 15);
        QCOMPARE(seek.generation, quint64(3));
    }
    void eventDensityAndTailIslandKeepJoinHistoryCompact()
    {
        MamHistorySource source;
        add(source, "old", "old-uid");
        for (int i = 0; i < 4; ++i)
            add(source, QString("join-%1").arg(i), QString("join-uid-%1").arg(i));
        QStringList uids { "old-uid" }, keys { "old" };
        for (int i = 0; i < 39; ++i) {
            uids << QString("receipt-%1").arg(i);
            keys << QString();
        }
        source.observePage(uids, keys, 10000, 9000, "join-uid-0", {}, true);
        source.enableVirtual();
        QVERIFY(source.visibleRatio() < .03);
        for (int i = 1; i < 5; ++i)
            QCOMPARE(source.logicalIndex(i), source.logicalIndex(i - 1) + 1);
        QCOMPARE(source.logicalIndex(4), source.itemCount() - 1);
        QVERIFY(source.itemCount() < 10000);
        QVERIFY(source.itemCount() > source.knownCount());
    }
    void missingMetadataAndRejectedIndexFallBackToKnownRows()
    {
        MamHistorySource source;
        add(source, "one", "u1");
        source.observePage({ "u1" }, { "one" }, -1, -1);
        source.enableVirtual();
        QVERIFY(!source.isVirtual());
        QCOMPARE(source.itemCount(), 1);
        source.observePage({ "u1" }, { "one" }, 1000, 999, {}, {}, true);
        source.enableVirtual();
        QVERIFY(source.isVirtual());
        source.rejectIndex();
        QVERIFY(!source.canSeek());
        QCOMPARE(source.itemCount(), 1);
        QCOMPARE(source.rows().first().message.messageId(), QString("one"));
    }
    void cachedBodiesAndOnlyProtocolReplayDoNotCollapseUnknownHistory()
    {
        MamHistorySource source;
        for (int i = 0; i < 10; ++i)
            add(source, QString("cached-%1").arg(i), QString("uid-%1").arg(i));
        source.restoreSnapshot(10000, "last-receipt");
        source.observePage({ "receipt-a", "receipt-b" }, { {}, {} }, -1, -1, "uid-9", {}, true);
        source.enableVirtual();
        QVERIFY(source.isVirtual());
        QVERIFY(source.itemCount() > 1000);
        QCOMPARE(source.knownCount(), 10);
        QVERIFY(source.visibleRatio() > 0);
    }
    void explicitSeekDoesNotTurnOverscanAdjacencyIntoSequentialScanning()
    {
        MamHistorySource source;
        add(source, "tail", "tail-uid");
        source.observePage({ "tail-uid" }, { "tail" }, 100, 99);
        source.enableVirtual();
        const auto scroll = source.requestForRange(60, 98, 12, 0);
        QCOMPARE(scroll.kind, MamHistorySource::Request::Before);
        const auto seek = source.requestForRange(60, 98, 12, 7, true);
        QCOMPARE(seek.kind, MamHistorySource::Request::Index);
        QVERIFY(seek.index < 90);
    }
    void adjacentRequestsUseRealUidInsteadOfAnotherIndex()
    {
        MamHistorySource source;
        add(source, "one", "u1");
        add(source, "two", "u2");
        source.observePage({ "u1", "u2" }, { "one", "two" }, 10000, 5000);
        source.enableVirtual();
        const int  first = source.logicalIndex(0);
        const auto older = source.requestForRange(first - 2, first - 1, 12, 0);
        QCOMPARE(older.kind, MamHistorySource::Request::Before);
        QCOMPARE(older.cursor, QString("u1"));
        const int  last  = source.logicalIndex(1);
        const auto newer = source.requestForRange(last + 1, last + 2, 12, 0);
        QCOMPARE(newer.kind, MamHistorySource::Request::After);
        QCOMPARE(newer.cursor, QString("u2"));
    }
};
QTEST_GUILESS_MAIN(MamSparseHistorySourceTest)
#include "MamSparseHistorySourceTest.moc"
