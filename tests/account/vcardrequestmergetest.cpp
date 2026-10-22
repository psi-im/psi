/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "vcardrequestmerge.h"
#include "iris/xmpp_vcard.h"

#include <QtTest/QtTest>

class VCardRequestMergeTest final : public QObject {
    Q_OBJECT
private slots:
    void modernFieldsWinAndLegacyFillsGaps()
    {
        XMPP::VCard4::VCard modern;
        modern.setFullName(QStringLiteral("Modern"));
        modern.setEmails(QStringList()); // No email in the modern store.
        XMPP::VCard4::VCard legacy;
        legacy.setFullName(QStringLiteral("Old name"));
        legacy.setNickName(QStringList { QStringLiteral("Old nickname") });
        legacy.setEmails(QStringLiteral("old@example.org"));
        legacy.setPhoto(XMPP::VCard4::UriValue(QByteArray("photo-pixels"), QStringLiteral("image/png")));

        const auto original = modern;
        VCardRequestMerge::supplement(modern, legacy);
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
        VCardRequestMerge::supplement(modern, legacy);
        QCOMPARE(modern.fullName().preferred().data, QStringLiteral("Legacy only"));
    }

    void avatarRefreshRetainsAllExistingFields()
    {
        XMPP::VCard4::VCard cached;
        cached.setFullName(QStringLiteral("Still modern"));
        cached.setNickName(QStringList { QStringLiteral("Nick") });
        cached.setPhoto(XMPP::VCard4::UriValue(QByteArray("old-photo"), QStringLiteral("image/jpeg")));

        XMPP::VCard temp;
        temp.setFullName(QStringLiteral("Stale old name"));
        temp.setPhoto(QByteArray("new-photo"));
        XMPP::VCard4::VCard incoming;
        incoming.fromVCardTemp(temp);

        QVERIFY(VCardRequestMerge::applyLegacyPhoto(cached, incoming));
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
        QVERIFY(!VCardRequestMerge::applyLegacyPhoto(cached, legacy));
        QCOMPARE(cached.fullName().preferred().data, QStringLiteral("Modern"));
        QCOMPARE(QByteArray(cached.photo()), QByteArray("original"));
    }
};

QTEST_GUILESS_MAIN(VCardRequestMergeTest)
#include "vcardrequestmergetest.moc"
