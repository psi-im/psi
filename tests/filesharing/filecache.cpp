/* SPDX-License-Identifier: GPL-2.0-or-later */
// What FileCache promises to avatars, Bits of Binary and shared files.
#include "filecache.h"
#include "applicationinfo.h"
#include "filedatabase.h"
#include "fileutil.h"
#include "optionstree.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <iris/xmpp_hash.h>
#include <memory>

using XMPP::Hash;

// FileCache needs only this much of two classes that bring in the whole application.
static QString profileDirectory;
QString        ApplicationInfo::currentProfileDir(HomedirType) { return profileDirectory; }
QString        ApplicationInfo::version() { return QStringLiteral("test"); }
QString        ApplicationInfo::fileCacheNS() { return QStringLiteral("https://psi-im.org/test/filecache"); }
QString        FileUtil::mimeToFileExt(const QString &) { return {}; }

namespace {
const QString partitionName = QStringLiteral("test");

Hash hashOf(const QByteArray &bytes) { return Hash::from(Hash::Sha1, bytes); }

// The registry format that caches used before they moved into the database.
void addLegacyItem(OptionsTree &registry, const QString &directory, const QList<Hash> &sums, const QByteArray &bytes,
                   const QVariantMap &metadata, unsigned int maxAge,
                   const QDateTime &created = QDateTime::currentDateTime())
{
    const auto prefix = QStringLiteral("h") + QString::fromLatin1(sums.first().toHex());
    registry.setOption(prefix + ".ha", sums.first().stringType());
    registry.setOption(prefix + ".metadata", metadata);
    registry.setOption(prefix + ".ctime", created.toString(Qt::ISODate));
    registry.setOption(prefix + ".max-age", int(maxAge));
    registry.setOption(prefix + ".size", qulonglong(bytes.size()));
    QStringList aliases;
    for (int i = 1; i < sums.size(); ++i)
        aliases.append(QStringLiteral("%1+%2").arg(sums.at(i).stringType(), QString::fromLatin1(sums.at(i).toHex())));
    registry.setOption(prefix + ".aliases", aliases);
    if (!bytes.isEmpty()) {
        QFile payload(directory + QLatin1Char('/') + QString::fromLatin1(sums.first().toHex()));
        if (payload.open(QIODevice::WriteOnly))
            payload.write(bytes);
    }
}

bool saveLegacyRegistry(OptionsTree &registry, const QString &directory)
{
    QDir().mkpath(directory);
    return registry.saveOptions(directory + QStringLiteral("/cache.xml"), "items", ApplicationInfo::fileCacheNS(),
                                ApplicationInfo::version());
}

int recordCount()
{
    QSqlQuery query(FileDatabase::connection());
    return query.exec(QStringLiteral("SELECT COUNT(*) FROM files")) && query.next() ? query.value(0).toInt() : -1;
}

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

    std::unique_ptr<FileCache> open() const { return std::make_unique<FileCache>(partitionName, dir->path()); }
    QString payloadPath(const Hash &hash) const { return dir->filePath(QString::fromLatin1(hash.toHex())); }

private slots:
    void init()
    {
        dir = std::make_unique<QTemporaryDir>();
        QVERIFY(dir->isValid());
        profileDirectory = dir->path();
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
        auto            *first = new FileCache(partitionName, dir->path(), &processThatCrashed);
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
        QCOMPARE(cache->get(sha256), item);
        QCOMPARE(item->sums().size(), 2);
        QVERIFY(item->sums().contains(sha256));
    }

    void appendingAgainKeepsOneRecord()
    {
        const QByteArray bytes("appended twice");
        const auto       hash = hashOf(bytes);
        {
            auto cache = open();
            cache->append(hash, bytes, { { QStringLiteral("generation"), 1 } });
            cache->sync();
            cache->append(hash, bytes, { { QStringLiteral("generation"), 2 } });
            cache->sync();
            QCOMPARE(recordCount(), 1);
        }
        auto cache = open();
        QCOMPARE(cache->get(hash)->metadata().value(QStringLiteral("generation")).toInt(), 2);
    }

    void changedMetadataSurvivesRestart()
    {
        const QByteArray bytes("metadata changes");
        const auto       hash = hashOf(bytes);
        {
            auto  cache = open();
            auto *item  = cache->append(hash, bytes);
            cache->sync();
            item->setMetadata({ { QStringLiteral("jids"), QStringList { QStringLiteral("a@b") } } });
        }
        auto cache = open();
        QCOMPARE(cache->get(hash)->metadata().value(QStringLiteral("jids")).toStringList(),
                 QStringList { QStringLiteral("a@b") });
    }

    void partitionsAreSeparate()
    {
        const QByteArray bytes("same bytes, different owners");
        const auto       hash = hashOf(bytes);
        QTemporaryDir    otherRoot;
        {
            FileCache first(QStringLiteral("first"), dir->path()), second(QStringLiteral("second"), otherRoot.path());
            first.append(hash, bytes);
            first.sync();
            QVERIFY(!second.get(hash));
            second.append(hash, bytes);
            second.sync();
            QCOMPARE(recordCount(), 2);
            second.remove(hash);
            QVERIFY(first.get(hash));
        }
        FileCache first(QStringLiteral("first"), dir->path()), second(QStringLiteral("second"), otherRoot.path());
        QVERIFY(first.get(hash));
        QVERIFY(!second.get(hash));
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
            auto   *first = new FileCache(partitionName, dir->path(), &processThatCrashed);
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

    void undeletableItemSurvivesEvictionAndRestart()
    {
        const QByteArray keep(100, 'a'), other(100, 'b'), newest(100, 'c');
        {
            auto  cache = open();
            auto *item  = cache->append(hashOf(keep), keep);
            item->setUndeletable();
            cache->setFileCacheSize(250);
            QTest::qWait(5);
            cache->append(hashOf(other), other);
            QTest::qWait(5);
            cache->append(hashOf(newest), newest);
            cache->sync();
            cache->sync();
            QVERIFY(cache->get(hashOf(keep)));
            QVERIFY(!cache->get(hashOf(other)));
        }
        auto  cache = open();
        auto *item  = cache->get(hashOf(keep));
        QVERIFY(item);
        item->setUndeletable(false);
        QVERIFY(!item->metadata().contains(QStringLiteral("fc_persistent")));
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
        CountingCache    cache(partitionName, dir->path());
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

    void legacyItemsAreImported()
    {
        QTemporaryDir     legacy;
        const QByteArray  bytes("legacy payload");
        const auto        sha1 = Hash::from(Hash::Sha1, bytes), sha256 = Hash::from(Hash::Sha256, bytes);
        const QVariantMap metadata { { QStringLiteral("jids"), QStringList { QStringLiteral("a@b") } } };
        const QByteArray  reference("only a link");
        OptionsTree       registry;
        addLegacyItem(registry, legacy.path(), { sha1, sha256 }, bytes, metadata, FileCache::Forever);
        addLegacyItem(registry, legacy.path(), { hashOf(reference) }, {},
                      { { QStringLiteral("link"), QStringLiteral("/an/original") } }, 3600);
        QVERIFY(saveLegacyRegistry(registry, legacy.path()));
        {
            auto cache = open();
            cache->importLegacy(legacy.path());
            auto *item = cache->get(sha1);
            QVERIFY(item);
            QCOMPARE(cache->get(sha256), item);
            QCOMPARE(item->data(), bytes);
            QCOMPARE(item->metadata(), metadata);
            QCOMPARE(item->maxAge(), FileCache::Forever);
            QVERIFY(QFile::exists(payloadPath(sha1)));
            QVERIFY2(QFile::exists(legacy.filePath(QString::fromLatin1(sha1.toHex()))), "another profile may need it");
            QCOMPARE(cache->get(hashOf(reference))->metadata().value(QStringLiteral("link")).toString(),
                     QStringLiteral("/an/original"));
        }
        auto cache = open();
        QCOMPARE(cache->get(sha256)->data(), bytes);
    }

    void legacyImportRunsOnce()
    {
        QTemporaryDir    legacy;
        const QByteArray bytes("removed after import");
        OptionsTree      registry;
        addLegacyItem(registry, legacy.path(), { hashOf(bytes) }, bytes, {}, FileCache::Forever);
        QVERIFY(saveLegacyRegistry(registry, legacy.path()));
        auto cache = open();
        cache->importLegacy(legacy.path());
        cache->remove(hashOf(bytes));
        cache->importLegacy(legacy.path());
        QVERIFY(!cache->get(hashOf(bytes)));
        cache.reset();
        cache = open();
        cache->importLegacy(legacy.path());
        QVERIFY(!cache->get(hashOf(bytes)));
    }

    void unusableLegacyItemsAreSkipped()
    {
        QTemporaryDir    legacy;
        const QByteArray expired("expired"), session("session"), missing("missing"), good("good");
        OptionsTree      registry;
        addLegacyItem(registry, legacy.path(), { hashOf(expired) }, expired, {}, 10,
                      QDateTime::currentDateTime().addDays(-1));
        addLegacyItem(registry, legacy.path(), { hashOf(session) }, session, {}, FileCache::Session);
        addLegacyItem(registry, legacy.path(), { hashOf(missing) }, missing, {}, FileCache::Forever);
        addLegacyItem(registry, legacy.path(), { hashOf(good) }, good, {}, FileCache::Forever);
        QVERIFY(QFile::remove(legacy.filePath(QString::fromLatin1(hashOf(missing).toHex()))));
        QVERIFY(saveLegacyRegistry(registry, legacy.path()));
        auto cache = open();
        cache->importLegacy(legacy.path());
        QVERIFY(!cache->get(hashOf(expired)));
        QVERIFY(!cache->get(hashOf(session)));
        QVERIFY(!cache->get(hashOf(missing)));
        QCOMPARE(cache->get(hashOf(good))->data(), good);
        QCOMPARE(recordCount(), 1);
    }

    void interruptedImportRepairsItsCopies()
    {
        QTemporaryDir    legacy;
        const QByteArray bytes("copied in full the second time");
        OptionsTree      registry;
        addLegacyItem(registry, legacy.path(), { hashOf(bytes) }, bytes, {}, FileCache::Forever);
        QVERIFY(saveLegacyRegistry(registry, legacy.path()));
        {
            QFile partial(payloadPath(hashOf(bytes)));
            QVERIFY(partial.open(QIODevice::WriteOnly));
            partial.write(bytes.left(3));
        }
        auto cache = open();
        cache->importLegacy(legacy.path());
        QCOMPARE(cache->get(hashOf(bytes))->data(), bytes);
    }

    void importWithoutALegacyDirectoryIsHarmless()
    {
        auto cache = open();
        cache->importLegacy(dir->filePath(QStringLiteral("never-existed")));
        QCOMPARE(recordCount(), 0);
        QVERIFY(!QDir(dir->filePath(QStringLiteral("never-existed"))).exists());
    }

    void twoProfilesShareNothing()
    {
        QTemporaryDir    profiles, legacyRoot;
        const auto       legacy = legacyRoot.filePath(QStringLiteral("shares"));
        const QByteArray bytes("both profiles start with this");
        OptionsTree      registry;
        QVERIFY(QDir().mkpath(legacy));
        addLegacyItem(registry, legacy, { hashOf(bytes) }, bytes, {}, FileCache::Forever);
        QVERIFY(saveLegacyRegistry(registry, legacy));

        const auto inProfile = [&profiles](const QString &name) {
            const auto path = profiles.filePath(name);
            QDir().mkpath(path);
            profileDirectory = path;
            return path;
        };
        const auto alpha = inProfile(QStringLiteral("alpha"));
        FileCache  first(partitionName, alpha);
        first.importLegacy(legacy);
        QVERIFY(first.get(hashOf(bytes)));
        const QByteArray onlyAlpha("only in alpha");
        first.append(hashOf(onlyAlpha), onlyAlpha);
        first.sync();
        FileDatabase::retireLegacyDirectory(legacy, { QStringLiteral("alpha"), QStringLiteral("beta") });
        QVERIFY2(QDir(legacy).exists(), "beta has not imported yet");

        const auto beta = inProfile(QStringLiteral("beta"));
        FileCache  second(partitionName, beta);
        second.importLegacy(legacy);
        QCOMPARE(second.get(hashOf(bytes))->data(), bytes);
        QVERIFY(!second.get(hashOf(onlyAlpha)));

        // Removing a file in one profile leaves the other's copy alone.
        second.remove(hashOf(bytes));
        profileDirectory = alpha;
        QCOMPARE(first.get(hashOf(bytes))->data(), bytes);

        FileDatabase::retireLegacyDirectory(legacy, { QStringLiteral("alpha"), QStringLiteral("beta") });
        QVERIFY2(!QDir(legacy).exists(), "every profile has imported");
        QCOMPARE(first.get(hashOf(bytes))->data(), bytes);
    }

    void aDirectoryNoProfileImportedFromIsNotRemoved()
    {
        QTemporaryDir unrelated;
        QFile         file(unrelated.filePath(QStringLiteral("somebody-elses-file")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        FileDatabase::retireLegacyDirectory(unrelated.path(), {});
        QVERIFY(file.exists());
    }
};

int main(int argc, char **argv)
{
    int result;
    {
        QCoreApplication app(argc, argv);
        FileCacheTest    test;
        result = QTest::qExec(&test, argc, argv);

        // A cache destroyed with the application must not touch the database, which
        // Qt has dropped by then. The Bits of Binary cache used to crash this way.
        QTemporaryDir last;
        profileDirectory      = last.path();
        auto            *late = new FileCache(partitionName, last.path(), &app);
        const QByteArray bytes("still pending at exit");
        late->append(hashOf(bytes), bytes);
    }
    return result;
}
#include "filecache.moc"
