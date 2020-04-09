/*
 * psirichtext.h - helper functions to handle Icons in QTextDocuments
 * Copyright (C) 2006  Michail Pishchagin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <QTextCursor>
#include <QTextDocument>

#include <functional>

class QTextEdit;
class ITEMediaOpener;

class PsiRichText {
public:
    using ParserRet  = std::pair<QTextCharFormat, QString>;
    using Parser     = std::function<ParserRet(const QStringRef &htmlElement, int insertAfter)>;
    using ParsersMap = QMap<QString, Parser>;

    static void install(QTextDocument *doc);
    static void ensureTextLayouted(QTextDocument *doc, int documentWidth, Qt::Alignment align = Qt::AlignLeft,
                                   Qt::LayoutDirection layoutDirection = Qt::LeftToRight, bool textWordWrap = true);
    static void setText(QTextDocument *doc, const QString &text, const ParsersMap &parsers = ParsersMap());
    static void insertIcon(QTextCursor &cursor, const QString &iconName, const QString &iconText);
    static void appendText(QTextDocument *doc, QTextCursor &cursor, const QString &text, bool append = true,
                           const ParsersMap &parsers = ParsersMap());
    static void addEmoticon(QTextEdit *textEdit, const QString &emoticon);
    static void setAllowedImageDirs(const QStringList &);

    static QString convertToPlainText(const QTextDocument *doc);

    static QTextCharFormat markerFormat(const QString &uniqueId);
    static void            insertMarker(QTextCursor &cursor, const QString &uniqueId);
    static QTextCursor     findMarker(const QTextCursor &cursor,
                                      const QString &    uniqueId); // will modify cursor to stay right after marker.

    struct Selection {
        int start, end;
    };
    static Selection saveSelection(QTextEdit *textEdit, QTextCursor &cursor);
    static void      restoreSelection(QTextEdit *textEdit, QTextCursor &cursor, Selection selection);
};
