/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/EmojiSupport.h"
#include <QFont>
#include <QKeyEvent>
#include <QTextCharFormat>
#include <QTextEdit>
#include <QtTest/QtTest>

// Standalone composer with the same key routing as ChatEdit, so the UI
// behavior is exercised without requiring accounts or Psi application state.
class TestEmojiComposer : public QTextEdit {
public:
    bool sent = false;

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Backspace && event->modifiers() == Qt::NoModifier
            && conversion_.restoreOnBackspace(this)) {
            event->accept();
            return;
        }
        conversion_.clear();
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            sent = true;
            event->accept();
            return;
        }
        const bool typed = event->text().size() == 1 && !textCursor().hasSelection()
            && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
        const int before = typed ? textCursor().position() : -1;
        QTextEdit::keyPressEvent(event);
        if (typed)
            conversion_.replaceTyped(this, before);
    }

private:
    EmojiSupport::ClassicSmileyConversion conversion_;
};

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

    void convertsImmediatelyBeforeEnterOrPunctuation()
    {
        TestEmojiComposer editor;
        editor.show();
        editor.setFocus();
        QTest::keyClicks(&editor, "Hi :-)");
        QCOMPARE(editor.toPlainText(), QString::fromUtf8("Hi 🙂"));
        QTest::keyClick(&editor, Qt::Key_Return);
        QVERIFY(editor.sent);
        QCOMPARE(editor.toPlainText(), QString::fromUtf8("Hi 🙂"));

        editor.clear();
        QTest::keyClicks(&editor, "Sentence ;-).");
        QCOMPARE(editor.toPlainText(), QString::fromUtf8("Sentence 😉."));

        editor.clear();
        QTest::keyClicks(&editor, "<3!");
        QCOMPARE(editor.toPlainText(), QString::fromUtf8("❤️!"));
    }

    void backspaceRestoresTheAliasWithoutReconversion()
    {
        TestEmojiComposer editor;
        editor.show();
        editor.setFocus();
        QTest::keyClicks(&editor, ":-)");
        QCOMPARE(editor.toPlainText(), QString::fromUtf8("🙂"));
        QTest::keyClick(&editor, Qt::Key_Backspace);
        QCOMPARE(editor.toPlainText(), QStringLiteral(":-)"));
        QTest::keyClicks(&editor, ".");
        QCOMPARE(editor.toPlainText(), QStringLiteral(":-)."));
        QTest::keyClick(&editor, Qt::Key_Backspace);
        QCOMPARE(editor.toPlainText(), QStringLiteral(":-)"));
        QTest::keyClick(&editor, Qt::Key_Backspace);
        QCOMPARE(editor.toPlainText(), QStringLiteral(":-"));

        editor.clear();
        QTest::keyClicks(&editor, ":-)");
        QTest::keyClicks(&editor, "!");
        QTest::keyClick(&editor, Qt::Key_Backspace);
        QCOMPARE(editor.toPlainText(), QString::fromUtf8("🙂"));
        QTest::keyClick(&editor, Qt::Key_Backspace);
        QVERIFY(editor.toPlainText().isEmpty());
    }

    void convertsOnlyUserTypedCompleteTokens()
    {
        TestEmojiComposer editor;
        editor.show();
        editor.setFocus();
        QTest::keyClicks(&editor, "prefix:-)");
        QCOMPARE(editor.toPlainText(), QStringLiteral("prefix:-)"));
        editor.clear();
        editor.insertPlainText(QStringLiteral(":-)"));
        QCOMPARE(editor.toPlainText(), QStringLiteral(":-)"));
        QTest::keyClicks(&editor, " rest");
        QCOMPARE(editor.toPlainText(), QStringLiteral(":-) rest"));

        editor.clear();
        QTest::keyClicks(&editor, ":)");
        QCOMPARE(editor.toPlainText(), QString::fromUtf8("🙂"));
#ifdef Q_OS_MACOS
        QTest::keyClick(&editor, Qt::Key_Z, Qt::MetaModifier);
#else
        QTest::keyClick(&editor, Qt::Key_Z, Qt::ControlModifier);
#endif
        QCOMPARE(editor.toPlainText(), QStringLiteral(":)"));
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
