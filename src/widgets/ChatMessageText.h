/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "psitextview.h"
#include <QColor>
#include <QTextDocument>
#include <QTextFormat>
#include <QPoint>
#include <functional>
#include <memory>
#include <optional>

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
    // Gets clicked links first; returning true keeps Psi's link menu away.
    void    setLinkHandler(std::function<bool(const QString &link)> handler) { linkHandler_ = std::move(handler); }
    // Images are scaled down to the text width and, if given, to this height.
    void    setMaximumImageHeight(int height);
    bool    hasImagesTallerThan(int height, int width) const;
    // The image or other inline object drawn at this viewport position.
    std::optional<QTextCharFormat>  objectAt(const QPoint &point) const;
    std::optional<QTextImageFormat> imageAt(const QPoint &point) const;
    int     heightForWidth(int width) const override;
    // Width of the content laid out without wrapping, e.g. to fit a bubble.
    int     naturalWidth() const;
    QSize   sizeHint() const override;
    QSize   minimumSizeHint() const override;

signals:
    // A plain left click that no link, selection or inline object claimed.
    void plainClicked();

protected:
    void       mousePressEvent(QMouseEvent *event) override;
    void       mouseReleaseEvent(QMouseEvent *event) override;
    void       resizeEvent(QResizeEvent *event) override;
    void       changeEvent(QEvent *event) override;
    void       wheelEvent(QWheelEvent *event) override;

private:
    void applyDocumentFont();
    // A copy of the document for measuring other widths. Changing the width of
    // the shown document behind QTextEdit's back can leave it laid out at a
    // width QTextEdit does not know about until its own width changes.
    QTextDocument *measuringDocument() const;
    void           resetMeasuring() const;
    QMargins       textChrome() const;
    void isolateLargeMedia();

    // Only media-rich messages need rescanning when async SIMS previews arrive.
    bool hasMediaMarkup_ = false;
    bool isolatingMedia_ = false;
    bool mediaLayoutScheduled_ = false;
    mutable int                            naturalWidth_ = -1;
    mutable std::unique_ptr<QTextDocument> measuring_;

    std::function<bool(const QString &)> linkHandler_;
    QString                              pressedLink_;
    std::optional<QPoint> pressPosition_; // Unset when an event filter took the press: a media player's control.
};
