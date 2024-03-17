#include "spellhighlighter.h"

#include "common.h"
#include "spellchecker.h"
#include <QRegularExpression>

void SpellHighlighter::highlightBlock(const QString &text)
{
    // Underline
    QTextCharFormat tcf;
    tcf.setUnderlineColor(QColor(255, 0, 0));
    if (qVersionInt() >= 0x040400 && qVersionInt() < 0x040402) {
        tcf.setUnderlineStyle(QTextCharFormat::DotLine);
    } else {
        tcf.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
    }

    // Match words (minimally)
    QRegularExpression expression("\\b\\w+\\b", QRegularExpression::UseUnicodePropertiesOption);
    QRegularExpression digit("^\\d+$");

    // Iterate through all words
    QRegularExpressionMatch match;
    int                     index = text.indexOf(expression, 0, &match);
    while (index >= 0) {
        int     length = match.capturedLength();
        QString word   = match.captured();
        if (!digit.match(word).hasMatch() && !SpellChecker::instance()->isCorrect(word))
            setFormat(index, length, tcf);
        index = text.indexOf(expression, index + length, &match);
    }
}
