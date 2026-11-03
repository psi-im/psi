// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QStringView>

#include <algorithm>
#include <limits>
#include <utility>

// XEP-0393 is not Markdown. Parse visible text, preserving existing HTML
// (linkification, icons, XHTML-IM) and styling directive characters.
namespace MessageStyling {
struct StylingGlyph {
    QChar ch;
    int begin;
    int end;
    bool protectedText;
};
struct StylingRange {
    int begin;
    int end;
    QString open;
    QString close;
};
static bool isDirective(QChar c)
{
    return c == QLatin1Char('*') || c == QLatin1Char('_') || c == QLatin1Char('~') || c == QChar(0x60);
}
static QList<StylingGlyph> stylingGlyphs(const QString &html)
{
    QList<StylingGlyph> glyphs;
    int protectedDepth = 0;
    for (int i = 0; i < html.size();) {
        const int start = i;
        if (html.at(i) == QLatin1Char('<')) {
            const int end = html.indexOf(QLatin1Char('>'), i + 1);
            if (end >= 0) {
                const QStringView tag = QStringView(html).mid(i, end - i + 1);
                if (tag.startsWith(QLatin1String("<br"), Qt::CaseInsensitive))
                    glyphs.append({ QLatin1Char('\n'), start, end + 1, false });
                else if (tag.startsWith(QLatin1String("</pre"), Qt::CaseInsensitive)
                         || tag.startsWith(QLatin1String("</code"), Qt::CaseInsensitive))
                    protectedDepth = qMax(0, protectedDepth - 1);
                else if (tag.startsWith(QLatin1String("<pre"), Qt::CaseInsensitive)
                         || tag.startsWith(QLatin1String("<code"), Qt::CaseInsensitive))
                    ++protectedDepth;
                i = end + 1;
                continue;
            }
        }
        if (html.at(i) == QLatin1Char('&')) {
            const int end = html.indexOf(QLatin1Char(';'), i + 1);
            if (end >= 0 && end - i <= 12) {
                const QStringView entity = QStringView(html).mid(i + 1, end - i - 1);
                QChar decoded;
                if (entity == QLatin1String("lt")) decoded = QLatin1Char('<');
                else if (entity == QLatin1String("gt")) decoded = QLatin1Char('>');
                else if (entity == QLatin1String("amp")) decoded = QLatin1Char('&');
                else if (entity == QLatin1String("quot")) decoded = QLatin1Char('"');
                else if (entity == QLatin1String("apos")) decoded = QLatin1Char('\'');
                else if (entity == QLatin1String("nbsp")) decoded = QChar::Nbsp;
                if (!decoded.isNull()) {
                    glyphs.append({ decoded, i, end + 1, protectedDepth > 0 });
                    i = end + 1;
                    continue;
                }
            }
        }
        glyphs.append({ html.at(i), i, i + 1, protectedDepth > 0 });
        ++i;
    }
    return glyphs;
}
static int quotePrefix(const QList<StylingGlyph> &g, int at, int end, int *depth,
                       int maximum = std::numeric_limits<int>::max())
{
    int nesting = 0;
    while (at < end && nesting < maximum && g.at(at).ch == QLatin1Char('>')) {
        ++nesting;
        ++at;
        if (at < end && g.at(at).ch == QLatin1Char(' '))
            ++at;
    }
    *depth = nesting;
    return at;
}
static void parseStylingSpans(const QList<StylingGlyph> &g, int first, int last,
                             QList<StylingRange> &ranges, int depth = 0)
{
    // The byte stream is lazily consumed from left to right within its
    // containing line. Adjacent opening directives can form nested spans.
    // A pair of backticks is also a valid preformatted span (XEP-0393 §6.2.5).
    if (depth > 32)
        return;
    for (int i = first; i < last; ++i) {
        const QChar ch = g.at(i).ch;
        if (!isDirective(ch) || g.at(i).protectedText)
            continue;
        const bool code = ch == QChar(0x60);
        const int run = code && i + 1 < last && g.at(i + 1).ch == ch ? 2 : 1;
        if (i + run >= last || g.at(i + run).ch.isSpace())
            continue;
        if (i > first && !g.at(i - 1).ch.isSpace() && !isDirective(g.at(i - 1).ch))
            continue;
        if ((i > first && g.at(i - 1).ch == ch) || (i + run < last && g.at(i + run).ch == ch))
            continue;
        int closing = -1;
        for (int j = i + run + 1; j < last; ++j) {
            if (g.at(j).ch != ch || g.at(j).protectedText
                || g.at(j - 1).ch.isSpace()
                || (j > i + run + 1 && g.at(j - 1).ch == ch))
                continue;
            bool matches = true;
            for (int k = 1; k < run; ++k)
                if (j + k >= last || g.at(j + k).ch != ch)
                    matches = false;
            if (!matches || (j + run < last && g.at(j + run).ch == ch))
                continue;
            closing = j;
            break;
        }
        if (closing < 0)
            continue;
        QString element;
        if (ch == QLatin1Char('*')) element = QLatin1String("b");
        else if (ch == QLatin1Char('_')) element = QLatin1String("i");
        else if (ch == QLatin1Char('~')) element = QLatin1String("s");
        else element = QLatin1String("code");
        ranges.append({ g.at(i).begin, g.at(closing + run - 1).end,
                        QLatin1Char('<') + element + QLatin1Char('>'),
                        QLatin1String("</") + element + QLatin1Char('>') });
        if (!code)
            parseStylingSpans(g, i + run, closing, ranges, depth + 1);
        i = closing + run - 1;
    }
}
static bool isFence(const QList<StylingGlyph> &g, int at, int end)
{
    return at + 2 < end && g.at(at).ch == QChar(0x60)
        && g.at(at + 1).ch == QChar(0x60) && g.at(at + 2).ch == QChar(0x60);
}

inline QString toHtml(const QString &in)
{
    const auto glyphs = stylingGlyphs(in);
    QList<StylingRange> ranges;
    int line = 0;
    while (line < glyphs.size()) {
        int end = line;
        while (end < glyphs.size() && glyphs.at(end).ch != QLatin1Char('\n'))
            ++end;
        int quoteDepth = 0;
        const int content = quotePrefix(glyphs, line, end, &quoteDepth);
        if (isFence(glyphs, content, end) && !glyphs.at(content).protectedText) {
            // Fence blocks end at the first bare closing fence or at the end of
            // their parent: the enclosing quote, or the message. Their contents
            // remain unparsed, including any further '>' in a line.
            const auto preformatted = [&glyphs, &ranges](int first, int last) {
                if (first < last)
                    ranges.append({ glyphs.at(first).begin, glyphs.at(last - 1).end,
                                    QLatin1String("<span style=\"font-family:monospace; white-space:pre-wrap\">"),
                                    QLatin1String("</span>") });
            };
            int first = content;
            int last = end;
            int next = end < glyphs.size() ? end + 1 : end;
            while (next < glyphs.size()) {
                int stop = next;
                while (stop < glyphs.size() && glyphs.at(stop).ch != QLatin1Char('\n'))
                    ++stop;
                int parentDepth = 0;
                const int child = quotePrefix(glyphs, next, stop, &parentDepth, quoteDepth);
                if (parentDepth < quoteDepth)
                    break;
                // The markers of an enclosing quote are not preformatted text.
                if (quoteDepth > 0) {
                    preformatted(first, last);
                    first = child;
                }
                last = stop;
                next = stop < glyphs.size() ? stop + 1 : stop;
                if (isFence(glyphs, child, stop) && child + 3 == stop)
                    break;
            }
            preformatted(first, last);
            line = next;
            continue;
        }
        parseStylingSpans(glyphs, content, end, ranges);
        line = end + 1;
    }
    QMap<int, QList<StylingRange>> openings, closings;
    for (const auto &range : std::as_const(ranges)) {
        openings[range.begin].append(range);
        closings[range.end].append(range);
    }
    QString result;
    result.reserve(in.size() + ranges.size() * 40);
    for (int pos = 0; pos <= in.size(); ++pos) {
        auto close = closings.value(pos);
        std::sort(close.begin(), close.end(), [](const StylingRange &a, const StylingRange &b) {
            return a.begin > b.begin;
        });
        for (const auto &range : std::as_const(close))
            result += range.close;
        auto open = openings.value(pos);
        std::sort(open.begin(), open.end(), [](const StylingRange &a, const StylingRange &b) {
            return a.end > b.end;
        });
        for (const auto &range : std::as_const(open))
            result += range.open;
        if (pos < in.size())
            result += in.at(pos);
    }
    return result;
}

} // namespace MessageStyling
