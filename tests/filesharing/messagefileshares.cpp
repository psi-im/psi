/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "messagefileshares.h"

#include "iris/jingle-pub.h"
#include "iris/xmpp_hash.h"
#include "iris/xmpp_message.h"
#include "iris/xmpp_reference.h"
#include "iris/xmpp_url.h"

#include <QCryptographicHash>
#include <QtTest>

using namespace XMPP;

class MessageFileSharesTest : public QObject {
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

        const auto result = MessageFileShares::collect(incoming);
        QCOMPARE(result.files.size(), 1);
        QVERIFY(result.files.first().reference.mediaSharing().isValid());
        QCOMPARE(result.files.first().description, QString("duplicate"));
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

        const auto result = MessageFileShares::forDisplay(incoming);
        QCOMPARE(result.files.size(), 1);
        const auto &attachment = result.files.first();
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

        const auto result = MessageFileShares::forDisplay(incoming);
        QVERIFY(result.files.isEmpty());
        QVERIFY(result.links.isEmpty());
    }

    void bodyMaySpellTheUrlWithoutItsFragment()
    {
        const QString url = "https://example.org/photo.png";
        Message       incoming;
        incoming.setBody("View: " + url);
        incoming.urlAdd(Url(url + "#frag", "photo"));
        QVERIFY(MessageFileShares::forDisplay(incoming).files.isEmpty()); // still the link in the text

        incoming.setBody(url);
        const auto result = MessageFileShares::forDisplay(incoming);
        QCOMPARE(result.files.size(), 1);
        QCOMPARE(result.files.first().reference.uri(), url + "#frag");
        QCOMPARE(result.files.first().reference.begin(), 0);
        QCOMPARE(result.files.first().reference.end(), url.size() - 1);
    }

    void attachmentOnlyAndRepeatedOobGiveOneAttachment()
    {
        Message incoming;
        incoming.urlAdd(Url("https://example.org/photo.png#one"));
        incoming.urlAdd(Url("https://example.org/photo.png#two"));

        const auto result = MessageFileShares::collect(incoming);
        QCOMPARE(result.files.size(), 1);
        QCOMPARE(result.files.first().reference.uri(), QString("https://example.org/photo.png#one"));
        QVERIFY(result.files.first().reference.begin() < 0);
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

        const auto result = MessageFileShares::forDisplay(incoming);
        QCOMPARE(result.files.size(), 2);
        QCOMPARE(result.files.at(0).reference.begin(), 0);
        QVERIFY(result.files.at(1).reference.begin() < 0); // shown after the body instead
    }

    void fileAnnouncementsDoNotDependOnBodyPlacement()
    {
        const QString url = "https://example.org/photo.png#fragment";
        Message       incoming;
        incoming.urlAdd(Url(url, "photo"));
        for (const auto &body : { QString(), url, QString("Caption: ") + url }) {
            incoming.setBody(body);
            const auto files = MessageFileShares::collect(incoming).files;
            QCOMPARE(files.size(), 1);
            QCOMPARE(files.first().reference.uri(), url);
            QCOMPARE(files.first().description, QString("photo"));
            QVERIFY(files.first().reference.begin() < 0);
            QVERIFY(files.first().fromOob);
        }
    }

    void mentionsAreNotFileSharesAndKeepTheirBodyRange()
    {
        const QString url = "https://example.org/photo.png";
        Reference     mention(Reference::Mention, "xmpp:alice@example.org");
        mention.setRange(0, url.size() - 1);
        Message incoming;
        incoming.setBody(url);
        incoming.setReferences({ mention });
        incoming.urlAdd(Url(url));
        const auto files = MessageFileShares::collect(incoming).files;
        QCOMPARE(files.size(), 1);
        QVERIFY(files.first().fromOob);
        QVERIFY(MessageFileShares::forDisplay(incoming).files.first().reference.begin() < 0);
        QCOMPARE(incoming.references().first().end(), url.size() - 1);
    }

    void fallbackRangesBelongOnlyToPresentation()
    {
        Message       incoming;
        const QString body = QString::fromUtf8("😀 https://example.org/photo.png");
        incoming.setBody(body);
        auto file = share("https://example.org/photo.png");
        incoming.setReferences({ file });
        // XEP-0428 positions count Unicode code points, not UTF-16 units.
        incoming.setFallbacks({ { "urn:xmpp:sims:1", { { 2, body.size() - 1 } }, {} } });
        const auto normalized = MessageFileShares::collect(incoming).files;
        QCOMPARE(normalized.size(), 1);
        QVERIFY(normalized.first().reference.begin() < 0);
        const auto displayed = MessageFileShares::forDisplay(incoming).files;
        QCOMPARE(displayed.size(), 1);
        QCOMPARE(displayed.first().reference.begin(), 3);
        QCOMPARE(displayed.first().reference.end(), body.size() - 1);
        QVERIFY(incoming.references().first().begin() < 0);
    }

    void urlOnlyDataReferenceCoversItsOob()
    {
        // History saved by Psi builds that rewrote OOB into such references.
        Message incoming;
        incoming.setReferences({ Reference(Reference::Data, "https://example.org/movie") });
        incoming.urlAdd(Url("https://example.org/movie", "movie"));

        const auto result = MessageFileShares::collect(incoming);
        QCOMPARE(result.files.size(), 1);
        QCOMPARE(result.files.first().description, QString("movie"));
    }

    void fileSharingIsAFile()
    {
        Message incoming;
        incoming.addFileSharing(sharing({ "https://example.org/photo.png", "http://mirror.example.org/photo.png" }));
        const auto attachments = MessageFileShares::collect(incoming).files;
        QCOMPARE(attachments.size(), 1);
        const auto &attachment = attachments.first();
        QVERIFY(attachment.hasContentHashes());
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
        const auto attachments = MessageFileShares::collect(incoming).files;
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
        const auto attachments = MessageFileShares::collect(incoming).files;
        QCOMPARE(attachments.size(), 1);
        QVERIFY(attachments.first().hasContentHashes());
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
        const auto attachments = MessageFileShares::collect(incoming).files;
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
        const auto attachments = MessageFileShares::collect(incoming).files;
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
        QCOMPARE(MessageFileShares::collect(incoming).files.size(), 2);
    }

    void fileSharingWithoutHashesKeepsOnlySecureSources()
    {
        Message both;
        both.addFileSharing(sharing({ "http://example.org/photo.png", "https://example.org/photo.png" }, false));
        const auto attachments = MessageFileShares::collect(both).files;
        QCOMPARE(attachments.size(), 1);
        QVERIFY(!attachments.first().hasContentHashes());
        QCOMPARE(attachments.first().url(), QUrl("https://example.org/photo.png"));
        QCOMPARE(attachments.first().description, QString("A photo"));

        Message insecure;
        insecure.addFileSharing(sharing({ "http://example.org/photo.png" }, false));
        QVERIFY(MessageFileShares::collect(insecure).files.isEmpty());
    }

    void insecureCompatibilityOobDoesNotBypassHashlessSfs()
    {
        Message       incoming;
        const QString url = "http://example.org/photo.png";
        incoming.setBody(url);
        incoming.addFileSharing(sharing({ url }, false));
        incoming.urlAdd(Url(url));
        QVERIFY(MessageFileShares::collect(incoming).files.isEmpty());
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
        const auto attachments = MessageFileShares::collect(incoming).files;
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
        const auto attachments = MessageFileShares::collect(incoming).files;
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
            const auto attachments = MessageFileShares::collect(incoming).files;
            QCOMPARE(attachments.size(), 1);
            QVERIFY(attachments.first().manualFetch);
        }
        auto shown = sharing({ "https://example.org/photo.png" });
        shown.setDisposition(StatelessFileSharing::Disposition::Inline);
        Message incoming;
        incoming.addFileSharing(shown);
        QVERIFY(!MessageFileShares::collect(incoming).files.first().manualFetch);
    }

    void fileSharingWithoutSourcesIsPending()
    {
        Message hashed;
        hashed.addFileSharing(sharing({}));
        const auto attachments = MessageFileShares::collect(hashed).files;
        QCOMPARE(attachments.size(), 1);
        QVERIFY(attachments.first().hasContentHashes());
        QVERIFY(attachments.first().reference.mediaSharing().sources.isEmpty());

        Message unhashed;
        unhashed.addFileSharing(sharing({}, false));
        QVERIFY(MessageFileShares::collect(unhashed).files.isEmpty());
    }

    void fileSharingWithOnlyUnknownSourcesLeavesTheBody()
    {
        QDomDocument                  doc;
        auto                          other = sharing({});
        StatelessFileSharing::Sources sources;
        sources.add(StatelessFileSharing::Source::fromElement(doc.createElementNS("urn:example:source", "somewhere")));
        other.setSources(sources);
        Message incoming;
        incoming.setBody("A fallback for those who cannot fetch it");
        incoming.addFileSharing(other);
        QVERIFY(MessageFileShares::hasFileShares(incoming));
        QVERIFY(MessageFileShares::collect(incoming).files.isEmpty());
    }

    void jingleSourceIsAskedByHash()
    {
        Jingle::JinglePub publication;
        publication.setFrom(Jid("alice@example.org/desktop"));
        publication.setId("published");
        publication.addDescription(Jingle::FileTransfer::NS);
        auto                          hashed = sharing({});
        StatelessFileSharing::Sources sources;
        sources.add(StatelessFileSharing::Source::fromJinglePub(publication));
        hashed.setSources(sources);
        Message incoming;
        incoming.addFileSharing(hashed);
        const auto attachments = MessageFileShares::collect(incoming).files;
        QCOMPARE(attachments.size(), 1);
        QCOMPARE(attachments.first().reference.mediaSharing().sources,
                 QStringList { "xmpp:alice@example.org/desktop?jingle-ft" });

        auto unhashed = sharing({}, false);
        unhashed.setSources(sources);
        Message nothing;
        nothing.addFileSharing(unhashed);
        QVERIFY2(MessageFileShares::collect(nothing).files.isEmpty(), "nothing could verify it");
    }

    void anyAttachmentCounts()
    {
        Message empty;
        empty.setBody("just text");
        QVERIFY(!MessageFileShares::hasFileShares(empty));
        Message reference;
        reference.setReferences({ share("https://example.org/a.png") });
        QVERIFY(MessageFileShares::hasFileShares(reference));
        Message oob;
        oob.urlAdd(Url("https://example.org/a.png"));
        QVERIFY(MessageFileShares::hasFileShares(oob));
        Message fileSharing;
        fileSharing.addFileSharing(sharing({ "https://example.org/a.png" }));
        QVERIFY(MessageFileShares::hasFileShares(fileSharing));
    }

    void attachmentsTellFilesFromUrls()
    {
        const MessageFileShares::FileShare file { share("https://example.org/photo.png"), {} };
        QVERIFY(file.hasContentHashes());
        QVERIFY(!file.url().isValid());

        const MessageFileShares::FileShare url { Reference(Reference::Data, "https://example.org/a.png#frag"), {} };
        QVERIFY(!url.hasContentHashes());
        QCOMPARE(url.url(), QUrl("https://example.org/a.png"));

        MediaSharing unhashed;
        unhashed.file.setName("photo.png");
        Reference reference(Reference::Data, "https://example.org/b.png");
        reference.setMediaSharing(unhashed);
        const MessageFileShares::FileShare hashless { reference, {} };
        QVERIFY(!hashless.hasContentHashes());
        QVERIFY(!hashless.url().isValid());

        const MessageFileShares::FileShare mention { Reference(Reference::Mention, "https://example.org/c"), {} };
        QVERIFY(!mention.hasContentHashes());
        QVERIFY(!mention.url().isValid());

        const MessageFileShares::FileShare link { Reference(Reference::Data, "xmpp:room@example.org?join"), {} };
        QVERIFY(!link.hasContentHashes());
        QVERIFY(!link.url().isValid());

        const MessageFileShares::FileShare nothing;
        QVERIFY(!nothing.hasContentHashes());
        QVERIFY(!nothing.url().isValid());
    }

    void urlIdentityIsStable()
    {
        // Persisted with the URL-to-content mappings and publications.
        const QByteArray key("psi-oob-url-v1:https://example.org/a.png");
        QCOMPARE(MessageFileShares::urlIdentity(QUrl("https://example.org/a.png")).toString(),
                 Hash(Hash::Sha256, QCryptographicHash::hash(key, QCryptographicHash::Sha256)).toString());
    }

    void nonHttpUrlsAreLinks()
    {
        Message incoming;
        incoming.urlAdd(Url("ftp://example.org/photo.png", "FTP"));
        incoming.urlAdd(Url("javascript:alert(1)", "unsafe"));
        incoming.urlAdd(Url("https://example.org/photo.png", "attachment"));

        const auto result = MessageFileShares::collect(incoming);
        QCOMPARE(result.links.size(), 2);
        QCOMPARE(result.links.first().desc(), QString("FTP"));
        QCOMPARE(result.links.last().url(), QString("javascript:alert(1)"));
        QCOMPARE(result.files.size(), 1);
        QCOMPARE(result.files.first().description, QString("attachment"));
    }
};

QTEST_GUILESS_MAIN(MessageFileSharesTest)
#include "messagefileshares.moc"
