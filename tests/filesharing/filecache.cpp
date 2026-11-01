/* SPDX-License-Identifier: GPL-2.0-or-later */
// What FileCache promises to avatars, Bits of Binary and shared files.
#include "filecache.h"
#include "applicationinfo.h"
#include "fileutil.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <iris/xmpp_hash.h>
#include <memory>

using XMPP::Hash;

// FileCache needs only this much of two classes that bring in the whole application.
QString ApplicationInfo::version() { return QStringLiteral("test"); }
QString ApplicationInfo::fileCacheNS() { return QStringLiteral("https://psi-im.org/test/filecache"); }
QString FileUtil::mimeToFileExt(const QString &) { return {}; }

namespace {
Hash hashOf(const QByteArray &bytes) { return Hash::from(Hash::Sha1, bytes); }

class CountingCache : public FileCache {
public:
    using FileCache::FileCache;
    int removals = 0;

protected:
    void removeItem(FileCacheItem *item, bool needSync) override
    {
        ++removals;
        FileCache::removeItem(item, needSync);
    }
};
}

class FileCacheTest : public QObject {
    Q_OBJECT
    std::unique_ptr<QTemporaryDir> dir;

    std::unique_ptr<FileCache> open() const { return std::make_unique<FileCache>(dir->path()); }
    QString payloadPath(const Hash &hash) const { return dir->filePath(QString::fromLatin1(hash.toHex())); }

private slots:
    void init()
    {
        dir = std::make_unique<QTemporaryDir>();
        QVERIFY(dir->isValid());
    }

    void dataRoundTrip()
    {
        auto             cache = open();
        const QByteArray bytes("payload");
        const auto       hash = hashOf(bytes);
        QVERIFY(!cache->get(hash));
        auto *item = cache->append(hash, bytes, { { QStringLiteral("type"), QStringLiteral("image/png") } });
        QVERIFY(item);
        QCOMPARE(cache->get(hash), item);
        QCOMPARE(item->id(), hash);
        QCOMPARE(item->size(), quint64(bytes.size()));
        QCOMPARE(item->data(), bytes);
        QCOMPARE(cache->getData(hash), bytes);
        QCOMPARE(item->metadata().value(QStringLiteral("type")).toString(), QStringLiteral("image/png"));
    }

    void emptyItemHasNoPayload()
    {
        auto       cache = open();
        const auto hash  = hashOf("reference only");
        cache->append(hash, {}, { { QStringLiteral("link"), QStringLiteral("/somewhere/else") } });
        cache->sync();
        QVERIFY(cache->get(hash)->isOnDisk());
        QVERIFY(!QFile::exists(payloadPath(hash)));
        QVERIFY(cache->get(hash)->data().isEmpty());
        cache.reset();
        cache = open();
        QVERIFY(cache->get(hash));
        QCOMPARE(cache->get(hash)->metadata().value(QStringLiteral("link")).toString(),
                 QStringLiteral("/somewhere/else"));
    }

    void itemsSurviveRestart()
    {
        const QByteArray  bytes("persistent payload");
        const auto        hash = hashOf(bytes);
        const QVariantMap metadata { { QStringLiteral("type"), QStringLiteral("image/png") },
                                     { QStringLiteral("jids"),
                                       QStringList { QStringLiteral("a@b"), QStringLiteral("c@d") } },
                                     { QStringLiteral("number"), 42 },
                                     { QStringLiteral("flag"), true },
                                     { QStringLiteral("bytes"), QByteArray("\x01\x02\xff", 3) } };
        QDateTime         created;
        {
            auto cache = open();
            created    = cache->append(hash, bytes, metadata, 3600)->created();
        }
        auto  cache = open();
        auto *item  = cache->get(hash);
        QVERIFY(item);
        QVERIFY(item->isOnDisk());
        QCOMPARE(item->data(), bytes);
        QCOMPARE(item->size(), quint64(bytes.size()));
        QCOMPARE(item->maxAge(), 3600u);
        QVERIFY(qAbs(item->created().secsTo(created)) <= 1);
        QCOMPARE(item->metadata(), metadata);
    }

    void itemRegisteredBeforeACrashIsKept()
    {
        const QByteArray bytes("no clean shutdown");
        const auto       hash = hashOf(bytes);
        QObject          processThatCrashed;
        auto            *first = new FileCache(dir->path(), &processThatCrashed);
        first->append(hash, bytes);
        first->sync();
        auto cache = open();
        QVERIFY(cache->get(hash));
        QCOMPARE(cache->get(hash)->data(), bytes);
    }

    void removedItemStaysRemoved()
    {
        const QByteArray bytes("to remove");
        const auto       hash = hashOf(bytes);
        {
            auto cache = open();
            cache->append(hash, bytes);
            cache->sync();
            QVERIFY(QFile::exists(payloadPath(hash)));
            cache->remove(hash);
            QVERIFY(!cache->get(hash));
            QVERIFY(!QFile::exists(payloadPath(hash)));
        }
        QVERIFY(!open()->get(hash));
    }

    void everyHashFindsTheItem()
    {
        const QByteArray bytes("two hashes");
        const auto       sha1 = Hash::from(Hash::Sha1, bytes), sha256 = Hash::from(Hash::Sha256, bytes);
        {
            auto  cache = open();
            auto *item  = cache->append(QList<Hash> { sha256, sha1 }, bytes);
            QCOMPARE(cache->get(sha1), item);
            QCOMPARE(cache->get(sha256), item);
            QCOMPARE(item->sums().size(), 2);
        }
        auto  cache = open();
        auto *item  = cache->get(sha1);
        QVERIFY(item);
        QCOMPARE(item->sums().size(), 2);
        QVERIFY(item->sums().contains(sha256));
    }

    void removingAnItemRemovesAllItsHashes()
    {
        const QByteArray bytes("two hashes to remove");
        const auto       sha1 = Hash::from(Hash::Sha1, bytes), sha256 = Hash::from(Hash::Sha256, bytes);
        auto             cache = open();
        cache->append(QList<Hash> { sha1, sha256 }, bytes);
        cache->remove(sha256);
        QVERIFY(!cache->get(sha1));
        QVERIFY(!cache->get(sha256));
    }

    void moveToCacheTakesTheFile()
    {
        const QByteArray bytes("moved payload");
        const auto       hash = hashOf(bytes);
        QTemporaryDir    elsewhere;
        const auto       source = elsewhere.filePath(QStringLiteral("file.bin"));
        {
            QFile file(source);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(bytes), qint64(bytes.size()));
        }
        const auto modified = QFileInfo(source).lastModified();
        {
            auto  cache = open();
            auto *item  = cache->moveToCache(hash, QFileInfo(source));
            QVERIFY(item);
            QVERIFY(item->isOnDisk());
            QCOMPARE(item->created(), modified);
            QVERIFY(!QFile::exists(source));
            QVERIFY(QFile::exists(payloadPath(hash)));
            QCOMPARE(item->data(), bytes);
        }
        auto cache = open();
        QVERIFY(cache->get(hash));
        QCOMPARE(cache->get(hash)->data(), bytes);
    }

    void expiredItemIsGone()
    {
        const QByteArray bytes("short lived");
        const auto       hash  = hashOf(bytes);
        auto             cache = open();
        cache->append(hash, bytes, {}, 1);
        cache->sync();
        QVERIFY(cache->get(hash));
        QTest::qWait(1100);
        QVERIFY(!cache->get(hash));
        QVERIFY(!QFile::exists(payloadPath(hash)));
    }

    void expiredItemIsDroppedOnRestart()
    {
        const QByteArray bytes("expired while closed");
        const auto       hash = hashOf(bytes);
        {
            QObject processThatCrashed;
            auto   *first = new FileCache(dir->path(), &processThatCrashed);
            first->append(hash, bytes, {}, 1);
            first->sync();
            QTest::qWait(1100);
            auto second = open();
            QVERIFY(!second->get(hash));
        }
        QVERIFY(!open()->get(hash));
    }

    void sessionItemEndsWithTheSession()
    {
        const QByteArray bytes("session only");
        const auto       hash = hashOf(bytes);
        {
            auto cache = open();
            cache->append(hash, bytes, {}, FileCache::Session);
            cache->sync();
            QVERIFY(cache->get(hash));
        }
        QVERIFY(!QFile::exists(payloadPath(hash)));
        QVERIFY(!open()->get(hash));
    }

    void gcDropsAnItemWhosePayloadIsGone()
    {
        const QByteArray bytes("payload deleted behind the cache");
        const auto       hash  = hashOf(bytes);
        auto             cache = open();
        cache->append(hash, bytes);
        cache->sync();
        QVERIFY(QFile::remove(payloadPath(hash)));
        cache->gc();
        QVERIFY(!cache->get(hash));
    }

    void oldestItemsAreEvictedOverQuota()
    {
        const QByteArray oldest(100, 'a'), middle(100, 'b'), newest(100, 'c');
        auto             cache = open();
        cache->setFileCacheSize(250);
        cache->append(hashOf(oldest), oldest);
        QTest::qWait(5);
        cache->append(hashOf(middle), middle);
        QTest::qWait(5);
        cache->append(hashOf(newest), newest);
        cache->sync(); // Puts them on disk, and the next pass counts them.
        cache->sync();
        QVERIFY(!cache->get(hashOf(oldest)));
        QVERIFY(!QFile::exists(payloadPath(hashOf(oldest))));
        QVERIFY(cache->get(hashOf(middle)));
        QVERIFY(cache->get(hashOf(newest)));
    }

    void sessionUndeletableItemSurvivesEviction()
    {
        const QByteArray keep(100, 'a'), other(100, 'b'), newest(100, 'c');
        auto             cache = open();
        cache->setFileCacheSize(250);
        cache->append(hashOf(keep), keep)->setSessionUndeletable();
        QTest::qWait(5);
        cache->append(hashOf(other), other);
        QTest::qWait(5);
        cache->append(hashOf(newest), newest);
        cache->sync();
        cache->sync();
        QVERIFY(cache->get(hashOf(keep)));
        QVERIFY(!cache->get(hashOf(other)));
        QVERIFY(cache->get(hashOf(newest)));
    }

    void dataLeavesMemoryOverTheLimit()
    {
        const QByteArray bytes(200, 'a');
        const auto       hash  = hashOf(bytes);
        auto             cache = open();
        cache->setMemoryCacheSize(100);
        auto *item = cache->append(hash, bytes);
        QVERIFY(item->inMemory());
        cache->sync();
        QVERIFY(!item->inMemory());
        QVERIFY(QFile::exists(payloadPath(hash)));
        QCOMPARE(item->data(), bytes);
    }

    void subclassSeesRemovals()
    {
        // AvatarCache relies on this to unbind icons from contacts.
        const QByteArray bytes(100, 'a'), other(100, 'b'), third(100, 'c');
        CountingCache    cache(dir->path());
        cache.append(hashOf(bytes), bytes);
        cache.remove(hashOf(bytes));
        QCOMPARE(cache.removals, 1);
        cache.setFileCacheSize(150);
        cache.append(hashOf(other), other);
        QTest::qWait(5);
        cache.append(hashOf(third), third);
        cache.sync();
        cache.sync();
        QCOMPARE(cache.removals, 2);
    }
};
QTEST_GUILESS_MAIN(FileCacheTest)
#include "filecache.moc"
