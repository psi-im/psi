#ifndef SPELLHIGHLIGHTER_H
#define SPELLHIGHLIGHTER_H

#include <QSyntaxHighlighter>

class QString;

class SpellHighlighter : public QSyntaxHighlighter {
public:
    using QSyntaxHighlighter::QSyntaxHighlighter;

    virtual void highlightBlock(const QString &text);
};

#endif // SPELLHIGHLIGHTER_H
