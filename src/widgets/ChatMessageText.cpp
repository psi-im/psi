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
class FittingImageHandler final : public QObject, public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)
public:
    using QObject::QObject;

    QSizeF intrinsicSize(QTextDocument *doc, int, const QTextFormat &format) override
    {
        const QTextImageFormat image = format.toImageFormat();
        QSizeF                 size(image.width(), image.height());
        if (size.width() <= 0 || size.height() <= 0) {
            const QImage bitmap  = load(doc, image);
            const QSizeF natural = bitmap.isNull() ? QSizeF(16, 16) : QSizeF(bitmap.size()) / bitmap.devicePixelRatio();
            if (size.width() > 0)
                size.setHeight(natural.width() > 0 ? natural.height() * size.width() / natural.width() : natural.height());
            else if (size.height() > 0)
                size.setWidth(natural.height() > 0 ? natural.width() * size.height() / natural.height() : natural.width());
            else
                size = natural;
        }
        // A negative text width means no limit (natural width); a zero width
        // (a widget not laid out yet) is a limit too.
        if (doc->textWidth() >= 0) {
            const qreal available = qMax<qreal>(1, doc->textWidth() - 2 * doc->documentMargin());
            if (size.width() > available)
                size = QSizeF(available, size.height() * available / size.width());
        }
        return size;
    }

    void drawObject(QPainter *painter, const QRectF &rect, QTextDocument *doc, int, const QTextFormat &format) override
    {
        const QImage bitmap = load(doc, format.toImageFormat());
        if (bitmap.isNull())
            return;
        painter->save();
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        painter->drawImage(rect, bitmap);
        painter->restore();
    }

private:
    static QImage load(QTextDocument *doc, const QTextImageFormat &format)
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
        connect(bar, &QAbstractSlider::valueChanged, this, [bar](int value) {
            if (value != 0)
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
    QTextDocument *doc = measuringDocument();
    doc->setTextWidth(qMax(1, width));
    const int height = int(std::ceil(doc->documentLayout()->documentSize().height()));
    return qMax(fontMetrics().height(), height);
}

int ChatMessageText::naturalWidth() const
{
    if (naturalWidth_ < 0) {
        QTextDocument *doc = measuringDocument();
        doc->setTextWidth(-1);
        naturalWidth_ = int(std::ceil(doc->idealWidth())) + 1;
    }
    return naturalWidth_;
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

#include "ChatMessageText.moc"
