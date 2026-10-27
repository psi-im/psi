/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "psitextview.h"
#include <QColor>
#include <QTextDocument>
#include <QPoint>
#include <functional>
#include <memory>

// An inline Psi rich-text document. The surrounding timeline owns scrolling;
// this widget reports the complete document height at the available width.
class ChatMessageText : public PsiTextView {
    Q_OBJECT
public:
    explicit ChatMessageText(QWidget *parent = nullptr);
    void    setRichText(const QString &text);
    QString toPlainText() const { return document()->toPlainText(); }
    void    setWholeMessageSelectionMode(bool active);
    void    setPlainMessage(const QString &text);
    // Removes every line that starts with '>', i.e. quotes within a quote.
    void    removeQuotedLines();
    // Shows lines that start with '>' as quotes: indented, with a bar per
    // level, in the quote colour. Copying restores the markers.
    void    markQuotedLines();
    void    setQuoteColor(const QColor &color);
    // By default the view always shows its document from the top and passes
    // wheel input to the enclosing scroll area. A scrollable view scrolls its
    // own document vertically instead.
    void    setScrollable(bool scrollable);
    // Gets clicked links first; returning true keeps Psi's link menu away.
    void    setLinkHandler(std::function<bool(const QString &link)> handler) { linkHandler_ = std::move(handler); }
    // Images are scaled down to the text width and, if given, to this height.
    void    setMaximumImageHeight(int height);
    bool    hasImagesTallerThan(int height, int width) const;
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
    void       paintEvent(QPaintEvent *event) override;
    QMimeData *createMimeDataFromSelection() const override;

private:
    void applyDocumentFont();
    // A copy of the document for measuring other widths. Changing the width of
    // the shown document behind QTextEdit's back can leave it laid out at a
    // width QTextEdit does not know about until its own width changes.
    QTextDocument *measuringDocument() const;
    void           resetMeasuring() const;
    QMargins       textChrome() const;
    void isolateLargeMedia();
    void applyQuoteColor();

    bool   scrollable_ = false;
    bool   hasQuotes_  = false;
    QColor quoteColor_;
    // Only media-rich messages need rescanning when async SIMS previews arrive.
    bool hasMediaMarkup_ = false;
    bool isolatingMedia_ = false;
    bool mediaLayoutScheduled_ = false;
    mutable int                            naturalWidth_ = -1;
    mutable std::unique_ptr<QTextDocument> measuring_;

    std::function<bool(const QString &)> linkHandler_;
    QString                              pressedLink_;
    QPoint  pressedLinkPosition_;
};
