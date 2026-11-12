/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aesgcmurl.h"
#include "filesharingfilejob.h"
#include "iris/xmpp_file-sharing.h"
#include <QtCrypto>
#ifdef QCA_STATIC
#include <QtPlugin>
#ifdef Q_OS_WIN
Q_IMPORT_PLUGIN(qca_wincrypto)
#else
Q_IMPORT_PLUGIN(qca_ossl)
#endif
#endif

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

class FileSharingFileJobTest final : public QObject {
    Q_OBJECT
    QCA::Initializer crypto;

private slots:
    void encryptedUrlKeepsKeyLocalAndRejectsMalformedMaterial()
    {
        const QString material = QString(24, 'a') + QString(64, 'b');
        const QUrl    url("aesgcm://files.example.org/media.png#" + material);
        const auto    parsed = AesGcmUrl::parse(url);
        QVERIFY(parsed);
        QCOMPARE(parsed->transport, QUrl("https://files.example.org/media.png"));
        QCOMPARE(parsed->iv.size(), 12);
        QCOMPARE(parsed->key.size(), 32);
        QVERIFY(!AesGcmUrl::parse(QUrl("aesgcm://files.example.org/media.png")));
        QVERIFY(!AesGcmUrl::parse(QUrl("aesgcm://files.example.org/media.png#" + material + '0')));
        QVERIFY(!AesGcmUrl::parse(QUrl("aesgcm://files.example.org/media.png#" + material.left(87) + 'z')));
        QVERIFY(!AesGcmUrl::parse(QUrl("http://files.example.org/media.png#" + material)));
    }
    void wholeDownloadsVerifyAllClaims_data()
    {
        QTest::addColumn<bool>("valid");
        QTest::newRow("verified") << true;
        QTest::newRow("mismatch") << false;
    }
    void wholeDownloadsVerifyAllClaims()
    {
        QFETCH(bool, valid);
        QTemporaryDir    dir;
        const auto       path = dir.filePath("download");
        const QByteArray payload("verified whole HTTP payload");
        QFile            file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(payload), qint64(payload.size()));
        file.close();
        QList<XMPP::Hash> claims { XMPP::Hash::from(XMPP::Hash::Sha512, valid ? payload : QByteArray("other")) };
        QObject           owner;
        bool              done = false;
        FileSharingFileJob::verifyTemporaryFile(
            path, claims, &owner, [&](QList<XMPP::Hash> hashes, MediaChunkError error) {
                QCOMPARE(error.isEmpty(), valid);
                if (valid) {
                    QVERIFY(hashes.contains(claims.first()));
                    QVERIFY(hashes.contains(XMPP::Hash::from(XMPP::Hash::Sha256, payload)));
                } else {
                    QCOMPARE(error.code(), MediaChunkError::IntegrityError);
                    QVERIFY(hashes.isEmpty());
                }
                done = true;
            });
        QTRY_VERIFY(done);
        QTRY_VERIFY(!QFileInfo::exists(path));
    }
    void authenticatedMediaNeverPublishesTamperedPlaintext_data()
    {
        QTest::addColumn<bool>("valid");
        QTest::newRow("verified") << true;
        QTest::newRow("bad-tag") << false;
    }
    void authenticatedMediaNeverPublishesTamperedPlaintext()
    {
        QFETCH(bool, valid);
        const QByteArray payload(1024 * 1024, 'x');
        const auto       encrypted
            = XMPP::StatelessFileSharing::encrypt(XMPP::StatelessFileSharing::Cipher::Aes256Gcm, payload);
        QVERIFY(encrypted);
        auto bytes = encrypted->data;
        if (!valid)
            bytes[bytes.size() - 1] = char(bytes.at(bytes.size() - 1) ^ 1);
        QTemporaryDir dir;
        const auto    path = dir.filePath("ciphertext");
        QFile         file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), qint64(bytes.size()));
        file.close();
        QObject owner;
        bool    done = false;
        QString plainPath;
        FileSharingFileJob::decryptTemporaryFile(path, encrypted->key, encrypted->iv, &owner,
                                                 [&](QString plain, MediaChunkError error) {
                                                     QCOMPARE(error.isEmpty(), valid);
                                                     plainPath = plain;
                                                     if (valid) {
                                                         QFile result(plain);
                                                         QVERIFY(result.open(QIODevice::ReadOnly));
                                                         QCOMPARE(result.readAll(), payload);
                                                     } else {
                                                         QVERIFY(plain.isEmpty());
                                                         QCOMPARE(error.code(), MediaChunkError::IntegrityError);
                                                     }
                                                     done = true;
                                                 });
        QTRY_VERIFY(done);
        QTRY_VERIFY(!QFileInfo::exists(path));
        if (valid)
            QTRY_VERIFY(!QFileInfo::exists(plainPath));
    }
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
        FileSharingFileJob::sha256TemporaryFile(path, &receiver, [&](XMPP::Hash hash) {
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
        FileSharingFileJob::sha256TemporaryFile(path, &receiver, [&](XMPP::Hash hash) {
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
        FileSharingFileJob::sha256TemporaryFile(path, receiver, [&](XMPP::Hash) { callbackRan = true; });
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
        FileSharingFileJob::sha256TemporaryFile(path, &receiver, [&](XMPP::Hash hash) {
            actual = hash;
            done = true;
        });
        QVERIFY(!done);
        QTRY_VERIFY_WITH_TIMEOUT(done, 30000);
        QCOMPARE(actual.data(), reference.result());
        QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(path), 5000);
    }
};
QTEST_GUILESS_MAIN(FileSharingFileJobTest)
#include "hashjob.moc"
