/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingnormalizer.h"

#include "iris/xmpp_hash.h"
#include "iris/xmpp_message.h"
#include "iris/xmpp_reference.h"
#include "iris/xmpp_url.h"

#include <QCryptographicHash>
#include <QtTest>

using namespace XMPP;

class FileSharingNormalizerTest : public QObject {
    Q_OBJECT

    Reference share(const QString &source)
    {
        MediaSharing media;
        media.file.setName("photo.png");
        media.file.setMediaType("image/png");
        media.file.setSize(7);
        media.file.addHash(Hash(Hash::Sha256,
                                QCryptographicHash::hash("payload", QCryptographicHash::Sha256)));
        media.sources.append(source);
        Reference reference(Reference::Data, source);
        reference.setMediaSharing(media);
        return reference;
    }

private slots:
    void sameSimsSourceIsRemovedFromOobBeforeRendering()
    {
        Message incoming;
        incoming.setBody("Caption");
        incoming.setReferences({ share("https://example.org/picture.png?a=1") });
        incoming.urlAdd(Url("https://EXAMPLE.org/picture.png?a=1#ignored", "duplicate"));
        incoming.urlAdd(Url("xmpp:bob@example.org", "unrelated"));

        const auto display = FileSharingNormalizer::prepareForDisplay(incoming);
        QCOMPARE(incoming.urlList().size(), 2); // Never change the wire/history data.
        QCOMPARE(incoming.references().size(), 1);
        QCOMPARE(display.references.size(), 1);
        QCOMPARE(display.remainingOob.size(), 1);
        QCOMPARE(display.remainingOob.first().url(), QString("xmpp:bob@example.org"));
        QCOMPARE(display.references.first().mediaSharing().sources.first(),
                 QString("https://example.org/picture.png?a=1"));
    }

    void oobWithoutSimsBecomesReferenceAtBodyUrl()
    {
        Message incoming;
        const QString url = "https://example.org/photo.png?version=2";
        incoming.setBody(QString("Look: %1 !").arg(url));
        incoming.urlAdd(Url(url, "photo"));
        const auto display = FileSharingNormalizer::prepareForDisplay(incoming);
        QCOMPARE(incoming.urlList().size(), 1);
        QVERIFY(incoming.references().isEmpty());
        QVERIFY(display.remainingOob.isEmpty());
        QCOMPARE(display.references.size(), 1);
        const auto ref = display.references.first();
        QCOMPARE(ref.uri(), url);
        QCOMPARE(ref.begin(), incoming.body().indexOf(url));
        QCOMPARE(ref.end(), ref.begin() + url.size() - 1);
        QVERIFY(!ref.mediaSharing().isValid());
    }

    void attachmentOnlyAndRepeatedOobAppendJustOneReference()
    {
        Message incoming;
        incoming.urlAdd(Url("https://example.org/photo.png#one"));
        incoming.urlAdd(Url("https://example.org/photo.png#two"));
        const auto display = FileSharingNormalizer::prepareForDisplay(incoming);
        QVERIFY(display.remainingOob.isEmpty());
        QCOMPARE(display.references.size(), 1);
        QCOMPARE(display.references.first().uri(), QString("https://example.org/photo.png"));
        QVERIFY(display.references.first().begin() < 0);
        QCOMPARE(incoming.urlList().size(), 2);
        QCOMPARE(incoming.references().size(), 0);
        const auto second = FileSharingNormalizer::prepareForDisplay(incoming);
        QCOMPARE(second.references.size(), 1);
    }

    void avoidOverwritingExistingSimsBodyRanges()
    {
        const QString url = QStringLiteral("https://example.org/photo.png");
        Message incoming;
        incoming.setBody(url + " " + url);
        auto ref = share("https://different.example.org/original.png");
        ref.setRange(0, url.size() - 1);
        incoming.setReferences({ ref });
        incoming.urlAdd(Url(url));
        const auto display = FileSharingNormalizer::prepareForDisplay(incoming);
        QCOMPARE(incoming.references().size(), 1);
        QCOMPARE(display.references.size(), 2);
        QCOMPARE(display.references.at(1).begin(), url.size() + 1);
        QCOMPARE(display.references.at(1).end(), 2 * url.size());
    }

    void nonHttpEntriesRemainOriginalLinks()
    {
        Message incoming;
        incoming.urlAdd(Url("ftp://example.org/photo.png", "FTP"));
        incoming.urlAdd(Url("javascript:alert(1)", "unsafe"));
        incoming.urlAdd(Url("https://example.org/photo.png", "attachment"));
        const auto display = FileSharingNormalizer::prepareForDisplay(incoming);
        QCOMPARE(incoming.urlList().size(), 3);
        QCOMPARE(display.remainingOob.size(), 2);
        QCOMPARE(display.remainingOob.first().desc(), QString("FTP"));
        QCOMPARE(display.remainingOob.last().url(), QString("javascript:alert(1)"));
        QCOMPARE(display.references.size(), 1);
    }
};

QTEST_GUILESS_MAIN(FileSharingNormalizerTest)
#include "medianormalizer.moc"
