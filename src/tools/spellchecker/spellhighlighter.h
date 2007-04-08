#ifndef SPELLHIGHLIGHTER_H
#define SPELLHIGHLIGHTER_H

 #include <QSyntaxHighlighter>

class QString;

class SpellHighlighter : public QSyntaxHighlighter
{
public:
	SpellHighlighter(QTextDocument*);

	virtual void highlightBlock(const QString& text);
};

#endif
