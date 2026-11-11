/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QPointer>
#include <QWidget>
#include <functional>

class QCloseEvent;
class QContextMenuEvent;
class QKeyEvent;
class QMouseEvent;
class QMoveEvent;
class QResizeEvent;
class QWheelEvent;

// A lightweight, independent window. Window geometry determines fit-to-image
// zoom, capped at 100% of the original. Ctrl+wheel and keyboard shortcuts zoom
// temporarily, deliberately decoupled from both the original size and the
// window fit factor; Ctrl+0 restores the fit. An oversized image can be dragged
// with the left mouse button. Navigation is supplied by the caller rather
// than inferred from the directory of an image's cached payload.
class ImageNavigationSource : public QObject {
public:
    using Result = std::function<void(const QImage &, const QString &, bool)>;
    using QObject::QObject;
    virtual void adjacent(int direction, Result result) = 0;
};

class ImageViewer final : public QWidget {
    Q_OBJECT
public:
    using SizeLoader = std::function<QSize()>;
    using SizeSaver  = std::function<void(const QSize &)>;
    using PathLoader = std::function<QString()>;
    using PathSaver  = std::function<void(const QString &)>;

    explicit ImageViewer(const QImage &image, QWidget *parent = nullptr);
    ImageViewer(const QImage &image, const QString &fileName, QWidget *parent = nullptr);

    static void setSizePersistence(SizeLoader loader, SizeSaver saver);
    static void setSavePathPersistence(PathLoader loader, PathSaver saver);
    static void showImage(const QImage &image, const QString &fileName, ImageNavigationSource *source = nullptr);
    // The viewer takes ownership. Replacing the source cancels pending navigation.
    void setNavigationSource(ImageNavigationSource *source);

    QRectF displayedImageRect() const;
    qreal  currentScale() const;
    bool   hasTemporaryZoom() const { return temporaryZoom_; }

protected:
    void closeEvent(QCloseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
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
    bool    showAdjacentImage(int step);
    void    setImage(const QImage &image, const QString &fileName);
    void    saveWindowSize() const;
    void    bringToFront();
    QString currentFilePath() const;
    void    showInFolder();
    void    saveImageAs();
    void    zoomBy(qreal factor);
    void    resetTemporaryZoom();

    QImage      image_;
    qreal       temporaryScale_ = 1.0;
    QPointF     panOffset_;
    QPoint      lastDragPosition_;
    QString     currentFilePath_;
    QString     currentFileName_;
    QPointer<ImageNavigationSource> navigationSource_;
    quint64     navigationSerial_ = 0;
    bool        navigationPending_ = false;
    bool        temporaryZoom_     = false;
    bool        dragging_          = false;
};
