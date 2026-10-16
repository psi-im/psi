#include "mediachunkcache.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <thread>

namespace {
QByteArray pattern(quint64 offset, quint64 size)
{
    QByteArray out(int(size), '\0');
    for (int i = 0; i < out.size(); ++i)
        out[i] = char((offset + quint64(i)) % 251);
    return out;
}
using Completion = MediaChunkCache::Completion;
struct Pending {
    quint64    offset, length;
    Completion done;
};
}

class ChunkCacheTest : public QObject {
    Q_OBJECT
private slots:
    void availabilityTracksVerifiedChunksAndEviction()
    {
        QTemporaryDir dir(QDir::currentPath() + "/chunks-XXXXXX");
        const quint64 chunk = 4 * 1024 * 1024;
        // No disk storage: the fifth full chunk evicts the oldest RAM chunk.
        MediaChunkCache cache(dir.path(), nullptr, chunk, 0);
        QSignalSpy      changed(&cache, &MediaChunkCache::rangesChanged);
        auto            fetch = [](quint64, quint64 size, Completion done) -> MediaChunkCache::Abort {
            done(QByteArray(int(size), 'a'), {});
            return {};
        };
        const quint64 size = chunk * 8;
        for (const auto index : { 0, 2, 3, 4, 5 }) {
            bool done = false;
            cache.read("source", size, quint64(index) * chunk, 1, "lane", fetch,
                       [&](QByteArray bytes, MediaChunkError error) {
                           QVERIFY(error.isEmpty());
                           QCOMPARE(bytes.size(), 1);
                           done = true;
                       });
            QTRY_VERIFY(done);
        }
        QCOMPARE(cache.cachedRanges("source", size), (MediaChunkCache::Ranges { { chunk * 2, chunk * 4 } }));
        QVERIFY(cache.cachedRanges("other-account", size).isEmpty());
        QVERIFY(cache.cachedRanges("source", size + 1).isEmpty());
        QTRY_VERIFY(changed.count() > 0);
        // Failure must never paint an unavailable chunk as cached.
        bool failed = false;
        cache.read(
            "source", size, chunk * 7, 1, "lane",
            [](quint64, quint64, Completion done) -> MediaChunkCache::Abort {
                done({}, QStringLiteral("offline"));
                return {};
            },
            [&](QByteArray, MediaChunkError error) { failed = !error.isEmpty(); });
        QTRY_VERIFY(failed);
        QCOMPARE(cache.cachedRanges("source", size), (MediaChunkCache::Ranges { { chunk * 2, chunk * 4 } }));
    }
    void cancellingQueuedMemoryHitSettlesOnce()
    {
        QTemporaryDir   dir(QDir::currentPath() + "/chunks-XXXXXX");
        MediaChunkCache cache(dir.path(), nullptr, 128);
        int             delivered = 0, canceled = 0;
        auto            fetch = [](quint64 offset, quint64 size, Completion done) -> MediaChunkCache::Abort {
            done(pattern(offset, size), {});
            return {};
        };
        cache.read("source", 1000, 0, 5, "lane", fetch, [&](QByteArray, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            ++delivered;
        });
        QTRY_COMPARE(delivered, 1);
        for (int i = 0; i < 1000; ++i) {
            auto ticket = cache.read("source", 1000, 0, 5, "lane", fetch, [&](QByteArray bytes, MediaChunkError error) {
                QVERIFY(bytes.isEmpty());
                QCOMPARE(error.code(), MediaChunkError::Cancelled);
                ++canceled;
            });
            QVERIFY(cache.cancel(ticket));
        }
        QTest::qWait(10);
        QCOMPARE(canceled, 1000);
        QCOMPARE(delivered, 1);
    }
    void workerCompletionAfterOwnerDestructionIsIgnored()
    {
        QTemporaryDir      dir(QDir::currentPath() + "/chunks-XXXXXX");
        auto               cache = new MediaChunkCache(dir.path(), nullptr, 128);
        Completion         complete;
        int                failures = 0;
        auto               lifetime = std::make_shared<int>(1);
        std::weak_ptr<int> weak     = lifetime;
        auto               fetch    = [&](quint64, quint64, Completion done) -> MediaChunkCache::Abort {
            complete = done;
            return [done, lifetime] { done({}, { MediaChunkError::Cancelled, QStringLiteral("aborted") }); };
        };
        cache->read("source", 1000, 0, 5, "lane", fetch, [&](QByteArray, MediaChunkError error) {
            QVERIFY(!error.isEmpty());
            ++failures;
        });
        QTRY_VERIFY(bool(complete));
        lifetime.reset();
        delete cache;
        std::thread worker([done = std::move(complete)] { done(pattern(0, 128), {}); });
        worker.join();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QTest::qWait(10);
        QCOMPARE(failures, 1);
        QVERIFY(weak.expired()); // An abort capturing completion must not form a cycle.
    }
    void admissionIsBoundedAndCancelledBeforeDiskMiss()
    {
        QTemporaryDir   dir(QDir::currentPath() + "/chunks-XXXXXX");
        MediaChunkCache cache(dir.path(), nullptr, 128);
        int             starts = 0, callbacks = 0;
        auto            fetch = [&](quint64, quint64, Completion) -> MediaChunkCache::Abort {
            ++starts;
            return {};
        };
        QList<quint64> tickets;
        for (int i = 0; i < MediaChunkCache::MaxRequests; ++i) {
            auto ticket = cache.read("source", 1000, 0, 5, "lane", fetch, [&](QByteArray, MediaChunkError error) {
                QCOMPARE(error.code(), MediaChunkError::Cancelled);
                ++callbacks;
            });
            QVERIFY(ticket);
            tickets.append(ticket);
        }
        for (int i = 0; i < 10000; ++i)
            QCOMPARE(cache.read("source", 1000, 0, 5, "lane", fetch,
                                [](QByteArray, MediaChunkError) { QFAIL("Rejected request retained a callback"); }),
                     quint64(0));
        for (auto ticket : tickets)
            QVERIFY(cache.cancel(ticket));
        QCOMPARE(callbacks, MediaChunkCache::MaxRequests);
        QTest::qWait(20);
        QCOMPARE(starts, 0);
    }

    void unavailableDiskStillServesMemoryCache()
    {
        QTemporaryDir dir(QDir::currentPath() + "/chunks-XXXXXX");
        QFile         blocker(dir.filePath("not-a-directory"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        MediaChunkCache cache(blocker.fileName(), nullptr, 128);
        int             starts = 0, delivered = 0;
        auto            fetch = [&](quint64 offset, quint64 size, Completion done) -> MediaChunkCache::Abort {
            ++starts;
            done(pattern(offset, size), {});
            return {};
        };
        for (int i = 1; i <= 2; ++i) {
            cache.read("source", 1000, 1, 5, "lane", fetch, [&](QByteArray bytes, MediaChunkError error) {
                QVERIFY(error.isEmpty());
                QCOMPARE(bytes, pattern(1, 5));
                ++delivered;
            });
            QTRY_COMPARE(delivered, i);
        }
        QCOMPARE(starts, 1);
        QCOMPARE(cache.diskBytes(), quint64(0));
    }

    void sharedReadsAndIndependentCancellation()
    {
        QTemporaryDir dir(QDir::currentPath() + "/chunks-XXXXXX");
        int           starts = 0, aborts = 0, deliveries = 0, canceled = 0;
        Pending       pending;
        auto          fetch = [&](quint64 offset, quint64 length, Completion done) {
            ++starts;
            pending = { offset, length, std::move(done) };
            return [&] { ++aborts; };
        };
        MediaChunkCache cache(dir.path(), nullptr, 128);
        const auto      a
            = cache.read("account:A:hash", 1000, 3, 7, "A", fetch, [&](QByteArray bytes, MediaChunkError error) {
                  QVERIFY(bytes.isEmpty());
                  QVERIFY(!error.isEmpty());
                  ++canceled;
              });
        cache.read("account:A:hash", 1000, 50, 12, "A", fetch, [&](QByteArray bytes, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            QCOMPARE(bytes, pattern(50, 12));
            ++deliveries;
        });
        QTRY_COMPARE(starts, 1);
        QVERIFY(cache.cancel(a));
        QCOMPARE(canceled, 1);
        QCOMPARE(aborts, 0);
        pending.done(pattern(pending.offset, pending.length), {});
        QTRY_COMPARE(deliveries, 1);
        cache.read("account:A:hash", 1000, 1, 5, "A", fetch, [&](QByteArray bytes, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            QCOMPARE(bytes, pattern(1, 5));
            ++deliveries;
        });
        QCOMPARE(deliveries, 1); // Hits must not reenter the caller synchronously.
        QTRY_COMPARE(deliveries, 2);
        QCOMPARE(starts, 1);
    }

    void persistenceIdentityAndCorruption()
    {
        QTemporaryDir dir(QDir::currentPath() + "/chunks-XXXXXX");
        int           starts = 0, delivered = 0;
        auto          fetch = [&](quint64 offset, quint64 length, Completion done) -> MediaChunkCache::Abort {
            ++starts;
            done(pattern(offset, length), {});
            return {};
        };
        auto done = [&](QByteArray bytes, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            QCOMPARE(bytes, pattern(7, 10));
            ++delivered;
        };
        {
            MediaChunkCache cache(dir.path(), nullptr, 128);
            cache.read("account:A:hash", 1000, 7, 10, "A", fetch, done);
            QTRY_COMPARE(delivered, 1);
            QCOMPARE(starts, 1);
            QVERIFY(cache.diskBytes() > 128);
        }
        {
            MediaChunkCache cache(dir.path(), nullptr, 128);
            cache.read("account:A:hash", 1000, 7, 10, "A", fetch, done);
            QTRY_COMPARE(delivered, 2);
            QCOMPARE(starts, 1);
        }
        auto files = QDir(dir.path()).entryList({ "*.chunk" }, QDir::Files);
        QCOMPARE(files.size(), 1);
        QFile file(dir.filePath(files.first()));
        QVERIFY(file.open(QIODevice::ReadWrite));
        const auto raw = file.readAll();
        QVERIFY(raw.startsWith("PSICHNK1"));
        QVERIFY(raw.endsWith(pattern(0, 128))); // Deliberately plaintext on disk.
        QVERIFY(file.seek(75));
        QCOMPARE(file.write("bad", 3), qint64(3));
        file.close();
        {
            MediaChunkCache cache(dir.path(), nullptr, 128);
            cache.read("account:A:hash", 1000, 7, 10, "A", fetch, done);
            QTRY_COMPARE(delivered, 3);
            QCOMPARE(starts, 2);
            cache.read("account:B:hash", 1000, 7, 10, "B", fetch, done);
            QTRY_COMPARE(delivered, 4);
            QCOMPARE(starts, 3);
            cache.read("account:A:hash", 1001, 7, 10, "A", fetch, done);
            QTRY_COMPARE(delivered, 5);
            QCOMPARE(starts, 4);
        }
        {
            MediaChunkCache cache(dir.path(), nullptr, 256);
            cache.read("account:A:hash", 1000, 7, 10, "A", fetch, done);
            QTRY_COMPARE(delivered, 6);
            QCOMPARE(starts, 5);
        }
    }

    void quotaAndFailureRetry()
    {
        QTemporaryDir dir(QDir::currentPath() + "/chunks-XXXXXX");
        int           starts = 0, delivered = 0;
        bool          shortRead = true;
        auto          fetch     = [&](quint64 offset, quint64 length, Completion done) -> MediaChunkCache::Abort {
            ++starts;
            done(pattern(offset, shortRead ? length - 1 : length), {});
            return {};
        };
        {
            MediaChunkCache cache(dir.path(), nullptr, 128, 400); // Two records including headers.
            cache.read("source", 1024, 0, 5, "lane", fetch, [&](QByteArray bytes, MediaChunkError error) {
                QVERIFY(bytes.isEmpty());
                QVERIFY(!error.isEmpty());
                ++delivered;
            });
            QTRY_COMPARE(delivered, 1);
            QCOMPARE(cache.diskBytes(), quint64(0));
            shortRead = false;
            for (const quint64 offset : { 0, 128, 256, 0 }) {
                const auto next = delivered + 1;
                cache.read("source", 1024, offset, 5, "lane", fetch,
                           [&, offset](QByteArray bytes, MediaChunkError error) {
                               QVERIFY(error.isEmpty());
                               QCOMPARE(bytes, pattern(offset, 5));
                               ++delivered;
                           });
                QTRY_COMPARE(delivered, next);
                QVERIFY(cache.diskBytes() <= 400);
            }
            QCOMPARE(starts, 4); // The evicted disk record can remain hot in RAM.
        }
        MediaChunkCache reopened(dir.path(), nullptr, 128, 400);
        reopened.read("source", 1024, 0, 5, "lane", fetch, [&](QByteArray bytes, MediaChunkError error) {
            QVERIFY(error.isEmpty());
            QCOMPARE(bytes, pattern(0, 5));
            ++delivered;
        });
        QTRY_COMPARE(delivered, 6);
        QCOMPARE(starts, 5); // Reopening proves that disk eviction actually happened.
    }

    void completionFromWorkerAndDuplicateCompletion()
    {
        QTemporaryDir dir(QDir::currentPath() + "/chunks-XXXXXX");
        Completion    complete;
        int           delivered = 0;
        auto          fetch     = [&](quint64, quint64, Completion done) -> MediaChunkCache::Abort {
            complete = std::move(done);
            return {};
        };
        MediaChunkCache cache(dir.path(), nullptr, 128);
        auto            done = [&](QByteArray bytes, MediaChunkError error) {
            QCOMPARE(QThread::currentThread(), qApp->thread());
            QVERIFY(error.isEmpty());
            QCOMPARE(bytes, pattern(0, 5));
            ++delivered;
        };
        cache.read("source", 1024, 0, 5, "lane", fetch, done);
        QTRY_VERIFY(bool(complete));
        std::thread worker([complete] { complete(pattern(0, 128), {}); });
        worker.join();
        QTRY_COMPARE(delivered, 1);
        complete(QByteArray(128, 'x'), {});
        cache.read("source", 1024, 0, 5, "lane", fetch, done);
        QTRY_COMPARE(delivered, 2); // A duplicate callback must not poison the cache.
    }

    void shutdownAndLateCompletion()
    {
        QTemporaryDir dir(QDir::currentPath() + "/chunks-XXXXXX");
        int           aborts = 0, failures = 0;
        Completion    pending;
        auto          fetch = [&](quint64, quint64, Completion done) {
            pending = std::move(done);
            return [&] { ++aborts; };
        };
        MediaChunkCache cache(dir.path(), nullptr, 128);
        cache.read("source", 1000, 0, 5, "lane", fetch, [&](QByteArray bytes, MediaChunkError error) {
            QVERIFY(bytes.isEmpty());
            QVERIFY(!error.isEmpty());
            ++failures;
        });
        QTRY_VERIFY(bool(pending));
        cache.shutdown();
        QCOMPARE(aborts, 1);
        QCOMPARE(failures, 1);
        pending(pattern(0, 128), {});
        QTest::qWait(10);
        QCOMPARE(failures, 1);
        QCOMPARE(cache.diskBytes(), quint64(0));
    }

    void readerCrossesChunksAndHonorsBackpressure()
    {
        QTemporaryDir dir(QDir::currentPath() + "/chunks-XXXXXX");
        int           starts = 0;
        auto          fetch  = [&](quint64 offset, quint64 length, Completion done) -> MediaChunkCache::Abort {
            ++starts;
            done(pattern(offset, length), {});
            return {};
        };
        MediaChunkCache  cache(dir.path(), nullptr, 128);
        QByteArray       received;
        MediaChunkReader reader(&cache, "source", 1024, 117, 350, "lane", fetch);
        QSignalSpy       errors(&reader, &MediaChunkReader::failed);
        connect(&reader, &QIODevice::readyRead, &reader, [&] { received += reader.readAll(); });
        QVERIFY(reader.open());
        QTRY_COMPARE(received.size(), 350);
        QCOMPARE(received, pattern(117, 350));
        QCOMPARE(starts, 4);
        QCOMPARE(errors.count(), 0);
        QVERIFY(!reader.isConnected());
        MediaChunkReader paused(&cache, "second", 1024, 0, 1024, "lane", fetch);
        QVERIFY(paused.open());
        QTRY_VERIFY(paused.bytesAvailable() > 0);
        QTRY_COMPARE(starts, 6); // Four previous chunks plus current and one lookahead.
        const int before = starts;
        QTest::qWait(20);
        QCOMPARE(starts, before); // At most one lookahead while the consumer is paused.
        paused.abort();
    }

    void deletingReaderCancelsItsDemand()
    {
        QTemporaryDir     dir(QDir::currentPath() + "/chunks-XXXXXX");
        int               starts = 0, aborts = 0;
        QList<Completion> pending;
        auto              fetch = [&](quint64, quint64, Completion done) {
            ++starts;
            pending.append(std::move(done));
            return [&] { ++aborts; };
        };
        MediaChunkCache cache(dir.path(), nullptr, 128);
        auto            reader = new MediaChunkReader(&cache, "source", 1024, 0, 1024, "lane", fetch);
        QVERIFY(reader->open());
        QTRY_COMPARE(starts, 2);
        delete reader;
        QCOMPARE(aborts, 2);
        for (const auto &done : pending)
            done({}, QStringLiteral("aborted"));
        QTest::qWait(10);
        QCOMPARE(cache.diskBytes(), quint64(0));
    }

    void aviIndexReplayUsesThreeChunkFetches()
    {
        QTemporaryDir dir(QDir::currentPath() + "/chunks-XXXXXX");
        int           starts = 0, delivered = 0;
        auto          fetch = [&](quint64 offset, quint64 length, Completion done) -> MediaChunkCache::Abort {
            ++starts;
            done(pattern(offset, length), {});
            return {};
        };
        MediaChunkCache cache(dir.path());
        // Offsets from the reported mpv/AVI index traversal, including the jump
        // between tiny superindex entries and 64 KiB subindexes near EOF.
        const QList<quint64> offsets
            = { 0ULL,    2339063896ULL, 260ULL,  2339129464ULL, 276ULL,  2339195032ULL, 292ULL,  2339260600ULL,
                308ULL,  2339326168ULL, 324ULL,  2339391736ULL, 340ULL,  2339457304ULL, 356ULL,  2339522872ULL,
                372ULL,  2339588440ULL, 388ULL,  2339654008ULL, 404ULL,  2339719576ULL, 420ULL,  2339785144ULL,
                436ULL,  2339850712ULL, 452ULL,  2339916280ULL, 468ULL,  2339981848ULL, 484ULL,  2340047416ULL,
                500ULL,  2340112984ULL, 516ULL,  2340178552ULL, 532ULL,  2340244120ULL, 548ULL,  2340309688ULL,
                564ULL,  2340375256ULL, 580ULL,  2340387768ULL, 4502ULL, 2340453336ULL, 4518ULL, 2340518904ULL,
                4534ULL, 2340584472ULL, 4550ULL, 2340650040ULL, 4566ULL, 2340715608ULL, 4582ULL, 2340781176ULL,
                4598ULL, 2340846744ULL, 4614ULL, 2340912312ULL, 4630ULL, 2340977880ULL, 4646ULL, 2341043448ULL,
                4662ULL, 2341109016ULL, 4678ULL };
        for (const auto offset : offsets) {
            const int next = delivered + 1;
            cache.read("account:movie-sha1", 2343036928ULL, offset, 16, "account", fetch,
                       [&, offset](QByteArray bytes, MediaChunkError error) {
                           QVERIFY(error.isEmpty());
                           QCOMPARE(bytes, pattern(offset, 16));
                           ++delivered;
                       });
            QTRY_COMPARE(delivered, next);
        }
        QCOMPARE(delivered, 67);
        QCOMPARE(starts, 3);
    }
};
QTEST_GUILESS_MAIN(ChunkCacheTest)
#include "chunkcache.moc"
