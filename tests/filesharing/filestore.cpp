#include "filesourceselector.h"
#include "mediachunkstore.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <atomic>

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
        QCOMPARE(QDir(directory.path()).entryList(QDir::Files).size(), 2);
        QFile metadata(directory.filePath(key + ".json"));
        QVERIFY(metadata.open(QIODevice::ReadOnly));
        const auto json = QJsonDocument::fromJson(metadata.readAll()).object();
        QCOMPARE(json.value("ranges").toArray().size(), 1);
        QCOMPARE(json.value("checksums").toArray().size(), 1);
        QCOMPARE(json.value("metadata").toObject().value("name").toString(), QString("sample.bin"));
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
        }
        QFile payload(directory.filePath(key + ".data"));
        QVERIFY(payload.open(QIODevice::ReadWrite));
        QCOMPARE(payload.write("x", 1), qint64(1));
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
        {
            MediaChunkStore store(directory.path(), 1024);
            store.configure(key, data.size(), { checksum(8, data.mid(8)) }, { });
            store.write(key, data.size(), 0, data, [&](QByteArray, MediaChunkError error) {
                QVERIFY(error.isEmpty());
                ++completed;
            });
            QTRY_COMPARE(completed, 1);
        }
        QFile payload(directory.filePath(key + ".data"));
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
};
QTEST_GUILESS_MAIN(FileStoreTest)
#include "filestore.moc"
