/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ImageViewer.h"

#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScreen>
#include <QSet>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace {
constexpr int   Margin   = 16;
constexpr qreal ZoomStep = 1.2;

const QSet<QString> &supportedImageSuffixes()
{
    static const QSet<QString> suffixes = [] {
        QSet<QString> result;
        for (const QByteArray &format : QImageReader::supportedImageFormats())
            result.insert(QString::fromLatin1(format).toLower());
        return result;
    }();
    return suffixes;
}
}

ImageViewer::ImageViewer(const QImage &image, QWidget *parent)
    : ImageViewer(image, QString(), parent)
{
}

ImageViewer::ImageViewer(const QImage &image, const QString &fileName, QWidget *parent)
    : QWidget(parent, Qt::Window), image_(image)
{
    setObjectName(QStringLiteral("imageViewer"));
    const QFileInfo fileInfo(fileName);
    currentFileName_ = fileInfo.fileName();
    setWindowTitle(currentFileName_.isEmpty() ? tr("Image") : currentFileName_);
    if (!fileName.isEmpty() && fileInfo.isAbsolute() && fileInfo.dir().exists()) {
        directoryPath_ = fileInfo.absolutePath();
        rebuildImageList();
    } else {
        resetImageListToCurrent();
    }
    setAttribute(Qt::WA_DeleteOnClose);
    setAutoFillBackground(true);
    setFocusPolicy(Qt::StrongFocus);
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

void ImageViewer::resetImageListToCurrent()
{
    imageFileNames_.clear();
    if (!currentFileName_.isEmpty())
        imageFileNames_.append(currentFileName_);
    currentImageIndex_ = imageFileNames_.isEmpty() ? -1 : 0;
}

void ImageViewer::rebuildImageList()
{
    imageFileNames_.clear();
    currentImageIndex_ = -1;
    if (directoryPath_.isEmpty()) {
        resetImageListToCurrent();
        return;
    }
    QDir directory(directoryPath_);
    if (!directory.exists()) {
        directoryPath_.clear();
        resetImageListToCurrent();
        return;
    }

    const QFileInfoList entries = directory.entryInfoList(QDir::Files | QDir::Readable, QDir::NoSort);
    QFileInfoList       images;
    images.reserve(entries.size());
    for (const QFileInfo &entry : entries) {
        if (supportedImageSuffixes().contains(entry.suffix().toLower()))
            images.append(entry);
    }

    const QFileInfo current(directory.filePath(currentFileName_));
    bool            currentListed = false;
    for (const QFileInfo &image : images) {
        if (image.fileName() == currentFileName_) {
            currentListed = true;
            break;
        }
    }
    if (!currentListed && current.isFile()) {
        QImageReader reader(current.absoluteFilePath());
        if (reader.canRead())
            images.append(current);
    }

    std::sort(images.begin(), images.end(), [](const QFileInfo &left, const QFileInfo &right) {
        const QDateTime leftTime  = left.lastModified();
        const QDateTime rightTime = right.lastModified();
        if (leftTime != rightTime)
            return leftTime < rightTime;
        return left.fileName() < right.fileName();
    });

    imageFileNames_.reserve(images.size());
    for (const QFileInfo &image : images)
        imageFileNames_.append(image.fileName());
    currentImageIndex_ = imageFileNames_.indexOf(currentFileName_);
}

bool ImageViewer::showAdjacentImage(int step)
{
    if (step == 0 || directoryPath_.isEmpty() || imageFileNames_.size() < 2 || currentImageIndex_ < 0)
        return false;

    const QDir directory(directoryPath_);
    int        index = currentImageIndex_;
    while (true) {
        index += step;
        if (index < 0 || index >= imageFileNames_.size())
            return false;
        const QFileInfo fileInfo(directory.filePath(imageFileNames_.at(index)));
        if (!fileInfo.isFile())
            continue;
        QImageReader reader(fileInfo.absoluteFilePath());
        reader.setAutoTransform(true);
        QImage nextImage = reader.read();
        if (nextImage.isNull())
            continue;

        image_           = nextImage;
        directoryPath_   = fileInfo.absolutePath();
        currentFileName_ = fileInfo.fileName();
        setWindowTitle(currentFileName_);
        resetTemporaryZoom();
        rebuildImageList();
        update();
        return true;
    }
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

void ImageViewer::keyPressEvent(QKeyEvent *event)
{
    const auto modifiers = event->modifiers() & ~Qt::KeypadModifier;
    if (modifiers == Qt::ControlModifier || modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
        switch (event->key()) {
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoomBy(ZoomStep);
            event->accept();
            return;
        case Qt::Key_Minus:
        case Qt::Key_Underscore:
            zoomBy(1.0 / ZoomStep);
            event->accept();
            return;
        case Qt::Key_0:
            resetTemporaryZoom();
            event->accept();
            return;
        default:
            break;
        }
    }
    QWidget::keyPressEvent(event);
}

void ImageViewer::wheelEvent(QWheelEvent *event)
{
    const int delta  = event->angleDelta().y();
    const int pixels = event->pixelDelta().y();
    if (!delta && !pixels) {
        event->ignore();
        return;
    }
    if (!(event->modifiers() & Qt::ControlModifier)) {
        // Wheel down advances to the next file; wheel up goes to the previous.
        showAdjacentImage((delta ? delta : pixels) < 0 ? 1 : -1);
        event->accept();
        return;
    }
    // High-resolution trackpads and traditional wheel notches both adjust
    // the current scale. Zoom can cross the native 100% boundary and the fit.
    const qreal turns = delta ? qreal(delta) / 120.0 : qreal(pixels) / 120.0;
    zoomBy(std::pow(ZoomStep, turns));
    event->accept();
}

void ImageViewer::zoomBy(qreal factor)
{
    temporaryScale_ = std::max(0.02, std::min(32.0, currentScale() * factor));
    temporaryZoom_  = true;
    panOffset_      = constrainedPanOffset(panOffset_);
    updatePanCursor();
    update();
}

void ImageViewer::resetTemporaryZoom()
{
    if (!temporaryZoom_ && panOffset_.isNull() && !dragging_)
        return;
    temporaryZoom_  = false;
    temporaryScale_ = 1.0;
    panOffset_      = {};
    dragging_       = false;
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
