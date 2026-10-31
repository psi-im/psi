/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "MessagePreview.h"
#include "ChatMessageText.h"
#include "LinkButton.h"
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QTextCursor>
#include <QTextImageFormat>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>

// Ordinary mouse clicks should reach the preview only when its child controls
// have nothing interactive to handle. Text selection and links stay native.
namespace {
class PreviewTitle final : public QLabel {
public:
    explicit PreviewTitle(QWidget *parent) : QLabel(parent) { }
    void setNavigateOnClick(bool enabled) { navigateOnClick_ = enabled; }
protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        QLabel::mouseReleaseEvent(event);
        if (navigateOnClick_ && event->button() == Qt::LeftButton && event->modifiers() == Qt::NoModifier
            && !hasSelectedText())
            event->ignore(); // An unselected ordinary click propagates to the frame.
    }
private:
    bool navigateOnClick_ = false;
};

class PreviewText final : public ChatMessageText {
public:
    explicit PreviewText(QWidget *parent) : ChatMessageText(parent) { }
    void setNavigateOnClick(bool enabled) { navigateOnClick_ = enabled; }
protected:
    bool imageAtPoint(const QPoint &point) const
    {
        for (auto block = document()->begin(); block.isValid(); block = block.next()) {
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                const auto fragment = it.fragment();
                if (!fragment.isValid() || !fragment.charFormat().isImageFormat())
                    continue;
                const QTextImageFormat format = fragment.charFormat().toImageFormat();
                QTextCursor cursor(document());
                cursor.setPosition(fragment.position());
                const QSize size(qRound(format.width()), qRound(format.height()));
                if (size.isValid() && QRect(cursorRect(cursor).topLeft(), size).contains(point))
                    return true;
            }
        }
        return false;
    }
    void mousePressEvent(QMouseEvent *event) override
    {
        ordinaryPress_ = navigateOnClick_ && event->button() == Qt::LeftButton
            && event->modifiers() == Qt::NoModifier && anchorAt(event->pos()).isEmpty()
            && !imageAtPoint(event->pos());
        pressPos_ = event->pos();
        ChatMessageText::mousePressEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        const bool unclaimed = ordinaryPress_ && event->button() == Qt::LeftButton
            && event->modifiers() == Qt::NoModifier
            && (event->pos() - pressPos_).manhattanLength() <= QApplication::startDragDistance()
            && anchorAt(event->pos()).isEmpty() && !imageAtPoint(event->pos());
        ordinaryPress_ = false;
        ChatMessageText::mouseReleaseEvent(event);
        if (unclaimed && !textCursor().hasSelection())
            event->ignore(); // Only then can Qt propagate to MessagePreview.
    }
private:
    bool navigateOnClick_ = false;
    bool ordinaryPress_ = false;
    QPoint pressPos_;
};
}

MessagePreview::MessagePreview(const QColor &barColor, const QColor &textColor, QWidget *parent) :
    QFrame(parent), barColor_(barColor), textColor_(textColor)
{
    setObjectName(QStringLiteral("messagePreview"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 1, 0, 1);
    layout->setSpacing(7);
    bar_ = new QFrame(this);
    bar_->setObjectName(QStringLiteral("previewBar"));
    bar_->setFixedWidth(3);
    bar_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    bar_->setBackgroundRole(QPalette::Window);
    bar_->setAutoFillBackground(true);
    bar_->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(bar_);
    column_ = new QVBoxLayout;
    column_->setContentsMargins(0, 0, 0, 0);
    column_->setSpacing(0);
    layout->addLayout(column_, 1);
    auto *header = new QHBoxLayout;
    title_       = new PreviewTitle(this);
    title_->setObjectName(QStringLiteral("previewTitle"));
    title_->setTextFormat(Qt::PlainText);
    title_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    QFont titleFont = font();
    titleFont.setBold(true);
    title_->setFont(titleFont);
    header->addWidget(title_, 1);
    close_ = new QPushButton(QString::fromUtf8("×"), this);
    close_->setObjectName(QStringLiteral("previewDismiss"));
    close_->setFlat(true);
    close_->setAutoDefault(false);
    close_->setFixedSize(24, 24);
    close_->setToolTip(tr("Cancel reply"));
    close_->hide();
    header->addWidget(close_);
    connect(close_, &QPushButton::clicked, this, &MessagePreview::dismissed);
    column_->addLayout(header);
    // The same view as a message's text: rich text, links, emoticons and media.
    text_ = new PreviewText(this);
    text_->setObjectName(QStringLiteral("previewText"));
    text_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); // updatePreview() sets the height.
    // Media that arrives later (a downloaded image) changes the height.
    connect(text_->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this, [this] {
        if (updateScheduled_)
            return;
        updateScheduled_ = true;
        QTimer::singleShot(0, this, [this] {
            updateScheduled_ = false;
            updatePreview();
        });
    });
    column_->addWidget(text_);
    // Track the press for drag detection, but never activate a preview before
    // the child has had a chance to consume its mouse release.
    title_->installEventFilter(this);
    text_->viewport()->installEventFilter(this);
    toggle_ = new LinkButton(this);
    toggle_->setObjectName(QStringLiteral("previewShowMore"));
    column_->addWidget(toggle_, 0, Qt::AlignLeft);
    connect(toggle_, &LinkButton::clicked, this, [this] {
        expanded_ = !expanded_;
        updatePreview();
    });
    refreshColors();
    updatePreview();
}

void MessagePreview::setColors(const QColor &barColor, const QColor &textColor)
{
    barColor_ = barColor;
    textColor_ = textColor;
    refreshColors();
}

void MessagePreview::setLinkColor(const QColor &color) { toggle_->setLinkColor(color); }

void MessagePreview::refreshColors()
{
    if (!title_ || !text_ || !bar_)
        return;
    // Only resolve the roles we own. Copying the resolved parent palette here
    // would freeze normal text/selection colors when the desktop theme changes.
    // Start from the inherited chat palette. A fresh default QPalette would
    // force black text whenever no explicit preview text color is supplied.
    QPalette foreground = palette();
    if (textColor_.isValid()) {
        foreground.setColor(QPalette::Text, textColor_);
        foreground.setColor(QPalette::WindowText, textColor_);
    }
    title_->setPalette(foreground);
    text_->setPalette(foreground);
    QPalette barPalette;
    if (barColor_.isValid())
        barPalette.setColor(QPalette::Window, barColor_);
    bar_->setPalette(barPalette);
}

void MessagePreview::setContent(const QString &title, const QString &text)
{
    if (richText_.isNull() && title_->text() == title && text_->toPlainText() == text)
        return;
    richText_.clear();
    setTitle(title);
    text_->setPlainMessage(text);
    updatePreview();
}

void MessagePreview::setRichContent(const QString &title, const QString &html, bool withoutQuotedLines)
{
    // Keep the expanded state and selection when the same content comes again.
    const QString key = (withoutQuotedLines ? QStringLiteral("q") : QStringLiteral("r")) + html;
    if (richText_ == key && title_->text() == title)
        return;
    richText_ = key;
    setTitle(title);
    text_->setRichText(html);
    if (withoutQuotedLines)
        text_->removeQuotedLines();
    updatePreview();
}

void MessagePreview::setMediaOpener(FileSharingDeviceOpener *opener) { text_->setMediaOpener(opener); }

void MessagePreview::setTitle(const QString &title)
{
    expanded_ = false;
    title_->setText(title);
    // No placeholder heading: without a title the header row collapses
    // (unless it still holds the dismiss button).
    title_->setVisible(!title.isEmpty());
}

void MessagePreview::setClosable(bool enabled) { close_->setVisible(enabled); }

void MessagePreview::setNavigationEnabled(bool enabled)
{
    if (navigationEnabled_ == enabled)
        return;
    navigationEnabled_ = enabled;
    pressedTarget_ = nullptr;
    static_cast<PreviewTitle *>(title_)->setNavigateOnClick(enabled);
    static_cast<PreviewText *>(text_)->setNavigateOnClick(enabled);
    if (enabled) {
        setCursor(Qt::PointingHandCursor);
        title_->setCursor(Qt::PointingHandCursor);
    } else {
        unsetCursor();
        title_->unsetCursor();
    }
}

bool MessagePreview::eventFilter(QObject *target, QEvent *event)
{
    if (!navigationEnabled_ || (target != title_ && target != text_->viewport()))
        return QFrame::eventFilter(target, event);
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        pressedTarget_ = nullptr;
        if (mouse->button() == Qt::LeftButton && mouse->modifiers() == Qt::NoModifier
            && (target != text_->viewport() || text_->anchorAt(mouse->pos()).isEmpty())) {
            pressedTarget_ = target;
            pressedPosition_ = static_cast<QWidget *>(target)->mapTo(this, mouse->pos());
        }
    } else if (event->type() == QEvent::MouseMove && pressedTarget_) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        const QPoint at = static_cast<QWidget *>(target)->mapTo(this, mouse->pos());
        if ((at - pressedPosition_).manhattanLength() > QApplication::startDragDistance())
            pressedTarget_ = nullptr;
    }
    return QFrame::eventFilter(target, event);
}

void MessagePreview::mousePressEvent(QMouseEvent *event)
{
    if (navigationEnabled_ && event->button() == Qt::LeftButton && event->modifiers() == Qt::NoModifier
        && !pressedTarget_) {
        const QWidget *hit = childAt(event->pos());
        if (!hit || hit == bar_) {
            pressedTarget_ = this;
            pressedPosition_ = event->pos();
        }
    }
    QFrame::mousePressEvent(event);
}

void MessagePreview::mouseReleaseEvent(QMouseEvent *event)
{
    const bool activate = navigationEnabled_ && pressedTarget_ && event->button() == Qt::LeftButton
        && event->modifiers() == Qt::NoModifier && rect().contains(event->pos())
        && (event->pos() - pressedPosition_).manhattanLength() <= QApplication::startDragDistance()
        && !text_->textCursor().hasSelection() && !title_->hasSelectedText();
    pressedTarget_ = nullptr;
    if (activate) {
        emit activated();
        event->accept();
        return;
    }
    QFrame::mouseReleaseEvent(event);
}

void MessagePreview::setFooter(QWidget *widget)
{
    if (footer_ == widget)
        return;
    if (footer_)
        delete footer_;
    footer_ = widget;
    if (footer_) {
        footer_->setParent(this);
        column_->addWidget(footer_);
    }
    updateGeometry();
    emit heightChanged();
}

int MessagePreview::naturalWidth() const
{
    int content = 0;
    if (!title_->isHidden() && !title_->text().isEmpty())
        content = QFontMetrics(title_->font()).horizontalAdvance(title_->text())
            + (close_->isHidden() ? 0 : close_->width() + 6);
    content = qMax(content, text_->naturalWidth());
    if (!toggle_->isHidden())
        content = qMax(content, toggle_->sizeHint().width());
    if (footer_)
        content = qMax(content, footer_->sizeHint().width());
    const QMargins margins = layout()->contentsMargins();
    return margins.left() + margins.right() + bar_->width() + layout()->spacing() + content;
}

void MessagePreview::updatePreview()
{
    const int cap             = qMax(fontMetrics().lineSpacing(), window()->height() / 2);
    const int collapsedHeight = qMin(cap, fontMetrics().lineSpacing() * 3);
    // Collapsed, an image is shown whole at the height of the collapsed text.
    text_->setMaximumImageHeight(expanded_ ? 0 : collapsedHeight);
    const QMargins margins    = layout()->contentsMargins();
    int            width      = qMax(1, this->width() - margins.left() - margins.right() - bar_->width()
                                            - layout()->spacing());
    int            fullHeight = text_->heightForWidth(width);
    if (expanded_ && fullHeight > cap) {
        width      = qMax(1, width - text_->verticalScrollBar()->sizeHint().width());
        fullHeight = qMax(fullHeight, text_->heightForWidth(width));
    }
    const int height = qMin(fullHeight, expanded_ ? cap : collapsedHeight);
    text_->setScrollable(expanded_ && fullHeight > cap);
    toggle_->setText(expanded_ ? tr("Show less") : tr("Show more…"));
    const bool toggleVisible
        = fullHeight > collapsedHeight || (!expanded_ && text_->hasImagesTallerThan(collapsedHeight, width));
    const bool changed       = text_->height() != height || toggle_->isHidden() == toggleVisible;
    text_->setFixedHeight(height);
    toggle_->setVisible(toggleVisible);
    if (changed) {
        updateGeometry();
        emit heightChanged();
    }
}

void MessagePreview::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    updatePreview();
}

void MessagePreview::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event->type() == QEvent::FontChange && title_) {
        QFont titleFont = font();
        titleFont.setBold(true);
        title_->setFont(titleFont);
    }
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        refreshColors();
    if ((event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange) && toggle_)
        updatePreview();
}
