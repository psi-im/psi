#include <QPaintEvent>
#include <QPainter>
#include <QHBoxLayout>
#include <QTimer>

#include "pixmapratiolabel.h"

PixmapRatioLabel::PixmapRatioLabel(QWidget *parent) : QLabel(parent)
{
}

void PixmapRatioLabel::setPixmap(const QPixmap &pix)
{
    _origPix = pix;
    update();
}

void PixmapRatioLabel::setMaxPixmapSize(const QSize &size)
{
    _maxPixSize = size;
}

void PixmapRatioLabel::paintEvent(QPaintEvent *event)
{
    bool needAdjust = false;
    QSize ms = _maxPixSize.isEmpty()? _origPix.size() : _maxPixSize;

    if (_policy == Policy::FitBoth) {
        ms = ms.boundedTo(event->rect().size());
    } else
    if (_policy == Policy::FitVertical) {
        ms.setHeight(qMin(ms.height(), event->rect().height()));
    } else
    if (_policy == Policy::FitHorizontal) {
        ms.setWidth(qMin(ms.width(), event->rect().width()));
    }
    if (_scaledPix.isNull()) {
        _scaledPix = _origPix;
        needAdjust = true;
    }
    QSize newPixSize = _scaledPix.size().scaled(ms, Qt::KeepAspectRatio);
    if (_scaledPix.size() != newPixSize) {
        _scaledPix = _origPix.scaled(ms, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        needAdjust = true;
    }
    if (needAdjust) {
        QTimer::singleShot(0, [this]() {
            // setMinWidth/Height invalidates layout. so all the widgets are resized
            if(_policy == Policy::FitVertical) {
                setMinimumWidth(_scaledPix.width());
            } else if (_policy == Policy::FitHorizontal) {
                setMinimumHeight(_scaledPix.height());
            }
        });
    }
    QPainter p(this);
    QRect pixRect(QPoint(0,0), newPixSize);
    pixRect.moveCenter(event->rect().center());
    p.drawPixmap(pixRect, _scaledPix);
}
