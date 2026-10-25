/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharinghashjob.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

class FileSharingHashJobTest final : public QObject {
    Q_OBJECT

private slots:
    void hashCompletesAsynchronouslyOnTheReceivingThread()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath(QStringLiteral("download.tmp"));
        const QByteArray payload("an HTTP OOB download without sender hashes");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(payload), qint64(payload.size()));
        file.close();

        QObject receiver;
        bool completed = false;
        bool onGuiThread = false;
        XMPP::Hash result;
        const QString moved = dir.filePath(QStringLiteral("in-cache"));
        FileSharingHashJob::sha256TemporaryFile(path, &receiver, [&](XMPP::Hash hash) {
            completed = true;
            onGuiThread = QThread::currentThread() == QCoreApplication::instance()->thread();
            result = hash;
            QVERIFY(QFile::rename(path, moved));
        });
        QVERIFY(!completed); // No full-file read or callback on the initiating stack.
        QTRY_VERIFY_WITH_TIMEOUT(completed, 15000);
        QVERIFY(onGuiThread);
        QVERIFY(result.isValid());
        QCOMPARE(result.type(), XMPP::Hash::Sha256);
        QCOMPARE(result.data(), QCryptographicHash::hash(payload, QCryptographicHash::Sha256));
        QVERIFY(QFileInfo::exists(moved));
    }

    void failedHashCleansUpTemporaryFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath(QStringLiteral("already-removed"));
        QObject receiver;
        bool completed = false;
        FileSharingHashJob::sha256TemporaryFile(path, &receiver, [&](XMPP::Hash hash) {
            QVERIFY(!hash.isValid());
            completed = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(completed, 15000);
        QVERIFY(!QFileInfo::exists(path));
    }

    void disappearingReceiverDoesNotLeavePartialPayloadBehind()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath(QStringLiteral("big-download.tmp"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray chunk(1024 * 1024, 'd');
        for (int i = 0; i < 16; ++i)
            QCOMPARE(file.write(chunk), qint64(chunk.size()));
        file.close();

        auto *receiver = new QObject;
        bool callbackRan = false;
        FileSharingHashJob::sha256TemporaryFile(path, receiver, [&](XMPP::Hash) {
            callbackRan = true;
        });
        delete receiver;
        QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(path), 20000);
        QVERIFY(!callbackRan);
    }

    void bigFileHashesWithoutLoadingItIntoMemory()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath(QStringLiteral("large.tmp"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray chunk(1024 * 1024, char(0x5c));
        QCryptographicHash reference(QCryptographicHash::Sha256);
        for (int i = 0; i < 32; ++i) {
            QCOMPARE(file.write(chunk), qint64(chunk.size()));
            reference.addData(chunk);
        }
        file.close();

        QObject receiver;
        bool done = false;
        XMPP::Hash actual;
        FileSharingHashJob::sha256TemporaryFile(path, &receiver, [&](XMPP::Hash hash) {
            actual = hash;
            done = true;
        });
        QVERIFY(!done);
        QTRY_VERIFY_WITH_TIMEOUT(done, 30000);
        QCOMPARE(actual.data(), reference.result());
        QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(path), 5000);
    }
};
QTEST_GUILESS_MAIN(FileSharingHashJobTest)
#include "hashjob.moc"
