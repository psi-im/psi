#include "filedatabase.h"
#include "filesourceselector.h"
#include "mediachunkstore.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <atomic>
#include <cstdlib>

namespace {
class RangeDatabase {
public:
    explicit RangeDatabase(const QString &directory) : path(directory + QStringLiteral("/files.sqlite")) { }
    ~RangeDatabase() { FileDatabase::closeConnection(path); }
    QVariant value(const QString &sql) const
    {
        QSqlQuery query(FileDatabase::connection(path));
        if (!query.exec(sql) || !query.next())
            return {};
        return query.value(0);
    }
    bool exec(const QString &sql) const
    {
        QSqlQuery query(FileDatabase::connection(path));
        return query.exec(sql);
    }
    QString path;
};
}

class FileStoreTest : public QObject {
    Q_OBJECT
    const QString     key = QString(64, QLatin1Char('a'));
    MediaFileChecksum checksum(quint64 offset, QByteArray bytes)
    {
        return { offset, quint64(bytes.size()), QStringLiteral("sha-256"),
                 QCryptographicHash::hash(bytes, QCryptographicHash::Sha256) };
    }
private slots:
    void activeAndExplicitPartialFilesSurviveEviction()
    {
        QTemporaryDir   directory;
        MediaChunkStore store(directory.path(), 16);
        const QString   other(64, QLatin1Char('b'));
        int             completed = 0;
        auto            done      = [&](QByteArray, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            ++completed;
        };
        store.configure(key, 32, { }, { { "retain-partial", true } });
        store.write(key, 32, 0, QByteArray(16, 'a'), done);
        QTRY_COMPARE(completed, 1);
        store.pin(other, true);
        store.configure(other, 16, { }, { });
        store.write(other, 16, 0, QByteArray(16, 'b'), done);
        QTRY_COMPARE(completed, 2);
        QCOMPARE(store.state(key).ranges, (MediaFileRanges { { 0, 16 } }));
        QVERIFY(store.state(other).complete);
        store.pin(other, false);
        QTRY_VERIFY(store.state(other).path.isEmpty());
        QVERIFY(!store.state(key).path.isEmpty());
        store.write(key, 32, 16, QByteArray(16, 'a'), done);
        QTRY_COMPARE(completed, 3);
        QVERIFY(store.state(key).complete);
    }
    void verificationRunsOnWorker()
    {
        QTemporaryDir     directory;
        std::atomic<bool> worker { false };
        const auto        ownerThread = QThread::currentThread();
        MediaChunkStore   store(directory.path(), 1024, nullptr, [&](const QString &, QIODevice *input) {
            worker = QThread::currentThread() != ownerThread;
            return QCryptographicHash::hash(input->readAll(), QCryptographicHash::Sha256);
        });
        const QByteArray  data("worker-thread hash");
        bool              done = false;
        store.configure(key, data.size(), { checksum(0, data) }, { });
        store.write(key, data.size(), 0, data, [&](QByteArray, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            done = true;
        });
        QTRY_VERIFY(done);
        QVERIFY(worker);
    }
    void emptyFileIsCompleteAndHasMetadata()
    {
        QTemporaryDir   directory;
        MediaChunkStore store(directory.path(), 1024);
        bool            done = false;
        store.configure(key, 0, { checksum(0, { }) }, { }, [&](QByteArray, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            done = true;
        });
        QTRY_VERIFY(done);
        QVERIFY(store.state(key).complete);
        QCOMPARE(QFileInfo(store.state(key).path).size(), qint64(0));
    }
    void multipleSourcesUseCapacityAndAvoidSlowPeers()
    {
        FileSourceSelector sources;
        const QStringList  candidates { "fast", "slow", "third" };
        QCOMPARE(sources.acquire(candidates), QString("fast"));
        QCOMPARE(sources.acquire(candidates), QString("slow"));
        sources.release("fast", 1024, 1, true, false);
        QCOMPARE(sources.acquire(candidates), QString("third"));
        sources.release("third", 1024, 10, true, false);
        sources.release("slow", 1024, 1000, true, false);
        QCOMPARE(sources.acquire(candidates), QString("fast"));
        QCOMPARE(sources.acquire(candidates), QString("fast")); // Another fast request beats the idle slow peer.
        sources.release("fast", 0, 1, false, false);
        QCOMPARE(sources.acquire(candidates), QString("third"));
    }
    void singleFileAndMergedRanges()
    {
        QTemporaryDir    directory;
        RangeDatabase    registry(directory.path());
        const QByteArray data("0123456789abcdefghijklmnopqrstuv");
        MediaChunkStore  store(directory.path(), 1024);
        int              completed = 0;
        store.configure(key, data.size(), { checksum(0, data) }, { { "name", "sample.bin" } });
        auto done = [&](QByteArray, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            ++completed;
        };
        store.write(key, data.size(), 16, data.mid(16), done);
        QTRY_COMPARE(completed, 1);
        QCOMPARE(store.state(key).ranges, (MediaFileRanges { { 16, 16 } }));
        QVERIFY(!store.state(key).complete);
        store.write(key, data.size(), 0, data.left(8), done);
        QTRY_COMPARE(completed, 2);
        QCOMPARE(store.state(key).ranges, (MediaFileRanges { { 0, 8 }, { 16, 16 } }));
        store.write(key, data.size(), 8, data.mid(8, 8), done);
        QTRY_COMPARE(completed, 3);
        const auto state = store.state(key);
        QCOMPARE(state.ranges, (MediaFileRanges { { 0, 32 } }));
        QVERIFY(state.complete);
        QFile payload(state.path);
        QVERIFY(payload.open(QIODevice::ReadOnly));
        QCOMPARE(payload.readAll(), data);
        QCOMPARE(QDir(directory.path()).entryList({ "*.data" }, QDir::Files).size(), 1);
        QVERIFY(QDir(directory.path()).entryList({ "*.json" }, QDir::Files).isEmpty());
        auto      db = FileDatabase::connection(directory.filePath("files.sqlite"));
        QSqlQuery query(db);
        QVERIFY(query.exec("SELECT count(*) FROM file_ranges"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
        QVERIFY(query.exec("SELECT count(*) FROM file_checksums WHERE verified=1"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
        QVERIFY(query.exec("SELECT metadata FROM files"));
        QVERIFY(query.next());
        QCOMPARE(FileDatabase::decodeMetadata(query.value(0).toByteArray()).value("name").toString(),
                 QString("sample.bin"));
    }
    void mismatchInvalidatesWholeHashSpanAndRequiresResume()
    {
        QTemporaryDir    directory;
        MediaChunkStore  store(directory.path(), 1024);
        const QByteArray bytes("0123456789abcdefghijklmnopqrstuv");
        store.configure(key, bytes.size(), { checksum(8, bytes.mid(8, 16)) }, { });
        int completed = 0;
        store.write(key, bytes.size(), 0, bytes.left(16), [&](QByteArray, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            ++completed;
        });
        QTRY_COMPARE(completed, 1);
        store.write(key, bytes.size(), 16, QByteArray(16, 'x'), [&](QByteArray data, MediaChunkError error) {
            QVERIFY(data.isEmpty());
            QCOMPARE(error.code(), MediaChunkError::IntegrityError);
            ++completed;
        });
        QTRY_COMPARE(completed, 2);
        QCOMPARE(store.state(key).ranges, (MediaFileRanges { { 0, 8 }, { 24, 8 } }));
        QVERIFY(!store.state(key).error.isEmpty());
        store.write(key, bytes.size(), 8, bytes.mid(8, 16), [&](QByteArray, MediaChunkError error) {
            QCOMPARE(error.code(), MediaChunkError::IntegrityError);
            ++completed;
        });
        QTRY_COMPARE(completed, 3);
        store.resume(key, [&](QByteArray, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            ++completed;
        });
        QTRY_COMPARE(completed, 4);
        store.write(key, bytes.size(), 8, bytes.mid(8, 16), [&](QByteArray, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            ++completed;
        });
        QTRY_COMPARE(completed, 5);
        QVERIFY(store.state(key).complete);
    }
    void lateChecksumAndRestart()
    {
        QTemporaryDir    directory;
        const QByteArray data("0123456789abcdef");
        int              completed = 0;
        QString          payloadPath;
        {
            MediaChunkStore store(directory.path(), 1024);
            store.write(key, data.size(), 0, data, [&](QByteArray, MediaChunkError error) {
                QVERIFY(error.isEmpty());
                ++completed;
            });
            QTRY_COMPARE(completed, 1);
            store.configure(key, data.size(), { checksum(0, data) }, { }, [&](QByteArray, MediaChunkError error) {
                QVERIFY(error.isEmpty());
                ++completed;
            });
            QVERIFY(!store.state(key).complete); // New hashes withdraw completion until worker verification.
            QTRY_COMPARE(completed, 2);
            QVERIFY(store.state(key).complete);
            payloadPath = store.state(key).path;
        }
        QFile payload(payloadPath);
        QVERIFY(payload.open(QIODevice::ReadWrite));
        QCOMPARE(payload.write("x", 1), qint64(1));
        QVERIFY(payload.flush());
        QVERIFY(payload.setFileTime(QDateTime::currentDateTime().addSecs(2), QFileDevice::FileModificationTime));
        payload.close();
        MediaChunkStore reopened(directory.path(), 1024);
        reopened.read(key, data.size(), 0, data.size(), [&](QByteArray bytes, MediaChunkError error) {
            QVERIFY(bytes.isEmpty());
            QCOMPARE(error.code(), MediaChunkError::IntegrityError);
            ++completed;
        });
        QTRY_COMPARE(completed, 3);
        QVERIFY(reopened.state(key).ranges.isEmpty());
    }
    void deletedPayloadIsNotResizedAndTrusted()
    {
        QTemporaryDir    directory;
        MediaChunkStore  store(directory.path(), 1024);
        const QByteArray bytes("0123456789abcdef");
        int              completed = 0;
        store.configure(key, bytes.size(), { checksum(0, bytes) }, { });
        store.write(key, bytes.size(), 0, bytes, [&](QByteArray, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            ++completed;
        });
        QTRY_COMPARE(completed, 1);
        QVERIFY(QFile::remove(store.state(key).path));
        store.configure(key, bytes.size(), { checksum(0, bytes) }, { }, [&](QByteArray, MediaChunkError error) {
            QCOMPARE(error.code(), MediaChunkError::IntegrityError);
            ++completed;
        });
        QTRY_COMPARE(completed, 2);
        QVERIFY(!store.state(key).complete);
        QVERIFY(store.state(key).ranges.isEmpty());
        QCOMPARE(QFileInfo(store.state(key).path).size(), qint64(0)); // Never fill deleted verified data with zeroes.
    }
    void truncatedPayloadKeepsChecksumSpansAndRequiresRetry()
    {
        QTemporaryDir    directory;
        const QByteArray data("0123456789abcdef");
        int              completed = 0;
        QString          payloadPath;
        {
            MediaChunkStore store(directory.path(), 1024);
            store.configure(key, data.size(), { checksum(8, data.mid(8)) }, { });
            store.write(key, data.size(), 0, data, [&](QByteArray, MediaChunkError error) {
                QVERIFY(error.isEmpty());
                ++completed;
            });
            QTRY_COMPARE(completed, 1);
            payloadPath = store.state(key).path;
        }
        QFile payload(payloadPath);
        QVERIFY(payload.open(QIODevice::ReadWrite));
        QVERIFY(payload.resize(12));
        payload.close();
        MediaChunkStore reopened(directory.path(), 1024);
        reopened.read(key, data.size(), 0, 4, [&](QByteArray, MediaChunkError error) {
            QCOMPARE(error.code(), MediaChunkError::IntegrityError);
            ++completed;
        });
        QTRY_COMPARE(completed, 2);
        QCOMPARE(reopened.state(key).ranges, (MediaFileRanges { { 0, 8 } }));
        QCOMPARE(reopened.state(key).checksums.size(), 1);
    }
    void overlappingChecksumsAndConflictingAnnouncementsPreserveAllHashes()
    {
        QTemporaryDir    directory;
        const QByteArray data("0123456789abcdef");
        MediaChunkStore  store(directory.path(), 1024);
        int              completed = 0;
        store.configure(key, data.size(), { checksum(0, data), checksum(4, data.mid(4, 8)) }, { });
        store.write(key, data.size(), 0, data, [&](QByteArray, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            ++completed;
        });
        QTRY_COMPARE(completed, 1);
        store.configure(key, data.size(), { checksum(4, QByteArray(8, 'x')) }, { },
                        [&](QByteArray, MediaChunkError error) {
                            QCOMPARE(error.code(), MediaChunkError::IntegrityError);
                            ++completed;
                        });
        QTRY_COMPARE(completed, 2);
        QCOMPARE(store.state(key).ranges, (MediaFileRanges { { 0, 4 }, { 12, 4 } }));
        QVERIFY(!store.state(key).complete);
    }
    void legacySidecarsAreIgnoredAndNeverOverwritten()
    {
        QTemporaryDir directory;
        QFile         oldData(directory.filePath(key + ".data"));
        QVERIFY(oldData.open(QIODevice::WriteOnly));
        QCOMPARE(oldData.write("old!"), qint64(4));
        oldData.close();
        QFile            sidecar(directory.filePath(key + ".json"));
        const QByteArray json(R"({"version":2,"size":"4","ranges":[["0","4"]],"checksums":[],"metadata":{}})");
        QVERIFY(sidecar.open(QIODevice::WriteOnly));
        QCOMPARE(sidecar.write(json), qint64(json.size()));
        sidecar.close();
        MediaChunkStore store(directory.path(), 1024);
        int             completed = 0;
        store.read(key, 4, 0, 4, [&](QByteArray bytes, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            QVERIFY(bytes.isEmpty());
            ++completed;
        });
        QTRY_COMPARE(completed, 1);
        store.write(key, 4, 0, "new!", [&](QByteArray bytes, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            QCOMPARE(bytes, QByteArray("new!"));
            ++completed;
        });
        QTRY_COMPARE(completed, 2);
        QVERIFY(store.state(key).path != oldData.fileName());
        QVERIFY(oldData.open(QIODevice::ReadOnly));
        QCOMPARE(oldData.readAll(), QByteArray("old!"));
        QVERIFY(sidecar.open(QIODevice::ReadOnly));
        QCOMPARE(sidecar.readAll(), json);
    }
    void verifiedFilesAreNotRehashedAtStartupOrOnAnUnchangedHit()
    {
        QTemporaryDir    directory;
        RangeDatabase    registry(directory.path());
        const QByteArray bytes("verified once");
        std::atomic<int> hashes { 0 };
        auto             hasher = [&](const QString &, QIODevice *input) {
            ++hashes;
            return QCryptographicHash::hash(input->readAll(), QCryptographicHash::Sha256);
        };
        int     completed = 0;
        QString path;
        {
            MediaChunkStore store(directory.path(), 1024, nullptr, hasher);
            store.configure(key, bytes.size(), { checksum(0, bytes) }, {});
            store.write(key, bytes.size(), 0, bytes, [&](QByteArray, MediaChunkError error) {
                QVERIFY(error.isEmpty());
                ++completed;
            });
            QTRY_COMPARE(completed, 1);
            path = store.state(key).path;
            QCOMPARE(hashes.load(), 1);
        }
        MediaChunkStore reopened(directory.path(), 1024, nullptr, hasher);
        QTRY_VERIFY(reopened.state(key).complete);
        QCOMPARE(reopened.state(key).path, path);
        QCOMPARE(hashes.load(), 1);
        QVERIFY(registry.exec("CREATE TRIGGER no_hit_rewrite BEFORE UPDATE ON files "
                              "BEGIN SELECT RAISE(ABORT,'cache hits must not rewrite metadata'); END"));
        reopened.read(key, bytes.size(), 0, bytes.size(), [&](QByteArray data, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            QCOMPARE(data, bytes);
            ++completed;
        });
        QTRY_COMPARE(completed, 2);
        QCOMPARE(hashes.load(), 1);
        QVERIFY(registry.exec("DROP TRIGGER no_hit_rewrite"));
    }
    void partialWritesAreCoalescedAndCheckpointedOnRequest()
    {
        QTemporaryDir   directory;
        RangeDatabase   registry(directory.path());
        MediaChunkStore store(directory.path(), 1024);
        int             completed = 0;
        auto            done      = [&](QByteArray, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            ++completed;
        };
        store.configure(key, 32, {}, {}, done);
        QTRY_COMPARE(completed, 1);
        store.write(key, 32, 0, "abcdefgh", done);
        store.write(key, 32, 8, "ijklmnop", done);
        QTRY_COMPARE(completed, 3);
        QCOMPARE(registry.value("SELECT count(*) FROM file_ranges").toInt(), 0);
        QCOMPARE(store.state(key).ranges, (MediaFileRanges { { 0, 16 } }));
        store.checkpoint(done);
        QTRY_COMPARE(completed, 4);
        QCOMPARE(registry.value("SELECT sum(length) FROM file_ranges").toInt(), 16);
    }
    void unchangedBytesWithANewStampAreVerifiedOnDemandAndBecomeComplete()
    {
        QTemporaryDir    directory;
        const QByteArray bytes("unchanged bytes");
        QString          path;
        int              completed = 0;
        {
            MediaChunkStore store(directory.path(), 1024);
            store.configure(key, bytes.size(), { checksum(0, bytes) }, {});
            store.write(key, bytes.size(), 0, bytes, [&](QByteArray, MediaChunkError error) {
                QVERIFY(error.isEmpty());
                ++completed;
            });
            QTRY_COMPARE(completed, 1);
            path = store.state(key).path;
        }
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(QDateTime::currentDateTime().addSecs(2), QFileDevice::FileModificationTime));
        file.close();
        MediaChunkStore reopened(directory.path(), 1024);
        QTRY_VERIFY(!reopened.state(key).path.isEmpty());
        QVERIFY(!reopened.isCompleteFile(path));
        reopened.read(key, bytes.size(), 0, bytes.size(), [&](QByteArray data, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            QCOMPARE(data, bytes);
            ++completed;
        });
        QTRY_COMPARE(completed, 2);
        QVERIFY(reopened.isCompleteFile(path));
    }
    void checkpointWithdrawsPayloadThatWasTruncatedAfterTheWrite()
    {
        QTemporaryDir   directory;
        RangeDatabase   registry(directory.path());
        MediaChunkStore store(directory.path(), 1024);
        int             completed = 0;
        auto            done      = [&](QByteArray, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            ++completed;
        };
        store.write(key, 32, 0, "abcdefghijklmnop", done);
        QTRY_COMPARE(completed, 1);
        QFile file(store.state(key).path);
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.resize(4));
        file.close();
        store.checkpoint(done);
        QTRY_COMPARE(completed, 2);
        QVERIFY(!store.state(key).error.isEmpty());
        QCOMPARE(store.state(key).ranges, (MediaFileRanges { { 0, 4 } }));
        QCOMPARE(registry.value("SELECT sum(length) FROM file_ranges").toInt(), 4);
        QCOMPARE(QFileInfo(file.fileName()).size(), qint64(4));
    }
    void failedCheckpointIsRetriedWithoutLosingLiveRanges()
    {
        QTemporaryDir   directory;
        RangeDatabase   registry(directory.path());
        MediaChunkStore store(directory.path(), 1024);
        int             completed = 0;
        store.write(key, 32, 0, "abcdefgh", [&](QByteArray, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            ++completed;
        });
        QTRY_COMPARE(completed, 1);
        QVERIFY(registry.exec("CREATE TRIGGER fail_checkpoint BEFORE INSERT ON file_ranges "
                              "BEGIN SELECT RAISE(ABORT,'checkpoint failure'); END"));
        store.checkpoint([&](QByteArray, MediaChunkError error) {
            QVERIFY(!error.isEmpty());
            ++completed;
        });
        QTRY_COMPARE(completed, 2);
        QCOMPARE(registry.value("SELECT count(*) FROM file_ranges").toInt(), 0);
        QCOMPARE(store.state(key).ranges, (MediaFileRanges { { 0, 8 } }));
        QVERIFY(registry.exec("DROP TRIGGER fail_checkpoint"));
        QTRY_COMPARE_WITH_TIMEOUT(registry.value("SELECT sum(length) FROM file_ranges").toInt(), 8, 6500);
    }
    void overwritingRequiresACommittedWithdrawal()
    {
        QTemporaryDir   directory;
        RangeDatabase   registry(directory.path());
        MediaChunkStore store(directory.path(), 1024);
        int             completed = 0;
        auto            done      = [&](QByteArray, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            ++completed;
        };
        store.write(key, 32, 0, "abcdefgh", done);
        store.checkpoint(done);
        QTRY_COMPARE(completed, 2);
        QVERIFY(registry.exec("CREATE TRIGGER fail_withdraw BEFORE DELETE ON file_ranges "
                              "BEGIN SELECT RAISE(ABORT,'withdrawal failure'); END"));
        store.write(key, 32, 0, "XXXXXXXX", [&](QByteArray bytes, MediaChunkError error) {
            QVERIFY(bytes.isEmpty());
            QVERIFY(!error.isEmpty());
            ++completed;
        });
        QTRY_COMPARE(completed, 3);
        QFile payload(store.state(key).path);
        QVERIFY(payload.open(QIODevice::ReadOnly));
        QCOMPARE(payload.readAll(), QByteArray("abcdefgh"));
        payload.close();
        QCOMPARE(registry.value("SELECT sum(length) FROM file_ranges").toInt(), 8);
        QVERIFY(registry.exec("DROP TRIGGER fail_withdraw"));
        store.write(key, 32, 0, "XXXXXXXX", done);
        QTRY_COMPARE(completed, 4);
        // The new bytes are live, but SQL no longer advertises the old checkpoint.
        QCOMPARE(registry.value("SELECT count(*) FROM file_ranges").toInt(), 0);
        QVERIFY(payload.open(QIODevice::ReadOnly));
        QCOMPARE(payload.readAll(), QByteArray("XXXXXXXX"));
    }
    void processCrashDoesNotAdvertiseUncheckpointedBytes()
    {
        const auto childDirectory = qEnvironmentVariable("PSI_RANGE_CRASH_DIRECTORY");
        if (!childDirectory.isEmpty()) {
            MediaChunkStore store(childDirectory, 1024);
            int             completed = 0;
            auto            done      = [&](QByteArray, MediaChunkError error) {
                QVERIFY(error.isEmpty());
                ++completed;
            };
            store.write(key, 32, 0, "abcdefgh", done);
            store.checkpoint(done);
            QTRY_COMPARE(completed, 2);
            store.write(key, 32, 8, "ijklmnop", done);
            QTRY_COMPARE(completed, 3);
            std::_Exit(0); // No QObject destructors, shutdown checkpoint or SQLite close.
        }
        QTemporaryDir directory;
        QProcess      process;
        auto          environment = QProcessEnvironment::systemEnvironment();
        environment.insert("PSI_RANGE_CRASH_DIRECTORY", directory.path());
        process.setProcessEnvironment(environment);
        process.start(QCoreApplication::applicationFilePath(), { "processCrashDoesNotAdvertiseUncheckpointedBytes" });
        QVERIFY(process.waitForFinished(10000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
        MediaChunkStore reopened(directory.path(), 1024);
        int             completed = 0;
        reopened.read(key, 32, 8, 8, [&](QByteArray bytes, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            QVERIFY(bytes.isEmpty());
            ++completed;
        });
        QTRY_COMPARE(completed, 1);
        QCOMPARE(reopened.state(key).ranges, (MediaFileRanges { { 0, 8 } }));
        reopened.read(key, 32, 0, 8, [&](QByteArray bytes, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            QCOMPARE(bytes, QByteArray("abcdefgh"));
            ++completed;
        });
        QTRY_COMPARE(completed, 2);
    }
    void retiredPayloadsUseTheSharedTrashJournalAndFreshGenerations()
    {
        QTemporaryDir directory;
        RangeDatabase registry(directory.path());
        QString       retired;
        const QString other(64, QLatin1Char('b'));
        int           completed = 0;
        auto          done      = [&](QByteArray, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            ++completed;
        };
        {
            MediaChunkStore store(directory.path(), 8);
            store.write(key, 8, 0, "abcdefgh", done);
            QTRY_COMPARE(completed, 1);
            retired = store.state(key).path;
            QVERIFY(QFile::remove(retired));
            QVERIFY(QDir().mkdir(retired)); // Deterministic unlink failure on Unix and Windows.
            store.write(other, 8, 0, "ijklmnop", done);
            QTRY_COMPARE(completed, 2);
            QVERIFY(store.state(key).path.isEmpty());
            QCOMPARE(registry.value("SELECT count(*) FROM file_trash").toInt(), 1);
            QCOMPARE(registry.value("SELECT count(*) FROM files").toInt(), 1);
            store.write(key, 8, 0, "newbytes", done);
            QTRY_COMPARE(completed, 3);
            QVERIFY(store.state(key).path != retired);
            QVERIFY(QDir().rmdir(retired));
            store.checkpoint(done);
            QTRY_COMPARE(completed, 4);
            QCOMPARE(registry.value("SELECT count(*) FROM file_trash").toInt(), 0);
            QVERIFY(QFileInfo::exists(store.state(key).path));
        }
        MediaChunkStore reopened(directory.path(), 8);
        reopened.read(key, 8, 0, 8, [&](QByteArray bytes, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            QCOMPARE(bytes, QByteArray("newbytes"));
            ++completed;
        });
        QTRY_COMPARE(completed, 5);
    }
};
QTEST_GUILESS_MAIN(FileStoreTest)
#include "filestore.moc"
