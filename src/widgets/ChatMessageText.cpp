/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ChatMessageText.h"
#include "psirichtext.h"
#include "urlobject.h"

#include <QAbstractTextDocumentLayout>
#include <QFontMetrics>
#include <QImage>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextFragment>
#include <QTextImageFormat>
#include <QTimer>
#include <QUrl>
#include <QVector>
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
    // Cached SIMS media is present at parse time; uncached images are inserted
    // later by PsiTextView after their downloads finish. Normalize both paths.
    connect(document(), &QTextDocument::contentsChanged, this, [this] {
        if (!hasMediaMarkup_ || isolatingMedia_ || mediaLayoutScheduled_)
            return;
        mediaLayoutScheduled_ = true;
        QTimer::singleShot(0, this, [this] {
            mediaLayoutScheduled_ = false;
            isolateLargeMedia();
        });
    });
}

void ChatMessageText::setRichText(const QString &text)
{
    hasMediaMarkup_ = text.contains(QLatin1String("<share"))
        || text.contains(QLatin1String("<img"))
        || text.contains(QLatin1String("<icon"));
    isolatingMedia_ = true;
    document()->setDefaultFont(font());
    PsiTextView::setRichText(text);
    applyDocumentFont();
    PsiRichText::recolorLinks(document(), palette().color(QPalette::Link));
    isolatingMedia_ = false;
    isolateLargeMedia();
}

// Postprocess the *rendered* QTextDocument, not its HTML source: native SIMS
// images, embedded players and <icon> objects have already received their
// actual QTextCharFormats from PsiRichText's object parsers. A media object's
// intrinsic height is compared to the text line height, so ordinary smileys
// remain inline, but large media never shares a line with a short caption.
//
// Iterate backwards to keep positions stable when inserting paragraph breaks.
// No image/link/object formats are recreated, and normal text selection,
// image-copy and per-account SIMS resources remain attached to the same object.
void ChatMessageText::isolateLargeMedia()
{
    if (!hasMediaMarkup_ || isolatingMedia_)
        return;

    QTextDocument *doc = document();
    const qreal threshold = 2.0 * QFontMetricsF(font()).height();
    QVector<int> mediaPositions;

    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid())
                continue;
            const QTextCharFormat format = fragment.charFormat();
            if (format.objectType() == QTextFormat::NoObject)
                continue;

            const QString fragmentText = fragment.text();
            for (int offset = 0; offset < fragmentText.size(); ++offset) {
                if (fragmentText.at(offset) != QChar::ObjectReplacementCharacter)
                    continue;

                const int position = fragment.position() + offset;
                qreal height = 0;
                if (format.isImageFormat()) {
                    const QTextImageFormat image = format.toImageFormat();
                    height = image.height();
                    if (height <= 0) {
                        const auto resource = doc->resource(QTextDocument::ImageResource, QUrl(image.name()));
                        const auto bitmap = resource.value<QImage>();
                        if (!bitmap.isNull()) {
                            height = image.width() > 0 && bitmap.width() > 0
                                ? qreal(bitmap.height()) * image.width() / bitmap.width()
                                : bitmap.height();
                        }
                    }
                } else if (auto *handler = doc->documentLayout()->handlerForObject(format.objectType())) {
                    // handlerForObject already returns QTextObjectInterface* in Qt5/Qt6.
                    height = handler->intrinsicSize(doc, position, format).height();
                }
                if (height > threshold)
                    mediaPositions.append(position);
            }
        }
    }

    if (mediaPositions.isEmpty())
        return;

    isolatingMedia_ = true;
    QTextCursor edit(doc);
    edit.beginEditBlock();
    for (int positionIndex = mediaPositions.size() - 1; positionIndex >= 0; --positionIndex) {
        const int position = mediaPositions.at(positionIndex);
        QTextBlock block = doc->findBlock(position);
        if (!block.isValid())
            continue;
        // Put following text (or another media object) into a new paragraph.
        if (position + 1 < block.position() + block.text().size()) {
            edit.setPosition(position + 1);
            edit.insertBlock();
        }
        // Put preceding text (or another media object) into its own paragraph.
        block = doc->findBlock(position);
        if (position > block.position()) {
            edit.setPosition(position);
            edit.insertBlock();
        }
    }
    edit.endEditBlock();
    isolatingMedia_ = false;
    updateGeometry();
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

    // Use QTextEdit's selection behavior but not PsiTextView's generic link
    // handler. That handler can open an anchor when link interaction is disabled
    // (whole-message selection), and does not reliably distinguish selection
    // state during a click inside the virtualized timeline.
    QTextEdit::mouseReleaseEvent(event);
    const bool clicked = event->button() == Qt::LeftButton
        && !pressedLink.isEmpty()
        && (textInteractionFlags() & Qt::LinksAccessibleByMouse)
        && (event->pos() - pressedLinkPosition_).manhattanLength() <= QApplication::startDragDistance()
        && anchorAt(event->pos()) == pressedLink;
    if (clicked) {
        URLObject::getInstance()->popupAction(pressedLink);
        event->accept();
    }
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
