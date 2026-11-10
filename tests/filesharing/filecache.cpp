/* SPDX-License-Identifier: GPL-2.0-or-later */
// What FileCache promises to avatars, Bits of Binary and shared files.
#include "filecache.h"
#include "applicationinfo.h"
#include "filedatabase.h"
#include "fileutil.h"
#include "optionstree.h"
#include <QDataStream>
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
    QString legacyPayloadPath(const Hash &hash) const { return dir->filePath(QString::fromLatin1(hash.toHex())); }

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

    void emptyItemHasARealPayload()
    {
        auto       cache = open();
        const auto hash  = hashOf({});
        const auto path  = cache->append(hash, {})->filePath();
        cache->sync();
        QVERIFY(cache->get(hash)->isOnDisk());
        QVERIFY(QFile::exists(path));
        QCOMPARE(QFileInfo(path).size(), qint64(0));
        QVERIFY(cache->get(hash)->data().isEmpty());
        cache.reset();
        cache = open();
        QVERIFY(cache->get(hash));
        QCOMPARE(cache->get(hash)->filePath(), path);
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
            const auto path  = cache->append(hash, bytes)->filePath();
            cache->sync();
            QVERIFY(QFile::exists(path));
            cache->remove(hash);
            QVERIFY(!cache->get(hash));
            QVERIFY(!QFile::exists(path));
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

    void replacementUsesANewPayloadGeneration()
    {
        const QByteArray bytes("same content, new generation");
        const auto       hash  = hashOf(bytes);
        auto             cache = open();
        const auto       old   = cache->append(hash, bytes)->filePath();
        cache->sync();
        const auto current = cache->append(hash, bytes)->filePath();
        QVERIFY(current != old);
        cache->sync();
        QCOMPARE(recordCount(), 1);
        QVERIFY(!QFileInfo::exists(old));
        QSqlQuery query(FileDatabase::connection());
        query.prepare("INSERT INTO file_trash(partition,path) VALUES(?,?)");
        query.addBindValue(partitionName);
        query.addBindValue(QFileInfo(old).fileName());
        QVERIFY(query.exec());
        cache->sync();
        QVERIFY(QFileInfo::exists(current));
        QCOMPARE(cache->getData(hash), bytes);
    }

    void publishingCanPersistOwnershipWithoutEvictingItsPayload()
    {
        auto cache = open();
        cache->setFileCacheSize(0);
        const QByteArray bytes("payload being published");
        const auto       hash = hashOf(bytes);
        auto             item = cache->append(hash, bytes);
        const auto       path = item->filePath();
        QVERIFY(cache->persist(hash));
        QVERIFY(item->isRegistered());
        QVERIFY(QFileInfo::exists(path));
        // A separate connection sees the committed owner before the catalog
        // writes its own publication transaction.
        {
            auto reader = QSqlDatabase::addDatabase("QSQLITE", "publication-owner-proof");
            reader.setDatabaseName(dir->filePath("files.sqlite"));
            QVERIFY(reader.open());
            QSqlQuery query(reader);
            QVERIFY(query.exec("SELECT COUNT(*) FROM files"));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 1);
            query.finish();
            reader.close();
        }
        QSqlDatabase::removeDatabase("publication-owner-proof");
        QCOMPARE(item->data(), bytes);
    }

    void aliasesCountOnceForMemoryAndDiskLimits()
    {
        const QByteArray  bytes(100, 'a');
        const QList<Hash> sums { hashOf(bytes), Hash::from(Hash::Sha256, bytes) };
        auto              cache = open();
        cache->setFileCacheSize(150);
        cache->setMemoryCacheSize(150);
        auto item = cache->append(sums, bytes);
        cache->sync();
        QCOMPARE(cache->get(sums.first()), item);
        QCOMPARE(cache->get(sums.last()), item);
        QVERIFY(item->inMemory());
        QCOMPARE(recordCount(), 1);
    }

    void externalOriginalSurvivesQuotaRemovalAndRestart()
    {
        QTemporaryDir    originalDirectory;
        const auto       path = originalDirectory.filePath("original.bin");
        const QByteArray bytes("original bytes");
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(bytes), qint64(bytes.size()));
        }
        const QList<Hash> sums { hashOf(bytes) };
        {
            auto cache = open();
            cache->setFileCacheSize(0);
            auto item = cache->trackExternal(sums, QFileInfo(path));
            QVERIFY(item);
            QVERIFY(item->isExternal());
            QCOMPARE(item->filePath(), path);
            QCOMPARE(item->size(), quint64(bytes.size()));
            cache->sync();
            QVERIFY(cache->get(sums.first()));
        }
        auto cache = open();
        auto item  = cache->get(sums.first());
        QVERIFY(item);
        QVERIFY(item->isExternal());
        QCOMPARE(item->filePath(), path);
        QCOMPARE(item->data(), bytes);
        cache->remove(sums.first());
        QVERIFY(!cache->get(sums.first()));
        QVERIFY(QFileInfo::exists(path));
    }

    void changedExternalOriginalIsUnboundWithoutDeletingIt()
    {
        QTemporaryDir originalDirectory;
        const auto    path = originalDirectory.filePath("original.bin");
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("before");
        }
        const auto hash  = hashOf("before");
        auto       cache = open();
        QVERIFY(cache->trackExternal({ hash }, QFileInfo(path)));
        cache->sync();
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("different bytes");
        }
        QVERIFY(!cache->get(hash));
        QCOMPARE(recordCount(), 0);
        QVERIFY(QFileInfo::exists(path));
    }

    void externalTrackingKeepsExistingManagedOwnership()
    {
        auto             cache = open();
        const QByteArray bytes("already managed");
        const auto       hash = hashOf(bytes);
        auto             item = cache->append(hash, bytes);
        cache->sync();
        const auto path = item->filePath();
        QCOMPARE(cache->trackExternal({ hash }, QFileInfo(path)), item);
        QVERIFY(!item->isExternal());
        QVERIFY(QFileInfo::exists(path));
        QCOMPARE(item->data(), bytes);
    }

    void movingAnExistingManagedFileCreatesANewGeneration()
    {
        auto             cache = open();
        const QByteArray bytes("move existing generation");
        const auto       hash = hashOf(bytes);
        const auto       old  = cache->append(hash, bytes)->filePath();
        cache->sync();
        auto item = cache->moveToCache(hash, QFileInfo(old));
        QVERIFY(item);
        QVERIFY(item->filePath() != old);
        cache->sync();
        QVERIFY(!QFileInfo::exists(old));
        QCOMPARE(item->data(), bytes);
        QCOMPARE(recordCount(), 1);
    }

    void retirementFailurePreservesTheRecordAndPayload()
    {
        const QByteArray bytes("retirement rollback");
        const auto       hash  = hashOf(bytes);
        auto             cache = open();
        const auto       path  = cache->append(hash, bytes)->filePath();
        cache->sync();
        QSqlQuery query(FileDatabase::connection());
        QVERIFY(query.exec("CREATE TRIGGER fail_retire BEFORE DELETE ON files BEGIN SELECT RAISE(ABORT,'test'); END"));
        cache->remove(hash);
        QVERIFY(cache->get(hash));
        QVERIFY(QFileInfo::exists(path));
        QCOMPARE(recordCount(), 1);
        QVERIFY(query.exec("SELECT COUNT(*) FROM file_trash"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
        query.finish();
        QVERIFY(query.exec("DROP TRIGGER fail_retire"));
        cache->remove(hash);
        QVERIFY(!cache->get(hash));
        QVERIFY(!QFileInfo::exists(path));
    }

    void failedRegistrationCanBeRetried()
    {
        auto      cache = open();
        QSqlQuery query(FileDatabase::connection());
        QVERIFY(
            query.exec("CREATE TRIGGER fail_register BEFORE INSERT ON files BEGIN SELECT RAISE(ABORT,'test'); END"));
        const QByteArray bytes("retry registration");
        auto             item = cache->append(hashOf(bytes), bytes);
        cache->sync();
        QVERIFY(item->isOnDisk());
        QVERIFY(!item->isRegistered());
        QCOMPARE(recordCount(), 0);
        QCOMPARE(item->data(), bytes);
        QVERIFY(query.exec("DROP TRIGGER fail_register"));
        QTRY_VERIFY_WITH_TIMEOUT(item->isRegistered(), 6500);
        QCOMPARE(recordCount(), 1);
    }

    void failedPayloadWriteKeepsTheOnlyCopyInMemory()
    {
        auto cache = open();
        cache->setMemoryCacheSize(0);
        const QByteArray bytes("unsaved payload");
        auto             item = cache->append(hashOf(bytes), bytes);
        const auto       path = item->filePath();
        QVERIFY(QDir().mkpath(path));
        cache->sync();
        QVERIFY(!item->isOnDisk());
        QVERIFY(!item->isRegistered());
        QVERIFY(item->inMemory());
        QCOMPARE(item->data(), bytes);
        QCOMPARE(recordCount(), 0);
        QVERIFY(QDir().rmdir(path));
        cache->sync();
        QVERIFY(item->isOnDisk());
        QVERIFY(item->isRegistered());
        QVERIFY(!item->inMemory());
        QCOMPARE(item->data(), bytes);
        QVERIFY(!item->inMemory()); // A read larger than the RAM limit does not pin a second copy.
    }

    void damagedManagedPayloadIsNotReturnedAsACacheHit()
    {
        auto             cache = open();
        const QByteArray bytes("expected complete payload");
        const auto       hash = hashOf(bytes);
        const auto       path = cache->append(hash, bytes)->filePath();
        cache->sync();
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("truncated");
        }
        QVERIFY(!cache->get(hash));
        QCOMPARE(recordCount(), 0);
    }

    void sqlitePathsMigrateWithoutMovingManagedPayloads()
    {
        const QByteArray bytes("old SQLite payload");
        const auto       hash       = hashOf(bytes);
        const auto       legacyPath = legacyPayloadPath(hash);
        {
            QFile file(legacyPath);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(bytes);
        }
        QTemporaryDir originals;
        const auto    original = originals.filePath("original");
        {
            QFile file(original);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("changed original");
        }
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", "pre-placement-schema");
            db.setDatabaseName(dir->filePath("files.sqlite"));
            QVERIFY(db.open());
            QSqlQuery query(db);
            QVERIFY(query.exec("CREATE TABLE files(id BLOB PRIMARY KEY,partition TEXT NOT NULL,size INTEGER NOT NULL,"
                               "created INTEGER NOT NULL,max_age INTEGER NOT NULL,metadata BLOB NOT NULL)"));
            QVERIFY(query.exec("CREATE TABLE file_hashes(file_id BLOB NOT NULL REFERENCES files(id) ON DELETE CASCADE,"
                               "position INTEGER NOT NULL,algorithm TEXT NOT NULL,digest BLOB NOT NULL,"
                               "PRIMARY KEY(file_id,position))"));
            QVERIFY(query.exec("PRAGMA user_version=2"));
            for (int index = 1; index <= 2; ++index) {
                QVariantMap metadata;
                if (index == 2)
                    metadata.insert("link", original);
                QByteArray  encoded;
                QDataStream stream(&encoded, QIODevice::WriteOnly);
                stream.setVersion(QDataStream::Qt_5_15);
                stream << metadata;
                const auto sum = index == 1 ? hash : hashOf("old original");
                const auto id  = QByteArray(16, char(index));
                query.prepare("INSERT INTO files VALUES(?,?,?,?,?,?)");
                query.addBindValue(id);
                query.addBindValue(partitionName);
                query.addBindValue(index == 1 ? bytes.size() : 0);
                query.addBindValue(QDateTime::currentMSecsSinceEpoch());
                query.addBindValue(qlonglong(FileCache::Forever));
                query.addBindValue(encoded);
                QVERIFY(query.exec());
                query.prepare("INSERT INTO file_hashes VALUES(?,?,?,?)");
                query.addBindValue(id);
                query.addBindValue(0);
                query.addBindValue(sum.stringType());
                query.addBindValue(sum.data());
                QVERIFY(query.exec());
            }
            db.close();
        }
        QSqlDatabase::removeDatabase("pre-placement-schema");
        auto cache = open();
        auto item  = cache->get(hash);
        QVERIFY(item);
        QCOMPARE(item->filePath(), legacyPath);
        QCOMPARE(item->data(), bytes);
        {
            QSqlQuery stamp(FileDatabase::connection());
            QVERIFY(stamp.exec("SELECT external_mtime FROM files WHERE placement=1"));
            QVERIFY(stamp.next());
            QVERIFY(stamp.value(0).isNull()); // Unknown is distinct from a valid timestamp at the Unix epoch.
        }
        QVERIFY(!cache->get(hashOf("old original"))); // No stamp existed to prove the old original is unchanged.
        QVERIFY(QFileInfo::exists(original));
        QSqlQuery query(FileDatabase::connection());
        QVERIFY(query.exec("SELECT path,placement FROM files"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QFileInfo(legacyPath).fileName());
        QCOMPARE(query.value(1).toInt(), 0);
        QVERIFY(!query.next());
        query.finish();
        cache->remove(hash);
        QVERIFY(!QFileInfo::exists(legacyPath));
        QVERIFY(QFileInfo::exists(original));
    }

    void failedUnlinkRemainsInTrashUntilItCanBeRemoved()
    {
        const QByteArray bytes("deferred removal");
        const auto       hash  = hashOf(bytes);
        auto             cache = open();
        const auto       path  = cache->append(hash, bytes)->filePath();
        cache->sync();
        QVERIFY(QFile::remove(path));
        QVERIFY(QDir().mkpath(path)); // Portable unlink failure: a file became a non-empty directory.
        {
            QFile child(path + "/in-use");
            QVERIFY(child.open(QIODevice::WriteOnly));
            child.write("busy");
        }
        cache->remove(hash);
        QVERIFY(!cache->get(hash));
        QCOMPARE(recordCount(), 0);
        QSqlQuery query(FileDatabase::connection());
        QVERIFY(query.exec("SELECT COUNT(*) FROM file_trash"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
        query.finish();
        QVERIFY(QDir(path).removeRecursively());
        cache.reset();
        cache = open();
        QVERIFY(query.exec("SELECT COUNT(*) FROM file_trash"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
    }

    void trashCannotRemoveALiveGenerationOrEscapeTheRoot()
    {
        QTemporaryDir external;
        const auto    original = external.filePath("original");
        {
            QFile file(original);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("original");
        }
        auto             cache = open();
        const QByteArray bytes("live generation");
        const auto       path = cache->append(hashOf(bytes), bytes)->filePath();
        cache->sync();
        QSqlQuery query(FileDatabase::connection());
        for (const auto &entry : { QFileInfo(path).fileName(), original, QString("../outside") }) {
            query.prepare("INSERT INTO file_trash(partition,path) VALUES(?,?)");
            query.addBindValue(partitionName);
            query.addBindValue(entry);
            QVERIFY(query.exec());
        }
        cache->sync();
        QVERIFY(QFileInfo::exists(path));
        QVERIFY(QFileInfo::exists(original));
        QCOMPARE(cache->getData(hashOf(bytes)), bytes);
    }

    void renewalWritesOnlyAfterHalfTheLifetime()
    {
        auto       cache = open();
        const auto hash  = hashOf("renewal");
        cache->append(hash, "renewal", {}, 10);
        cache->sync();
        QSqlQuery query(FileDatabase::connection());
        QVERIFY(query.exec("SELECT total_changes()"));
        QVERIFY(query.next());
        const auto before = query.value(0).toLongLong();
        query.finish();
        QVERIFY(cache->get(hash, true));
        QVERIFY(query.exec("SELECT total_changes()"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toLongLong(), before);
        query.finish();
        cache.reset();
        const auto old = QDateTime::currentDateTime().addSecs(-6).toMSecsSinceEpoch();
        query.prepare("UPDATE files SET created=?");
        query.addBindValue(old);
        QVERIFY(query.exec());
        cache     = open();
        auto item = cache->get(hash, true);
        QVERIFY(item);
        QVERIFY(item->created().toMSecsSinceEpoch() > old + 5000);
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
            QVERIFY(file.flush());
            QVERIFY(file.setFileTime(QDateTime::currentDateTime().addYears(-3), QFileDevice::FileModificationTime));
        }
        const auto modified = QFileInfo(source).lastModified();
        QVERIFY(modified < QDateTime::currentDateTime().addYears(-2));
        {
            auto  cache = open();
            const auto inserted = QDateTime::currentDateTime();
            auto      *item     = cache->moveToCache(hash, QFileInfo(source), {}, 60);
            QVERIFY(item);
            QVERIFY(item->isOnDisk());
            QVERIFY(item->created() >= inserted);
            QCOMPARE(QFileInfo(item->filePath()).lastModified(), modified);
            QVERIFY(cache->get(hash));
            QVERIFY(!QFile::exists(source));
            QVERIFY(QFile::exists(item->filePath()));
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
        const auto       path  = cache->append(hash, bytes, {}, 1)->filePath();
        cache->sync();
        QVERIFY(cache->get(hash));
        QTest::qWait(1100);
        QVERIFY(!cache->get(hash));
        QVERIFY(!QFile::exists(path));
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
        QString          path;
        {
            auto cache = open();
            path       = cache->append(hash, bytes, {}, FileCache::Session)->filePath();
            cache->sync();
            QVERIFY(cache->get(hash));
        }
        QVERIFY(!QFile::exists(path));
        QVERIFY(!open()->get(hash));
    }

    void gcDropsAnItemWhosePayloadIsGone()
    {
        const QByteArray bytes("payload deleted behind the cache");
        const auto       hash  = hashOf(bytes);
        auto             cache = open();
        cache->append(hash, bytes);
        cache->sync();
        QVERIFY(QFile::remove(cache->get(hash)->filePath()));
        cache->gc();
        QVERIFY(!cache->get(hash));
    }

    void oldestItemsAreEvictedOverQuota()
    {
        const QByteArray oldest(100, 'a'), middle(100, 'b'), newest(100, 'c');
        auto             cache = open();
        cache->setFileCacheSize(250);
        const auto oldestPath = cache->append(hashOf(oldest), oldest)->filePath();
        QTest::qWait(5);
        cache->append(hashOf(middle), middle);
        QTest::qWait(5);
        cache->append(hashOf(newest), newest);
        cache->sync(); // Puts them on disk, and the next pass counts them.
        cache->sync();
        QVERIFY(!cache->get(hashOf(oldest)));
        QVERIFY(!QFile::exists(oldestPath));
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
        QVERIFY(QFile::exists(item->filePath()));
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
        const auto        original = legacy.filePath("original.bin");
        {
            QFile file(original);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(reference);
        }
        OptionsTree       registry;
        addLegacyItem(registry, legacy.path(), { sha1, sha256 }, bytes, metadata, FileCache::Forever);
        addLegacyItem(registry, legacy.path(), { hashOf(reference) }, {}, { { QStringLiteral("link"), original } },
                      3600);
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
            QVERIFY(QFile::exists(item->filePath()));
            QVERIFY2(QFile::exists(legacy.filePath(QString::fromLatin1(sha1.toHex()))), "another profile may need it");
            QVERIFY(!cache->get(hashOf(reference))); // An unstamped legacy link cannot certify today's bytes.
            QVERIFY(QFileInfo::exists(original));
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
            QFile partial(legacyPayloadPath(hashOf(bytes)));
            QVERIFY(partial.open(QIODevice::WriteOnly));
            partial.write(bytes.left(3));
        }
        auto cache = open();
        cache->importLegacy(legacy.path());
        QCOMPARE(cache->get(hashOf(bytes))->data(), bytes);
    }

    void legacyImportDoesNotOverwriteAnExistingAlias()
    {
        QTemporaryDir    legacy;
        const QByteArray bytes("existing profile payload");
        const auto       sha1 = hashOf(bytes), sha256 = Hash::from(Hash::Sha256, bytes);
        OptionsTree      registry;
        addLegacyItem(registry, legacy.path(), { sha1, sha256 }, bytes, {}, FileCache::Forever);
        QVERIFY(saveLegacyRegistry(registry, legacy.path()));
        auto cache = open();
        auto item  = cache->append(sha256, bytes, { { "owner", "profile" } });
        cache->sync();
        const auto path = item->filePath();
        cache->importLegacy(legacy.path());
        QCOMPARE(cache->get(sha256), item);
        QCOMPARE(item->filePath(), path);
        QCOMPARE(item->metadata().value("owner").toString(), QString("profile"));
        QCOMPARE(recordCount(), 1);
    }

    void failedLegacyImportRollsBackCopiesAndCanBeRetried()
    {
        QTemporaryDir    legacy;
        const QByteArray bytes("legacy import rollback");
        const auto       hash = hashOf(bytes);
        OptionsTree      registry;
        addLegacyItem(registry, legacy.path(), { hash }, bytes, {}, FileCache::Forever);
        QVERIFY(saveLegacyRegistry(registry, legacy.path()));
        auto      cache = open();
        QSqlQuery query(FileDatabase::connection());
        QVERIFY(query.exec("CREATE TRIGGER fail_import BEFORE INSERT ON files BEGIN SELECT RAISE(ABORT,'test'); END"));
        const auto before = QDir(dir->path()).entryList(QDir::Files);
        cache->importLegacy(legacy.path());
        QVERIFY(!cache->get(hash));
        QCOMPARE(recordCount(), 0);
        QCOMPARE(QDir(dir->path()).entryList(QDir::Files), before);
        QVERIFY(query.exec("SELECT COUNT(*) FROM file_legacy_imports"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
        query.finish();
        QVERIFY(query.exec("DROP TRIGGER fail_import"));
        cache->importLegacy(legacy.path());
        QVERIFY(cache->get(hash));
        QCOMPARE(cache->getData(hash), bytes);
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
