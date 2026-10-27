/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "psirichtext.h"

#include "qsourcehighliter.h"

#include <QColor>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>

#include <algorithm>
#include <optional>
#include <vector>

namespace {

using SourceLanguage = QSourceHighlite::QSourceHighliter::Language;

std::optional<SourceLanguage> sourceLanguageForName(QString language)
{
    language = language.trimmed().toLower();
    const int whitespace = [&language] {
        const int space = language.indexOf(QLatin1Char(' '));
        const int tab = language.indexOf(QLatin1Char('\t'));
        if (space < 0) {
            return tab;
        }
        if (tab < 0) {
            return space;
        }
        return std::min(space, tab);
    }();
    if (whitespace >= 0) {
        language.truncate(whitespace);
    }

    if (language == QLatin1String("cpp") || language == QLatin1String("c++")
        || language == QLatin1String("cxx") || language == QLatin1String("cc")) {
        return SourceLanguage::CodeCpp;
    }
    if (language == QLatin1String("c")) {
        return SourceLanguage::CodeC;
    }
    if (language == QLatin1String("js") || language == QLatin1String("javascript")) {
        return SourceLanguage::CodeJs;
    }
    if (language == QLatin1String("bash") || language == QLatin1String("sh")
        || language == QLatin1String("shell")) {
        return SourceLanguage::CodeBash;
    }
    if (language == QLatin1String("php")) {
        return SourceLanguage::CodePHP;
    }
    if (language == QLatin1String("qml")) {
        return SourceLanguage::CodeQML;
    }
    if (language == QLatin1String("py") || language == QLatin1String("python")) {
        return SourceLanguage::CodePython;
    }
    if (language == QLatin1String("rs") || language == QLatin1String("rust")) {
        return SourceLanguage::CodeRust;
    }
    if (language == QLatin1String("java")) {
        return SourceLanguage::CodeJava;
    }
    if (language == QLatin1String("cs") || language == QLatin1String("c#")
        || language == QLatin1String("csharp")) {
        return SourceLanguage::CodeCSharp;
    }
    if (language == QLatin1String("go")) {
        return SourceLanguage::CodeGo;
    }
    if (language == QLatin1String("v")) {
        return SourceLanguage::CodeV;
    }
    if (language == QLatin1String("sql")) {
        return SourceLanguage::CodeSQL;
    }
    if (language == QLatin1String("json")) {
        return SourceLanguage::CodeJSON;
    }
    if (language == QLatin1String("xml") || language == QLatin1String("html")) {
        return SourceLanguage::CodeXML;
    }
    if (language == QLatin1String("css")) {
        return SourceLanguage::CodeCSS;
    }
    if (language == QLatin1String("ts") || language == QLatin1String("typescript")) {
        return SourceLanguage::CodeTypeScript;
    }
    if (language == QLatin1String("yaml") || language == QLatin1String("yml")) {
        return SourceLanguage::CodeYAML;
    }
    if (language == QLatin1String("ini")) {
        return SourceLanguage::CodeINI;
    }
    if (language == QLatin1String("vex")) {
        return SourceLanguage::CodeVex;
    }
    if (language == QLatin1String("cmake")) {
        return SourceLanguage::CodeCMake;
    }
    if (language == QLatin1String("make") || language == QLatin1String("makefile")) {
        return SourceLanguage::CodeMake;
    }
    if (language == QLatin1String("asm") || language == QLatin1String("assembly")) {
        return SourceLanguage::CodeAsm;
    }
    if (language == QLatin1String("lua")) {
        return SourceLanguage::CodeLua;
    }
    if (language == QLatin1String("rhai")) {
        return SourceLanguage::CodeRhai;
    }

    return std::nullopt;
}

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
            if (const auto lang = sourceLanguageForName(language)) {
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
