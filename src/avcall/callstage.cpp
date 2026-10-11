// SPDX-License-Identifier: GPL-2.0-or-later
#include "callstage.h"

#include <QAbstractButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace {
const QSize PreviewSize(192, 120);

// Covers a preview: shows its title and puts the tile on the main stage when clicked.
class PreviewOverlay : public QAbstractButton {
public:
    explicit PreviewOverlay(QWidget *parent) : QAbstractButton(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF frame = QRectF(rect()).adjusted(1, 1, -1, -1);
        if (underMouse() || hasFocus()) {
            painter.setPen(QPen(QColor("#93c5fd"), 2));
            painter.drawRoundedRect(frame, 10, 10);
        }
        const QFontMetrics metrics(font());
        const QString      title = metrics.elidedText(text(), Qt::ElideRight, width() - 20);
        QRectF caption(8, height() - metrics.height() - 12, metrics.horizontalAdvance(title) + 12,
                       metrics.height() + 4);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(17, 24, 39, 200));
        painter.drawRoundedRect(caption, 6, 6);
        painter.setPen(QColor("#f3f4f6"));
        painter.drawText(caption, Qt::AlignCenter, title);
    }
};
}

class CallStage::TileFrame : public QFrame {
public:
    TileFrame(QWidget *content, const QString &title, int priority, QWidget *parent) :
        QFrame(parent), content(content), priority(priority)
    {
        setObjectName(QStringLiteral("callTile"));
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        content->setParent(this);
        content->setMinimumSize(0, 0);
        content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        content->show();
        layout->addWidget(content);
        overlay = new PreviewOverlay(this);
        overlay->setText(title);
        overlay->setAccessibleName(tr("Show %1 on the main stage").arg(title));
        overlay->hide();
        hide();
    }

    void setPreview(bool preview)
    {
        if (preview) {
            setFixedSize(PreviewSize);
            overlay->setGeometry(rect());
            overlay->show();
            overlay->raise();
        } else {
            overlay->hide();
            setMinimumSize(0, 0);
            setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        }
    }

    QWidget         *content;
    QAbstractButton *overlay;
    int              priority;
    bool             visible = false;

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QFrame::resizeEvent(event);
        overlay->setGeometry(rect());
    }
};

CallStage::CallStage(QWidget *parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("callStage"));
    auto layout = new QHBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(12, 12, 12, 12);
    mainLayout_ = new QVBoxLayout;
    mainLayout_->setContentsMargins(0, 0, 0, 0);
    placeholder_ = new QLabel(this);
    placeholder_->setAlignment(Qt::AlignCenter);
    placeholder_->setStyleSheet(QStringLiteral("color: #aab6c8; font-size: 18px;"));
    mainLayout_->addWidget(placeholder_);
    layout->addLayout(mainLayout_, 1);
    side_       = new QWidget(this);
    sideLayout_ = new QVBoxLayout(side_);
    sideLayout_->setContentsMargins(0, 0, 0, 0);
    sideLayout_->setSpacing(12);
    sideLayout_->addStretch();
    side_->hide();
    layout->addWidget(side_);
}

void CallStage::addTile(QWidget *content, const QString &title, int priority)
{
    auto frame = new TileFrame(content, title, priority, this);
    connect(frame->overlay, &QAbstractButton::clicked, this, [this, content] { setMainTile(content); });
    tiles_.append(frame);
}

void CallStage::removeTile(QWidget *content)
{
    auto frame = frameOf(content);
    if (!frame)
        return;
    tiles_.removeOne(frame);
    mainLayout_->removeWidget(frame);
    sideLayout_->removeWidget(frame);
    delete frame;
    updateLayout();
}

void CallStage::setTileVisible(QWidget *content, bool visible)
{
    auto frame = frameOf(content);
    if (!frame || frame->visible == visible)
        return;
    frame->visible = visible;
    updateLayout();
}

bool CallStage::isTileVisible(QWidget *content) const
{
    auto frame = frameOf(content);
    return frame && frame->visible;
}

void CallStage::hideAllTiles()
{
    for (auto frame : std::as_const(tiles_))
        frame->visible = false;
    updateLayout();
}

QWidget *CallStage::mainTile() const { return main_; }

void CallStage::setMainTile(QWidget *content)
{
    if (!isTileVisible(content))
        return;
    pinned_ = content;
    updateLayout();
}

void CallStage::setPlaceholderText(const QString &text) { placeholder_->setText(text); }

CallStage::TileFrame *CallStage::frameOf(QWidget *content) const
{
    for (auto frame : tiles_)
        if (frame->content == content)
            return frame;
    return nullptr;
}

void CallStage::updateLayout()
{
    if (pinned_ && !isTileVisible(pinned_))
        pinned_ = nullptr;
    TileFrame *selected = frameOf(pinned_);
    if (!selected) {
        for (auto frame : std::as_const(tiles_))
            if (frame->visible && (!selected || frame->priority > selected->priority))
                selected = frame;
    }

    for (auto frame : std::as_const(tiles_)) {
        mainLayout_->removeWidget(frame);
        sideLayout_->removeWidget(frame);
    }
    bool previews = false;
    for (auto frame : std::as_const(tiles_)) {
        if (!frame->visible) {
            frame->hide();
            continue;
        }
        frame->setPreview(frame != selected);
        if (frame == selected) {
            mainLayout_->addWidget(frame);
        } else {
            sideLayout_->insertWidget(sideLayout_->count() - 1, frame);
            previews = true;
        }
        frame->show();
    }
    placeholder_->setVisible(!selected);
    side_->setVisible(previews);

    QWidget *content = selected ? selected->content : nullptr;
    if (main_ != content) {
        main_ = content;
        emit mainTileChanged(content);
    }
}
