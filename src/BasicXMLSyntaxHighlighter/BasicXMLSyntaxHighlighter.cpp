/*
 * The MIT License (MIT)

 * Copyright (c) 2015 Dmitry Ivanov

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 */

#include "BasicXMLSyntaxHighlighter.h"
#include <QRegularExpressionMatchIterator>

BasicXMLSyntaxHighlighter::BasicXMLSyntaxHighlighter(QObject *parent) : QSyntaxHighlighter(parent)
{
    setRegexes();
    setFormats();
}

BasicXMLSyntaxHighlighter::BasicXMLSyntaxHighlighter(QTextDocument *parent) : QSyntaxHighlighter(parent)
{
    setRegexes();
    setFormats();
}

BasicXMLSyntaxHighlighter::BasicXMLSyntaxHighlighter(QTextEdit *parent) : QSyntaxHighlighter(parent)
{
    setRegexes();
    setFormats();
}

void BasicXMLSyntaxHighlighter::highlightBlock(const QString &text)
{
    // Special treatment for xml element regex as we use captured text to emulate lookbehind
    auto matchIt = m_xmlElementRegex.globalMatch(text);

    while (matchIt.hasNext()) {
        auto match = matchIt.next();
        setFormat(match.capturedStart(), match.capturedLength(), m_xmlElementFormat);
    }

    // Highlight xml keywords *after* xml elements to fix any occasional / captured into the enclosing element
    for (auto it = m_xmlKeywordRegexes.begin(); it != m_xmlKeywordRegexes.end(); ++it) {
        const QRegularExpression &regex = *it;
        highlightByRegex(m_xmlKeywordFormat, regex, text);
    }

    highlightByRegex(m_xmlAttributeFormat, m_xmlAttributeRegex, text);
    highlightByRegex(m_xmlCommentFormat, m_xmlCommentRegex, text);
    highlightByRegex(m_xmlValueFormat, m_xmlValueRegex, text);
}

void BasicXMLSyntaxHighlighter::highlightByRegex(const QTextCharFormat &format, const QRegularExpression &regex,
                                                 const QString &text)
{
    auto matchIt = regex.globalMatch(text);

    while (matchIt.hasNext()) {
        auto match = matchIt.next();
        setFormat(match.capturedStart(), match.capturedLength(), format);
    }
}

void BasicXMLSyntaxHighlighter::setRegexes()
{
    m_xmlElementRegex.setPattern("<[?\\s]*[/]?[\\s]*([^\\n][^>]*)(?=[\\s/>])");
    m_xmlAttributeRegex.setPattern("\\w+(?=\\=)");
    m_xmlValueRegex.setPattern("\"[^\\n\"]+\"(?=[?\\s/>])");
    m_xmlCommentRegex.setPattern("<!--[^\\n]*-->");

    m_xmlKeywordRegexes = QList<QRegularExpression>()
        << QRegularExpression("<\\?") << QRegularExpression("/>") << QRegularExpression(">") << QRegularExpression("<")
        << QRegularExpression("</") << QRegularExpression("\\?>");
}

void BasicXMLSyntaxHighlighter::setFormats()
{
    m_xmlKeywordFormat.setForeground(Qt::blue);
    m_xmlKeywordFormat.setFontWeight(QFont::Bold);

    m_xmlElementFormat.setForeground(Qt::darkMagenta);
    m_xmlElementFormat.setFontWeight(QFont::Bold);

    m_xmlAttributeFormat.setForeground(Qt::darkGreen);
    m_xmlAttributeFormat.setFontWeight(QFont::Bold);
    m_xmlAttributeFormat.setFontItalic(true);

    m_xmlValueFormat.setForeground(Qt::darkRed);

    m_xmlCommentFormat.setForeground(Qt::gray);
}
