/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/EmojiSupport.h"
#include <QtTest/QtTest>

class EmojiSupportTest : public QObject {
    Q_OBJECT

private slots:
    void classicSmileysConvertOnlyCompleteTokens()
    {
        const auto smile = EmojiSupport::completedClassicSmiley(QStringView(QStringLiteral("hello :-)")));
        QCOMPARE(smile.first, 3);
        QCOMPARE(smile.second, QString::fromUtf8("🙂"));

        const auto heart = EmojiSupport::completedClassicSmiley(QStringView(QStringLiteral("<3")));
        QCOMPARE(heart.first, 2);
        QCOMPARE(heart.second, QString::fromUtf8("❤️"));

        const auto wink = EmojiSupport::completedClassicSmiley(QStringView(QStringLiteral("ok\t;-)")));
        QCOMPARE(wink.first, 3);
        QCOMPARE(wink.second, QString::fromUtf8("😉"));

        for (const auto &input : { QStringLiteral("http://example.org/:)"),
                                   QStringLiteral("prefix:-)"), QStringLiteral("a:("),
                                   QStringLiteral("just text"), QStringLiteral(":-"),
                                   QStringLiteral(":-)suffix") }) {
            const auto result = EmojiSupport::completedClassicSmiley(QStringView(input));
            QCOMPARE(result.first, 0);
            QVERIFY(result.second.isEmpty());
        }
    }

    void descriptionsUseUnicodeNames()
    {
        QCOMPARE(EmojiSupport::description(QString::fromUtf8("👍")), QStringLiteral("thumbs up"));
        QCOMPARE(EmojiSupport::description(QString::fromUtf8("🔥")), QStringLiteral("fire"));
        QCOMPARE(EmojiSupport::description(QString::fromUtf8("❤️")), QStringLiteral("red heart"));
        QCOMPARE(EmojiSupport::description(QStringLiteral("¤")), QStringLiteral("U+A4"));
    }

    void composerEmojiSizeIsOnePointThreeTimesBase()
    {
        QFont points(QStringLiteral("Sans Serif"));
        points.setPointSizeF(10.0);
        const auto fmt = EmojiSupport::editorFormat(points);
        QCOMPARE(fmt.fontPointSize(), qreal(13.0));
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        QVERIFY(fmt.fontFamilies().toStringList().isEmpty());
#endif
        QFont pixels(QStringLiteral("Sans Serif"));
        pixels.setPixelSize(20);
        const auto px = EmojiSupport::editorFormat(pixels);
        QCOMPARE(px.intProperty(QTextFormat::FontPixelSize), 26);
    }

    void familyOverridesAreLimitedToOldQt()
    {
        QFont font(QStringLiteral("Sans Serif"), 11);
        EmojiSupport::applyLegacyFamily(font);
        QTextCharFormat format;
        EmojiSupport::applyLegacyFamily(format);
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
        QCOMPARE(font.family(), EmojiSupport::platformFamily());
        QCOMPARE(format.fontFamilies().toStringList(), QStringList { EmojiSupport::platformFamily() });
#else
        QCOMPARE(font.family(), QStringLiteral("Sans Serif"));
        QVERIFY(format.fontFamilies().toStringList().isEmpty());
#endif
    }
};

QTEST_MAIN(EmojiSupportTest)
#include "EmojiSupportTest.moc"
