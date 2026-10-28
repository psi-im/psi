/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "psirichtext.h"
#include "psicodelanguage.h"

#include "qsourcehighliter.h"

#include <QColor>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QStringList>

#include <algorithm>
#include <optional>
#include <vector>

namespace {

struct SourceLine {
    int start = 0;
    int end = 0;
    QString text;
};

bool isFence(const QString &line)
{
    return line.startsWith(QLatin1String("```"))
        && (line.size() == 3 || line.at(3) != QLatin1Char('`'));
}

bool isClosingFence(const QString &line)
{
    return isFence(line) && line.mid(3).trimmed().isEmpty();
}

// XEP-0393 preformatted markers are visible in the original document.
// Convert the text to lines only for locating styling ranges. The document
// itself is never replaced, so selection/copy spans remain uninterrupted.
std::vector<SourceLine> sourceLines(const QString &plain)
{
    std::vector<SourceLine> result;
    for (int at = 0; at <= plain.size();) {
        int end = plain.indexOf(QLatin1Char('\n'), at);
        if (end < 0)
            end = plain.size();
        result.push_back({ at, end, plain.mid(at, end - at) });
        if (end == plain.size())
            break;
        at = end + 1;
    }
    return result;
}

void applyRange(QTextCursor &cursor, int from, int to, const QTextCharFormat &format)
{
    if (from >= to)
        return;
    cursor.setPosition(from);
    cursor.setPosition(to, QTextCursor::KeepAnchor);
    cursor.mergeCharFormat(format);
}

} // namespace

QList<PsiRichText::CodeBlock> PsiRichText::codeBlocks(const QTextDocument *doc)
{
    QList<CodeBlock> result;
    if (!doc)
        return result;
    const QString text = doc->toPlainText();
    if (!text.contains(QString(3, QChar(0x60))) || text.size() != doc->characterCount() - 1)
        return result;

    const auto lines = sourceLines(text);
    QTextCursor marker(const_cast<QTextDocument *>(doc));
    for (size_t first = 0; first < lines.size(); ++first) {
        const auto &opening = lines[first];
        if (!isFence(opening.text))
            continue;
        marker.setPosition(opening.start);
        if (!marker.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor)
            || !isMonospace(marker.charFormat()))
            continue;
        marker.clearSelection();

        size_t closing = first + 1;
        for (; closing < lines.size(); ++closing)
            if (isClosingFence(lines[closing].text))
                break;
        // A separate widget needs a completed fence. Incomplete or oddly
        // structured messages remain rendered by the shared QTextDocument.
        if (closing >= lines.size())
            continue;
        QStringList code;
        code.reserve(int(closing - first - 1));
        for (size_t line = first + 1; line < closing; ++line)
            code.append(lines[line].text);
        result.append({ opening.start, lines[closing].end,
                        opening.text.mid(3).trimmed(),
                        code.join(QLatin1Char('\n')) });
        first = closing;
    }
    return result;
}

void PsiRichText::highlightCodeBlocks(QTextDocument *doc)
{
    if (!doc)
        return;
    const QString text = doc->toPlainText();
    if (!text.contains(QLatin1String("```")))
        return;
    // Plain-text offsets match QTextDocument's one-character paragraph and
    // line separators. Other QTextDocument contents may collapse characters
    // (e.g. tables); do not risk selecting the wrong ranges in that case.
    if (text.size() != doc->characterCount() - 1)
        return;

    const auto lines = sourceLines(text);
    QTextCursor marker(doc);
    QTextCursor cursor(doc);
    cursor.beginEditBlock();
    for (size_t first = 0; first < lines.size(); ++first) {
        const auto &opening = lines[first];
        if (!isFence(opening.text))
            continue;

        // Only real XEP-0393 preformatted blocks (formatted by TextUtil)
        // should be highlighted. A literal fence in an unstyled message is
        // not sufficient to turn code coloring on.
        marker.setPosition(opening.start);
        if (!marker.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor)
            || !isMonospace(marker.charFormat()))
            continue;
        marker.clearSelection();

        const QString language = opening.text.mid(3).trimmed();
        size_t closing = first + 1;
        for (; closing < lines.size(); ++closing)
            if (isClosingFence(lines[closing].text))
                break;

        const int codeCount = int(closing - first - 1);
        if (codeCount > 0) {
            QTextCharFormat base;
            base.setFontFixedPitch(true);
            base.setForeground(QColor(227, 226, 214));
            base.setBackground(QColor(39, 40, 34));
            QStringList code;
            code.reserve(codeCount);
            for (size_t line = first + 1; line < closing; ++line) {
                const auto &entry = lines[line];
                applyRange(cursor, entry.start, entry.end, base);
                code.append(entry.text);
            }

            // QSourceHighliter operates on a transient, independent
            // document containing ONLY the source lines. Copy the resulting
            // QTextCharFormat ranges into the original message document,
            // rather than placing a syntax highlighter over the entire chat
            // or creating a second selectable message widget.
            if (const auto lang = PsiCodeLanguage::fromName(language)) {
                QTextDocument source;
                source.setPlainText(code.join(QLatin1Char('\n')));
                QSourceHighlite::QSourceHighliter highlighter(
                    &source, QSourceHighlite::QSourceHighliter::Monokai);
                highlighter.setCurrentLanguage(*lang);
                highlighter.rehighlight();

                auto sourceBlock = source.begin();
                for (int index = 0; index < codeCount && sourceBlock.isValid();
                     ++index, sourceBlock = sourceBlock.next()) {
                    const auto &entry = lines[first + 1 + size_t(index)];
                    if (const auto *layout = sourceBlock.layout()) {
                        for (const auto &range : layout->formats()) {
                            if (range.length <= 0 || range.start < 0
                                || range.start >= entry.end - entry.start)
                                continue;
                            const int length = std::min(range.length, entry.end - entry.start - range.start);
                            if (!range.format.foreground().isOpaque())
                                continue;
                            QTextCharFormat color;
                            color.setForeground(range.format.foreground());
                            applyRange(cursor, entry.start + range.start,
                                       entry.start + range.start + length, color);
                        }
                    }
                }
            }
        }
        first = closing < lines.size() ? closing : lines.size() - 1;
    }
    cursor.endEditBlock();
}
