/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "messageattachments.h"

#include "iris/xmpp_hash.h"
#include "iris/xmpp_message.h"
#include "iris/xmpp_reference.h"
#include "iris/xmpp_url.h"

#include <QCryptographicHash>
#include <QtTest>

using namespace XMPP;

class MessageAttachmentsTest : public QObject {
    Q_OBJECT

    Reference share(const QString &source)
    {
        MediaSharing media;
        media.file.setName("photo.png");
        media.file.setMediaType("image/png");
        media.file.setSize(7);
        media.file.addHash(Hash(Hash::Sha256, QCryptographicHash::hash("payload", QCryptographicHash::Sha256)));
        media.sources.append(source);
        Reference reference(Reference::Data, source);
        reference.setMediaSharing(media);
        return reference;
    }

private slots:
    void simsSourceIsNotAttachedTwice()
    {
        Message incoming;
        incoming.setBody("Caption");
        incoming.setReferences({ share("https://example.org/picture.png?a=1") });
        incoming.urlAdd(Url("https://EXAMPLE.org/picture.png?a=1#ignored", "duplicate"));
        incoming.urlAdd(Url("xmpp:bob@example.org", "unrelated"));

        const auto result = MessageAttachments::collect(incoming);
        QCOMPARE(result.attachments.size(), 1);
        QVERIFY(result.attachments.first().reference.mediaSharing().isValid());
        QCOMPARE(result.attachments.first().description, QString("duplicate"));
        QCOMPARE(result.links.size(), 1);
        QCOMPARE(result.links.first().url(), QString("xmpp:bob@example.org"));
        QCOMPARE(incoming.urlList().size(), 2); // the message itself is not changed
        QCOMPARE(incoming.references().size(), 1);
    }

    void bodyThatIsJustTheUrlGivesWayToTheAttachment()
    {
        const QString url = "https://example.org/photo.png?version=2#frag";
        Message       incoming;
        incoming.setBody(url);
        incoming.urlAdd(Url(url, "photo"));

        const auto result = MessageAttachments::collect(incoming);
        QCOMPARE(result.attachments.size(), 1);
        const auto &attachment = result.attachments.first();
        QCOMPARE(attachment.reference.uri(), url); // the full URL, fragment included, to open
        QCOMPARE(attachment.reference.begin(), 0);
        QCOMPARE(attachment.reference.end(), url.size() - 1);
        QVERIFY(!attachment.reference.mediaSharing().isValid());
        QCOMPARE(attachment.description, QString("photo"));
        QVERIFY(result.links.isEmpty());
        QVERIFY(incoming.references().isEmpty());
    }

    void urlWithinTextStaysALink()
    {
        const QString url = "https://example.org/article";
        Message       incoming;
        incoming.setBody(QString("Look: %1 !").arg(url));
        incoming.urlAdd(Url(url, "article"));

        const auto result = MessageAttachments::collect(incoming);
        QVERIFY(result.attachments.isEmpty());
        QVERIFY(result.links.isEmpty());
    }

    void bodyMaySpellTheUrlWithoutItsFragment()
    {
        const QString url = "https://example.org/photo.png";
        Message       incoming;
        incoming.setBody("View: " + url);
        incoming.urlAdd(Url(url + "#frag", "photo"));
        QVERIFY(MessageAttachments::collect(incoming).attachments.isEmpty()); // still the link in the text

        incoming.setBody(url);
        const auto result = MessageAttachments::collect(incoming);
        QCOMPARE(result.attachments.size(), 1);
        QCOMPARE(result.attachments.first().reference.uri(), url + "#frag");
        QCOMPARE(result.attachments.first().reference.begin(), 0);
        QCOMPARE(result.attachments.first().reference.end(), url.size() - 1);
    }

    void attachmentOnlyAndRepeatedOobGiveOneAttachment()
    {
        Message incoming;
        incoming.urlAdd(Url("https://example.org/photo.png#one"));
        incoming.urlAdd(Url("https://example.org/photo.png#two"));

        const auto result = MessageAttachments::collect(incoming);
        QCOMPARE(result.attachments.size(), 1);
        QCOMPARE(result.attachments.first().reference.uri(), QString("https://example.org/photo.png#one"));
        QVERIFY(result.attachments.first().reference.begin() < 0);
    }

    void existingSimsBodyRangeIsKept()
    {
        const QString url = QStringLiteral("https://example.org/photo.png");
        Message       incoming;
        incoming.setBody(url);
        auto ref = share("https://different.example.org/original.png");
        ref.setRange(0, url.size() - 1);
        incoming.setReferences({ ref });
        incoming.urlAdd(Url(url));

        const auto result = MessageAttachments::collect(incoming);
        QCOMPARE(result.attachments.size(), 2);
        QCOMPARE(result.attachments.at(0).reference.begin(), 0);
        QVERIFY(result.attachments.at(1).reference.begin() < 0); // shown after the body instead
    }

    void urlOnlyDataReferenceCoversItsOob()
    {
        // History saved by Psi builds that rewrote OOB into such references.
        Message incoming;
        incoming.setReferences({ Reference(Reference::Data, "https://example.org/movie") });
        incoming.urlAdd(Url("https://example.org/movie", "movie"));

        const auto result = MessageAttachments::collect(incoming);
        QCOMPARE(result.attachments.size(), 1);
        QCOMPARE(result.attachments.first().description, QString("movie"));
    }

    void nonHttpUrlsAreLinks()
    {
        Message incoming;
        incoming.urlAdd(Url("ftp://example.org/photo.png", "FTP"));
        incoming.urlAdd(Url("javascript:alert(1)", "unsafe"));
        incoming.urlAdd(Url("https://example.org/photo.png", "attachment"));

        const auto result = MessageAttachments::collect(incoming);
        QCOMPARE(result.links.size(), 2);
        QCOMPARE(result.links.first().desc(), QString("FTP"));
        QCOMPARE(result.links.last().url(), QString("javascript:alert(1)"));
        QCOMPARE(result.attachments.size(), 1);
        QCOMPARE(result.attachments.first().description, QString("attachment"));
    }
};

QTEST_GUILESS_MAIN(MessageAttachmentsTest)
#include "messageattachments.moc"
