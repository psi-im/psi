/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "MessagePreview.h"
#include "ChatMessageText.h"
#include "CodeBlockWidget.h"
#include "LinkButton.h"
#include "psirichtext.h"
#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>

namespace {
bool isPlainClick(const QMouseEvent *event, const QPoint &pressPosition)
{
    return event->button() == Qt::LeftButton && event->modifiers() == Qt::NoModifier
        && (event->pos() - pressPosition).manhattanLength() <= QApplication::startDragDistance();
}

// A selectable heading that also reports a plain click on it.
class PreviewTitle final : public QLabel {
    Q_OBJECT
public:
    explicit PreviewTitle(QWidget *parent) : QLabel(parent) { }
signals:
    void plainClicked();

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        pressPosition_ = event->pos();
        QLabel::mousePressEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        QLabel::mouseReleaseEvent(event);
        if (isPlainClick(event, pressPosition_) && !hasSelectedText())
            emit plainClicked();
    }
private:
    QPoint pressPosition_;
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
    auto *title  = new PreviewTitle(this);
    title_       = title;
    title_->setObjectName(QStringLiteral("previewTitle"));
    title_->setTextFormat(Qt::PlainText);
    title_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    QFont titleFont = font();
    titleFont.setBold(true);
    title_->setFont(titleFont);
    connect(title, &PreviewTitle::plainClicked, this, &MessagePreview::handlePlainClick);
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

    clip_ = new QWidget(this);
    clip_->setObjectName(QStringLiteral("previewClip"));
    clip_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); // updatePreview() sets the height.
    column_->addWidget(clip_);
    content_ = new QWidget(clip_);
    content_->setObjectName(QStringLiteral("previewContent"));
    contentLayout_ = new QVBoxLayout(content_);
    contentLayout_->setContentsMargins(0, 0, 0, 0);
    contentLayout_->setSpacing(0);
    // Any part changing its size: media that arrives later, formatted code...
    content_->installEventFilter(this);
    // The same view as a message's text: rich text, links, emoticons and media.
    text_ = new ChatMessageText(content_);
    text_->setObjectName(QStringLiteral("previewText"));
    connect(text_, &ChatMessageText::plainClicked, this, &MessagePreview::handlePlainClick);
    contentLayout_->addWidget(text_);

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

void MessagePreview::scheduleUpdatePreview()
{
    if (updateScheduled_)
        return;
    updateScheduled_ = true;
    QTimer::singleShot(0, this, [this] {
        updateScheduled_ = false;
        updatePreview();
    });
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
    for (auto *prose : extraProse_)
        prose->setPalette(foreground);
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
    clearParts();
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
    rebuildParts(html);
    updatePreview();
}

void MessagePreview::setMediaOpener(FileSharingDeviceOpener *opener) { text_->setMediaOpener(opener); }

void MessagePreview::clearParts()
{
    while (contentLayout_->count() > 1) {
        auto *item = contentLayout_->takeAt(1);
        delete item->widget();
        delete item;
    }
    extraProse_.clear();
    codeBlocks_.clear();
    text_->show();
}

void MessagePreview::addPart(QWidget *part)
{
    contentLayout_->addWidget(part);
    filterChild(part);
}

void MessagePreview::filterChild(QWidget *child)
{
    if (!childFilter_)
        return;
    child->installEventFilter(childFilter_);
    for (auto *descendant : child->findChildren<QWidget *>())
        descendant->installEventFilter(childFilter_);
}

// Fenced code gets the same real CodeBlockWidget as in the timeline. text_
// keeps showing the first prose part; only other parts need a widget of their own.
void MessagePreview::rebuildParts(const QString &html)
{
    clearParts();
    const auto parts = PsiRichText::splitCodeBlocks(text_->document(), html);
    for (int i = 0; i < parts.size(); ++i) {
        const auto &part = parts.at(i);
        if (part.kind == PsiRichText::ContentPart::Code) {
            auto *code = new CodeBlockWidget(content_);
            code->setCode(part.text, part.language);
            codeBlocks_.append(code);
            addPart(code);
        } else if (i == 0) {
            text_->setRichText(part.text);
        } else {
            auto *prose = new ChatMessageText(content_);
            prose->setObjectName(QStringLiteral("previewCodeProse"));
            prose->setPalette(text_->palette());
            prose->setRichText(part.text);
            connect(prose, &ChatMessageText::plainClicked, this, &MessagePreview::handlePlainClick);
            extraProse_.append(prose);
            addPart(prose);
        }
    }
    if (!parts.isEmpty() && parts.first().kind == PsiRichText::ContentPart::Code)
        text_->hide();
}

void MessagePreview::setTitle(const QString &title)
{
    expanded_ = false;
    title_->setText(title);
    // No placeholder heading: without a title the header row collapses
    // (unless it still holds the dismiss button).
    title_->setVisible(!title.isEmpty());
}

void MessagePreview::setClosable(bool enabled) { close_->setVisible(enabled); }

void MessagePreview::setChildEventFilter(QObject *filter)
{
    childFilter_ = filter;
    for (auto *child : findChildren<QWidget *>())
        child->installEventFilter(filter);
}

void MessagePreview::setActivatable(bool activatable)
{
    if (activatable_ == activatable)
        return;
    activatable_ = activatable;
    if (activatable) {
        setCursor(Qt::PointingHandCursor);
        title_->setCursor(Qt::PointingHandCursor);
    } else {
        unsetCursor();
        title_->unsetCursor();
    }
}

void MessagePreview::handlePlainClick()
{
    if (activatable_)
        emit activated();
}

bool MessagePreview::eventFilter(QObject *target, QEvent *event)
{
    if (target == content_ && event->type() == QEvent::LayoutRequest)
        scheduleUpdatePreview();
    return QFrame::eventFilter(target, event);
}

// The card's own surface: the bar and the gaps. Children report their clicks.
void MessagePreview::mousePressEvent(QMouseEvent *event)
{
    surfacePress_ = childAt(event->pos()) ? std::nullopt : std::make_optional(event->pos());
    QFrame::mousePressEvent(event);
}

void MessagePreview::mouseReleaseEvent(QMouseEvent *event)
{
    const bool clicked = surfacePress_ && isPlainClick(event, *surfacePress_);
    surfacePress_.reset();
    QFrame::mouseReleaseEvent(event);
    if (clicked)
        handlePlainClick();
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
        filterChild(footer_);
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
    if (!text_->isHidden())
        content = qMax(content, text_->naturalWidth());
    for (auto *prose : extraProse_)
        content = qMax(content, prose->naturalWidth());
    for (auto *code : codeBlocks_)
        content = qMax(content, code->naturalWidth());
    if (!toggle_->isHidden())
        content = qMax(content, toggle_->sizeHint().width());
    if (footer_)
        content = qMax(content, footer_->sizeHint().width());
    const QMargins margins = layout()->contentsMargins();
    return margins.left() + margins.right() + bar_->width() + layout()->spacing() + content;
}

void MessagePreview::updatePreview()
{
    // Only the collapsed excerpt is clipped. Once expanded, the surrounding
    // timeline owns scrolling; CodeBlockWidget bounds its own source editor.
    const int collapsedHeight = fontMetrics().lineSpacing() * 3;
    // Collapsed, an image is shown whole at the height of the collapsed text.
    text_->setMaximumImageHeight(expanded_ ? 0 : collapsedHeight);
    const QMargins margins = layout()->contentsMargins();
    const int width = qMax(1, this->width() - margins.left() - margins.right() - bar_->width() - layout()->spacing());
    const int fullHeight = contentLayout_->hasHeightForWidth() ? contentLayout_->totalHeightForWidth(width)
                                                               : contentLayout_->totalSizeHint().height();
    const int height = expanded_ ? fullHeight : qMin(fullHeight, collapsedHeight);
    content_->setGeometry(0, 0, width, fullHeight);
    toggle_->setText(expanded_ ? tr("Show less") : tr("Show more…"));
    const bool toggleVisible
        = fullHeight > collapsedHeight || (!expanded_ && text_->hasImagesTallerThan(collapsedHeight, width));
    const bool changed = clip_->height() != height || toggle_->isHidden() == toggleVisible;
    clip_->setFixedHeight(height);
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

#include "MessagePreview.moc"
