/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "psitextview.h"
#include <QTextDocument>

// An inline Psi rich-text document. The surrounding timeline owns scrolling;
// this widget reports the complete document height at the available width.
class ChatMessageText : public PsiTextView {
    Q_OBJECT
public:
    explicit ChatMessageText(QWidget *parent = nullptr);
    void    setRichText(const QString &text);
    QString toPlainText() const { return document()->toPlainText(); }
    void    setWholeMessageSelectionMode(bool active);
    int     heightForWidth(int width) const override;
    QSize   sizeHint() const override;
    QSize   minimumSizeHint() const override;

protected:
    void       resizeEvent(QResizeEvent *event) override;
    void       changeEvent(QEvent *event) override;
    void       wheelEvent(QWheelEvent *event) override;
    void       mousePressEvent(QMouseEvent *event) override;
    void       mouseReleaseEvent(QMouseEvent *event) override;
    QMimeData *createMimeDataFromSelection() const override;

private:
    void applyDocumentFont();

    QString pressedLink_;
    QPoint  pressedLinkPosition_;
};
