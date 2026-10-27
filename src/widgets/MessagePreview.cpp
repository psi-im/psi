/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "MessagePreview.h"
#include "ChatMessageText.h"
#include "LinkButton.h"
#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>

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
    title_       = new QLabel(this);
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
    text_ = new ChatMessageText(this);
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
    QPalette foreground;
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
