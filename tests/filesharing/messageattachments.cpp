/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "messageattachments.h"

#include "iris/jingle-pub.h"
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

    StatelessFileSharing::FileSharing sharing(const QStringList &urls, bool hashed = true)
    {
        Jingle::FileTransfer::File file;
        file.setName("photo.png");
        file.setMediaType("image/png");
        file.setSize(5);
        file.setDescription("A photo");
        if (hashed)
            file.addHash(Hash::from(Hash::Sha256, QByteArrayLiteral("sharing")));
        StatelessFileSharing::Sources sources;
        for (const auto &url : urls)
            sources.add(StatelessFileSharing::Source::fromUrl(QUrl(url)));
        StatelessFileSharing::FileSharing result;
        result.setFile(file);
        result.setSources(sources);
        return result;
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

    void fileSharingIsAFile()
    {
        Message incoming;
        incoming.addFileSharing(sharing({ "https://example.org/photo.png", "http://mirror.example.org/photo.png" }));
        const auto attachments = MessageAttachments::collect(incoming).attachments;
        QCOMPARE(attachments.size(), 1);
        const auto &attachment = attachments.first();
        QVERIFY(attachment.isFile());
        QVERIFY(!attachment.manualFetch);
        const auto media = attachment.reference.mediaSharing();
        QCOMPARE(media.file.name(), QString("photo.png"));
        QCOMPARE(media.file.size().value(), quint64(5));
        // With hashes any source is as good as another, since the file is verified.
        QCOMPARE(media.sources,
                 QStringList({ "https://example.org/photo.png", "http://mirror.example.org/photo.png" }));
    }

    void fileSharingAfterReferencesAndBeforeOob()
    {
        Message incoming;
        incoming.addFileSharing(sharing({ "https://example.org/photo.png" }));
        incoming.setReferences({ share("https://example.org/other.png") });
        incoming.urlAdd(Url("https://example.org/oob.png"));
        const auto attachments = MessageAttachments::collect(incoming).attachments;
        QCOMPARE(attachments.size(), 3);
        QCOMPARE(attachments.at(0).reference.uri(), QString("https://example.org/other.png"));
        QCOMPARE(attachments.at(1).reference.uri(), QString("https://example.org/photo.png"));
        QCOMPARE(attachments.at(2).reference.uri(), QString("https://example.org/oob.png"));
    }

    void oobCopyOfAFileSharingIsNotAttachedTwice()
    {
        Message incoming;
        incoming.setBody("https://example.org/photo.png");
        incoming.addFileSharing(sharing({ "https://example.org/photo.png" }));
        incoming.urlAdd(Url("https://example.org/photo.png", "Named by OOB"));
        const auto attachments = MessageAttachments::collect(incoming).attachments;
        QCOMPARE(attachments.size(), 1);
        QVERIFY(attachments.first().isFile());
        QCOMPARE(attachments.first().description, QString("Named by OOB"));
    }

    void sameFileByBothProtocolsIsOneAttachment()
    {
        auto reference = share("https://example.org/picture.png");
        auto media     = reference.mediaSharing();
        reference.setMediaSharing(media);
        auto duplicate = sharing({ "https://example.org/photo.png" });
        duplicate.setFile(media.file);
        Message incoming;
        incoming.setReferences({ reference });
        incoming.addFileSharing(duplicate);
        const auto attachments = MessageAttachments::collect(incoming).attachments;
        QCOMPARE(attachments.size(), 1);
        QCOMPARE(attachments.first().reference.uri(), QString("https://example.org/picture.png"));
    }

    void duplicateProtocolsKeepSourcesAndManualDisposition()
    {
        auto reference = share("https://example.org/sims.png");
        auto media     = reference.mediaSharing();
        auto sfs       = sharing({ "https://example.org/sfs.png" });
        sfs.setFile(media.file);
        sfs.setDisposition(StatelessFileSharing::Disposition::Attachment);
        Message incoming;
        incoming.setReferences({ reference });
        incoming.addFileSharing(sfs);
        const auto attachments = MessageAttachments::collect(incoming).attachments;
        QCOMPARE(attachments.size(), 1);
        QVERIFY(attachments.first().manualFetch);
        const auto sources = attachments.first().reference.mediaSharing().sources;
        QVERIFY(sources.contains("https://example.org/sims.png"));
        QVERIFY(sources.contains("https://example.org/sfs.png"));
    }

    void sharedWeakHashDoesNotMergeConflictingFiles()
    {
        auto       reference = share("https://example.org/first.png");
        auto       first     = reference.mediaSharing();
        const auto common    = Hash::from(Hash::Sha1, QByteArrayLiteral("weak alias"));
        first.file.addHash(common);
        reference.setMediaSharing(first);
        auto sfs    = sharing({ "https://example.org/second.png" });
        auto second = sfs.file();
        second.addHash(common);
        second.setSize(*first.file.size());
        sfs.setFile(second);
        Message incoming;
        incoming.setReferences({ reference });
        incoming.addFileSharing(sfs);
        QCOMPARE(MessageAttachments::collect(incoming).attachments.size(), 2);
    }

    void fileSharingWithoutHashesKeepsOnlySecureSources()
    {
        Message both;
        both.addFileSharing(sharing({ "http://example.org/photo.png", "https://example.org/photo.png" }, false));
        const auto attachments = MessageAttachments::collect(both).attachments;
        QCOMPARE(attachments.size(), 1);
        QVERIFY(!attachments.first().isFile());
        QCOMPARE(attachments.first().url(), QUrl("https://example.org/photo.png"));
        QCOMPARE(attachments.first().description, QString("A photo"));

        Message insecure;
        insecure.addFileSharing(sharing({ "http://example.org/photo.png" }, false));
        QVERIFY(MessageAttachments::collect(insecure).attachments.isEmpty());
    }

    void insecureCompatibilityOobDoesNotBypassHashlessSfs()
    {
        Message       incoming;
        const QString url = "http://example.org/photo.png";
        incoming.setBody(url);
        incoming.addFileSharing(sharing({ url }, false));
        incoming.urlAdd(Url(url));
        QVERIFY(MessageAttachments::collect(incoming).attachments.isEmpty());
        QCOMPARE(incoming.body(), url);
    }

    void weakCompatibilityCopyKeepsManualSourcePolicy()
    {
        const QString url       = "https://example.org/photo.png";
        auto          reference = share(url);
        auto          media     = reference.mediaSharing();
        media.file.setHashes({ Hash::from(Hash::Sha1, QByteArrayLiteral("legacy")) });
        reference.setMediaSharing(media);
        auto manual = sharing({ url });
        manual.setDisposition(StatelessFileSharing::Disposition::Attachment);
        Message incoming;
        incoming.setReferences({ reference });
        incoming.addFileSharing(manual);
        const auto attachments = MessageAttachments::collect(incoming).attachments;
        QCOMPARE(attachments.size(), 2); // No proven strong identity edge.
        for (const auto &attachment : attachments)
            QVERIFY(attachment.manualFetch);
    }

    void repeatedSfsAnnouncementsUnionSources()
    {
        auto manual = sharing({ "https://example.org/two.png" });
        manual.setDisposition(StatelessFileSharing::Disposition::Attachment);
        Message incoming;
        incoming.addFileSharing(sharing({ "https://example.org/one.png" }));
        incoming.addFileSharing(manual);
        const auto attachments = MessageAttachments::collect(incoming).attachments;
        QCOMPARE(attachments.size(), 1);
        QCOMPARE(attachments.first().reference.mediaSharing().sources.size(), 2);
        QVERIFY(attachments.first().manualFetch);
    }

    void dispositionAttachmentWaitsForTheUser()
    {
        for (const bool hashed : { true, false }) {
            auto manual = sharing({ "https://example.org/photo.png" }, hashed);
            manual.setDisposition(StatelessFileSharing::Disposition::Attachment);
            Message incoming;
            incoming.addFileSharing(manual);
            const auto attachments = MessageAttachments::collect(incoming).attachments;
            QCOMPARE(attachments.size(), 1);
            QVERIFY(attachments.first().manualFetch);
        }
        auto shown = sharing({ "https://example.org/photo.png" });
        shown.setDisposition(StatelessFileSharing::Disposition::Inline);
        Message incoming;
        incoming.addFileSharing(shown);
        QVERIFY(!MessageAttachments::collect(incoming).attachments.first().manualFetch);
    }

    void fileSharingWithoutSourcesIsPending()
    {
        Message hashed;
        hashed.addFileSharing(sharing({}));
        const auto attachments = MessageAttachments::collect(hashed).attachments;
        QCOMPARE(attachments.size(), 1);
        QVERIFY(attachments.first().isFile());
        QVERIFY(attachments.first().reference.mediaSharing().sources.isEmpty());

        Message unhashed;
        unhashed.addFileSharing(sharing({}, false));
        QVERIFY(MessageAttachments::collect(unhashed).attachments.isEmpty());
    }

    void fileSharingWithOnlyUnknownSourcesLeavesTheBody()
    {
        QDomDocument doc;
        auto         other = sharing({});
        StatelessFileSharing::Sources sources;
        sources.add(StatelessFileSharing::Source::fromElement(doc.createElementNS("urn:example:source", "somewhere")));
        other.setSources(sources);
        Message incoming;
        incoming.setBody("A fallback for those who cannot fetch it");
        incoming.addFileSharing(other);
        QVERIFY(MessageAttachments::hasAttachments(incoming));
        QVERIFY(MessageAttachments::collect(incoming).attachments.isEmpty());
    }

    void jingleSourceIsAskedByHash()
    {
        Jingle::JinglePub publication;
        publication.setFrom(Jid("alice@example.org/desktop"));
        publication.setId("published");
        publication.addDescription(Jingle::FileTransfer::NS);
        auto hashed = sharing({});
        StatelessFileSharing::Sources sources;
        sources.add(StatelessFileSharing::Source::fromJinglePub(publication));
        hashed.setSources(sources);
        Message incoming;
        incoming.addFileSharing(hashed);
        const auto attachments = MessageAttachments::collect(incoming).attachments;
        QCOMPARE(attachments.size(), 1);
        QCOMPARE(attachments.first().reference.mediaSharing().sources,
                 QStringList { "xmpp:alice@example.org/desktop?jingle-ft" });

        auto unhashed = sharing({}, false);
        unhashed.setSources(sources);
        Message nothing;
        nothing.addFileSharing(unhashed);
        QVERIFY2(MessageAttachments::collect(nothing).attachments.isEmpty(), "nothing could verify it");
    }

    void anyAttachmentCounts()
    {
        Message empty;
        empty.setBody("just text");
        QVERIFY(!MessageAttachments::hasAttachments(empty));
        Message reference;
        reference.setReferences({ share("https://example.org/a.png") });
        QVERIFY(MessageAttachments::hasAttachments(reference));
        Message oob;
        oob.urlAdd(Url("https://example.org/a.png"));
        QVERIFY(MessageAttachments::hasAttachments(oob));
        Message fileSharing;
        fileSharing.addFileSharing(sharing({ "https://example.org/a.png" }));
        QVERIFY(MessageAttachments::hasAttachments(fileSharing));
    }

    void attachmentsTellFilesFromUrls()
    {
        const MessageAttachments::Attachment file { share("https://example.org/photo.png"), {} };
        QVERIFY(file.isFile());
        QVERIFY(!file.url().isValid());

        const MessageAttachments::Attachment url { Reference(Reference::Data, "https://example.org/a.png#frag"), {} };
        QVERIFY(!url.isFile());
        QCOMPARE(url.url(), QUrl("https://example.org/a.png"));

        MediaSharing unhashed;
        unhashed.file.setName("photo.png");
        Reference reference(Reference::Data, "https://example.org/b.png");
        reference.setMediaSharing(unhashed);
        const MessageAttachments::Attachment hashless { reference, {} };
        QVERIFY(!hashless.isFile());
        QVERIFY(!hashless.url().isValid());

        const MessageAttachments::Attachment mention { Reference(Reference::Mention, "https://example.org/c"), {} };
        QVERIFY(!mention.isFile());
        QVERIFY(!mention.url().isValid());

        const MessageAttachments::Attachment link { Reference(Reference::Data, "xmpp:room@example.org?join"), {} };
        QVERIFY(!link.isFile());
        QVERIFY(!link.url().isValid());

        const MessageAttachments::Attachment nothing;
        QVERIFY(!nothing.isFile());
        QVERIFY(!nothing.url().isValid());
    }

    void urlIdentityIsStable()
    {
        // Persisted with the URL-to-content mappings and publications.
        const QByteArray key("psi-oob-url-v1:https://example.org/a.png");
        QCOMPARE(MessageAttachments::urlIdentity(QUrl("https://example.org/a.png")).toString(),
                 Hash(Hash::Sha256, QCryptographicHash::hash(key, QCryptographicHash::Sha256)).toString());
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
