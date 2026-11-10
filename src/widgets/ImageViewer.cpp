/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ImageViewer.h"

#include <QCursor>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScreen>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace {
constexpr int Margin = 16;
}

ImageViewer::ImageViewer(const QImage &image, QWidget *parent)
    : QWidget(parent, Qt::Window), image_(image)
{
    setObjectName(QStringLiteral("imageViewer"));
    setWindowTitle(tr("Image"));
    setAttribute(Qt::WA_DeleteOnClose);
    setAutoFillBackground(true);
    setMinimumSize(200, 160);

    const QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QSize available = screen ? screen->availableGeometry().size() : QSize(1024, 768);
    const QSize cap(qMax(minimumWidth(), qRound(available.width() * 0.90)),
                    qMax(minimumHeight(), qRound(available.height() * 0.90)));
    resize(qBound(minimumWidth(), image_.width() + 2 * Margin, cap.width()),
           qBound(minimumHeight(), image_.height() + 2 * Margin, cap.height()));
}

qreal ImageViewer::autoFitScale() const
{
    if (image_.isNull())
        return 1.0;
    const qreal x = qreal(qMax(1, width() - 2 * Margin)) / image_.width();
    const qreal y = qreal(qMax(1, height() - 2 * Margin)) / image_.height();
    return qMin(qreal(1.0), qMin(x, y)); // Never enlarge a small image automatically.
}

qreal ImageViewer::currentScale() const
{
    return temporaryZoom_ ? temporaryScale_ : autoFitScale();
}

bool ImageViewer::canPan() const
{
    if (image_.isNull())
        return false;
    return image_.width() * currentScale() > width() || image_.height() * currentScale() > height();
}

QPointF ImageViewer::constrainedPanOffset(const QPointF &offset) const
{
    // Never expose empty space beyond an oversized image. On a fitting axis,
    // keep the image centred even if the other axis is scrollable.
    const qreal maxX = qMax(qreal(0), (image_.width() * currentScale() - width()) / 2);
    const qreal maxY = qMax(qreal(0), (image_.height() * currentScale() - height()) / 2);
    return QPointF(qBound(-maxX, offset.x(), maxX), qBound(-maxY, offset.y(), maxY));
}

void ImageViewer::updatePanCursor()
{
    if (!canPan()) {
        dragging_ = false;
        unsetCursor();
    } else {
        setCursor(dragging_ ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
    }
}

QRectF ImageViewer::displayedImageRect() const
{
    if (image_.isNull())
        return {};
    const QSizeF size(image_.width() * currentScale(), image_.height() * currentScale());
    return QRectF(QPointF((width() - size.width()) / 2, (height() - size.height()) / 2) + panOffset_, size);
}

void ImageViewer::paintEvent(QPaintEvent *)
{
    if (image_.isNull())
        return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(displayedImageRect(), image_);
}

void ImageViewer::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && canPan() && displayedImageRect().contains(event->pos())) {
        dragging_ = true;
        lastDragPosition_ = event->pos();
        updatePanCursor();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ImageViewer::mouseMoveEvent(QMouseEvent *event)
{
    if (dragging_) {
        const QPoint position = event->pos();
        const QPointF offset = constrainedPanOffset(panOffset_ + position - lastDragPosition_);
        lastDragPosition_ = position;
        if (offset != panOffset_) {
            panOffset_ = offset;
            update();
        }
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void ImageViewer::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        updatePanCursor();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ImageViewer::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    const int pixels = event->pixelDelta().y();
    if (!delta && !pixels) {
        event->ignore();
        return;
    }
    // High-resolution trackpads and traditional wheel notches both adjust
    // the current scale. Zoom can cross the native 100% boundary and the fit.
    const qreal turns = delta ? qreal(delta) / 120.0 : qreal(pixels) / 120.0;
    temporaryScale_ = std::max(0.02, std::min(32.0, currentScale() * std::pow(1.2, turns)));
    temporaryZoom_  = true;
    panOffset_ = constrainedPanOffset(panOffset_);
    updatePanCursor();
    update();
    event->accept();
}

void ImageViewer::resetTemporaryZoom()
{
    if (!temporaryZoom_ && panOffset_.isNull() && !dragging_)
        return;
    temporaryZoom_  = false;
    temporaryScale_ = 1.0;
    panOffset_ = {};
    dragging_ = false;
    updatePanCursor();
    update();
}

void ImageViewer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    resetTemporaryZoom();
}

void ImageViewer::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
    resetTemporaryZoom();
}

void ImageViewer::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange)
        resetTemporaryZoom();
}
