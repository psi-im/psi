/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "messagestyling.h"
#include <QtTest/QtTest>

class MessageStylingTest : public QObject {
    Q_OBJECT
private slots:
    void spans_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        QTest::newRow("strong") << QString("*strong*") << QString("<b>*strong*</b>");
        QTest::newRow("emphasis") << QString("_emphasis_") << QString("<i>_emphasis_</i>");
        QTest::newRow("strike") << QString("~deleted~") << QString("<s>~deleted~</s>");
        QTest::newRow("code") << QString("a ") + QChar(0x60) + QString("*literal*") + QChar(0x60)
                               << QString("a <code>") + QChar(0x60) + QString("*literal*") + QChar(0x60) + QString("</code>");
        QTest::newRow("nested") << QString("*strong _italic_*")
                                  << QString("<b>*strong <i>_italic_</i>*</b>");
        QTest::newRow("consecutive") << QString("*strong*plain*")
                                       << QString("<b>*strong*</b>plain*");
        QTest::newRow("multiple") << QString("_em_ *bold* ~gone~")
                                    << QString("<i>_em_</i> <b>*bold*</b> <s>~gone~</s>");
        QTest::newRow("html-br") << QString("one<br>_two_") << QString("one<br><i>_two_</i>");
        QTest::newRow("quoted") << QString("&gt; *quoted*<br>reply")
                                  << QString("&gt; <b>*quoted*</b><br>reply");
        QTest::newRow("existing-link") << QString("*<a href=\"https://example.test/foo_bar\">link</a>*")
                                         << QString("<b>*<a href=\"https://example.test/foo_bar\">link</a>*</b>");
        QTest::newRow("unformatted-multiline") << QString("*cannot<br>cross*")
                                                << QString("*cannot<br>cross*");
    }
    void spans()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(MessageStyling::toHtml(input), expected);
    }
    void invalid_data()
    {
        QTest::addColumn<QString>("input");
        QTest::newRow("two") << QString("**");
        QTest::newRow("three") << QString("***");
        QTest::newRow("four") << QString("****");
        QTest::newRow("markdown") << QString("**bold**");
        QTest::newRow("whitespace-open") << QString("* not strong*");
        QTest::newRow("whitespace-close") << QString("*not strong *");
        QTest::newRow("no-close") << QString("*not strong");
        QTest::newRow("embedded") << QString("word*strong*word");
        QTest::newRow("url") << QString("https://example.test/path/to/x_y");
        QTest::newRow("attributes") << QString("<a href=\"https://example.test/a_b*c~d\">link</a>");
        QTest::newRow("html-code") << QString("<code>*literal*</code>");
        QTest::newRow("img") << QString("<img src=\"/data/my_image*tag*.png\">");
    }
    void invalid()
    {
        QFETCH(QString, input);
        QCOMPARE(MessageStyling::toHtml(input), input);
    }
    void fences()
    {
        const QChar tick(0x60);
        const QString fence(3, tick);
        const QString input = fence + "cpp<br>*not bold* _not italic_<br>" + fence + "<br>ok *bold*";
        const QString out = MessageStyling::toHtml(input);
        QVERIFY(out.startsWith(QStringLiteral("<span style=\"font-family:monospace; white-space:pre-wrap\">")));
        QVERIFY(out.contains(QStringLiteral("*not bold* _not italic_")));
        QVERIFY(!out.contains(QStringLiteral("<b>*not bold*</b>")));
        QVERIFY(out.contains(QStringLiteral("</span><br>ok <b>*bold*</b>")));
    }
    void quotedUnclosedFence()
    {
        const QString fence(3, QChar(0x60));
        const QString html = "&gt; " + fence + "<br>&gt; *literal*<br>outside _emphasis_";
        const QString out = MessageStyling::toHtml(html);
        QVERIFY(out.contains(QStringLiteral("&gt; *literal*</span><br>outside <i>_emphasis_</i>")));
    }
};

QTEST_MAIN(MessageStylingTest)
#include "MessageStylingTest.moc"
