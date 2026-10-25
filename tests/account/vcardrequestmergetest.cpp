/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "vcardfactory.h"
#include "iris/xmpp_jid.h"
#include "iris/xmpp_vcard4.h"
#include "iris/xmpp_vcard.h"

#include <QtTest/QtTest>
#include <QDomDocument>
#include <QTemporaryDir>

class VCardRequestMergeTest final : public QObject {
    Q_OBJECT
private slots:
    void selfPhotoPublicationUsesBytesAndPreservesFields()
    {
        const QByteArray png = QByteArray::fromBase64(
            "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aLk8AAAAASUVORK5CYII=");
        QVERIFY(!png.isEmpty());

        XMPP::VCard4::VCard card;
        card.setFullName(QStringLiteral("Existing modern full name"));
        const XMPP::VCard4::VCard original = card;

        QVERIFY(VCardFactory::setEmbeddedPhoto(card, png));
        QCOMPARE(QByteArray(card.photo()), png);
        QCOMPARE(card.photo().first().data.mediaType, QStringLiteral("image/png"));
        QCOMPARE(card.fullName().preferred().data, QStringLiteral("Existing modern full name"));
        QVERIFY(original.photo().isEmpty()); // Do not mutate shared Iris vCard data.
        QVERIFY(!VCardFactory::setEmbeddedPhoto(card, png)); // Duplicate is a no-op.

        QVERIFY(!VCardFactory::setEmbeddedPhoto(card, QByteArrayLiteral("not an image")));
        QCOMPARE(QByteArray(card.photo()), png);
    }

    void explicitPhotoRemovalCannotResurrectOldBytes()
    {
        const QByteArray png = QByteArray::fromBase64(
            "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aLk8AAAAASUVORK5CYII=");
        XMPP::VCard4::VCard card;
        card.setFullName(QStringLiteral("Kept modern name"));
        QVERIFY(VCardFactory::setEmbeddedPhoto(card, png));

        const XMPP::VCard4::VCard oldSnapshot = card;
        QVERIFY(VCardFactory::clearPhoto(card));
        QVERIFY(card.photo().isEmpty()); // Not a list containing an empty URI.
        QCOMPARE(card.fullName().preferred().data, QStringLiteral("Kept modern name"));
        QCOMPARE(QByteArray(oldSnapshot.photo()), png);
        QVERIFY(!VCardFactory::clearPhoto(card));
    }

    void explicitDeletionSurvivesDiskCacheReload()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("contact.xml"));
        const QByteArray png = QByteArray::fromBase64(
            "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aLk8AAAAASUVORK5CYII=");

        XMPP::VCard4::VCard original;
        original.setFullName(QStringLiteral("Stay in cache"));
        QVERIFY(VCardFactory::setEmbeddedPhoto(original, png));
        QVERIFY(original.save(path));
        auto cached = VCardFactory::readCachedVCard(path);
        QVERIFY(cached);
        QCOMPARE(QByteArray(cached.photo()), png);

        QVERIFY(VCardFactory::clearPhoto(cached));
        QVERIFY(cached.save(path));
        const auto reloaded = VCardFactory::readCachedVCard(path);
        QVERIFY(reloaded);
        QVERIFY(reloaded.photo().isEmpty()); // An old PHOTO cannot be reimported.
        QCOMPARE(reloaded.fullName().preferred().data, QStringLiteral("Stay in cache"));
    }

    void uriOnlyPhotoIsRemovedAsAnEntireProperty()
    {
        XMPP::VCard4::VCard card;
        card.setPhoto(XMPP::VCard4::UriValue(QStringLiteral("https://example.org/photo.png")));
        QVERIFY(!card.photo().isEmpty());
        QVERIFY(QByteArray(card.photo()).isEmpty());
        QVERIFY(VCardFactory::clearPhoto(card));
        QVERIFY(card.photo().isEmpty());
    }

    void publicationWithoutCachedVCardCanStartWithPhoto()
    {
        const QByteArray png = QByteArray::fromBase64(
            "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aLk8AAAAASUVORK5CYII=");
        XMPP::VCard4::VCard empty;
        QVERIFY(!empty);
        QVERIFY(VCardFactory::setEmbeddedPhoto(empty, png));
        QVERIFY(empty);
        QCOMPARE(QByteArray(empty.photo()), png);
        QVERIFY(VCardFactory::clearPhoto(empty));
        QVERIFY(empty.photo().isEmpty());
    }

    void mergedCacheIdentityIsAccountScoped()
    {
        using F = VCardFactory;
        const XMPP::Jid peer(QStringLiteral("contact@example.org/laptop"));
        const XMPP::Jid otherResource(QStringLiteral("contact@example.org/phone"));
        const auto a = F::accountCacheKey(QStringLiteral("account-a"), peer, {});
        const auto b = F::accountCacheKey(QStringLiteral("account-b"), peer, {});
        QVERIFY(a != b);
        QCOMPARE(a, F::accountCacheKey(QStringLiteral("account-a"), otherResource, {}));
        QVERIFY(F::accountCacheKey(QStringLiteral("account-a"), peer, F::MucUser)
             != F::accountCacheKey(QStringLiteral("account-a"), otherResource, F::MucUser));

        const QString dir = QStringLiteral("/tmp/test-vcards");
        const auto pa = F::mergedCardFile(dir, QStringLiteral("account-a"), peer, {});
        const auto pb = F::mergedCardFile(dir, QStringLiteral("account-b"), peer, {});
        QVERIFY(pa != pb);
        QCOMPARE(pa, F::mergedCardFile(dir, QStringLiteral("account-a"), otherResource, {}));
        QVERIFY(pa != F::sourceCardFile(dir, QStringLiteral("account-a"), peer, {}, true));
        QVERIFY(pa != F::sourceCardFile(dir, QStringLiteral("account-a"), peer, {}, false));
    }

    void requestsNeverCoalesceAcrossAccountsOrSessions()
    {
        using F = VCardFactory;
        const XMPP::Jid peer(QStringLiteral("contact@example.org/laptop"));
        const auto base = F::requestQueueKey(QStringLiteral("alice"), 1, peer, {});
        QCOMPARE(base, F::requestQueueKey(QStringLiteral("alice"), 1,
                                          XMPP::Jid(QStringLiteral("contact@example.org/phone")), {}));
        QCOMPARE(base, F::requestQueueKey(QStringLiteral("alice"), 1, peer, F::Cache));
        QVERIFY(base != F::requestQueueKey(QStringLiteral("bob"), 1, peer, {}));
        QVERIFY(base != F::requestQueueKey(QStringLiteral("alice"), 2, peer, {}));
        QVERIFY(base != F::requestQueueKey(QStringLiteral("alice"), 1, peer, F::PhotoOnly));
        QVERIFY(base != F::requestQueueKey(QStringLiteral("alice"), 1, peer, F::ForceVCardTemp));
        QVERIFY(base != F::requestQueueKey(QStringLiteral("alice"), 1, peer, F::Silent));
        QVERIFY(base != F::requestQueueKey(QStringLiteral("alice"), 1, peer, F::MucUser));
        QVERIFY(base != F::requestQueueKey(QStringLiteral("alice"), 1,
                                           XMPP::Jid(QStringLiteral("other@example.org")), {}));
        const auto muc1 = F::requestQueueKey(QStringLiteral("alice"), 1, peer, F::MucUser);
        const auto muc2 = F::requestQueueKey(QStringLiteral("alice"), 1,
                                             XMPP::Jid(QStringLiteral("contact@example.org/other")), F::MucUser);
        QVERIFY(muc1 != muc2); // Occupants, unlike ordinary contacts, need full JIDs.
    }

    void legacyPhotoRemovalPreservesModernImageWithoutNetwork()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const XMPP::Jid jid(QStringLiteral("contact@example.org/mobile"));
        const QString modernFile = VCardFactory::sourceCardFile(
            dir.path(), QStringLiteral("account-a"), jid, {}, true);
        const QString legacyFile = VCardFactory::sourceCardFile(
            dir.path(), QStringLiteral("account-a"), jid, {}, false);
        XMPP::VCard4::VCard modern;
        modern.setFullName(QStringLiteral("Modern name"));
        modern.setPhoto(XMPP::VCard4::UriValue(
            QByteArrayLiteral("modern-photo"), QStringLiteral("image/png")));
        XMPP::VCard4::VCard legacy;
        legacy.setPhoto(XMPP::VCard4::UriValue(
            QByteArrayLiteral("legacy-photo"), QStringLiteral("image/jpeg")));

        QVERIFY(modern.save(modernFile));
        QVERIFY(legacy.save(legacyFile));
        auto restoredModern = VCardFactory::readCachedVCard(modernFile);
        auto restoredLegacy = VCardFactory::readCachedVCard(legacyFile);
        const auto both = VCardFactory::mergeSourceCards(restoredModern, restoredLegacy);
        QCOMPARE(QByteArray(both.photo()), QByteArrayLiteral("modern-photo"));

        QVERIFY(VCardFactory::clearPhoto(restoredLegacy));
        QVERIFY(restoredLegacy.save(legacyFile));
        restoredModern = VCardFactory::readCachedVCard(modernFile);
        restoredLegacy = VCardFactory::readCachedVCard(legacyFile);
        const auto afterRemoval = VCardFactory::mergeSourceCards(restoredModern, restoredLegacy);
        QCOMPARE(QByteArray(afterRemoval.photo()), QByteArrayLiteral("modern-photo"));
        QCOMPARE(afterRemoval.fullName().preferred().data, QStringLiteral("Modern name"));
        QVERIFY(restoredLegacy.photo().isEmpty());
    }

    void sourceFileNamesAreAccountIsolatedAndModeSensitive()
    {
        const QString dir = QStringLiteral("/tmp/cache");
        const XMPP::Jid jid(QStringLiteral("room@example.org/alice"));
        const auto a = VCardFactory::sourceCardFile(dir, QStringLiteral("acc-a"), jid, {}, true);
        const auto b = VCardFactory::sourceCardFile(dir, QStringLiteral("acc-b"), jid, {}, true);
        const auto legacy = VCardFactory::sourceCardFile(dir, QStringLiteral("acc-a"), jid, {}, false);
        const auto occupant = VCardFactory::sourceCardFile(
            dir, QStringLiteral("acc-a"), jid, VCardFactory::MucUser, true);
        QVERIFY(a != b);
        QVERIFY(a != legacy);
        QVERIFY(a != occupant);
        QCOMPARE(a, VCardFactory::sourceCardFile(dir, QStringLiteral("acc-a"),
            XMPP::Jid(QStringLiteral("room@example.org/other")), {}, true));
    }

    void legacyOnlySourceCanClearPhotoWithoutNetwork()
    {
        XMPP::VCard4::VCard modern;
        XMPP::VCard4::VCard legacy;
        legacy.setFullName(QStringLiteral("Legacy name"));
        legacy.setPhoto(XMPP::VCard4::UriValue(
            QByteArrayLiteral("legacy-photo"), QStringLiteral("image/jpeg")));
        auto merged = VCardFactory::mergeSourceCards(modern, legacy);
        QCOMPARE(QByteArray(merged.photo()), QByteArrayLiteral("legacy-photo"));
        QVERIFY(VCardFactory::clearPhoto(legacy));
        merged = VCardFactory::mergeSourceCards(modern, legacy);
        QVERIFY(merged.photo().isEmpty());
        QCOMPARE(merged.fullName().preferred().data, QStringLiteral("Legacy name"));
    }

    void fullAndPhotoOnlyRequestModes()
    {
        using F = VCardFactory;
        QVERIFY(F::shouldFetchModern({}));
        QVERIFY(F::shouldFetchModern(F::Cache));
        QVERIFY(F::shouldFetchModern(F::Silent));
        QVERIFY(!F::shouldFetchModern(F::PhotoOnly));
        QVERIFY(!F::shouldFetchModern(F::ForceVCardTemp));
        QVERIFY(!F::shouldFetchModern(F::MucRoom));
        QVERIFY(!F::shouldFetchModern(F::MucUser));
        QVERIFY(!F::shouldFetchModern(F::PhotoOnly | F::Silent));
    }
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
