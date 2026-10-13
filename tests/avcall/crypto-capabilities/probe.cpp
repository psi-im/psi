// SPDX-License-Identifier: GPL-2.0-or-later
#include "mediacryptocapabilities.h"
#include <QSemaphore>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <thread>

class CryptoCapabilitiesTest : public QObject {
    Q_OBJECT
private slots:
    void delayedProbeDoesNotBlockUi()
    {
        QSemaphore              entered, release;
        std::atomic<int>        executions { 0 };
        std::atomic<bool>       offUiThread { false };
        const auto              uiThread = QThread::currentThread();
        MediaCryptoCapabilities probe;
        struct ReleaseOnExit {
            QSemaphore &gate;
            ~ReleaseOnExit() { gate.release(); }
        } cleanup { release };
        bool       notificationOnUi = false;
        QSignalSpy changed(&probe, &MediaCryptoCapabilities::changed);
        connect(&probe, &MediaCryptoCapabilities::changed, this,
                [&] { notificationOnUi = QThread::currentThread() == uiThread; });
        probe.start([&] {
            ++executions;
            offUiThread = QThread::currentThread() != uiThread;
            entered.release();
            release.acquire();
            return QStringList { QStringLiteral("SRTP_AES128_CM_HMAC_SHA1_80") };
        });
        QVERIFY(entered.tryAcquire(1, 3000));
        QVERIFY(offUiThread);
        bool heartbeat = false;
        QTimer::singleShot(0, this, [&] { heartbeat = true; });
        for (int i = 0; i < 10000; ++i) {
            QVERIFY(!probe.complete());
            QVERIFY(probe.profiles().isEmpty());
        }
        QCoreApplication::processEvents();
        QVERIFY(heartbeat);
        QCOMPARE(changed.count(), 0);
        probe.start([] { return QStringList { QStringLiteral("unexpected second probe") }; });
        release.release();
        QTRY_VERIFY(probe.complete());
        QCOMPARE(changed.count(), 1);
        QVERIFY(notificationOnUi);
        QCOMPARE(executions.load(), 1);
        const auto profiles = probe.profiles();
        QCOMPARE(profiles, (QStringList { QStringLiteral("SRTP_AES128_CM_HMAC_SHA1_80") }));
        for (int i = 0; i < 10000; ++i)
            QCOMPARE(probe.profiles(), profiles);
        QCOMPARE(executions.load(), 1);
    }

    void unsupportedCryptoCompletesWithEmptySnapshot()
    {
        MediaCryptoCapabilities probe;
        probe.start([] { return QStringList {}; });
        QTRY_VERIFY(probe.complete());
        QVERIFY(probe.profiles().isEmpty());
    }

    void destructionDrainsWorker()
    {
        QSemaphore        entered, release;
        std::atomic<bool> finished { false };
        auto              probe = new MediaCryptoCapabilities;
        probe->start([&] {
            entered.release();
            release.acquire();
            finished = true;
            return QStringList {};
        });
        const bool  started = entered.tryAcquire(1, 3000);
        std::thread unblock([&] { release.release(); });
        delete probe;
        unblock.join();
        QVERIFY(started);
        QVERIFY(finished);
        QCoreApplication::processEvents(); // queued finished notification must be cancelled
    }
};
QTEST_GUILESS_MAIN(CryptoCapabilitiesTest)
#include "probe.moc"
