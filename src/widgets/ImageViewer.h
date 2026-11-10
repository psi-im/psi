/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QWidget>

class QKeyEvent;
class QMouseEvent;
class QMoveEvent;
class QResizeEvent;
class QWheelEvent;

// A lightweight, independent window. Window geometry determines fit-to-image
// zoom, capped at 100% of the original. Ctrl+wheel and keyboard shortcuts zoom
// temporarily, deliberately decoupled from both the original size and the
// window fit factor; Ctrl+0 restores the fit. An oversized image can be dragged
// with the left mouse button.
class ImageViewer final : public QWidget {
    Q_OBJECT
public:
    explicit ImageViewer(const QImage &image, QWidget *parent = nullptr);
    ImageViewer(const QImage &image, const QString &fileName, QWidget *parent = nullptr);

    QRectF displayedImageRect() const;
    qreal  currentScale() const;
    bool   hasTemporaryZoom() const { return temporaryZoom_; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    qreal   autoFitScale() const;
    bool    canPan() const;
    QPointF constrainedPanOffset(const QPointF &offset) const;
    void    updatePanCursor();
    void    zoomBy(qreal factor);
    void    resetTemporaryZoom();

    QImage  image_;
    qreal   temporaryScale_ = 1.0;
    QPointF panOffset_;
    QPoint  lastDragPosition_;
    bool    temporaryZoom_ = false;
    bool    dragging_      = false;
};
