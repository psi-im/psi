/*
 * textutiltest.cpp - Unit tests for TextUtil
 * Copyright (C) 2026  Psi IM
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "textutil.h"
#include "coloropt.h"
#include "psiiconset.h"
#include "iconset.h"

#include <QObject>
#include <QtTest/QtTest>

std::unique_ptr<ColorOpt> ColorOpt::instance_;
ColorOpt::ColorOpt() : QObject(nullptr) { }
ColorOpt *ColorOpt::instance()
{
    if (!instance_)
        instance_.reset(new ColorOpt());
    return instance_.get();
}
void ColorOpt::reset() { instance_.reset(nullptr); }
void ColorOpt::optionChanged(const QString &) { }
QColor ColorOpt::color(const QString &, const QColor &defaultColor) const
{
    return defaultColor.isValid() ? defaultColor : QColor(Qt::blue);
}
QPalette::ColorRole ColorOpt::colorRole(const QString &) const { return QPalette::Link; }
bool ColorOpt::compatibleColors(const QColor &, const QColor &) { return true; }
QColor ColorOpt::ensureContrast(const QColor &foreground, const QColor &, const QColor &)
{
    return foreground.isValid() ? foreground : QColor(Qt::blue);
}
QColor ColorOpt::adaptBackground(const QColor &background, const QPalette &) { return background; }

class Iconset::Private : public QSharedData {
public:
    Private() { }
};

class PsiIcon::Private : public QSharedData {
public:
    Private() { }
    QRegularExpression rx;
    QString name;
};

Iconset::Iconset() : d(new Private) { }
Iconset::Iconset(const Iconset &other) : d(other.d) { }
Iconset::~Iconset() { }
Iconset &Iconset::operator=(const Iconset &other) { d = other.d; return *this; }
QListIterator<PsiIcon *> Iconset::iterator() const
{
    static const QList<PsiIcon *> emptyList;
    return QListIterator<PsiIcon *>(emptyList);
}

PsiIcon::PsiIcon() : QObject(nullptr), d(new Private) { }
PsiIcon::PsiIcon(const PsiIcon &other) : QObject(nullptr), d(other.d) { }
PsiIcon::~PsiIcon() { }
PsiIcon &PsiIcon::operator=(const PsiIcon &other) { d = other.d; return *this; }
const QRegularExpression &PsiIcon::regExp() const { return d->rx; }
const QString &PsiIcon::name() const { return d->name; }
bool PsiIcon::isAnimated() const { return false; }
QPixmap PsiIcon::pixmap(const QSize &) const { return QPixmap(); }
QImage PsiIcon::image(const QSize &) const { return QImage(); }
QIcon PsiIcon::icon() const { return QIcon(); }
const Impix &PsiIcon::impix() const { static Impix imp; return imp; }
const Impix &PsiIcon::frameImpix() const { static Impix imp; return imp; }
int PsiIcon::frameNumber() const { return 0; }
PsiIcon *PsiIcon::copy() const { return new PsiIcon(); }
void PsiIcon::activated(bool) { }
void PsiIcon::stop() { }

PsiIconset *PsiIconset::instance_ = nullptr;
PsiIconset::PsiIconset() : QObject(nullptr), d(nullptr) { }
PsiIconset::~PsiIconset() { }
PsiIconset *PsiIconset::instance()
{
    if (!instance_)
        instance_ = new PsiIconset();
    return instance_;
}
void PsiIconset::reset() { }
void PsiIconset::optionChanged(const QString &) { }

class TextUtilTest : public QObject {
    Q_OBJECT

private slots:
    // legacyFormat tests (XEP-0393 & Markdown formatting)
    void testLegacyFormat_data();
    void testLegacyFormat();
    void testLegacyFormat_underlineXep();
    void testLegacyFormat_italicXep();
    void testLegacyFormat_strikethroughXep();
    void testLegacyFormat_multipleStyles();
    void testLegacyFormat_noMatchNonBoundary();
    void testLegacyFormat_AdditionalMD();

    // escape and unescape tests
    void testEscape();
    void testUnescape();

    // quote tests
    void testQuote();

    // plain2rich and rich2plain tests
    void testPlain2Rich();
    void testRich2Plain();

    // resolveEntities tests
    void testResolveEntities();

    // linkify tests
    void testLinkify();

    // img2title tests
    void testImg2Title();

    // sizeUnit and roundedNumber tests
    void testSizeUnit();
    void testRoundedNumber();
};

void TextUtilTest::testLegacyFormat_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    // Empty and plain text
    QTest::newRow("empty") << QString("") << QString("");
    QTest::newRow("plain") << QString("Hello World") << QString("Hello World");
    QTest::newRow("url") << QString("https://example.com/path/to/resource/")
                         << QString("https://example.com/path/to/resource/");

    // Markdown bold italic (***text***)
    QTest::newRow("md_bold_italic_start") << QString("***text***") << QString("<b>text</b>");
    QTest::newRow("md_bold_italic_space") << QString("word ***text*** word") << QString("word <b>text</b> word");
    QTest::newRow("md_bold_italic_quote") << QString(">***text***") << QString("><b>text</b>");
    QTest::newRow("md_bold_italic_tag") << QString("***text***<br>") << QString("<b>text</b><br>");

    // Markdown bold (**text**)
    QTest::newRow("md_bold_start") << QString("**text**") << QString("<b>text</b>");
    QTest::newRow("md_bold_space") << QString("word **text** word") << QString("word <b>text</b> word");
    QTest::newRow("md_bold_quote") << QString(">**text**") << QString("><b>text</b>");
    QTest::newRow("md_bold_tag") << QString("**text**<br>") << QString("<b>text</b><br>");

    // XEP-0393 bold (*text*)
    QTest::newRow("xep_bold_start") << QString("*text*") << QString("<b>*text*</b>");
    QTest::newRow("xep_bold_space") << QString("word *text* word") << QString("word <b>*text*</b> word");
    QTest::newRow("xep_bold_quote") << QString(">*text*") << QString("><b>*text*</b>");
    QTest::newRow("xep_bold_tag") << QString("*text*<br>") << QString("<b>*text*</b><br>");

    // XEP-0393 underline (_text_)
    QTest::newRow("xep_underline_start") << QString("_text_") << QString("<u>_text_</u>");
    QTest::newRow("xep_underline_space") << QString("word _text_ word") << QString("word <u>_text_</u> word");
    QTest::newRow("xep_underline_quote") << QString(">_text_") << QString("><u>_text_</u>");
    QTest::newRow("xep_underline_tag") << QString("_text_<br>") << QString("<u>_text_</u><br>");

    // XEP-0393 italic (/text/)
    QTest::newRow("xep_italic_start") << QString("/text/") << QString("<i>/text/</i>");
    QTest::newRow("xep_italic_space") << QString("word /text/ word") << QString("word <i>/text/</i> word");
    QTest::newRow("xep_italic_quote") << QString(">/text/") << QString("><i>/text/</i>");
    QTest::newRow("xep_italic_tag") << QString("/text/<br>") << QString("<i>/text/</i><br>");

    // XEP-0393 strikethrough (~text~)
    QTest::newRow("xep_strike_start") << QString("~text~") << QString("<s>~text~</s>");
    QTest::newRow("xep_strike_space") << QString("word ~text~ word") << QString("word <s>~text~</s> word");
    QTest::newRow("xep_strike_quote") << QString(">~text~") << QString("><s>~text~</s>");
    QTest::newRow("xep_strike_tag") << QString("~text~<br>") << QString("<s>~text~</s><br>");

    // Combined formatting in one string
    QTest::newRow("combined_styles")
        << QString("*bold* and _under_ and /italic/ and ~strike~ and **mdbold** and ***mditalic***")
        << QString("<b>*bold*</b> and <u>_under_</u> and <i>/italic/</i> and <s>~strike~</s> and <b>mdbold</b> and "
                   "<b>mditalic</b>");

    // Non-matching embedded punctuation
    QTest::newRow("embedded_asterisk") << QString("word*text*word") << QString("word*text*word");
    QTest::newRow("embedded_underscore") << QString("word_text_word") << QString("word_text_word");
    QTest::newRow("embedded_slash") << QString("word/text/word") << QString("word/text/word");
    QTest::newRow("embedded_tilde") << QString("word~text~word") << QString("word~text~word");
    QTest::newRow("embedded_double_asterisk") << QString("word**text**word") << QString("word**text**word");
}

void TextUtilTest::testLegacyFormat()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);

    QCOMPARE(TextUtil::legacyFormat(input), expected);
}

void TextUtilTest::testLegacyFormat_boldXep()
{
    QCOMPARE(TextUtil::legacyFormat("*bold*"), QString("<b>*bold*</b>"));
    QCOMPARE(TextUtil::legacyFormat("before *bold* after"), QString("before <b>*bold*</b> after"));
    QCOMPARE(TextUtil::legacyFormat(">*bold*"), QString("><b>*bold*</b>"));
}

void TextUtilTest::testLegacyFormat_underlineXep()
{
    QCOMPARE(TextUtil::legacyFormat("_underline_"), QString("<u>_underline_</u>"));
    QCOMPARE(TextUtil::legacyFormat("before _underline_ after"), QString("before <u>_underline_</u> after"));
    QCOMPARE(TextUtil::legacyFormat(">_underline_"), QString("><u>_underline_</u>"));
}

void TextUtilTest::testLegacyFormat_italicXep()
{
    QCOMPARE(TextUtil::legacyFormat("/italic/"), QString("<i>/italic/</i>"));
    QCOMPARE(TextUtil::legacyFormat("before /italic/ after"), QString("before <i>/italic/</i> after"));
    QCOMPARE(TextUtil::legacyFormat(">/italic/"), QString("><i>/italic/</i>"));
}

void TextUtilTest::testLegacyFormat_strikethroughXep()
{
    QCOMPARE(TextUtil::legacyFormat("~strike~"), QString("<s>~strike~</s>"));
    QCOMPARE(TextUtil::legacyFormat("before ~strike~ after"), QString("before <s>~strike~</s> after"));
    QCOMPARE(TextUtil::legacyFormat(">~strike~"), QString("><s>~strike~</s>"));
}

void TextUtilTest::testLegacyFormat_multipleStyles()
{
    const QString input = "*bold* _underline_ /italic/ ~strike~";
    const QString expected = "<b>*bold*</b> <u>_underline_</u> <i>/italic/</i> <s>~strike~</s>";
    QCOMPARE(TextUtil::legacyFormat(input), expected);
}

void TextUtilTest::testLegacyFormat_noMatchNonBoundary()
{
    QCOMPARE(TextUtil::legacyFormat("prefix*bold*suffix"), QString("prefix*bold*suffix"));
    QCOMPARE(TextUtil::legacyFormat("prefix_underline_suffix"), QString("prefix_underline_suffix"));
    QCOMPARE(TextUtil::legacyFormat("prefix/italic/suffix"), QString("prefix/italic/suffix"));
    QCOMPARE(TextUtil::legacyFormat("prefix~strike~suffix"), QString("prefix~strike~suffix"));
}

void TextUtilTest::testLegacyFormat_AdditionalMD()
{
    QCOMPARE(TextUtil::legacyFormat("***bold italic***"), QString("<b><i>bold italic</i></b>"));
    QCOMPARE(TextUtil::legacyFormat("before ***bold italic*** after"), QString("before <b><i>bold italic</i></b> after"));
    QCOMPARE(TextUtil::legacyFormat(">***bold italic***"), QString("><b><i>bold italic</i></b>"));
    QCOMPARE(TextUtil::legacyFormat("**bold**"), QString("<b>bold</b>"));

    QCOMPARE(TextUtil::legacyFormat("before **bold** after"), QString("before <b>bold</b> after"));
    QCOMPARE(TextUtil::legacyFormat(">**bold**"), QString("><b>bold</b>"));
}

void TextUtilTest::testEscape()
{
    QCOMPARE(TextUtil::escape(""), QString(""));
    QCOMPARE(TextUtil::escape("Hello World"), QString("Hello World"));
    QCOMPARE(TextUtil::escape("<tag & \"quote\">"), QString("&lt;tag &amp; &quot;quote&quot;&gt;"));
}

void TextUtilTest::testUnescape()
{
    QCOMPARE(TextUtil::unescape(""), QString(""));
    QCOMPARE(TextUtil::unescape("Hello World"), QString("Hello World"));
    QCOMPARE(TextUtil::unescape("&lt;tag &amp; &quot;quote&quot; &gt;"), QString("<tag & \"quote\" >"));
}

void TextUtilTest::testQuote()
{
    // Basic quoting
    QCOMPARE(TextUtil::quote("Hello world", 80, true), QString("> Hello world\n\n"));

    // Empty lines without quoteEmpty
    QCOMPARE(TextUtil::quote("Line1\n\nLine2", 80, false), QString("> Line1\n\n> Line2\n\n"));

    // Empty lines with quoteEmpty
    QCOMPARE(TextUtil::quote("Line1\n\nLine2", 80, true), QString("> Line1\n>\n> Line2\n\n"));

    // Line wrapping
    QString longText = "This is a very long line that should be wrapped";
    QString quotedLong = TextUtil::quote(longText, 20, true);
    QVERIFY(quotedLong.startsWith("> "));
    QVERIFY(quotedLong.endsWith("\n\n"));
    QVERIFY(quotedLong.contains("\n> "));
}

void TextUtilTest::testPlain2Rich()
{
    QCOMPARE(TextUtil::plain2rich("hello\nworld"), QString("hello<br>world"));
    QCOMPARE(TextUtil::plain2rich("<tag> & \"test\""), QString("&lt;tag&gt; &amp; &quot;test&quot;"));
    QCOMPARE(TextUtil::plain2rich("a  b"), QString("a &nbsp;b"));
    QCOMPARE(TextUtil::plain2rich("\ttab"), QString("&nbsp; &nbsp; &nbsp; tab"));
}

void TextUtilTest::testRich2Plain()
{
    QCOMPARE(TextUtil::rich2plain("hello<br>world"), QString("hello\nworld"));
    QCOMPARE(TextUtil::rich2plain("<p>line1</p><p>line2</p>"), QString("line1\nline2"));
    QCOMPARE(TextUtil::rich2plain("&lt;tag&amp;&gt;&quot;&apos;"), QString("<tag&>\"'"));
    QCOMPARE(TextUtil::rich2plain("a&nbsp;&nbsp;b", false), QString("a  b"));
}

void TextUtilTest::testResolveEntities()
{
    QCOMPARE(TextUtil::resolveEntities(QStringView(u"&amp;&lt;&gt;&quot;&apos;")), QString("&<>\"'"));
    QCOMPARE(TextUtil::resolveEntities(QStringView(u"&nbsp;")), QString(QChar(0xa0)));
    QCOMPARE(TextUtil::resolveEntities(QStringView(u"plain text")), QString("plain text"));
}

void TextUtilTest::testLinkify()
{
    QString http = TextUtil::linkify("visit https://example.com/ today");
    QVERIFY(http.contains("<a href=\"https://example.com/\">https://example.com/</a>"));

    QString xmpp = TextUtil::linkify("contact xmpp:user@example.com");
    QVERIFY(xmpp.contains("<a href=\"xmpp:user@example.com\""));
    QVERIFY(xmpp.contains("xmpp:user@example.com</a>"));

    QString mailto = TextUtil::linkify("email mailto:user@example.com");
    QVERIFY(mailto.contains("<a href=\"mailto:user@example.com\""));

    QString www = TextUtil::linkify("check www.example.com");
    QVERIFY(www.contains("<a href=\"https://www.example.com\""));
}

void TextUtilTest::testImg2Title()
{
    QCOMPARE(TextUtil::img2title("<img src='foo.png' title='smile'>"), QString("smile"));
    QCOMPARE(TextUtil::img2title("<img src=\"foo.png\" title=\"smile\">"), QString("smile"));
    QCOMPARE(TextUtil::img2title("no img tag"), QString("no img tag"));
}

void TextUtilTest::testSizeUnit()
{
    qlonglong div = 0;

    QCOMPARE(TextUtil::sizeUnit(500, &div), QString("B"));
    QCOMPARE(div, qlonglong(1));

    QCOMPARE(TextUtil::sizeUnit(1024, &div), QString("KB"));
    QCOMPARE(div, qlonglong(1024));

    QCOMPARE(TextUtil::sizeUnit(1024 * 1024 * 5, &div), QString("MB"));
    QCOMPARE(div, qlonglong(1024 * 1024));

    QCOMPARE(TextUtil::sizeUnit(1024LL * 1024 * 1024 * 2, &div), QString("GB"));
    QCOMPARE(div, qlonglong(1024LL * 1024 * 1024));
}

void TextUtilTest::testRoundedNumber()
{
    QCOMPARE(TextUtil::roundedNumber(500, 1), QString("500"));
    QCOMPARE(TextUtil::roundedNumber(1536, 1024), QString("1.5"));
    QCOMPARE(TextUtil::roundedNumber(2048, 1024), QString("2.0"));
}

QTEST_MAIN(TextUtilTest)
#include "textutiltest.moc"
