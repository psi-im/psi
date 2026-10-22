/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QImage>
#include <QRectF>
#include <QWidget>

class QMoveEvent;
class QResizeEvent;
class QWheelEvent;

// A lightweight, independent window. Window geometry determines fit-to-image
// zoom, capped at 100% of the original. Wheel zoom is temporary and deliberately
// decoupled from both the original size and the window fit factor.
class MamImageViewer final : public QWidget {
    Q_OBJECT
public:
    explicit MamImageViewer(const QImage &image, QWidget *parent = nullptr);

    QRectF displayedImageRect() const;
    qreal  currentScale() const;
    bool   hasTemporaryZoom() const { return temporaryZoom_; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    qreal autoFitScale() const;
    void resetTemporaryZoom();

    QImage image_;
    qreal temporaryScale_ = 1.0;
    bool temporaryZoom_   = false;
};
