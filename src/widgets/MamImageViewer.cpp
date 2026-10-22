/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "MamImageViewer.h"

#include <QCursor>
#include <QGuiApplication>
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

MamImageViewer::MamImageViewer(const QImage &image, QWidget *parent)
    : QWidget(parent, Qt::Window), image_(image)
{
    setObjectName(QStringLiteral("mamImageViewer"));
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

qreal MamImageViewer::autoFitScale() const
{
    if (image_.isNull())
        return 1.0;
    const qreal x = qreal(qMax(1, width() - 2 * Margin)) / image_.width();
    const qreal y = qreal(qMax(1, height() - 2 * Margin)) / image_.height();
    return qMin(qreal(1.0), qMin(x, y)); // Never enlarge a small image automatically.
}

qreal MamImageViewer::currentScale() const
{
    return temporaryZoom_ ? temporaryScale_ : autoFitScale();
}

QRectF MamImageViewer::displayedImageRect() const
{
    if (image_.isNull())
        return {};
    const QSizeF size(image_.width() * currentScale(), image_.height() * currentScale());
    return QRectF(QPointF((width() - size.width()) / 2, (height() - size.height()) / 2), size);
}

void MamImageViewer::paintEvent(QPaintEvent *)
{
    if (image_.isNull())
        return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(displayedImageRect(), image_);
}

void MamImageViewer::wheelEvent(QWheelEvent *event)
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
    update();
    event->accept();
}

void MamImageViewer::resetTemporaryZoom()
{
    if (!temporaryZoom_)
        return;
    temporaryZoom_  = false;
    temporaryScale_ = 1.0;
    update();
}

void MamImageViewer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    resetTemporaryZoom();
}

void MamImageViewer::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
    resetTemporaryZoom();
}

void MamImageViewer::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange)
        resetTemporaryZoom();
}
