/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "MessagePreview.h"
#include "LinkButton.h"
#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <cmath>

namespace {
class PreviewText : public QTextBrowser {
public:
    using QTextBrowser::QTextBrowser;

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        if (verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff)
            event->ignore();
        else
            QTextBrowser::wheelEvent(event);
    }
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
    text_ = new PreviewText(this);
    text_->setObjectName(QStringLiteral("previewText"));
    text_->setFrameShape(QFrame::NoFrame);
    text_->setReadOnly(true);
    text_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    text_->setOpenLinks(false);
    text_->setOpenExternalLinks(false);
    text_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    text_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    text_->viewport()->setAutoFillBackground(false);
    text_->document()->setDocumentMargin(0);
    connect(text_->verticalScrollBar(), &QScrollBar::valueChanged, text_, [browser = text_](int value) {
        if (value && browser->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff)
            browser->verticalScrollBar()->setValue(0);
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
    if (title_->text() == title && text_->toPlainText() == text)
        return;
    expanded_ = false;
    title_->setText(title);
    text_->setPlainText(text);
    updatePreview();
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

void MessagePreview::updatePreview()
{
    QTextDocument measure;
    measure.setDefaultFont(font());
    measure.setDocumentMargin(0);
    measure.setPlainText(text_->toPlainText());
    const int cap   = qMax(fontMetrics().lineSpacing(), window()->height() / 2);
    int       width = qMax(1, this->width() - 12);
    measure.setTextWidth(width);
    int fullHeight = qMax(fontMetrics().height(), int(std::ceil(measure.size().height())));
    if (expanded_ && fullHeight > cap) {
        width = qMax(1, width - text_->verticalScrollBar()->sizeHint().width());
        measure.setTextWidth(width);
        fullHeight = qMax(fullHeight, int(std::ceil(measure.size().height())));
    }
    const int collapsedHeight = qMin(cap, fontMetrics().lineSpacing() * 3);
    const int height          = qMin(fullHeight, expanded_ ? cap : collapsedHeight);
    text_->setVerticalScrollBarPolicy(expanded_ && fullHeight > cap ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    if (!expanded_)
        text_->verticalScrollBar()->setValue(0);
    toggle_->setText(expanded_ ? tr("Show less") : tr("Show more…"));
    const bool toggleVisible = fullHeight > collapsedHeight;
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
