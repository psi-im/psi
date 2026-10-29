// SPDX-License-Identifier: GPL-2.0-or-later
#include "qcaproviderpolicy.h"
#include <QTest>
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

private:
    QCA::Initializer _qca;
};
QTEST_GUILESS_MAIN(NativeProviderTest)
#include "nativeprovider.moc"
