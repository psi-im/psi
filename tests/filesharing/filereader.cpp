/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filereader.h"
#include "filedatabase.h"
#include "filedownload.h"
#include "mediachunkcache.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QSemaphore>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>
#include <atomic>
#include <thread>
#include <utility>

namespace {
struct ReleaseHash {
    QSemaphore &gate;
    bool        released = false;
    void        release()
    {
        if (!std::exchange(released, true))
            gate.release();
    }
    ~ReleaseHash() { release(); }
};
struct ScopedFileDatabase {
    QString path;
    ~ScopedFileDatabase() { FileDatabase::closeConnection(path); }
};
QByteArray pattern(quint64 offset, quint64 length)
{
    QByteArray bytes(int(length), '\0');
    for (int i = 0; i < bytes.size(); ++i)
        bytes[i] = char((offset + quint64(i)) % 251);
    return bytes;
}
struct WaitingConsumer {
    FileReader &reader;
    std::thread worker;
    explicit WaitingConsumer(FileReader &reader) : reader(reader) { }
    ~WaitingConsumer()
    {
        reader.abort();
        if (worker.joinable())
            worker.join();
    }
};
}
class FileReaderTest : public QObject {
    Q_OBJECT
private slots:
    void fullGoalReusesReadyLocalFileWithoutFetchingOrCopyingRanges()
    {
        QTemporaryDir directory;
        QFile         file(directory.filePath("already-downloaded"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(pattern(0, 1000)), qint64(1000));
        file.close();
        MediaChunkCache cache(directory.path());
        int             fetches = 0, finished = 0;
        FileDownload    download(
            &cache, "movie", 1000, "account",
            [&](quint64, quint64, FileDownload::FullDownloadAllowed, MediaChunkCompletion) -> MediaChunkAbort {
                ++fetches;
                return {};
            });
        download.setLocalFileResolver([&] { return file.fileName(); });
        QVERIFY(download.downloadAll([&](const QString &path, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            QCOMPARE(path, file.fileName());
            ++finished;
        }));
        QTRY_COMPARE(finished, 1);
        QCOMPARE(fetches, 0);
        QVERIFY(download.state().complete);
        QVERIFY(cache.cachedRanges("movie", 1000).isEmpty());
    }

    void codecsCanProbeAndDecodeFromPartialStorage_data()
    {
        QTest::addColumn<QByteArray>("format");
        QTest::newRow("png") << QByteArray("PNG");
        QTest::newRow("jpeg") << QByteArray("JPEG");
    }
    void codecsCanProbeAndDecodeFromPartialStorage()
    {
        QFETCH(QByteArray, format);
        QImage image(128, 96, QImage::Format_RGB32);
        image.fill(Qt::red);
        QByteArray encoded;
        QBuffer    output(&encoded);
        QVERIFY(output.open(QIODevice::WriteOnly));
        if (!image.save(&output, format.constData()))
            QSKIP("Image codec is unavailable");
        QTemporaryDir   directory;
        MediaChunkCache cache(directory.path(), nullptr, 64);
        int             fetches        = 0;
        bool            wholeRequested = false;
        FileDownload    download(&cache, "image", quint64(encoded.size()), "account",
                                 [&](quint64 offset, quint64 length, FileDownload::FullDownloadAllowed whole,
                                  MediaChunkCompletion done) -> MediaChunkAbort {
                                  wholeRequested |= whole();
                                  ++fetches;
                                  done(encoded.mid(int(offset), int(length)), {});
                                  return {};
                              });
        FileReader      reader(&download);
        QVERIFY(reader.open());
        WaitingConsumer   consumer(reader);
        std::atomic<bool> finished { false };
        QImage            decoded;
        consumer.worker = std::thread([&] {
            QImageReader decoder(&reader);
            decoder.setAutoTransform(true);
            decoded = decoder.read();
            finished.store(true);
        });
        QTRY_VERIFY_WITH_TIMEOUT(finished.load(), 10000);
        QCOMPARE(decoded.size(), image.size());
        QVERIFY(fetches > 0);
        QVERIFY(!wholeRequested);
    }

    void localSeekPeekAndWindow()
    {
        QTemporaryDir directory;
        QFile         file(directory.filePath("complete"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(pattern(0, 1000)), qint64(1000));
        file.close();
        FileReader reader(file.fileName(), 123, 400);
        QVERIFY(reader.open());
        QVERIFY(!reader.isSequential());
        QCOMPARE(reader.size(), qint64(400));
        QCOMPARE(reader.peek(17), pattern(123, 17));
        QCOMPARE(reader.pos(), qint64(0));
        QCOMPARE(reader.read(31), pattern(123, 31));
        QVERIFY(reader.seek(390));
        QCOMPARE(reader.read(40), pattern(513, 10));
        QVERIFY(reader.atEnd());
        QVERIFY(reader.seek(0));
        QCOMPARE(reader.read(10), pattern(123, 10));
        QVERIFY(!reader.seek(-1));
        QVERIFY(!reader.seek(401));
    }
    void playbackIsDemandOnlyAndTailIsClipped()
    {
        QTemporaryDir                  directory;
        MediaChunkCache                cache(directory.path());
        QList<QPair<quint64, quint64>> fetched;
        const quint64                  size = 10 * 1024 * 1024 + 134;
        FileDownload                   download(&cache, "movie", size, "account",
                                                [&](quint64 offset, quint64 length, FileDownload::FullDownloadAllowed whole,
                                  MediaChunkCompletion done) -> MediaChunkAbort {
                                  if (whole()) {
                                      done({}, QStringLiteral("Unexpected full download"));
                                      return {};
                                  }
                                  fetched.append({ offset, length });
                                  done(pattern(offset, length), {});
                                  return {};
                              });
        FileReader                     reader(&download);
        QSignalSpy                     failures(&reader, &FileReader::failed);
        QVERIFY(reader.open());
        QTest::qWait(20);
        QVERIFY(fetched.isEmpty()); // Neither opening nor seeking starts network work.
        QVERIFY(reader.seek(qint64(size - 100)));
        QTest::qWait(20);
        QVERIFY(fetched.isEmpty());
        QVERIFY(reader.read(100).isEmpty()); // Owner thread stays asynchronous.
        QVERIFY(!reader.atEnd());
        QTRY_COMPARE(reader.bytesAvailable(), qint64(100));
        QCOMPARE(reader.read(100), pattern(size - 100, 100));
        QVERIFY(reader.atEnd());
        QCOMPARE(fetched, (QList<QPair<quint64, quint64>> { { size - 134, 134 } }));
        QTest::qWait(20);
        QCOMPARE(fetched.size(), 1);
        QVERIFY(reader.seek(qint64(size - 50)));
        QCOMPARE(reader.peek(15), pattern(size - 50, 15));
        QCOMPARE(reader.pos(), qint64(size - 50));
        QCOMPARE(reader.read(15), pattern(size - 50, 15));
        QCOMPARE(failures.size(), 0);
    }
    void unavailableDiskUsesSharedMemoryWithoutClaimingCompletion()
    {
        QTemporaryDir directory;
        QFile         blocker(directory.filePath("blocked"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        MediaChunkCache cache(blocker.fileName());
        int             starts = 0;
        FileDownload    download(&cache, "movie", 1024 * 1024, "account",
                                 [&](quint64 offset, quint64 length, FileDownload::FullDownloadAllowed,
                                  MediaChunkCompletion done) -> MediaChunkAbort {
                                  ++starts;
                                  done(pattern(offset, length), {});
                                  return {};
                              });
        FileReader      first(&download);
        QVERIFY(first.open());
        QVERIFY(first.seek(900000));
        QVERIFY(first.read(100).isEmpty());
        QTRY_COMPARE(first.bytesAvailable(), qint64(100));
        QCOMPARE(first.read(100), pattern(900000, 100));
        QCOMPARE(starts, 1);
        FileReader again(&download);
        QVERIFY(again.open());
        QVERIFY(again.seek(900000));
        QVERIFY(again.read(100).isEmpty());
        QTRY_COMPARE(again.bytesAvailable(), qint64(100));
        QCOMPARE(again.read(100), pattern(900000, 100));
        QCOMPARE(starts, 1); // The same RAM LRU serves independent readers.
        QVERIFY(cache.cachedRanges("movie", download.size()).isEmpty());
        QVERIFY(!download.state().complete);
        int finished = 0;
        QVERIFY(download.downloadAll([&](const QString &path, MediaChunkError error) {
            QCOMPARE(error.code(), MediaChunkError::StorageUnavailable);
            QVERIFY(path.isEmpty());
            ++finished;
        }));
        QTRY_COMPARE(finished, 1);
    }
    void volatileBytesStillCheckAvailableHashes()
    {
        QTemporaryDir directory;
        QFile         blocker(directory.filePath("blocked"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        MediaChunkCache cache(blocker.fileName());
        const auto      original = pattern(0, 1024);
        cache.configure("movie", 1024,
                        { { 0, 1024, "sha-256", QCryptographicHash::hash(original, QCryptographicHash::Sha256) } });
        FileDownload download(&cache, "movie", 1024, "account",
                              [](quint64, quint64 length, FileDownload::FullDownloadAllowed,
                                 MediaChunkCompletion done) -> MediaChunkAbort {
                                  done(QByteArray(int(length), 'x'), {});
                                  return {};
                              });
        FileReader   reader(&download);
        QSignalSpy   errors(&reader, &FileReader::failed);
        QVERIFY(reader.open());
        QVERIFY(reader.read(1).isEmpty());
        QTRY_COMPARE(errors.size(), 1);
        QCOMPARE(reader.errorCode(), MediaChunkError::IntegrityError);
        QCOMPARE(reader.bytesAvailable(), qint64(0));
    }
    void sequentialFullDownloadExposesPrefixBeforeTransportCompletion()
    {
        QTemporaryDir   directory;
        MediaChunkCache cache(directory.path());
        const quint64   size = 128 * 1024;
        struct Pending {
            quint64              offset, length;
            MediaChunkCompletion done;
        };
        QList<Pending> pending;
        FileDownload   download(&cache, "movie", size, "account",
                                [&](quint64 offset, quint64 length, FileDownload::FullDownloadAllowed,
                                  MediaChunkCompletion done) -> MediaChunkAbort {
                                  pending.append({ offset, length, std::move(done) });
                                  return {};
                              });
        int            finished = 0;
        QVERIFY(download.downloadAll([&](const QString &, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            ++finished;
        }));
        QTRY_COMPARE(pending.size(), 2);
        FileReader reader(&download);
        QVERIFY(reader.open());
        QVERIFY(reader.read(100).isEmpty());
        int written = 0;
        cache.storeBytes("movie", size, 0, pattern(0, 64 * 1024), [&](QByteArray, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            ++written;
        });
        QTRY_COMPARE(written, 1);
        QTRY_COMPARE(reader.bytesAvailable(), qint64(100));
        QCOMPARE(reader.read(100), pattern(0, 100));
        QCOMPARE(finished, 0);
        cache.storeBytes("movie", size, 64 * 1024, pattern(64 * 1024, 64 * 1024),
                         [&](QByteArray, MediaChunkError error) {
                             QVERIFY(error.isEmpty());
                             ++written;
                         });
        QTRY_COMPARE(written, 2);
        QTest::qWait(20);
        QCOMPARE(finished, 0); // Disk coverage alone is not transport success.
        for (const auto &request : pending)
            request.done(pattern(request.offset, request.length), {});
        QTRY_COMPARE(finished, 1);
    }
    void fallbackPermissionTracksGoalsDuringNegotiation()
    {
        QTemporaryDir                     directory;
        MediaChunkCache                   cache(directory.path());
        FileDownload::FullDownloadAllowed allowed;
        MediaChunkCompletion              complete;
        FileDownload                      download(&cache, "movie", 128 * 1024, "account",
                                                   [&](quint64, quint64, FileDownload::FullDownloadAllowed policy,
                                  MediaChunkCompletion done) -> MediaChunkAbort {
                                  allowed  = std::move(policy);
                                  complete = std::move(done);
                                  return {};
                              });
        FileReader                        reader(&download);
        QVERIFY(reader.open());
        QVERIFY(reader.read(1).isEmpty());
        QTRY_VERIFY(bool(allowed));
        QVERIFY(!allowed());
        const auto goal = download.downloadAll(
            [](const QString &, MediaChunkError error) { QCOMPARE(error.code(), MediaChunkError::Cancelled); });
        QVERIFY(goal);
        QVERIFY(allowed()); // A goal added after the request began authorizes fallback.
        QVERIFY(download.cancelGoal(goal));
        QVERIFY(!allowed()); // A late Range refusal cannot revive a cancelled full download.
        reader.abort();
        complete({}, { MediaChunkError::Cancelled, "cancelled" });
    }
    void fullDownloadFillsFromPlayheadAndSurvivesPausedReader()
    {
        QTemporaryDir                  directory;
        MediaChunkCache                cache(directory.path());
        const quint64                  size = 1024 * 1024, middle = size / 2;
        QList<QPair<quint64, quint64>> fetched;
        bool                           full = false;
        FileDownload                   download(&cache, "movie", size, "account",
                                                [&](quint64 offset, quint64 length, FileDownload::FullDownloadAllowed whole,
                                  MediaChunkCompletion done) -> MediaChunkAbort {
                                  if (whole() != full)
                                      done({}, QStringLiteral("Incorrect download policy"));
                                  else {
                                      fetched.append({ offset, length });
                                      done(pattern(offset, length), {});
                                  }
                                  return {};
                              });
        FileReader                     reader(&download);
        QVERIFY(reader.open());
        QVERIFY(reader.seek(qint64(middle)));
        QVERIFY(reader.read(1).isEmpty());
        QTRY_VERIFY(reader.bytesAvailable() > 0);
        QCOMPARE(reader.read(1), pattern(middle, 1));
        full = true;
        QString path;
        int     finished = 0;
        QVERIFY(download.downloadAll([&](const QString &result, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            path = result;
            ++finished;
        }));
        QTRY_COMPARE(finished, 1); // Reader does not have to consume more data.
        QCOMPARE(reader.pos(), qint64(middle + 1));
        QVERIFY(download.state().complete);
        bool wrapped = false;
        for (const auto &range : fetched) {
            if (range.first < middle)
                wrapped = true;
            else
                QVERIFY(!wrapped); // Tail before prefix, despite changing chunk sizes.
        }
        QVERIFY(wrapped);
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), pattern(0, size));
    }
    void fullDownloadWaitsForLargeVerification_data()
    {
        QTest::addColumn<bool>("matchingHash");
        QTest::addColumn<bool>("checkpointFails");
        QTest::newRow("verified") << true << false;
        QTest::newRow("mismatch") << false << false;
        QTest::newRow("checkpoint-error") << true << true;
    }
    void fullDownloadWaitsForLargeVerification()
    {
        QFETCH(bool, matchingHash);
        QFETCH(bool, checkpointFails);
        QTemporaryDir      directory;
        ScopedFileDatabase registry { directory.filePath("files.sqlite") };
        const QByteArray   bytes(8 * 1024 * 1024, 'x');
        QSemaphore         gate;
        std::atomic<int>   hashing { 0 };
        MediaChunkCache    cache(directory.path(), nullptr, MediaChunkMaxSize, 32 * 1024 * 1024,
                                 [&](const QString &, QIODevice *input) {
                                  ++hashing;
                                  gate.acquire();
                                  QCryptographicHash hash(QCryptographicHash::Sha256);
                                  return hash.addData(input) ? hash.result() : QByteArray();
                              });
        ReleaseHash        release { gate };
        auto               digest = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
        if (!matchingHash)
            digest[0] = char(digest.at(0) ^ 1);
        cache.configure("movie", quint64(bytes.size()), { { 0, quint64(bytes.size()), "sha-256", digest } });
        FileDownload    download(&cache, "movie", quint64(bytes.size()), "account",
                                 [&](quint64 offset, quint64 length, FileDownload::FullDownloadAllowed,
                                  MediaChunkCompletion done) -> MediaChunkAbort {
                                  done(bytes.mid(qsizetype(offset), qsizetype(length)), {});
                                  return {};
                              });
        int             finished = 0;
        QString         path;
        MediaChunkError result;
        QVERIFY(download.downloadAll([&](const QString &file, MediaChunkError error) {
            path   = file;
            result = error;
            ++finished;
        }));
        QTRY_COMPARE_WITH_TIMEOUT(hashing.load(), 1, 10000);
        QCOMPARE(finished, 0);
        QVERIFY(!download.state().complete);
        FileReader reader(&download);
        QVERIFY(reader.open());
        QVERIFY(reader.seek(bytes.size() / 2));
        QVERIFY(reader.read(4096).isEmpty());
        QTRY_COMPARE(reader.bytesAvailable(), qint64(4096));
        QCOMPARE(reader.read(4096), bytes.mid(bytes.size() / 2, 4096));
        QCOMPARE(finished, 0); // All bytes are readable, but verification is still blocked.
        if (checkpointFails) {
            QSqlQuery query(FileDatabase::connection(registry.path));
            QVERIFY(query.exec("CREATE TRIGGER reject_completion BEFORE UPDATE OF complete ON files "
                               "WHEN NEW.complete = 1 BEGIN SELECT RAISE(ABORT, 'test checkpoint failure'); END"));
        }
        release.release();
        QTRY_COMPARE(finished, 1);
        QCOMPARE(hashing.load(), 1);
        if (!matchingHash) {
            QCOMPARE(result.code(), MediaChunkError::IntegrityError);
            QVERIFY(path.isEmpty());
            QVERIFY(!download.state().complete);
        } else if (checkpointFails) {
            QCOMPARE(result.code(), MediaChunkError::StorageUnavailable);
            QVERIFY(path.isEmpty());
            QVERIFY(!download.state().complete);
            QVERIFY(!download.state().completionError.isEmpty());
            QVERIFY(reader.seek(0));
            QVERIFY(reader.read(4096).isEmpty());
            QTRY_COMPARE(reader.bytesAvailable(), qint64(4096));
            QCOMPARE(reader.read(4096), bytes.left(4096));
        } else {
            QVERIFY(result.isEmpty());
            QCOMPARE(path, download.state().path);
            QVERIFY(download.state().complete);
        }
    }
    void foreignReadWaitsAndAbortWakesIt()
    {
        QTemporaryDir        directory;
        MediaChunkCache      cache(directory.path());
        MediaChunkCompletion complete;
        int                  starts = 0, aborts = 0;
        FileDownload         download(
            &cache, "movie", 1024, "account",
            [&](quint64, quint64, FileDownload::FullDownloadAllowed, MediaChunkCompletion done) -> MediaChunkAbort {
                ++starts;
                complete = std::move(done);
                return [&] {
                    ++aborts;
                    complete({}, { MediaChunkError::Cancelled, "cancelled" });
                };
            });
        FileReader reader(&download);
        QVERIFY(reader.open());
        std::atomic<bool> finished { false };
        QByteArray        bytes;
        WaitingConsumer   consumer(reader);
        consumer.worker = std::thread([&] {
            bytes    = reader.read(10);
            finished = true;
        });
        QTRY_COMPARE(starts, 1);
        QVERIFY(!finished.load());
        reader.abort();
        QTRY_VERIFY(finished.load());
        consumer.worker.join();
        QVERIFY(bytes.isEmpty());
        QCOMPARE(reader.errorCode(), MediaChunkError::Cancelled);
        QCOMPARE(aborts, 1);
    }
    void foreignReadGetsDataAndSeekDiscardsStaleCompletion()
    {
        QTemporaryDir   directory;
        MediaChunkCache cache(directory.path());
        FileDownload    download(&cache, "movie", 1024 * 1024, "account",
                                 [](quint64 offset, quint64 length, FileDownload::FullDownloadAllowed,
                                 MediaChunkCompletion done) -> MediaChunkAbort {
                                  done(pattern(offset, length), {});
                                  return {};
                              });
        FileReader      reader(&download);
        QVERIFY(reader.open());
        QVERIFY(reader.read(10).isEmpty());
        QVERIFY(reader.seek(900000)); // Previous queued request must not fill this cursor.
        std::atomic<bool> finished { false };
        QByteArray        bytes;
        WaitingConsumer   consumer(reader);
        consumer.worker = std::thread([&] {
            bytes    = reader.read(10);
            finished = true;
        });
        QTRY_VERIFY(finished.load());
        consumer.worker.join();
        QCOMPARE(bytes, pattern(900000, 10));
        QCOMPARE(reader.pos(), qint64(900010));
    }
};
QTEST_GUILESS_MAIN(FileReaderTest)
#include "filereader.moc"
