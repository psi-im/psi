#include "spellhighlighter.h"
#include "spellchecker.h"

SpellHighlighter::SpellHighlighter(QTextDocument* d) : QSyntaxHighlighter(d)
{
}

void SpellHighlighter::highlightBlock(const QString& text)
{
	// Underline 
	QTextCharFormat tcf;
	tcf.setUnderlineColor(QBrush(QColor(255,0,0)));
	tcf.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);

	// Match words (minimally)
	QRegExp expression("\\b\\w+\\b");

	// Iterate through all words
	int index = text.indexOf(expression);
	while (index >= 0) {
		int length = expression.matchedLength();
		if (!SpellChecker::instance()->isCorrect(expression.cap()))
			setFormat(index, length, tcf);
		index = text.indexOf(expression, index + length);
	}
}
