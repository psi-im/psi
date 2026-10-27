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
#include <QSet>
#include <QSignalBlocker>
#include <QTextDocumentFragment>
#include <QTextOption>
#include <QPainter>
#include <QPixmap>
#include <QTextObjectInterface>
#include <QScrollBar>
#include <QWheelEvent>
#include <cmath>

namespace {
// Draws images as Qt does, but never wider than the text: a picture wider than
// the message (a narrow window, or a speech bubble) is scaled down to fit,
// instead of overflowing a view that has no horizontal scrolling. The size is
// derived from the document's current text width, so heightForWidth() and
// painting agree for any width.
const char *const MaximumImageHeight = "_psiMaximumImageHeight";

QImage loadImage(QTextDocument *doc, const QTextImageFormat &format)
{
    const QVariant data = doc->resource(QTextDocument::ImageResource, QUrl(format.name()));
    if (data.userType() == QMetaType::QImage)
        return qvariant_cast<QImage>(data);
    if (data.userType() == QMetaType::QPixmap)
        return qvariant_cast<QPixmap>(data).toImage();
    if (data.userType() == QMetaType::QByteArray)
        return QImage::fromData(data.toByteArray());
    return {};
}

// An image's size in a text that is `available` wide (negative: no limit),
// at most `maximumHeight` high (0: no limit), keeping its aspect ratio.
QSizeF fittedImageSize(QTextDocument *doc, const QTextImageFormat &image, qreal available, qreal maximumHeight)
{
    QSizeF size(image.width(), image.height());
    if (size.width() <= 0 || size.height() <= 0) {
        const QImage bitmap  = loadImage(doc, image);
        const QSizeF natural = bitmap.isNull() ? QSizeF(16, 16) : QSizeF(bitmap.size()) / bitmap.devicePixelRatio();
        if (size.width() > 0)
            size.setHeight(natural.width() > 0 ? natural.height() * size.width() / natural.width() : natural.height());
        else if (size.height() > 0)
            size.setWidth(natural.height() > 0 ? natural.width() * size.height() / natural.height() : natural.width());
        else
            size = natural;
    }
    if (available >= 0 && size.width() > available)
        size = QSizeF(available, size.height() * available / size.width());
    if (maximumHeight > 0 && size.height() > maximumHeight)
        size = QSizeF(size.width() * maximumHeight / size.height(), maximumHeight);
    return size;
}

class FittingImageHandler final : public QObject, public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)
public:
    using QObject::QObject;

    QSizeF intrinsicSize(QTextDocument *doc, int, const QTextFormat &format) override
    {
        // A negative text width means no limit (natural width); a zero width
        // (a widget not laid out yet) is a limit too.
        const qreal available
            = doc->textWidth() >= 0 ? qMax<qreal>(1, doc->textWidth() - 2 * doc->documentMargin()) : -1.0;
        return fittedImageSize(doc, format.toImageFormat(), available, doc->property(MaximumImageHeight).toReal());
    }

    void drawObject(QPainter *painter, const QRectF &rect, QTextDocument *doc, int, const QTextFormat &format) override
    {
        const QImage bitmap = loadImage(doc, format.toImageFormat());
        if (bitmap.isNull())
            return;
        painter->save();
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        painter->drawImage(rect, bitmap);
        painter->restore();
    }
};
}

ChatMessageText::ChatMessageText(QWidget *parent) : PsiTextView(parent)
{
    setReadOnly(true);
    setKeepAtBottom(false); // The outer timeline owns scrolling and viewport anchoring.
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setContentsMargins(0, 0, 0, 0);
    viewport()->setAutoFillBackground(false);
    // The surrounding timeline scrolls; this view always shows its whole
    // document from the top left. QTextEdit can keep stale scroll ranges and
    // offsets: after a layout at width 0 that scrolled to the end, Qt's
    // layout-loop guard in QTextEdit's relayoutDocument() may skip adjusting
    // the scroll bars when the real width arrives together with a height that
    // fits the document, leaving the content scrolled out of view until the
    // next width change.
    for (QScrollBar *bar : { horizontalScrollBar(), verticalScrollBar() })
        connect(bar, &QAbstractSlider::valueChanged, this, [this, bar](int value) {
            if (value != 0 && !(scrollable_ && bar == verticalScrollBar()))
                bar->setValue(0);
        });
    static auto *images = new FittingImageHandler(qApp);
    document()->documentLayout()->registerHandler(QTextFormat::ImageObject, images);
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
    connect(document(), &QTextDocument::contentsChanged, this, [this] { resetMeasuring(); });
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
    resetMeasuring();
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

QTextDocument *ChatMessageText::measuringDocument() const
{
    if (!measuring_) {
        measuring_.reset(document()->clone());
        // clone() keeps the root frame's margins but not this setting.
        measuring_->setDocumentMargin(document()->documentMargin());
        measuring_->setUseDesignMetrics(document()->useDesignMetrics());
        measuring_->setProperty(MaximumImageHeight, document()->property(MaximumImageHeight));
        // The same object handlers, so images, icons and players measure as
        // they are drawn.
        QSet<int> types { QTextFormat::ImageObject };
        for (const QTextFormat &format : document()->allFormats())
            if (format.isCharFormat() && format.objectType() != QTextFormat::NoObject)
                types.insert(format.objectType());
        QAbstractTextDocumentLayout *shown = document()->documentLayout();
        for (int type : std::as_const(types))
            if (auto *handler = dynamic_cast<QObject *>(shown->handlerForObject(type)))
                measuring_->documentLayout()->registerHandler(type, handler);
    }
    return measuring_.get();
}

void ChatMessageText::resetMeasuring() const
{
    measuring_.reset();
    naturalWidth_ = -1;
}

int ChatMessageText::heightForWidth(int width) const
{
    // Layouts also ask about hypothetical widths while computing size hints.
    // Measure a copy, never the document QTextEdit lays out and paints.
    // A style sheet border or padding sits between the widget and the text.
    const QMargins chrome = textChrome();
    QTextDocument *doc    = measuringDocument();
    doc->setTextWidth(qMax(1, width - chrome.left() - chrome.right()));
    const int height = int(std::ceil(doc->documentLayout()->documentSize().height()));
    return qMax(fontMetrics().height(), height) + chrome.top() + chrome.bottom();
}

QMargins ChatMessageText::textChrome() const
{
    // QFrame keeps its frame, including a style sheet's border and padding,
    // in the contents margins.
    return contentsMargins() + viewportMargins();
}

int ChatMessageText::naturalWidth() const
{
    if (naturalWidth_ < 0) {
        QTextDocument *doc = measuringDocument();
        doc->setTextWidth(-1);
        naturalWidth_ = int(std::ceil(doc->idealWidth())) + 1;
    }
    const QMargins chrome = textChrome();
    return naturalWidth_ + chrome.left() + chrome.right();
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
        if (!linkHandler_ || !linkHandler_(pressedLink))
            URLObject::getInstance()->popupAction(pressedLink);
        event->accept();
    }
}

void ChatMessageText::resizeEvent(QResizeEvent *event)
{
    PsiTextView::resizeEvent(event);
    updateGeometry();
}
void ChatMessageText::setPlainMessage(const QString &text)
{
    hasMediaMarkup_ = false;
    document()->setPlainText(text);
    clearQuoteFormatting();
    applyDocumentFont();
}

void ChatMessageText::setMaximumImageHeight(int height)
{
    height = qMax(0, height);
    if (document()->property(MaximumImageHeight).toInt() == height)
        return;
    document()->setProperty(MaximumImageHeight, height);
    resetMeasuring();
    document()->markContentsDirty(0, document()->characterCount());
    updateGeometry();
}

bool ChatMessageText::hasImagesTallerThan(int height, int width) const
{
    const QMargins chrome    = textChrome();
    const qreal    available = qMax(1, width - chrome.left() - chrome.right()) - 2 * document()->documentMargin();
    for (auto block = document()->begin(); block.isValid(); block = block.next())
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextCharFormat format = it.fragment().charFormat();
            if (format.isImageFormat()
                && fittedImageSize(document(), format.toImageFormat(), qMax<qreal>(1, available), 0).height() > height)
                return true;
        }
    return false;
}

void ChatMessageText::setScrollable(bool scrollable)
{
    scrollable_ = scrollable;
    setVerticalScrollBarPolicy(scrollable ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    if (!scrollable)
        verticalScrollBar()->setValue(0);
}

void ChatMessageText::removeQuotedLines()
{
    // Lines are separated by paragraphs or by line breaks within one. Remove
    // each line that starts with '>' together with the break before it (or
    // after it, for the first line), then trim blank lines at either end.
    QTextDocument *doc = document();
    const auto     isBreak
        = [](QChar c) { return c == QChar::ParagraphSeparator || c == QChar::LineSeparator || c == QLatin1Char('\n'); };
    QList<QPair<int, int>> lines;
    const int              end = doc->characterCount() - 1; // The final paragraph separator is not removable.
    for (int start = 0; start <= end;) {
        int stop = start;
        while (stop < end && !isBreak(doc->characterAt(stop)))
            ++stop;
        lines.append({ start, stop });
        start = stop + 1;
    }
    const auto quoted = [doc](const QPair<int, int> &line) {
        for (int at = line.first; at < line.second; ++at) {
            const QChar c = doc->characterAt(at);
            if (c == QLatin1Char('>'))
                return true;
            if (!c.isSpace())
                return false;
        }
        return false;
    };
    const auto blank = [doc](const QPair<int, int> &line) {
        for (int at = line.first; at < line.second; ++at)
            if (!doc->characterAt(at).isSpace())
                return false;
        return true;
    };
    QVector<bool> remove(lines.size());
    for (int i = 0; i < lines.size(); ++i)
        remove[i] = quoted(lines.at(i));
    for (int i = 0; i < lines.size() && (remove.at(i) || blank(lines.at(i))); ++i)
        remove[i] = true;
    for (int i = lines.size() - 1; i >= 0 && (remove.at(i) || blank(lines.at(i))); --i)
        remove[i] = true;
    if (!remove.contains(true))
        return;
    QTextCursor cursor(doc);
    cursor.beginEditBlock();
    for (int i = lines.size() - 1; i >= 0; --i) {
        if (!remove.at(i))
            continue;
        const auto line = lines.at(i);
        // Take the break before the line, or the one after the first line.
        const int from = i > 0 ? line.first - 1 : line.first;
        const int to   = i > 0 ? line.second : qMin(line.second + 1, end);
        cursor.setPosition(from);
        cursor.setPosition(to, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    }
    cursor.endEditBlock();
}

void ChatMessageText::wheelEvent(QWheelEvent *event)
{
    if (scrollable_) {
        PsiTextView::wheelEvent(event);
        return;
    }
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
                                  event->phase(), event->inverted(), event->source()
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                                  , event->pointingDevice()
#endif
            );
            QCoreApplication::sendEvent(area->viewport(), &forwarded);
            event->accept();
            return;
        }
    }
    event->ignore();
}


#include "ChatMessageText.moc"
