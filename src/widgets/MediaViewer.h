/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "mediafilter.h"
#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QPointer>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QWidget>
#include <functional>

class QCloseEvent;
class QContextMenuEvent;
class QKeyEvent;
class QMouseEvent;
class QMoveEvent;
class QResizeEvent;
class QWheelEvent;
class QLabel;
class QProgressBar;
class QTimer;

// A lightweight, independent window. Window geometry determines fit-to-image
// zoom, capped at 100% of the original. Ctrl+wheel and keyboard shortcuts zoom
// temporarily, deliberately decoupled from both the original size and the
// window fit factor; Ctrl+0 restores the fit. An oversized image can be dragged
// with the left mouse button. Navigation is supplied by the caller rather
// than inferred from the directory of an image's cached payload.
class MediaNavigationSource : public QObject {
    Q_OBJECT
public:
    enum class State { Ready, Loading, Failed, Empty };
    struct View {
        QString                                                    key, name, path, error;
        QImage                                                     image;
        quint64                                                    revision = 0, index = 0, total = 0;
        int                                                        progressPercent = -1; // Unknown/indeterminate.
        State                                                      state      = State::Ready;
        bool                                                       navigating = false;
        std::function<QWidget *(QWidget *, std::function<void()>)> video;
    };
    using QObject::QObject;
    virtual void start() { }
    virtual void adjacent(int direction) = 0;
    // Combine additional wheel events while a navigation query is in flight.
    virtual bool enqueueAdjacent(int) { return false; }
    virtual void setFilter(MediaFilter) { }
    virtual void retry() { }
    virtual bool saveAs(QWidget *) { return false; }
signals:
    void changed(const MediaNavigationSource::View &);
};

class MediaViewer final : public QWidget {
    Q_OBJECT
public:
    using SizeLoader = std::function<QSize()>;
    using SizeSaver  = std::function<void(const QSize &)>;
    using PathLoader = std::function<QString()>;
    using PathSaver  = std::function<void(const QString &)>;
    using FilterLoader = std::function<MediaFilter()>;
    using FilterSaver  = std::function<void(MediaFilter)>;

    explicit MediaViewer(const QImage &image, QWidget *parent = nullptr);
    MediaViewer(const QImage &image, const QString &fileName, QWidget *parent = nullptr);

    static void setSizePersistence(SizeLoader loader, SizeSaver saver);
    static void setSavePathPersistence(PathLoader loader, PathSaver saver);
    static void setFilterPersistence(FilterLoader loader, FilterSaver saver);
    static void showImage(const QImage &image, const QString &fileName, MediaNavigationSource *source = nullptr,
                          bool fullscreen = false);
    // The viewer takes ownership. Replacing the source cancels pending navigation.
    void setNavigationSource(MediaNavigationSource *source);

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
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void    applyView(const MediaNavigationSource::View &);
    void    setupControls();
    void    applyFilter(bool persist = false);
    void    toggleFullscreen();
    void    updateFullscreenUi();
    void    noteFullscreenActivity(bool force = false);
    void    hideIdleVideoControls();
    void    setVideoControlsShown(QWidget *widget, bool visible, bool animated);
    void    trackVideoWidget(QWidget *widget);
    QRect   contentRect() const;
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

    QImage                          image_;
    qreal                           temporaryScale_ = 1.0;
    QPointF                         panOffset_;
    QPoint                          lastDragPosition_;
    QString                         currentFilePath_;
    QString                         currentFileName_;
    QPointer<MediaNavigationSource> navigationSource_;
    QWidget                        *controls_ = nullptr;
    QLabel                         *position_ = nullptr, *status_ = nullptr;
    QProgressBar                   *loading_ = nullptr;
    QPointer<QWidget>               video_;
    QPointer<QWidget>               videoControls_;
    QTimer                         *videoIdleTimer_ = nullptr;
    QPoint                          lastVideoCursorPos_;
    bool                            videoControlsHidden_ = false;
    QString                         currentKey_;
    quint64                         currentRevision_   = 0;
    bool                            navigationPending_ = false;
    bool                            temporaryZoom_     = false;
    bool                            dragging_          = false;
};
