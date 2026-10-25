/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "vcardfactory.h"
#include "iris/xmpp_vcard4.h"
#include "iris/xmpp_vcard.h"

#include <QtTest/QtTest>
#include <QDomDocument>

class VCardRequestMergeTest final : public QObject {
    Q_OBJECT
private slots:
    void modernFieldsWinAndLegacyFillsGaps()
    {
        XMPP::VCard4::VCard modern;
        modern.setFullName(QStringLiteral("Modern"));
        XMPP::VCard4::VCard legacy;
        legacy.setFullName(QStringLiteral("Old name"));
        legacy.setNickName(QStringList { QStringLiteral("Old nickname") });
        legacy.setEmails(QStringLiteral("old@example.org"));
        legacy.setPhoto(XMPP::VCard4::UriValue(QByteArray("photo-pixels"), QStringLiteral("image/png")));

        const auto original = modern;
        VCardFactory::supplementFromLegacy(modern, legacy);
        QCOMPARE(modern.fullName().preferred().data, QStringLiteral("Modern"));
        QCOMPARE(modern.nickName().preferred().data.value(0), QStringLiteral("Old nickname"));
        QCOMPARE(modern.emails().preferred().data, QStringLiteral("old@example.org"));
        QCOMPARE(QByteArray(modern.photo()), QByteArray("photo-pixels"));
        QVERIFY(original.nickName().isEmpty()); // Copy-on-write preserved the original.
    }

    void legacyIsUsedIfPepHasNoCard()
    {
        XMPP::VCard4::VCard modern;
        XMPP::VCard4::VCard legacy;
        legacy.setFullName(QStringLiteral("Legacy only"));
        VCardFactory::supplementFromLegacy(modern, legacy);
        QCOMPARE(modern.fullName().preferred().data, QStringLiteral("Legacy only"));
    }

    void embeddedLegacyPhotoWinsOverModernUriOnlyPhoto()
    {
        XMPP::VCard4::VCard modern;
        modern.setFullName(QStringLiteral("Modern"));
        modern.setPhoto(XMPP::VCard4::UriValue(QStringLiteral("https://example.org/photo")));
        XMPP::VCard4::VCard legacy;
        legacy.setPhoto(XMPP::VCard4::UriValue(QByteArray("embedded"), QStringLiteral("image/jpeg")));
        VCardFactory::supplementFromLegacy(modern, legacy);
        QCOMPARE(modern.fullName().preferred().data, QStringLiteral("Modern"));
        QCOMPARE(QByteArray(modern.photo()), QByteArray("embedded"));
    }

    void avatarRefreshRetainsAllExistingFields()
    {
        XMPP::VCard4::VCard cached;
        cached.setFullName(QStringLiteral("Still modern"));
        cached.setNickName(QStringList { QStringLiteral("Nick") });
        cached.setPhoto(XMPP::VCard4::UriValue(QByteArray("old-photo"), QStringLiteral("image/jpeg")));

        // XMPP::VCard's default constructor creates a null card; its setters
        // require initialized data. Parse the same legacy XML as a real IQ.
        QDomDocument doc;
        const QDomElement root = doc.createElementNS(QStringLiteral("vcard-temp"), QStringLiteral("vCard"));
        doc.appendChild(root);
        XMPP::VCard temp = XMPP::VCard::fromXml(root);
        QVERIFY(!temp.isNull());
        temp.setFullName(QStringLiteral("Stale old name"));
        temp.setPhoto(QByteArray("new-photo"));
        XMPP::VCard4::VCard incoming;
        incoming.fromVCardTemp(temp);

        QVERIFY(VCardFactory::applyLegacyPhoto(cached, incoming));
        QCOMPARE(cached.fullName().preferred().data, QStringLiteral("Still modern"));
        QCOMPARE(cached.nickName().preferred().data.value(0), QStringLiteral("Nick"));
        QCOMPARE(QByteArray(cached.photo()), QByteArray("new-photo"));
    }

    void missingPhotoDoesNotEraseCachedPhotoOrOtherData()
    {
        XMPP::VCard4::VCard cached;
        cached.setFullName(QStringLiteral("Modern"));
        cached.setPhoto(XMPP::VCard4::UriValue(QByteArray("original"), QStringLiteral("image/jpeg")));
        XMPP::VCard4::VCard legacy;
        legacy.setFullName(QStringLiteral("Legacy no photo"));
        QVERIFY(!VCardFactory::applyLegacyPhoto(cached, legacy));
        QCOMPARE(cached.fullName().preferred().data, QStringLiteral("Modern"));
        QCOMPARE(QByteArray(cached.photo()), QByteArray("original"));
    }
};

QTEST_GUILESS_MAIN(VCardRequestMergeTest)
#include "vcardrequestmergetest.moc"
