/*
 * psitextview.h - PsiIcon-aware QTextView subclass widget
 * Copyright (C) 2003-2006  Michail Pishchagin
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

#ifndef PSITEXTVIEW_H
#define PSITEXTVIEW_H

#include <QTextEdit>

class QMimeData;
class QTextCursor;
class FileSharingDeviceOpener;

class PsiTextView : public QTextEdit {
    Q_OBJECT
public:
    PsiTextView(QWidget *parent = nullptr);

    // Reimplemented
    QMenu *createStandardContextMenu(const QPoint &position);

    bool atBottom();

    virtual void appendText(const QString &text);
    virtual void insertText(const QString &text, QTextCursor &cursor);

    QString getHtml() const;
    QString getPlainText() const;
    void    setMediaOpener(FileSharingDeviceOpener *opener);

public slots:
    void scrollToBottom();
    void scrollToTop();

protected:
    // make these functions unusable, because they modify
    // document structure and we can't override them to
    // handle Icons correctly
    void append(const QString &) {}
    void toHtml() const {}
    void toPlainText() const {}
    void insertHtml(const QString &) {}
    void insertPlainText(const QString &) {}
    void setHtml(const QString &) {}
    void setPlainText(const QString &) {}

    // reimplemented
    void       contextMenuEvent(QContextMenuEvent *e);
    void       mouseMoveEvent(QMouseEvent *e);
    void       mousePressEvent(QMouseEvent *e);
    void       mouseReleaseEvent(QMouseEvent *e);
    QMimeData *createMimeDataFromSelection() const;
    void       resizeEvent(QResizeEvent *);

    QString getTextHelper(bool html) const;

    class Private;

private:
    Private *d;
    bool     isSelectedBlock();
};

#endif // PSITEXTVIEW_H
