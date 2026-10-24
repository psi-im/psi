/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "vcarddiskcache.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class VCardDiskCacheTest : public QObject {
    Q_OBJECT
private slots:
    void modernCardAvailableOnFirstRead()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("modern.xml"));

        XMPP::VCard4::VCard original;
        original.setFullName(QStringLiteral("Modern contact"));
        original.setNickName(QStringList { QStringLiteral("Modern nickname") });
        QVERIFY(original.save(path));

        const auto first = VCardDiskCache::read(path);
        QVERIFY(first);
        QCOMPARE(first.fullName().preferred().data, QStringLiteral("Modern contact"));
        QCOMPARE(first.nickName().preferred().data.value(0), QStringLiteral("Modern nickname"));

        const auto again = VCardDiskCache::read(path);
        QVERIFY(again);
        QCOMPARE(again.fullName().preferred().data, QStringLiteral("Modern contact"));
    }

    void legacyCardAvailableOnFirstRead()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("legacy.xml"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray xml = QByteArrayLiteral("<vCard xmlns='vcard-temp'><FN>Legacy contact</FN>"
                                                "<NICKNAME>Legacy nick</NICKNAME></vCard>");
        QCOMPARE(file.write(xml), qint64(xml.size()));
        file.close();

        const auto first = VCardDiskCache::read(path);
        QVERIFY(first);
        QCOMPARE(first.fullName().preferred().data, QStringLiteral("Legacy contact"));
        QCOMPARE(first.nickName().preferred().data.value(0), QStringLiteral("Legacy nick"));
    }

    void uriOnlyPhotoIsNotNoPhoto()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("uri.xml"));
        XMPP::VCard4::VCard original;
        original.setPhoto(XMPP::VCard4::UriValue(QStringLiteral("https://example.org/photo.png")));
        QVERIFY(original.save(path));

        const auto card = VCardDiskCache::read(path);
        QVERIFY(card);
        QVERIFY(!card.photo().isEmpty()); // A reference exists even without embedded bytes.
        QVERIFY(QByteArray(card.photo()).isEmpty());
        QCOMPARE(card.photo().first().data.url.toString(), QStringLiteral("https://example.org/photo.png"));
    }

    void missingAndInvalidFilesAreNotPositiveHits()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(!VCardDiskCache::read(dir.filePath(QStringLiteral("missing.xml"))));

        const QString path = dir.filePath(QStringLiteral("invalid.xml"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("<broken") > 0);
        file.close();
        QVERIFY(!VCardDiskCache::read(path));
    }
};

QTEST_GUILESS_MAIN(VCardDiskCacheTest)
#include "vcarddiskcachetest.moc"
