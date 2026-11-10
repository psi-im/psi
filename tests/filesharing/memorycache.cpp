/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filecachememory.h"
#include <QTest>

class MemoryCacheTest : public QObject {
    Q_OBJECT
private slots:
    void byteBudgetUsesAccessOrder()
    {
        FileCacheMemory cache(8);
        cache.put("a", 0, "aaaa");
        cache.put("b", 0, "bbbb");
        QCOMPARE(*cache.get("a", 0, 4), QByteArray("aaaa"));
        cache.put("c", 0, "cccc");
        QVERIFY(!cache.contains("b", 0, 4));
        QVERIFY(cache.contains("a", 0, 4));
        QVERIFY(cache.contains("c", 0, 4));
        QCOMPARE(cache.bytes(), quint64(8));
        cache.put("large", 0, QByteArray(9, 'x'));
        QCOMPARE(cache.bytes(), quint64(8));
    }
    void subrangesReuseOneAllocationAndOverwritesWithdrawOverlaps()
    {
        FileCacheMemory cache(32);
        cache.put("generation", 0, "abcdefghijklmnop");
        QCOMPARE(*cache.get("generation", 4, 4), QByteArray("efgh"));
        QCOMPARE(cache.count(), 1);
        QCOMPARE(cache.bytes(), quint64(16));
        cache.put("generation", 4, "WXYZ");
        QCOMPARE(cache.bytes(), quint64(4));
        QVERIFY(!cache.contains("generation", 0, 16));
        QCOMPARE(*cache.get("generation", 4, 4), QByteArray("WXYZ"));
        cache.remove("generation", 8, 4);
        cache.remove("generation", 4, 0);
        QVERIFY(cache.contains("generation", 4, 4));
        cache.remove("generation", 7, 1);
        QCOMPARE(cache.bytes(), quint64(0));
    }
    void generationsAreIsolatedAndShrinkingIsImmediate()
    {
        FileCacheMemory cache(16);
        cache.put("old", 0, "oldbytes");
        cache.put("new", 0, "newbytes");
        cache.remove("old");
        QCOMPARE(*cache.get("new", 0, 8), QByteArray("newbytes"));
        cache.setLimit(7);
        QCOMPARE(cache.bytes(), quint64(0));
        QCOMPARE(cache.count(), 0);
    }
    void churnDoesNotKeepRetiredKeys()
    {
        FileCacheMemory cache(32);
        for (int i = 0; i < 2048; ++i)
            cache.put(QByteArray::number(i), 0, "x");
        QCOMPARE(cache.count(), 32);
        QCOMPARE(cache.bytes(), quint64(32));
        QVERIFY(!cache.contains("0", 0, 1));
        QVERIFY(cache.contains("2047", 0, 1));
        cache.setLimit(0);
        QCOMPARE(cache.count(), 0);
        cache.setLimit(1);
        cache.put("0", 0, "y");
        QCOMPARE(*cache.get("0", 0, 1), QByteArray("y"));
    }
    void invalidIntervalsDoNotWrap()
    {
        FileCacheMemory cache(32);
        cache.put("file", 0, "ok");
        cache.put("file", std::numeric_limits<quint64>::max() - 1, "bad");
        QVERIFY(!cache.contains("file", std::numeric_limits<quint64>::max(), 2));
        cache.remove("file", std::numeric_limits<quint64>::max(), 1);
        QCOMPARE(*cache.get("file", 0, 2), QByteArray("ok"));
    }
    void tinySpansAlsoHaveABoundedIndex()
    {
        FileCacheMemory cache(64 * 1024);
        for (int i = 0; i < FileCacheMemory::MaxEntries + 100; ++i)
            cache.put(QByteArray::number(i), 0, "x");
        QCOMPARE(cache.count(), FileCacheMemory::MaxEntries);
        QCOMPARE(cache.bytes(), quint64(FileCacheMemory::MaxEntries));
        QVERIFY(!cache.contains("0", 0, 1));
        QVERIFY(cache.contains(QByteArray::number(FileCacheMemory::MaxEntries + 99), 0, 1));
    }
};
QTEST_GUILESS_MAIN(MemoryCacheTest)
#include "memorycache.moc"
