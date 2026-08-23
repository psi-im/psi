#include "psirichtext.h"

#include <QTextCursor>
#include <QTextDocument>
#include <QtTest/QtTest>

class TestRichText : public QObject {
    Q_OBJECT
private:
    int countText(QTextDocument *doc, QString text)
    {
        int         result = 0;
        QTextCursor cursor(doc);
        cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);

        forever
        {
            cursor = doc->find(text, cursor);
            if (cursor.isNull())
                break;

            result++;
        }

        return result;
    }

private slots:
    void testSetText()
    {
        QTextDocument doc;
        PsiRichText::setText(&doc, "Test <icon name=\"foo\" text=\"bar\">");
        QCOMPARE(countText(&doc, QString(QChar::ObjectReplacementCharacter)), 1);
    }

    void testInsertedRange()
    {
        QTextDocument doc;
        QTextCursor   cursor(&doc);
        QTextCursor   inserted;

        PsiRichText::appendText(&doc, cursor, "<b>first</b>", true, {}, &inserted);
        QCOMPARE(inserted.selectedText(), QString("first"));

        QTextCursor continuation(&doc);
        continuation.setPosition(inserted.selectionEnd());
        PsiRichText::insertTextFragment(&doc, continuation, " second", {}, &inserted);
        QCOMPARE(inserted.selectedText(), QString("second"));
        QCOMPARE(doc.toPlainText(), QString("firstsecond"));
    }

    void testAutoForeground()
    {
        QTextDocument doc;
        QTextCursor   cursor(&doc);
        QTextCursor   inserted;
        const QColor  initial(Qt::red);
        const QColor  updated(Qt::blue);

        PsiRichText::appendText(&doc, cursor, "<span style=\"color:#ff0000\">colored</span> plain", true, {},
                                &inserted);
        PsiRichText::markAutoForeground(inserted, initial, 42, QString("data"));
        PsiRichText::recolorAutoForegrounds(&doc, [updated](int kind, const QVariant &data) {
            return kind == 42 && data.toString() == "data" ? updated : QColor();
        });

        QTextCursor colored = doc.find("colored");
        QTextCursor plain   = doc.find("plain");
        QCOMPARE(colored.charFormat().foreground().color(), updated);
        QVERIFY(plain.charFormat().foreground().style() == Qt::NoBrush);
    }

    void testAutoForegroundDoesNotLeakIntoNextFragment()
    {
        QTextDocument doc;
        QTextCursor   cursor(&doc);
        QTextCursor   prefix;
        QTextCursor   body;

        PsiRichText::appendText(&doc, cursor, "<span style=\"color:#ff0000\">prefix</span>", true, {}, &prefix);
        PsiRichText::markAutoForeground(prefix, QColor(Qt::red), 42);

        cursor.setPosition(prefix.selectionEnd());
        cursor.insertText(" ");
        PsiRichText::insertTextFragment(&doc, cursor, "<span style=\"color:#0000ff\">body</span>", {}, &body);

        QTextCursor prefixText = doc.find("prefix");
        QTextCursor bodyText   = doc.find("body");
        QVERIFY(prefixText.charFormat().hasProperty(PsiRichText::AutoColorKind));
        QVERIFY(!bodyText.charFormat().hasProperty(PsiRichText::AutoColorKind));
    }

    void testParserFormatsAfterImage()
    {
        QTextDocument doc;
        QTextCursor   cursor(&doc);
        PsiRichText::ParsersMap parsers {
            { "marker",
              [](const QStringView &, int) {
                  return PsiRichText::ParserRet { PsiRichText::markerFormat("test"), QString() };
              } }
        };

        PsiRichText::appendText(&doc, cursor, "<img src=\"icon:test\" /><marker>", true, parsers);
        QTextCursor marker = PsiRichText::findMarker(QTextCursor(&doc), "test");
        QVERIFY(!marker.isNull());
    }
};

QTEST_MAIN(TestRichText)
#include "main.moc"
