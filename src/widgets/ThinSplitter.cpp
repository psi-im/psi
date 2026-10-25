/*
 * Copyright 2026 Sergei Ilinykh
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * Adapted from MatterLeast's ThinSplitter for Psi.
 */
#include "ThinSplitter.h"

#include <QPainter>
#include <QPalette>
#include <QRegion>
#include <QResizeEvent>
#include <QSplitterHandle>

namespace {

class ThinSplitterHandle final : public QSplitterHandle
{
public:
    ThinSplitterHandle(Qt::Orientation orientation, QSplitter *parent) :
        QSplitterHandle(orientation, parent)
    {
        // Qt's tiny-handle mode normally grows its hit target on *both*
        // sides. Preserve the preceding pane by using asymmetric margins.
        setAttribute(Qt::WA_MouseNoMask, true);
        updateInteractionMargins();
    }

    QSize sizeHint() const override
    {
        // The additional hit area overlays the following pane; it does not
        // consume extra layout space.
        return QSize(ThinSplitter::VisibleHandleExtent, ThinSplitter::VisibleHandleExtent);
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        updateInteractionMargins();
        setMask(QRegion(contentsRect()));
        // QSplitterHandle::resizeEvent() would restore symmetric margins.
        QWidget::resizeEvent(event);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(contentsRect(), palette().color(QPalette::Mid));
    }

private:
    void updateInteractionMargins()
    {
        if (orientation() == Qt::Horizontal) {
            if (splitter() && splitter()->isRightToLeft())
                setContentsMargins(ThinSplitter::TrailingHitPadding, 0, 0, 0);
            else
                setContentsMargins(0, 0, ThinSplitter::TrailingHitPadding, 0);
        } else {
            setContentsMargins(0, 0, 0, ThinSplitter::TrailingHitPadding);
        }
    }
};

} // namespace

ThinSplitter::ThinSplitter(QWidget *parent) : ThinSplitter(Qt::Horizontal, parent) { }

ThinSplitter::ThinSplitter(Qt::Orientation orientation, QWidget *parent) : QSplitter(orientation, parent)
{
    setHandleWidth(VisibleHandleExtent);
}

QSplitterHandle *ThinSplitter::createHandle()
{
    return new ThinSplitterHandle(orientation(), this);
}
