/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ChatMessageText.h"
#include "psirichtext.h"
#include "urlobject.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QMouseEvent>
#include <QCoreApplication>
#include <QMimeData>
#include <QSignalBlocker>
#include <QTextDocumentFragment>
#include <QTextOption>
#include <QWheelEvent>
#include <cmath>

ChatMessageText::ChatMessageText(QWidget *parent) : PsiTextView(parent)
{
    setReadOnly(true);
    setKeepAtBottom(false); // The outer timeline owns scrolling and viewport anchoring.
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setContentsMargins(0, 0, 0, 0);
    viewport()->setAutoFillBackground(false);
    auto policy = QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    document()->setDocumentMargin(0);
    auto option = document()->defaultTextOption();
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    document()->setDefaultTextOption(option);
    connect(document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this,
            [this] { updateGeometry(); });
}

void ChatMessageText::setRichText(const QString &text)
{
    document()->setDefaultFont(font());
    PsiTextView::setRichText(text);
    applyDocumentFont();
    PsiRichText::recolorLinks(document(), palette().color(QPalette::Link));
}

void ChatMessageText::setWholeMessageSelectionMode(bool active)
{
    const Qt::TextInteractionFlags flags
        = active ? Qt::NoTextInteraction : Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse;
    if (textInteractionFlags() != flags)
        setTextInteractionFlags(flags);
    if (active && textCursor().hasSelection()) {
        auto cursor = textCursor();
        cursor.clearSelection();
        setTextCursor(cursor);
    }
}

void ChatMessageText::applyDocumentFont()
{
    document()->setDefaultFont(font());
    // PsiRichText stores a font on inserted fragments. Updating the widget's
    // inherited font alone leaves those old fragments at the previous size.
    QTextCharFormat format;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    format.setFontFamilies({ font().family() });
#else
    format.setFontFamily(font().family());
#endif
    if (font().pointSizeF() > 0)
        format.setFontPointSize(font().pointSizeF());
    else
        format.setProperty(QTextFormat::FontPixelSize, font().pixelSize());
    QTextCursor cursor(document());
    cursor.select(QTextCursor::Document);
    cursor.mergeCharFormat(format); // Preserve bold, italic, links and inline objects.
    updateGeometry();
}

void ChatMessageText::changeEvent(QEvent *event)
{
    PsiTextView::changeEvent(event);
    if (event->type() == QEvent::FontChange)
        applyDocumentFont();
    else if (event->type() == QEvent::PaletteChange) {
        // A chat window stylesheet can stop Qt propagating the new palette
        // into an existing QTextEdit viewport, even if no rule matches it.
        // Keep the painted surface/text in sync without rebuilding the document.
        viewport()->setPalette(palette());
        PsiRichText::recolorLinks(document(), palette().color(QPalette::Link));
    }
}

int ChatMessageText::heightForWidth(int width) const
{
    // Layouts also ask about hypothetical widths while computing size hints.
    // Measure those without changing the document currently painted/selected.
    const QSignalBlocker blocker(document()->documentLayout());
    const qreal          previousWidth = document()->textWidth();
    document()->setTextWidth(qMax(1, width));
    const int height = int(std::ceil(document()->documentLayout()->documentSize().height()));
    document()->setTextWidth(previousWidth);
    return qMax(fontMetrics().height(), height);
}

QSize ChatMessageText::sizeHint() const
{
    // QTextEdit's preferred size includes a large scrollable editing area.
    // Rows instead use heightForWidth(), including when width grows again.
    return minimumSizeHint();
}
QSize ChatMessageText::minimumSizeHint() const { return QSize(0, fontMetrics().height()); }

void ChatMessageText::mousePressEvent(QMouseEvent *event)
{
    pressedLink_.clear();
    if (event->button() == Qt::LeftButton && (textInteractionFlags() & Qt::LinksAccessibleByMouse)) {
        pressedLink_ = anchorAt(event->pos());
        pressedLinkPosition_ = event->pos();
    }
    PsiTextView::mousePressEvent(event);
}

void ChatMessageText::mouseReleaseEvent(QMouseEvent *event)
{
    const QString pressedLink = pressedLink_;
    pressedLink_.clear();

    if (!pressedLink.isEmpty()) {
        // PsiTextView's generic QTextEdit release handler may see a temporary
        // text selection on a link, especially in the embedded message
        // timeline. Handle a click explicitly and let an actual drag retain
        // normal QTextEdit selection behavior instead.
        QTextEdit::mouseReleaseEvent(event);
        const bool clicked = event->button() == Qt::LeftButton
            && (textInteractionFlags() & Qt::LinksAccessibleByMouse)
            && (event->pos() - pressedLinkPosition_).manhattanLength() <= QApplication::startDragDistance()
            && anchorAt(event->pos()) == pressedLink;
        if (clicked) {
            URLObject::getInstance()->popupAction(pressedLink);
            event->accept();
        }
        return;
    }
    PsiTextView::mouseReleaseEvent(event);
}

void ChatMessageText::resizeEvent(QResizeEvent *event)
{
    PsiTextView::resizeEvent(event);
    updateGeometry();
}
void ChatMessageText::wheelEvent(QWheelEvent *event)
{
    // The text editor is a scroll area itself. Route wheel input to the enclosing
    // timeline, rather than scrolling a document with hidden scrollbars.
    for (QWidget *parent = parentWidget(); parent; parent = parent->parentWidget()) {
        if (auto *area = qobject_cast<QAbstractScrollArea *>(parent)) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            const QPointF global = event->globalPosition();
#else
            const QPointF global = event->globalPosF();
#endif
            QWheelEvent forwarded(QPointF(area->viewport()->mapFromGlobal(global.toPoint())), global,
                                  event->pixelDelta(), event->angleDelta(), event->buttons(), event->modifiers(),
                                  event->phase(), event->inverted(), event->source());
            QCoreApplication::sendEvent(area->viewport(), &forwarded);
            event->accept();
            return;
        }
    }
    event->ignore();
}

QMimeData *ChatMessageText::createMimeDataFromSelection() const
{
    QTextDocument selected;
    QTextCursor(&selected).insertFragment(textCursor().selection());
    const QString plain = PsiRichText::convertToPlainText(&selected);
    auto         *data  = new QMimeData;
    data->setText(plain);
    data->setHtml(Qt::convertFromPlainText(plain));
    return data;
}
