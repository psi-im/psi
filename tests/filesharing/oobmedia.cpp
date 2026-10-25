/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "oobmediapolicy.h"
#include <QtTest>

class OobMediaPolicyTest final : public QObject {
    Q_OBJECT
private slots:
    void simsSourceSuppressesMatchingOobOnly()
    {
        const QSet<QString> sims {
            QStringLiteral("https://example.org/media/photo.png?token=abc")
        };
        const QStringList oob {
            QStringLiteral("https://EXAMPLE.org/media/photo.png?token=abc#preview"),
            QStringLiteral("https://example.org/media/other.png?token=abc"),
            QStringLiteral("xmpp:alice@example.org"),
            QStringLiteral("https://example.org/media/other.png?token=abc#again")
        };
        const auto entries = OobMediaPolicy::classify(sims, oob);
        QCOMPARE(entries.size(), 4); // Unsupported schemes are retained in their input positions.
        QVERIFY(entries.at(0).redundant);
        QVERIFY(!entries.at(1).redundant);
        QVERIFY(!entries.at(2).source.isValid());
        QVERIFY(entries.at(3).redundant);
        QCOMPARE(entries.at(1).source.toString(QUrl::FullyEncoded),
                 QStringLiteral("https://example.org/media/other.png?token=abc"));
    }

    void queryStringDistinguishesDifferentFiles()
    {
        const auto entries = OobMediaPolicy::classify({},
            { QStringLiteral("https://cdn.example/a.png?file=1"),
              QStringLiteral("https://cdn.example/a.png?file=2") });
        QCOMPARE(entries.size(), 2);
        QVERIFY(!entries.at(0).redundant);
        QVERIFY(!entries.at(1).redundant);
    }

    void onlyFullyQualifiedHttpSourcesAreRenderedAsMedia()
    {
        const auto entries = OobMediaPolicy::classify({},
            { QStringLiteral("file:///home/user/private.png"),
              QStringLiteral("ftp://example.org/a.png"),
              QStringLiteral("javascript:alert(1)"),
              QStringLiteral("https:///invalid.png"),
              QStringLiteral("https://example.org/okay.png") });
        QCOMPARE(entries.size(), 5);
        for (int i = 0; i < 4; ++i)
            QVERIFY(!entries.at(i).source.isValid());
        QCOMPARE(entries.last().original, QStringLiteral("https://example.org/okay.png"));
        QVERIFY(!entries.last().redundant);
    }

    void sameSourceWithFragmentIsOneAttachment()
    {
        const auto entries = OobMediaPolicy::classify({},
            { QStringLiteral("https://example.org/foo%20bar.png#a"),
              QStringLiteral("https://example.org/foo%20bar.png#b") });
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries.at(0).source, entries.at(1).source);
        QVERIFY(!entries.at(0).redundant);
        QVERIFY(entries.at(1).redundant);
    }
};

QTEST_GUILESS_MAIN(OobMediaPolicyTest)
#include "oobmedia.moc"
