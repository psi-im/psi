/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "psitextview.h"
#include <QTextDocument>
#include <QPoint>

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
    // Width of the content laid out without wrapping, e.g. to fit a bubble.
    int     naturalWidth() const;
    QSize   sizeHint() const override;
    QSize   minimumSizeHint() const override;

protected:
    void       mousePressEvent(QMouseEvent *event) override;
    void       mouseReleaseEvent(QMouseEvent *event) override;
    void       resizeEvent(QResizeEvent *event) override;
    void       changeEvent(QEvent *event) override;
    void       wheelEvent(QWheelEvent *event) override;
    QMimeData *createMimeDataFromSelection() const override;

private:
    void applyDocumentFont();
    void isolateLargeMedia();

    // Only media-rich messages need rescanning when async SIMS previews arrive.
    bool hasMediaMarkup_ = false;
    bool isolatingMedia_ = false;
    bool mediaLayoutScheduled_ = false;
    mutable int naturalWidth_ = -1;

    QString pressedLink_;
    QPoint  pressedLinkPosition_;
};
