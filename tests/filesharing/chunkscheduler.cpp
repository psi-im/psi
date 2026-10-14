// Adapted from Ri0n/AnyKeep PR #156, db77d42ef791225839d03ecb0c62cd06462b31e3.
// Original implementation: Sergei Ilinykh (AnyKeep).
#include "mediachunkscheduler.h"
#include <QtTest>
#include <functional>
#include <utility>

class MediaChunkSchedulerTest : public QObject {
    Q_OBJECT
private slots:
    void oneFetchForMultipleWaiters()
    {
        MediaChunkScheduler             scheduler;
        int                             starts     = 0;
        int                             deliveries = 0;
        MediaChunkScheduler::Completion complete;
        const auto                      fetch = [&](MediaChunkScheduler::Completion done) {
            ++starts;
            complete = std::move(done);
        };
        const auto callback = [&](QByteArray bytes, QString error) {
            QVERIFY(error.isEmpty());
            QCOMPARE(bytes, QByteArrayLiteral("verified"));
            ++deliveries;
        };
        const quint64 first  = scheduler.request(QStringLiteral("server/account/revision"), 5, QStringLiteral("webdav"),
                                                 10, fetch, callback);
        const quint64 second = scheduler.request(QStringLiteral("server/account/revision"), 5, QStringLiteral("webdav"),
                                                 100, fetch, callback);
        QVERIFY(first && second && first != second);
        QTRY_COMPARE(starts, 1);
        QCOMPARE(scheduler.activeCount(), 1);
        QCOMPARE(scheduler.pendingCount(), 0);
        complete(QByteArrayLiteral("verified"), {});
        QTRY_COMPARE(deliveries, 2);
        QCOMPARE(scheduler.activeCount(), 0);
        QVERIFY(!scheduler.cancel(first));
        QVERIFY(!scheduler.cancel(second));
    }

    void foregroundPriorityAndSingleLaneAdmission()
    {
        MediaChunkScheduler scheduler;
        scheduler.setMaxActive(2);
        scheduler.setLaneMaxActive(QStringLiteral("dav"), 1);
        QStringList                                     started;
        QHash<QString, MediaChunkScheduler::Completion> finish;
        const auto                                      fetch = [&](QString key) {
            return [&, key](MediaChunkScheduler::Completion done) {
                started.append(key);
                finish.insert(key, std::move(done));
            };
        };
        const auto callback = [](QByteArray, QString error) { QVERIFY(error.isEmpty()); };
        scheduler.request(QStringLiteral("active"), 0, QStringLiteral("dav"), 0, fetch(QStringLiteral("active")),
                          callback);
        QTRY_COMPARE(started, QStringList({ QStringLiteral("active") }));
        scheduler.request(QStringLiteral("low"), 0, QStringLiteral("dav"), 1, fetch(QStringLiteral("low")), callback);
        const quint64 high = scheduler.request(QStringLiteral("high"), 0, QStringLiteral("dav"), 2,
                                               fetch(QStringLiteral("high")), callback);
        QVERIFY(high);
        QVERIFY(scheduler.reprioritize(high, 50));
        scheduler.request(QStringLiteral("xmpp"), 0, QStringLiteral("jingle"), 5, fetch(QStringLiteral("xmpp")),
                          callback);
        QTRY_COMPARE(started.size(), 2);
        QVERIFY(started.contains(QStringLiteral("xmpp")));
        QCOMPARE(scheduler.pendingCount(), 2);
        finish.take(QStringLiteral("active"))({}, {});
        QTRY_COMPARE(started.size(), 3);
        QCOMPARE(started[2], QStringLiteral("high"));
        finish.take(QStringLiteral("high"))({}, {});
        QTRY_COMPARE(started.size(), 4);
        QCOMPARE(started[3], QStringLiteral("low"));
        finish.take(QStringLiteral("low"))({}, {});
        finish.take(QStringLiteral("xmpp"))({}, {});
        QTRY_COMPARE(scheduler.activeCount(), 0);
    }

    void cancellationDetachesOnlyOneWaiter()
    {
        MediaChunkScheduler             scheduler;
        int                             starts = 0;
        MediaChunkScheduler::Completion finish;
        int                             canceled = 0, received = 0;
        const auto                      fetch = [&](MediaChunkScheduler::Completion callback) {
            ++starts;
            finish = std::move(callback);
        };
        const auto first = scheduler.request(QStringLiteral("rev"), 1, QStringLiteral("xmpp"), 10, fetch,
                                             [&](QByteArray bytes, QString error) {
                                                 QVERIFY(bytes.isEmpty());
                                                 QVERIFY(!error.isEmpty());
                                                 ++canceled;
                                             });
        scheduler.request(QStringLiteral("rev"), 1, QStringLiteral("xmpp"), 10, fetch,
                          [&](QByteArray bytes, QString error) {
                              QVERIFY(error.isEmpty());
                              QCOMPARE(bytes, QByteArrayLiteral("ok"));
                              ++received;
                          });
        QTRY_COMPARE(starts, 1);
        QVERIFY(scheduler.cancel(first));
        QCOMPARE(canceled, 1);
        QCOMPARE(starts, 1);
        finish(QByteArrayLiteral("ok"), {});
        QTRY_COMPARE(received, 1);
        QCOMPARE(canceled, 1);
        QCOMPARE(starts, 1);
    }

    void canceledQueueDoesNotStartAndShutdownSettlesAll()
    {
        MediaChunkScheduler scheduler;
        scheduler.setMaxActive(1);
        int                             starts = 0, failures = 0;
        MediaChunkScheduler::Completion staleCompletion;
        auto                            fetch = [&](MediaChunkScheduler::Completion done) {
            ++starts;
            staleCompletion = std::move(done);
        };
        const auto failed = [&](QByteArray bytes, QString error) {
            QVERIFY(bytes.isEmpty());
            QVERIFY(!error.isEmpty());
            ++failures;
        };
        scheduler.request(QStringLiteral("first"), 0, QStringLiteral("dav"), 0, fetch, failed);
        QTRY_COMPARE(starts, 1);
        auto queued = scheduler.request(QStringLiteral("second"), 0, QStringLiteral("dav"), 10, fetch, failed);
        QVERIFY(queued);
        QVERIFY(scheduler.cancel(queued));
        QCOMPARE(failures, 1);
        QCOMPARE(scheduler.pendingCount(), 0);
        scheduler.shutdown();
        QCOMPARE(failures, 2);
        QCOMPARE(scheduler.activeCount(), 0);
        staleCompletion(QByteArrayLiteral("late"), {});
        QTest::qWait(20);
        QCOMPARE(failures, 2);
        QCOMPARE(starts, 1);
        QVERIFY(!scheduler.request(QStringLiteral("third"), 0, QStringLiteral("dav"), 0, fetch, failed));
        QCOMPARE(failures, 3);
    }

    void cancelLastWaiterAbortsPhysicalFetchButHoldsNetworkSlot()
    {
        MediaChunkScheduler scheduler;
        scheduler.setMaxActive(1);
        int                                    starts = 0, aborts = 0, cancellations = 0;
        QList<MediaChunkScheduler::Completion> completions;
        const auto fetch = [&](MediaChunkScheduler::Completion done) -> MediaChunkScheduler::Abort {
            ++starts;
            completions.append(std::move(done));
            return [&] { ++aborts; };
        };
        const quint64 ticket = scheduler.requestCancelable(QStringLiteral("file"), 0, QStringLiteral("dav"), 100, fetch,
                                                           [&](QByteArray bytes, QString error) {
                                                               QVERIFY(bytes.isEmpty());
                                                               QVERIFY(!error.isEmpty());
                                                               ++cancellations;
                                                           });
        QVERIFY(ticket);
        QTRY_COMPARE(starts, 1);
        const quint64 next = scheduler.requestCancelable(QStringLiteral("another"), 0, QStringLiteral("dav"), 50, fetch,
                                                         [&](QByteArray, QString error) { QVERIFY(error.isEmpty()); });
        QVERIFY(next);
        QVERIFY(scheduler.cancel(ticket));
        QCOMPARE(aborts, 1);
        QCOMPARE(cancellations, 1);
        QCOMPARE(scheduler.activeCount(), 1);
        QCOMPARE(scheduler.pendingCount(), 1);
        QCOMPARE(starts, 1); // Abort has NOT released physical admission.
        completions.takeFirst()({}, QStringLiteral("Aborted"));
        QTRY_COMPARE(starts, 2);
        completions.takeFirst()({}, {});
        QTRY_COMPARE(scheduler.activeCount(), 0);
        QCOMPARE(aborts, 1);
    }

    void lateWaiterRetriesAfterOrphanAbort()
    {
        MediaChunkScheduler                    scheduler;
        int                                    starts = 0, aborts = 0, canceled = 0, delivered = 0;
        QList<MediaChunkScheduler::Completion> completions;
        auto fetch = [&](MediaChunkScheduler::Completion done) -> MediaChunkScheduler::Abort {
            ++starts;
            completions.append(std::move(done));
            return [&] { ++aborts; };
        };
        const quint64 oldTicket = scheduler.requestCancelable(QStringLiteral("rev"), 9, QStringLiteral("dav"), 10,
                                                              fetch, [&](QByteArray, QString error) {
                                                                  QVERIFY(!error.isEmpty());
                                                                  ++canceled;
                                                              });
        QTRY_COMPARE(starts, 1);
        QVERIFY(scheduler.cancel(oldTicket));
        QCOMPARE(aborts, 1);
        scheduler.requestCancelable(QStringLiteral("rev"), 9, QStringLiteral("dav"), 100, fetch,
                                    [&](QByteArray bytes, QString error) {
                                        QVERIFY(error.isEmpty());
                                        QCOMPARE(bytes, QByteArrayLiteral("authenticated"));
                                        ++delivered;
                                    });
        QCOMPARE(starts, 1);
        completions.takeFirst()({}, QStringLiteral("Aborted previous waiter"));
        QTRY_COMPARE(starts, 2);
        QCOMPARE(delivered, 0);
        completions.takeFirst()(QByteArrayLiteral("authenticated"), {});
        QTRY_COMPARE(delivered, 1);
        QCOMPARE(canceled, 1);
        QCOMPARE(scheduler.activeCount(), 0);
    }

    void stoppedTransportCanReleaseAnOrphanedActiveSlot()
    {
        MediaChunkScheduler scheduler;
        scheduler.setMaxActive(1);
        int                             started = 0, failures = 0;
        MediaChunkScheduler::Completion stale;
        auto                            fetch = [&](MediaChunkScheduler::Completion finish) {
            ++started;
            stale = std::move(finish);
        };
        const quint64 ticket = scheduler.request(QStringLiteral("abandoned"), 0, QStringLiteral("nextcloud"), 10, fetch,
                                                 [&](QByteArray, QString error) {
                                                     QVERIFY(!error.isEmpty());
                                                     ++failures;
                                                 });
        QTRY_COMPARE(started, 1);
        QVERIFY(scheduler.abandonStopped(ticket));
        QCOMPARE(failures, 1);
        QCOMPARE(scheduler.activeCount(), 0);
        // A late completion from a defunct worker must not resurrect state.
        stale(QByteArrayLiteral("obsolete"), {});
        QTest::qWait(10);
        QCOMPARE(scheduler.activeCount(), 0);
        QCOMPARE(failures, 1);
        QVERIFY(!scheduler.abandonStopped(ticket));
    }

    void observedJingleTransportDefersWebDavWithoutBreakingBundleHandover()
    {
        MediaChunkScheduler scheduler;
        scheduler.setMaxActive(2);
        auto *firstSession  = new QObject;
        auto *secondSession = new QObject;
        // Multiple Jingle contents in one BUNDLE share a physical session.
        scheduler.observeExternalTransfer(firstSession);
        scheduler.observeExternalTransfer(firstSession);
        QCOMPARE(scheduler.externalActiveCount(), 1);
        scheduler.observeExternalTransfer(secondSession);
        QCOMPARE(scheduler.externalActiveCount(), 2);
        QCOMPARE(scheduler.activeCount(), 2);

        // SCTP/BUNDLE handover is not blocked or forcibly paused. It may
        // briefly oversubscribe if another provider already has a slot.
        auto *handoverSession = new QObject;
        scheduler.observeExternalTransfer(handoverSession);
        QCOMPARE(scheduler.externalActiveCount(), 3);
        int                             starts = 0, deliveries = 0;
        MediaChunkScheduler::Completion finish;
        scheduler.request(
            QStringLiteral("dav/account/rev"), 1, QStringLiteral("webdav"), 100,
            [&](MediaChunkScheduler::Completion completion) {
                ++starts;
                finish = std::move(completion);
            },
            [&](QByteArray value, QString error) {
                QVERIFY(error.isEmpty());
                QCOMPARE(value, QByteArrayLiteral("verified"));
                ++deliveries;
            });
        QCoreApplication::processEvents();
        QCOMPARE(starts, 0);
        // Draining an old session frees its physical reservation.
        scheduler.releaseExternalTransfer(firstSession);
        QCOMPARE(scheduler.externalActiveCount(), 2);
        QCoreApplication::processEvents();
        QCOMPARE(starts, 0);
        delete firstSession;
        QCOMPARE(scheduler.externalActiveCount(), 2);
        delete secondSession; // QObject destruction also releases reservation.
        QTRY_COMPARE(starts, 1);
        QCOMPARE(scheduler.activeCount(), 2); // handover + WebDAV
        finish(QByteArrayLiteral("verified"), {});
        QTRY_COMPARE(deliveries, 1);
        QCOMPARE(scheduler.activeCount(), 1);
        delete handoverSession;
        QCOMPARE(scheduler.activeCount(), 0);
    }

    void retiringProviderReleasesOrphanedAndSharedFetches()
    {
        QObject             workerOwner;
        QObject             survivingOwner;
        MediaChunkScheduler scheduler;
        scheduler.setMaxActive(1);
        int                             networkStarts     = 0;
        int                             canceled          = 0;
        int                             survivingFailures = 0;
        MediaChunkScheduler::Completion abandonedNetwork;
        auto                            fetch = [&](MediaChunkScheduler::Completion completion) {
            ++networkStarts;
            abandonedNetwork = std::move(completion);
        };
        const auto staleTicket = scheduler.request(
            QStringLiteral("account:revisionA"), 2, QStringLiteral("dav"), 20, fetch,
            [&](QByteArray, QString error) {
                QVERIFY(!error.isEmpty());
                ++canceled;
            },
            &workerOwner);
        QVERIFY(staleTicket);
        scheduler.request(
            QStringLiteral("account:revisionA"), 2, QStringLiteral("dav"), 20, fetch,
            [&](QByteArray, QString error) {
                QVERIFY(!error.isEmpty());
                ++survivingFailures;
            },
            &survivingOwner);
        QTRY_COMPARE(networkStarts, 1);
        QVERIFY(scheduler.cancel(staleTicket));
        QCOMPARE(canceled, 1);
        QCOMPARE(scheduler.activeCount(), 1);
        // Even with no old ticket, the original fetch is still owned by the
        // stopped worker. Consumers from another owner fail once and may retry.
        scheduler.retireStoppedOwner(&workerOwner);
        QCOMPARE(scheduler.activeCount(), 0);
        QCOMPARE(survivingFailures, 1);
        QCOMPARE(canceled, 1);
        abandonedNetwork(QByteArrayLiteral("stale"), {});
        QTest::qWait(10);
        QCOMPARE(scheduler.activeCount(), 0);
        QCOMPARE(survivingFailures, 1);

        int        retried     = 0;
        auto       finishRetry = MediaChunkScheduler::Completion();
        const auto retry       = [&](MediaChunkScheduler::Completion completion) {
            ++networkStarts;
            finishRetry = std::move(completion);
        };
        scheduler.request(
            QStringLiteral("account:revisionA"), 2, QStringLiteral("dav"), 20, retry,
            [&](QByteArray bytes, QString error) {
                QVERIFY(error.isEmpty());
                QCOMPARE(bytes, QByteArrayLiteral("fresh"));
                ++retried;
            },
            &survivingOwner);
        QTRY_COMPARE(networkStarts, 2);
        finishRetry(QByteArrayLiteral("fresh"), {});
        QTRY_COMPARE(retried, 1);
    }

    void immutableKeysAndTransportGroupsAreIsolated()
    {
        MediaChunkScheduler scheduler;
        scheduler.setMaxActive(3);
        scheduler.setLaneMaxActive(QStringLiteral("dav"), 3);
        int                                    starts = 0;
        QList<MediaChunkScheduler::Completion> finishes;
        const auto                             fetch = [&](MediaChunkScheduler::Completion done) {
            ++starts;
            finishes.append(std::move(done));
        };
        int        completions = 0, mismatches = 0;
        const auto okay = [&](QByteArray, QString error) {
            QVERIFY(error.isEmpty());
            ++completions;
        };
        scheduler.request(QStringLiteral("accountA:etag1"), 0, QStringLiteral("dav"), 1, fetch, okay);
        scheduler.request(QStringLiteral("accountB:etag1"), 0, QStringLiteral("dav"), 1, fetch, okay);
        scheduler.request(QStringLiteral("accountA:etag2"), 0, QStringLiteral("dav"), 1, fetch, okay);
        scheduler.request(QStringLiteral("accountA:etag1"), 0, QStringLiteral("xmpp"), 1, fetch,
                          [&](QByteArray, QString error) {
                              QVERIFY(!error.isEmpty());
                              ++mismatches;
                          });
        QCOMPARE(mismatches, 1);
        QTRY_COMPARE(starts, 3);
        for (auto &finish : finishes)
            finish({}, {});
        QTRY_COMPARE(completions, 3);
    }
};

QTEST_GUILESS_MAIN(MediaChunkSchedulerTest)
#include "chunkscheduler.moc"
