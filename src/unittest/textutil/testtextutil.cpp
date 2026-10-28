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
    // XEP-0393 is deliberately distinct from Markdown.
    void testLegacyFormat_data();
    void testLegacyFormat();
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
    QTest::newRow("empty") << QString() << QString();
    QTest::newRow("plain") << QStringLiteral("Hello world") << QStringLiteral("Hello world");
    QTest::newRow("strong") << QStringLiteral("before *bold* after")
                              << QStringLiteral("before <b>*bold*</b> after");
    QTest::newRow("emphasis") << QStringLiteral("before _italic_ after")
                                << QStringLiteral("before <i>_italic_</i> after");
    QTest::newRow("strike") << QStringLiteral("~gone~")
                              << QStringLiteral("<s>~gone~</s>");
    QTest::newRow("code") << QStringLiteral("a ") + QChar(0x60) + QStringLiteral("*literal*") + QChar(0x60)
                            << QStringLiteral("a <code>") + QChar(0x60) + QStringLiteral("*literal*") + QChar(0x60) + QStringLiteral("</code>");
    QTest::newRow("nested") << QStringLiteral("*bold _italic_*")
                              << QStringLiteral("<b>*bold <i>_italic_</i>*</b>");
    QTest::newRow("adjacent-nesting") << QStringLiteral("*_both_*")
                                        << QStringLiteral("<b>*<i>_both_</i>*</b>");
    QTest::newRow("line-break") << QStringLiteral("a<br>*strong*")
                                  << QStringLiteral("a<br><b>*strong*</b>");
    QTest::newRow("quoted") << QStringLiteral("&gt; *strong*")
                              << QStringLiteral("&gt; <b>*strong*</b>");
    QTest::newRow("link") << QStringLiteral("*<a href=\"https://psi-im.org\">Psi</a>*")
                            << QStringLiteral("<b>*<a href=\"https://psi-im.org\">Psi</a>*</b>");
    QTest::newRow("html attributes") << QStringLiteral("<a href=\"https://psi-im.org/a_b*c~d\">plain</a>")
                                       << QStringLiteral("<a href=\"https://psi-im.org/a_b*c~d\">plain</a>");
    QTest::newRow("spaces") << QStringLiteral("* invalid*") << QStringLiteral("* invalid*");
    QTest::newRow("span cannot cross lines") << QStringLiteral("*before<br>after*")
                                               << QStringLiteral("*before<br>after*");
    QTest::newRow("embedded") << QStringLiteral("word*strong*word")
                                << QStringLiteral("word*strong*word");
    QTest::newRow("markdown strong") << QStringLiteral("**bold**")
                                       << QStringLiteral("**bold**");
    QTest::newRow("slash") << QStringLiteral("/italic/") << QStringLiteral("/italic/");
    QTest::newRow("URL") << QStringLiteral("https://example.org/foo_bar/")
                            << QStringLiteral("https://example.org/foo_bar/");
}

void TextUtilTest::testLegacyFormat()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QCOMPARE(TextUtil::legacyFormat(input), expected);
}

void TextUtilTest::testLegacyFormat_AdditionalMD()
{
    // Markdown/Discord extensions are not XEP-0393.
    for (const QString &plain : { QStringLiteral("***strong italic***"), QStringLiteral("**strong**"),
                                  QStringLiteral("__underline__"), QStringLiteral("/italic/"),
                                  QStringLiteral("~~strike~~") })
        QCOMPARE(TextUtil::legacyFormat(plain), plain);

    const QString fence(3, QChar(0x60));
    const QString pre = fence + QStringLiteral("sh<br>*not strong*<br>") + fence + QStringLiteral("<br>*strong*");
    const QString out = TextUtil::legacyFormat(pre);
    QVERIFY(out.contains(QStringLiteral("*not strong*")));
    QVERIFY(!out.contains(QStringLiteral("<b>*not strong*</b>")));
    QVERIFY(out.endsWith(QStringLiteral("<br><b>*strong*</b>")));
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
