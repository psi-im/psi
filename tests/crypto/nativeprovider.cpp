// SPDX-License-Identifier: GPL-2.0-or-later
#include "qcaproviderpolicy.h"
#include <QSignalSpy>
#include <QTest>
#include <iterator>
#include <windows.h>
#ifdef QCA_STATIC
#include <QtPlugin>
Q_IMPORT_PLUGIN(qca_wincrypto)
#endif

class NativeProviderTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void nativeOnly()
    {
        QVERIFY(PsiCrypto::preferNativeProvider());
        const QString native = QStringLiteral("qca-wincrypto");
        QVERIFY(
            QCA::isSupported(QStringList { QStringLiteral("random"), QStringLiteral("sha1"), QStringLiteral("sha256"),
                                           QStringLiteral("sha512"), QStringLiteral("hmac(sha256)"),
                                           QStringLiteral("hkdf(sha256)"), QStringLiteral("pbkdf2(sha1)"),
                                           QStringLiteral("aes128-cbc-pkcs7"), QStringLiteral("aes128-ctr"),
                                           QStringLiteral("aes256-gcm"), QStringLiteral("rsa"), QStringLiteral("cert"),
                                           QStringLiteral("tls"), QStringLiteral("dtls"), QStringLiteral("dtls-srtp") },
                             native));
        QCA::Hash hash(QStringLiteral("sha256"));
        QCOMPARE(hash.provider()->name(), native);
        QVERIFY(!QCA::findProvider(QStringLiteral("qca-ossl")));
        for (const auto *name : { L"qca-ossl.dll", L"qca-ossld.dll", L"libqca-ossl.dll", L"libqca-ossld.dll",
                                  L"qca3-ossl.dll", L"qca3-ossld.dll", L"libqca3-ossl.dll", L"libqca3-ossld.dll" })
            QVERIFY(!GetModuleHandleW(name));
    }
    void keysAndEncryption()
    {
        auto key = QCA::KeyGenerator().createRSA(2048);
        QVERIFY(!key.isNull());
        QCOMPARE(key.provider()->name(), QStringLiteral("qca-wincrypto"));
        QCA::ConvertResult result;
        auto               loaded = QCA::PrivateKey::fromDER(key.toDER(), { }, &result);
        QCOMPARE(result, QCA::ConvertGood);
        const QCA::SecureArray message("native provider smoke test");
        const auto             signature = loaded.signMessage(message, QCA::EMSA3_SHA256);
        QVERIFY(loaded.toPublicKey().verifyMessage(message, signature, QCA::EMSA3_SHA256));
        const QCA::SymmetricKey         cipherKey(32);
        const QCA::InitializationVector cipherIV(12);
        QCA::Cipher cipher(QStringLiteral("aes256"), QCA::Cipher::GCM, QCA::Cipher::NoPadding, QCA::Encode, cipherKey,
                           cipherIV, QCA::AuthTag(16));
        QCOMPARE(cipher.provider()->name(), QStringLiteral("qca-wincrypto"));
        const auto encrypted = cipher.process(message);
        QVERIFY(cipher.ok());
        QCA::Cipher decrypt(QStringLiteral("aes256"), QCA::Cipher::GCM, QCA::Cipher::NoPadding, QCA::Decode, cipherKey,
                            cipherIV, cipher.tag());
        QCOMPARE(QCA::SecureArray(decrypt.process(encrypted)), message);
        QVERIFY(decrypt.ok());
    }

    void staticSaslMechanisms_data()
    {
        QTest::addColumn<QString>("mechanism");
        QTest::addColumn<QString>("provider");
        for (const auto *provider : { "qca-cyrus-sasl", "qca-wingss" }) {
            for (const auto *name :
                 { "PLAIN", "SCRAM-SHA-1", "SCRAM-SHA-256", "SCRAM-SHA-384", "SCRAM-SHA-512", "SCRAM-SHA-1-PLUS",
                   "SCRAM-SHA-256-PLUS", "SCRAM-SHA-384-PLUS", "SCRAM-SHA-512-PLUS" }) {
                const QByteArray row = QByteArray(provider) + '/' + name;
                QTest::newRow(row.constData()) << QString::fromLatin1(name) << QString::fromLatin1(provider);
            }
        }
    }
    void staticSaslMechanisms()
    {
        QFETCH(QString, mechanism);
        QFETCH(QString, provider);
        if (!QCA::isSupported("sasl", provider))
            QSKIP("This QCA configuration does not include the selected SASL provider");
        QCA::SASL sasl(nullptr, provider);
        sasl.setConstraints(QCA::SASL::AllowPlain);
        sasl.setUsername(QStringLiteral("user"));
        sasl.setPassword(QCA::SecureArray("pencil"));
        if (mechanism.endsWith(QLatin1String("-PLUS"))) {
            QVERIFY(sasl.supportsChannelBinding());
            QVERIFY(sasl.setChannelBinding(QStringLiteral("tls-exporter"), QByteArray(32, '\x5a'), true));
        }
        QSignalSpy started(&sasl, &QCA::SASL::clientStarted);
        QSignalSpy errors(&sasl, &QCA::SASL::error);
        sasl.startClient(QStringLiteral("test"), QStringLiteral("example.test"), { mechanism });
        QTRY_VERIFY_WITH_TIMEOUT(!started.isEmpty() || !errors.isEmpty(), 5000);
        QVERIFY2(errors.isEmpty(), "The built-in SASL mechanism could not start without external plugins");
        for (const auto *name :
             { L"sasl2.dll", L"sasl2d.dll", L"libsasl.dll", L"libsasld.dll", L"libsasl2.dll", L"libsasl2d.dll",
               L"libsasl2-3.dll", L"saslPLAIN.dll", L"saslSCRAM.dll", L"libplain-3.dll", L"libscram-3.dll" })
            if (const auto module = GetModuleHandleW(name)) {
                wchar_t     path[32768];
                const DWORD length = GetModuleFileNameW(module, path, DWORD(std::size(path)));
                QFAIL(qPrintable(
                    QStringLiteral("Unexpected external SASL module: %1")
                        .arg(length ? QString::fromWCharArray(path, int(length)) : QString::fromWCharArray(name))));
            }
        QCOMPARE(sasl.mechanism(), mechanism);
        QVERIFY(started.constFirst().at(0).toBool());
        QVERIFY(!started.constFirst().at(1).toByteArray().isEmpty());
        if (mechanism.endsWith(QLatin1String("-PLUS")))
            QVERIFY(started.constFirst().at(1).toByteArray().startsWith("p=tls-exporter,,"));
    }

private:
    QCA::Initializer _qca;
};
QTEST_GUILESS_MAIN(NativeProviderTest)
#include "nativeprovider.moc"
