/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "MediaViewer.h"
#include "fileutil.h"

#include <QFrame>
#include <QClipboard>
#include <QChildEvent>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QEvent>
#include <QEasingCurve>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QGraphicsOpacityEffect>
#include <QApplication>
#include <QHBoxLayout>
#include <QImageWriter>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPropertyAnimation>
#include <QSlider>
#include <QTimer>
#include <QProgressBar>
#include <QResizeEvent>
#include <QScreen>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWindow>
#include <algorithm>
#include <cmath>
#include <utility>

namespace {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
QPoint globalMousePoint(const QMouseEvent *event) { return event->globalPosition().toPoint(); }
#else
QPoint globalMousePoint(const QMouseEvent *event) { return event->globalPos(); }
#endif

constexpr int   Margin   = 16;
constexpr qreal ZoomStep = 1.2;

MediaViewer::SizeLoader imageViewerSizeLoader;
MediaViewer::SizeSaver  imageViewerSizeSaver;
MediaViewer::PathLoader imageViewerSavePathLoader;
MediaViewer::PathSaver  imageViewerSavePathSaver;
MediaViewer::FilterLoader mediaViewerFilterLoader;
MediaViewer::FilterSaver  mediaViewerFilterSaver;

QPointer<MediaViewer> &currentMediaViewer()
{
    static QPointer<MediaViewer> viewer;
    return viewer;
}

// Prefer the user's icon theme. Qt's built-in icon is a fallback for themes
// that do not implement one of the FreeDesktop/KDE icon names.
QIcon themedIcon(std::initializer_list<const char *> names, QStyle *style, QStyle::StandardPixmap fallback)
{
    for (const char *name : names) {
        const QIcon icon = QIcon::fromTheme(QLatin1String(name));
        if (!icon.isNull())
            return icon;
    }
    return style->standardIcon(fallback);
}

QIcon fullscreenArrowsIcon(const QPalette &palette, bool inward)
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(palette.color(QPalette::ButtonText), 2, Qt::SolidLine, Qt::RoundCap,
                        Qt::RoundJoin));
    if (inward) {
        painter.drawLine(QPointF(4, 4), QPointF(10, 10));
        painter.drawLine(QPointF(10, 10), QPointF(10, 5));
        painter.drawLine(QPointF(10, 10), QPointF(5, 10));
        painter.drawLine(QPointF(20, 20), QPointF(14, 14));
        painter.drawLine(QPointF(14, 14), QPointF(14, 19));
        painter.drawLine(QPointF(14, 14), QPointF(19, 14));
    } else {
        painter.drawLine(QPointF(10, 10), QPointF(4, 4));
        painter.drawLine(QPointF(4, 4), QPointF(4, 9));
        painter.drawLine(QPointF(4, 4), QPointF(9, 4));
        painter.drawLine(QPointF(14, 14), QPointF(20, 20));
        painter.drawLine(QPointF(20, 20), QPointF(15, 20));
        painter.drawLine(QPointF(20, 20), QPointF(20, 15));
    }
    painter.end();
    return QIcon(pixmap);
}


// A single transparent style for both actions and filters. Active filters
// receive a restrained palette-accent outline; action icons stay fully opaque.
class MediaToolbarButton final : public QToolButton {
public:
    explicit MediaToolbarButton(QWidget *parent) : QToolButton(parent)
    {
        setAutoRaise(true);
        setFocusPolicy(Qt::StrongFocus);
        setFixedSize(34, 34);
        setIconSize(QSize(20, 20));
    }

    void setFileState(int state) { fileState_ = state; }
    void setCategorySymbol(int kind) { categorySymbol_ = kind; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF bounds = QRectF(rect()).adjusted(2, 2, -2, -2);
        const QColor accent = palette().color(QPalette::Highlight);
        if (isCheckable() && isChecked()) {
            QColor tint = accent;
            tint.setAlpha(22);
            QColor frame = accent;
            frame.setAlpha(155);
            painter.setBrush(tint);
            painter.setPen(QPen(frame, 0.9));
        } else {
            QColor tint = palette().color(QPalette::Button);
            tint.setAlpha(underMouse() || isDown() ? 100 : 55);
            painter.setPen(Qt::NoPen);
            painter.setBrush(tint);
        }
        painter.drawRoundedRect(bounds, 5, 5);
        if (hasFocus()) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(accent, 1, Qt::DotLine));
            painter.drawRoundedRect(bounds.adjusted(2, 2, -2, -2), 4, 4);
        }

        painter.setOpacity(!isEnabled() ? 0.25 : isCheckable() && !isChecked() ? 0.47 : 1.0);
        if (fileState_ != -1) {
            painter.save();
            painter.translate((width() - 20) / 2.0, (height() - 20) / 2.0);
            painter.scale(20.0 / 24.0, 20.0 / 24.0);
            const QColor ink = palette().color(QPalette::ButtonText);
            QPainterPath outline;
            outline.moveTo(5, 2);
            outline.lineTo(14, 2);
            outline.lineTo(20, 8);
            outline.lineTo(20, 22);
            outline.lineTo(5, 22);
            outline.closeSubpath();
            auto draw = [&](bool dotted) {
                QPen pen(ink, 1.7, dotted ? Qt::DotLine : Qt::SolidLine,
                         Qt::RoundCap, Qt::RoundJoin);
                painter.setPen(pen);
                painter.setBrush(Qt::NoBrush);
                painter.drawPath(outline);
            };
            if (fileState_ == 4) {
                draw(false);
            } else if (fileState_ == 1) {
                draw(true);
            } else {
                painter.save();
                painter.setClipRect(QRectF(0, 0, 12, 24));
                draw(false);
                painter.restore();
                painter.setClipRect(QRectF(12, 0, 12, 24));
                draw(true);
            }
            painter.restore();
        } else if (categorySymbol_ != 0) {
            // MIME-type icons from some themes already contain translucent
            // artwork. Draw these two category symbols with the same solid,
            // palette-aware stroke as the file availability pictograms.
            painter.save();
            painter.translate((width() - 20) / 2.0, (height() - 20) / 2.0);
            painter.scale(20.0 / 24.0, 20.0 / 24.0);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(palette().color(QPalette::ButtonText), 1.7,
                                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            if (categorySymbol_ == 1) {
                painter.drawRoundedRect(QRectF(3, 5, 18, 14), 1.8, 1.8);
                painter.drawEllipse(QRectF(15, 7.5, 2.8, 2.8));
                QPainterPath hills;
                hills.moveTo(5.5, 16.5);
                hills.lineTo(10.5, 11.5);
                hills.lineTo(13.5, 14.5);
                hills.lineTo(16, 12.5);
                hills.lineTo(19, 16.5);
                painter.drawPath(hills);
            } else {
                painter.drawRoundedRect(QRectF(3, 4, 18, 16), 1.4, 1.4);
                painter.drawRect(QRectF(8.5, 5.8, 7, 12.4));
                for (int y : { 7, 12, 17 }) {
                    painter.drawPoint(QPointF(5.5, y));
                    painter.drawPoint(QPointF(18.5, y));
                }
            }
            painter.restore();
        } else {
            const QPixmap pixmap = icon().pixmap(iconSize(),
                isEnabled() ? QIcon::Normal : QIcon::Disabled,
                isChecked() ? QIcon::On : QIcon::Off);
            if (!pixmap.isNull()) {
                const QSizeF logical = QSizeF(pixmap.size()) / pixmap.devicePixelRatio();
                painter.drawPixmap(QRectF((width() - logical.width()) / 2.0,
                                          (height() - logical.height()) / 2.0,
                                          logical.width(), logical.height()),
                                   pixmap, QRectF(QPointF(0, 0), pixmap.size()));
            }
        }
    }

private:
    int fileState_ = -1;
    int categorySymbol_ = 0;
};

QIcon rotateClockwiseIcon(const QPalette &palette)
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(palette.color(QPalette::ButtonText), 2, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(QRectF(4, 4, 16, 16), 30 * 16, 280 * 16);
    painter.drawLine(QPointF(17, 5), QPointF(20, 8));
    painter.drawLine(QPointF(20, 8), QPointF(16, 9));
    painter.end();
    return QIcon(pixmap);
}

}

void MediaViewer::setSizePersistence(SizeLoader loader, SizeSaver saver)
{
    imageViewerSizeLoader = std::move(loader);
    imageViewerSizeSaver  = std::move(saver);
}

void MediaViewer::setSavePathPersistence(PathLoader loader, PathSaver saver)
{
    imageViewerSavePathLoader = std::move(loader);
    imageViewerSavePathSaver  = std::move(saver);
}

void MediaViewer::setFilterPersistence(FilterLoader loader, FilterSaver saver)
{
    mediaViewerFilterLoader = std::move(loader);
    mediaViewerFilterSaver = std::move(saver);
}

void MediaViewer::showImage(const QImage &image, const QString &fileName, MediaNavigationSource *source,
                            bool fullscreen)
{
    if (image.isNull() && !source) {
        delete source;
        return;
    }
    if (MediaViewer *viewer = currentMediaViewer()) {
        viewer->setImage(image, fileName);
        viewer->setNavigationSource(source);
        if (fullscreen)
            viewer->showFullScreen();
        viewer->bringToFront();
        return;
    }
    auto *viewer = new MediaViewer(image, fileName);
    viewer->setNavigationSource(source);
    currentMediaViewer() = viewer;
    viewer->show();
    if (fullscreen)
        viewer->showFullScreen();
    viewer->bringToFront();
}

MediaViewer::MediaViewer(const QImage &image, QWidget *parent) : MediaViewer(image, QString(), parent) { }

MediaViewer::MediaViewer(const QImage &image, const QString &fileName, QWidget *parent) :
    QWidget(parent, Qt::Window), image_(image)
{
    setObjectName(QStringLiteral("imageViewer"));
    setAttribute(Qt::WA_DeleteOnClose);
    setAutoFillBackground(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(200, 160);
    setupControls();

    const QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QSize available = screen ? screen->availableGeometry().size() : QSize(1024, 768);
    const QSize cap(qMax(minimumWidth(), qRound(available.width() * 0.90)),
                    qMax(minimumHeight(), qRound(available.height() * 0.90)));
    QSize       windowSize(qBound(minimumWidth(), image_.width() + 2 * Margin, cap.width()),
                           qBound(minimumHeight(), image_.height() + 2 * Margin, cap.height()));
    if (imageViewerSizeLoader) {
        const QSize savedSize = imageViewerSizeLoader();
        if (savedSize.isValid() && !savedSize.isEmpty())
            windowSize = savedSize;
    }
    resize(windowSize.expandedTo(minimumSize()));

    setImage(image, fileName);

    if (auto *app = QCoreApplication::instance())
        connect(app, &QCoreApplication::aboutToQuit, this, &MediaViewer::saveWindowSize);
}

qreal MediaViewer::autoFitScale() const
{
    if (image_.isNull())
        return 1.0;
    const qreal x = qreal(qMax(1, contentRect().width() - 2 * Margin)) / image_.width();
    const qreal y = qreal(qMax(1, contentRect().height() - 2 * Margin)) / image_.height();
    return qMin(qreal(1.0), qMin(x, y)); // Never enlarge a small image automatically.
}

qreal MediaViewer::currentScale() const { return temporaryZoom_ ? temporaryScale_ : autoFitScale(); }

void MediaViewer::setImage(const QImage &image, const QString &fileName)
{
    image_ = image;
    const QFileInfo fileInfo(fileName);
    currentFileName_ = fileInfo.fileName();
    currentFilePath_ = fileInfo.isAbsolute() ? fileInfo.absoluteFilePath() : QString();
    setWindowTitle(currentFileName_.isEmpty() ? tr("Image") : currentFileName_);
    resetTemporaryZoom();
    update();
}

void MediaViewer::setNavigationSource(MediaNavigationSource *source)
{
    if (navigationSource_ == source)
        return;
    navigationPending_ = false;
    delete navigationSource_.data();
    navigationSource_ = source;
    currentKey_.clear();
    currentRevision_ = 0;
    if (video_)
        delete video_.data();
    videoControls_ = nullptr;
    videoControlsHidden_ = false;
    if (videoIdleTimer_)
        videoIdleTimer_->stop();
    unsetCursor();
    setVideoControlsShown(controls_, source != nullptr, false);
    if (source) {
        source->setParent(this);
        // An explicit open starts a fresh selection containing that resource.
        // Restore the user's last choice, or use the focused initial view.
        // Reset controls without writing options or sending intermediate SQL
        // queries for each bit of the restored filter.
        const MediaFilter selected = mediaViewerFilterLoader
            ? mediaViewerFilterLoader()
            : MediaFilter { MediaFilter::Images | MediaFilter::Videos,
                            MediaFilter::Complete | MediaFilter::Partial };
        for (auto *filter : controls_->findChildren<QToolButton *>()) {
            const auto type = filter->property("typeBit");
            const auto availability = filter->property("availabilityBit");
            if (!type.isValid() && !availability.isValid())
                continue;
            const QSignalBlocker blocker(filter);
            filter->setChecked(type.isValid() ? (selected.types & type.toInt()) != 0
                                             : (selected.availability & availability.toInt()) != 0);
        }
        connect(source, &MediaNavigationSource::filterAdjustmentRequested, this,
                [this, source](MediaFilter filter) {
                    if (navigationSource_ != source)
                        return;
                    bool changed = false;
                    for (auto *button : controls_->findChildren<QToolButton *>()) {
                        const auto type = button->property("typeBit");
                        const auto availability = button->property("availabilityBit");
                        if (!type.isValid() && !availability.isValid())
                            continue;
                        const bool requested = type.isValid() ? bool(filter.types & type.toInt())
                                                             : bool(filter.availability & availability.toInt());
                        if (button->isChecked() == requested)
                            continue;
                        const QSignalBlocker blocker(button);
                        button->setChecked(requested);
                        changed = true;
                    }
                    if (changed)
                        applyFilter(false);
                });
        connect(source, &MediaNavigationSource::changed, this, &MediaViewer::applyView);
        connect(source, &QObject::destroyed, this, [this] { navigationPending_ = false; });
        setMinimumSize(680, 360);
        resize(size().expandedTo(minimumSize()));
        controls_->setGeometry(0, 0, width(), controls_->sizeHint().height());
        applyFilter();
        source->start();
    }
    updateFullscreenUi();
}

QRect MediaViewer::contentRect() const
{
    auto area = rect();
    if (controls_ && !controls_->isHidden() && !(isFullScreen() && video_))
        area.setTop(controls_->height());
    return area;
}

void MediaViewer::setupControls()
{
    controls_ = new QWidget(this);
    controls_->setObjectName(QStringLiteral("mediaViewerControls"));
    auto *row = new QHBoxLayout(controls_);
    row->setContentsMargins(8, 4, 8, 4);
    row->setSpacing(4);

    const auto separator = [this, row] {
        auto *line = new QFrame(controls_);
        line->setFrameShape(QFrame::VLine);
        line->setFrameShadow(QFrame::Sunken);
        row->addSpacing(3);
        row->addWidget(line);
        row->addSpacing(3);
    };
    const auto filterButton = [this, row](int group, int bit, const QString &label,
                                          std::initializer_list<const char *> icons,
                                          QStyle::StandardPixmap fallback) {
        auto *button = new MediaToolbarButton(controls_);
        button->setObjectName(QStringLiteral("mediaFilter%1_%2").arg(group).arg(bit));
        button->setProperty(group ? "availabilityBit" : "typeBit", bit);
        button->setCheckable(true);
        const bool checkedByDefault = !(group == 0 && bit == 4) && !(group == 1 && bit == 1);
        button->setProperty("defaultChecked", checkedByDefault);
        button->setChecked(checkedByDefault);
        if (group == 1)
            button->setFileState(bit);
        else {
            button->setIcon(themedIcon(icons, style(), fallback));
            if (bit == 1 || bit == 2)
                button->setCategorySymbol(bit);
        }
        button->setToolTip(label);
        button->setAccessibleName(label);
        row->addWidget(button);
        connect(button, &QToolButton::toggled, this, [this] { applyFilter(true); });
        return button;
    };
    filterButton(0, 1, tr("Images"), { "image-x-generic" }, QStyle::SP_FileIcon);
    filterButton(0, 2, tr("Audio and video"), { "video-x-generic" }, QStyle::SP_MediaPlay);
    filterButton(0, 4, tr("Other files"), { "application-x-generic" }, QStyle::SP_FileIcon);
    separator();
    filterButton(1, 4, tr("Downloaded"), {}, QStyle::SP_FileIcon);
    filterButton(1, 2, tr("Partially downloaded"), {}, QStyle::SP_FileIcon);
    filterButton(1, 1, tr("Remote"), {}, QStyle::SP_FileIcon);
    separator();

    const auto actionButton = [this, row](const QString &name, const QString &label,
                                           std::initializer_list<const char *> icons,
                                           QStyle::StandardPixmap fallback,
                                           std::function<void()> action) {
        auto *button = new MediaToolbarButton(controls_);
        button->setObjectName(name);
        button->setToolTip(label);
        button->setAccessibleName(label);
        button->setIcon(themedIcon(icons, style(), fallback));
        button->setIconSize(QSize(18, 18));
        connect(button, &QToolButton::clicked, this, std::move(action));
        row->addWidget(button);
        return button;
    };
    actionButton("mediaPrevious", tr("Previous"), { "go-previous" }, QStyle::SP_ArrowBack,
                 [this] { showAdjacentImage(-1); });
    actionButton("mediaNext", tr("Next"), { "go-next" }, QStyle::SP_ArrowForward,
                 [this] { showAdjacentImage(1); });
    position_ = new QLabel(QStringLiteral("0 / 0"), controls_);
    position_->setObjectName(QStringLiteral("mediaPosition"));
    row->addWidget(position_);
    loading_ = new QProgressBar(controls_);
    loading_->setObjectName(QStringLiteral("mediaLoading"));
    loading_->setRange(0, 0);
    loading_->setFixedSize(48, 12);
    loading_->setTextVisible(false);
    loading_->hide();
    row->addWidget(loading_);
    status_ = new QLabel(controls_);
    status_->setObjectName(QStringLiteral("mediaStatus"));
    status_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    row->addWidget(status_, 1);
    actionButton("mediaRetry", tr("Retry loading this item (including download if needed)"),
                 { "view-refresh" }, QStyle::SP_BrowserReload, [this] {
        if (navigationSource_)
            navigationSource_->retry();
    });
    auto *rotate = actionButton("mediaRotate", tr("Rotate clockwise"), { "object-rotate-right" },
                                QStyle::SP_ArrowForward, [this] {
        if (image_.isNull())
            return;
        image_ = image_.transformed(QTransform().rotate(90));
        resetTemporaryZoom();
        update();
    });
    rotate->setIcon(QIcon::fromTheme(QStringLiteral("object-rotate-right"), rotateClockwiseIcon(palette())));
    actionButton("mediaCopy", tr("Copy image"), { "edit-copy" }, QStyle::SP_FileIcon, [this] {
        if (!image_.isNull())
            QGuiApplication::clipboard()->setImage(image_);
    });
    actionButton("mediaSave", tr("Save as…"), { "document-save-as", "document-save" },
                 QStyle::SP_DialogSaveButton, [this] { saveImageAs(); });
    auto *full = actionButton("mediaFullscreen", tr("Full screen"), { "view-fullscreen" },
                              QStyle::SP_TitleBarMaxButton, [this] { toggleFullscreen(); });
    full->setIcon(QIcon::fromTheme(QStringLiteral("view-fullscreen"), fullscreenArrowsIcon(palette(), false)));
    controls_->hide();
    trackVideoWidget(controls_);
    videoIdleTimer_ = new QTimer(this);
    videoIdleTimer_->setObjectName(QStringLiteral("mediaVideoIdleTimer"));
    videoIdleTimer_->setSingleShot(true);
    videoIdleTimer_->setInterval(2000);
    connect(videoIdleTimer_, &QTimer::timeout, this, &MediaViewer::hideIdleVideoControls);
    auto *fullscreen = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(fullscreen, &QShortcut::activated, this, [this] { toggleFullscreen(); });
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escape, &QShortcut::activated, this, [this] {
        if (isFullScreen())
            toggleFullscreen();
        else
            close();
    });
}

void MediaViewer::toggleFullscreen()
{
    if (isFullScreen())
        showNormal();
    else
        showFullScreen();
    updateFullscreenUi();
}

void MediaViewer::trackVideoWidget(QWidget *widget)
{
    if (!widget)
        return;
    widget->setMouseTracking(true);
    widget->installEventFilter(this);
    for (auto *child : widget->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
        trackVideoWidget(child);
}

void MediaViewer::setVideoControlsShown(QWidget *widget, bool visible, bool animated)
{
    if (!widget)
        return;
    // Animate only the overlay bars; never apply opacity to the video
    // surface itself (it may be backed by a native/GPU window).
    auto *effect = qobject_cast<QGraphicsOpacityEffect *>(widget->graphicsEffect());
    if (!effect) {
        effect = new QGraphicsOpacityEffect(widget);
        effect->setOpacity(widget->isHidden() ? 0.0 : 1.0);
        widget->setGraphicsEffect(effect);
    }
    auto *transition = widget->findChild<QPropertyAnimation *>(
        QStringLiteral("mediaControlFade"), Qt::FindDirectChildrenOnly);
    if (!transition) {
        transition = new QPropertyAnimation(effect, "opacity", widget);
        transition->setObjectName(QStringLiteral("mediaControlFade"));
        transition->setDuration(200);
        transition->setEasingCurve(QEasingCurve::InOutQuad);
        connect(transition, &QPropertyAnimation::finished, widget, [widget, effect] {
            if (effect->opacity() < 0.01)
                widget->hide();
        });
    }
    transition->stop();
    if (!animated) {
        effect->setOpacity(visible ? 1.0 : 0.0);
        widget->setVisible(visible);
        return;
    }
    if (visible)
        widget->show();
    transition->setStartValue(effect->opacity());
    transition->setEndValue(visible ? 1.0 : 0.0);
    transition->start();
}

void MediaViewer::noteFullscreenActivity(bool force)
{
    if (!isFullScreen() || !video_ || !videoIdleTimer_)
        return;
    const auto position = QCursor::pos();
    if (!force && position == lastVideoCursorPos_)
        return;
    lastVideoCursorPos_ = position;
    if (videoControlsHidden_) {
        videoControlsHidden_ = false;
        setVideoControlsShown(controls_, true, true);
        setVideoControlsShown(videoControls_, true, true);
        video_->setGeometry(contentRect());
        controls_->raise();
    }
    unsetCursor();
    videoIdleTimer_->start();
}

void MediaViewer::hideIdleVideoControls()
{
    if (!isFullScreen() || !video_ || videoControlsHidden_)
        return;
    // Do not hide a timeline or a button in the middle of a mouse gesture.
    const bool sliderInUse = videoControls_ && videoControls_->findChild<QSlider *>()
        && videoControls_->findChild<QSlider *>()->isSliderDown();
    if (QApplication::mouseButtons() != Qt::NoButton || sliderInUse) {
        videoIdleTimer_->start();
        return;
    }
    videoControlsHidden_ = true;
    setVideoControlsShown(controls_, false, true);
    setVideoControlsShown(videoControls_, false, true);
    // In full-screen video the top bar is an overlay, so its fade does not
    // resize or jump the picture underneath it.
    setCursor(Qt::BlankCursor);
}

void MediaViewer::updateFullscreenUi()
{
    if (!controls_ || !videoIdleTimer_)
        return;
    auto *button = controls_->findChild<QToolButton *>(QStringLiteral("mediaFullscreen"));
    if (button) {
        const bool full = isFullScreen();
        const QString label = full ? tr("Exit full screen") : tr("Full screen");
        button->setToolTip(label);
        button->setAccessibleName(label);
        button->setIcon(QIcon::fromTheme(full ? QStringLiteral("view-restore") : QStringLiteral("view-fullscreen"),
                                         fullscreenArrowsIcon(palette(), full)));
    }
    if (video_) {
        if (auto *videoFullscreen = video_->findChild<QToolButton *>(QStringLiteral("qite-video-fullscreen-button"))) {
            const bool full = isFullScreen();
            videoFullscreen->setIcon(
                QIcon::fromTheme(full ? QStringLiteral("view-restore") : QStringLiteral("view-fullscreen"),
                                 fullscreenArrowsIcon(palette(), full)));
        }
    }
    if (isFullScreen() && video_) {
        noteFullscreenActivity(true);
        return;
    }
    videoIdleTimer_->stop();
    videoControlsHidden_ = false;
    unsetCursor();
    setVideoControlsShown(controls_, navigationSource_ != nullptr, false);
    setVideoControlsShown(videoControls_, true, false);
    if (video_)
        video_->setGeometry(contentRect());
}

bool MediaViewer::eventFilter(QObject *object, QEvent *event)
{
    if (isFullScreen() && video_) {
        if (event->type() == QEvent::MouseMove) {
            auto *move = static_cast<QMouseEvent *>(event);
            if (globalMousePoint(move) != lastVideoCursorPos_) {
                noteFullscreenActivity(true);
                lastVideoCursorPos_ = globalMousePoint(move);
            }
        } else if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::KeyPress
                   || event->type() == QEvent::Wheel) {
            noteFullscreenActivity(true);
        }
    }
    // Child video surfaces can be created lazily by the multimedia backend.
    if (event->type() == QEvent::ChildAdded) {
        auto *added = static_cast<QChildEvent *>(event)->child();
        if (auto *widget = qobject_cast<QWidget *>(added)) {
            const QPointer<QWidget> guard(widget);
            QTimer::singleShot(0, this, [this, guard] {
                if (guard && (guard == controls_ || controls_->isAncestorOf(guard)
                              || (video_ && (guard == video_ || video_->isAncestorOf(guard)))))
                    trackVideoWidget(guard);
            });
        }
    }
    return QWidget::eventFilter(object, event);
}
void MediaViewer::applyFilter(bool persist)
{
    if (!navigationSource_)
        return;
    MediaFilter filter;
    filter.types        = 0;
    filter.availability = 0;
    for (const auto *button : controls_->findChildren<QToolButton *>()) {
        if (!button->isCheckable() || !button->isChecked())
            continue;
        filter.types |= button->property("typeBit").toInt();
        filter.availability |= button->property("availabilityBit").toInt();
    }
    navigationSource_->setFilter(filter);
    if (persist && mediaViewerFilterSaver)
        mediaViewerFilterSaver(filter);
}
void MediaViewer::applyView(const MediaNavigationSource::View &view)
{
    navigationPending_ = view.navigating;
    if (currentKey_ != view.key) {
        if (video_) {
            delete video_.data();
            videoControls_ = nullptr;
            if (videoIdleTimer_)
                videoIdleTimer_->stop();
            videoControlsHidden_ = false;
            unsetCursor();
            setVideoControlsShown(controls_, true, false);
        }
        image_           = {};
        currentKey_      = view.key;
        currentRevision_ = 0;
    }
    if (view.revision != currentRevision_) {
        setImage(view.image, view.name);
        currentRevision_ = view.revision;
    }
    currentFileName_ = view.name;
    currentFilePath_ = view.path;
    setWindowTitle(view.name.isEmpty() ? tr("Media") : view.name);
    position_->setText(QStringLiteral("%1 / %2").arg(view.index).arg(view.total));
    const bool loading = view.state == MediaNavigationSource::State::Loading;
    const bool determinate = loading && view.progressPercent >= 0;
    loading_->setRange(0, determinate ? 100 : 0);
    if (determinate)
        loading_->setValue(qBound(0, view.progressPercent, 100));
    status_->setText(loading
                         ? (determinate ? tr("Loading… %1%").arg(view.progressPercent) : tr("Loading…"))
                         : view.error);
    status_->setToolTip(view.error);
    loading_->setVisible(loading || view.navigating);
    // Retrying is meaningful only when this item failed to load.
    controls_->findChild<QToolButton *>("mediaRetry")->setVisible(
        view.state == MediaNavigationSource::State::Failed);
    controls_->findChild<QToolButton *>("mediaPrevious")
        ->setEnabled(!view.navigating && view.total && (view.index == 0 || view.index > 1));
    controls_->findChild<QToolButton *>("mediaNext")
        ->setEnabled(!view.navigating && view.total && view.index < view.total);
    if (view.video && !video_) {
        const QPointer<MediaViewer> guard(this);
        video_ = view.video(this, [guard] {
            if (guard)
                guard->toggleFullscreen();
        });
        if (video_) {
            videoControls_ = video_->findChild<QWidget *>(QStringLiteral("qite-video-controls"));
            if (auto *videoFullscreen = video_->findChild<QToolButton *>(QStringLiteral("qite-video-fullscreen-button"))) {
                // qite updates its own icon after invoking the host's fullscreen
                // callback; our later connection reapplies the themed arrows.
                connect(videoFullscreen, &QToolButton::clicked, this, [this] {
                    updateFullscreenUi();
                });
            }
            trackVideoWidget(video_);
            video_->setGeometry(contentRect());
            video_->show();
            updateFullscreenUi();
        }
    }
    controls_->raise();
    for (const auto name : { "mediaCopy", "mediaRotate" })
        if (auto button = controls_->findChild<QToolButton *>(name))
            button->setEnabled(!image_.isNull() && !video_);
    update();
}

void MediaViewer::saveWindowSize() const
{
    if (imageViewerSizeSaver)
        imageViewerSizeSaver(size());
}

void MediaViewer::bringToFront()
{
    if (isMinimized())
        showNormal();
    else
        show();

    // Qt's raise/activate may be ignored by a Wayland compositor. Psi uses the
    // same short hint toggle in common.cpp to make the window appear on top.
    if (QGuiApplication::platformName() == QLatin1String("wayland")
        && (QGuiApplication::applicationState() & Qt::ApplicationInactive)) {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
        show();
    }

    raise();
    activateWindow();
    if (QWindow *window = windowHandle())
        window->requestActivate();
}

QString MediaViewer::currentFilePath() const { return currentFilePath_; }

void MediaViewer::showInFolder()
{
    const QString path = currentFilePath();
    if (!path.isEmpty())
        FileUtil::openFolder(path);
}

void MediaViewer::saveImageAs()
{
    if (navigationSource_ && navigationSource_->saveAs(this))
        return;
    QString currentName = currentFileName_.isEmpty() ? QStringLiteral("image.png") : currentFileName_;
    if (QFileInfo(currentName).suffix().isEmpty())
        currentName += QStringLiteral(".png");
    QString initialPath;
    if (imageViewerSavePathLoader)
        initialPath = imageViewerSavePathLoader();
    if (initialPath.isEmpty()) {
        initialPath = QDir::home().filePath(currentName);
    } else {
        const QFileInfo info(initialPath);
        if (info.isDir())
            initialPath = QDir(initialPath).filePath(currentName);
        else if (!info.dir().exists())
            initialPath = QDir::home().filePath(currentName);
    }

    QStringList imagePatterns;
    for (const QByteArray &format : QImageWriter::supportedImageFormats())
        imagePatterns.append(QStringLiteral("*.") + QString::fromLatin1(format).toLower());
    const QString filter = imagePatterns.isEmpty()
        ? tr("All files (*)")
        : tr("Images (%1);;All files (*)").arg(imagePatterns.join(QLatin1Char(' ')));

    const QString path = QFileDialog::getSaveFileName(this, tr("Save image"), initialPath, filter);
    if (path.isEmpty())
        return;
    if (!image_.save(path)) {
        QMessageBox::warning(this, tr("Error"), tr("Could not save the image."));
        return;
    }
    if (imageViewerSavePathSaver)
        imageViewerSavePathSaver(path);
}

void MediaViewer::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    const QString filePath     = currentFilePath();
    auto         *showInFolder = menu.addAction(tr("Show in folder"));
    showInFolder->setObjectName(QStringLiteral("imageViewerShowInFolder"));
    showInFolder->setEnabled(!filePath.isEmpty() && QFileInfo(filePath).dir().exists());
    connect(showInFolder, &QAction::triggered, this, &MediaViewer::showInFolder);

    auto *copyImage = menu.addAction(tr("Copy image"));
    copyImage->setEnabled(!image_.isNull());
    copyImage->setObjectName(QStringLiteral("imageViewerCopyImage"));
    connect(copyImage, &QAction::triggered, this, [this] { QGuiApplication::clipboard()->setImage(image_); });

    auto *saveAs = menu.addAction(tr("Save as..."));
    saveAs->setObjectName(QStringLiteral("imageViewerSaveAs"));
    connect(saveAs, &QAction::triggered, this, &MediaViewer::saveImageAs);

    menu.exec(event->globalPos());
    event->accept();
}

bool MediaViewer::showAdjacentImage(int step)
{
    if (!navigationSource_ || step == 0)
        return false;
    if (navigationPending_)
        return navigationSource_->enqueueAdjacent(step);
    navigationPending_ = true;
    navigationSource_->adjacent(step);
    return true;
}

bool MediaViewer::canPan() const
{
    if (image_.isNull())
        return false;
    return image_.width() * currentScale() > contentRect().width()
        || image_.height() * currentScale() > contentRect().height();
}

QPointF MediaViewer::constrainedPanOffset(const QPointF &offset) const
{
    // Never expose empty space beyond an oversized image. On a fitting axis,
    // keep the image centred even if the other axis is scrollable.
    const qreal maxX = qMax(qreal(0), (image_.width() * currentScale() - contentRect().width()) / 2);
    const qreal maxY = qMax(qreal(0), (image_.height() * currentScale() - contentRect().height()) / 2);
    return QPointF(qBound(-maxX, offset.x(), maxX), qBound(-maxY, offset.y(), maxY));
}

void MediaViewer::updatePanCursor()
{
    if (!canPan()) {
        dragging_ = false;
        unsetCursor();
    } else {
        setCursor(dragging_ ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
    }
}

QRectF MediaViewer::displayedImageRect() const
{
    if (image_.isNull())
        return {};
    const QSizeF size(image_.width() * currentScale(), image_.height() * currentScale());
    const auto   area = QRectF(contentRect());
    return QRectF(area.center() - QPointF(size.width() / 2, size.height() / 2) + panOffset_, size);
}

void MediaViewer::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setClipRect(contentRect());
    if (image_.isNull()) {
        if (!video_)
            painter.drawText(contentRect(), Qt::AlignCenter, currentFileName_);
        return;
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(displayedImageRect(), image_);
}

void MediaViewer::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && canPan() && displayedImageRect().contains(event->pos())) {
        dragging_         = true;
        lastDragPosition_ = event->pos();
        updatePanCursor();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void MediaViewer::mouseMoveEvent(QMouseEvent *event)
{
    if (isFullScreen() && video_ && globalMousePoint(event) != lastVideoCursorPos_) {
        noteFullscreenActivity(true);
        lastVideoCursorPos_ = globalMousePoint(event);
    }
    if (dragging_) {
        const QPoint  position = event->pos();
        const QPointF offset   = constrainedPanOffset(panOffset_ + position - lastDragPosition_);
        lastDragPosition_      = position;
        if (offset != panOffset_) {
            panOffset_ = offset;
            update();
        }
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void MediaViewer::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        updatePanCursor();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void MediaViewer::keyPressEvent(QKeyEvent *event)
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

void MediaViewer::wheelEvent(QWheelEvent *event)
{
    const int delta  = event->angleDelta().y();
    const int pixels = event->pixelDelta().y();
    if (!delta && !pixels) {
        event->ignore();
        return;
    }
    if (!(event->modifiers() & Qt::ControlModifier)) {
        // Wheel down advances to the next file; wheel up goes to the previous.
        const int movement = delta ? delta : pixels;
        // A fast wheel notch stream can represent several media positions.
        // Let the source select the destination without decoding each one.
        const int steps = qBound(1, std::abs(movement) / 120, 40);
        showAdjacentImage(movement < 0 ? steps : -steps);
        event->accept();
        return;
    }
    // High-resolution trackpads and traditional wheel notches both adjust
    // the current scale. Zoom can cross the native 100% boundary and the fit.
    const qreal turns = delta ? qreal(delta) / 120.0 : qreal(pixels) / 120.0;
    zoomBy(std::pow(ZoomStep, turns));
    event->accept();
}

void MediaViewer::zoomBy(qreal factor)
{
    temporaryScale_ = std::max(0.02, std::min(32.0, currentScale() * factor));
    temporaryZoom_  = true;
    panOffset_      = constrainedPanOffset(panOffset_);
    updatePanCursor();
    update();
}

void MediaViewer::resetTemporaryZoom()
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

void MediaViewer::closeEvent(QCloseEvent *event)
{
    saveWindowSize();
    if (currentMediaViewer() == this)
        currentMediaViewer() = nullptr;
    QWidget::closeEvent(event);
}

void MediaViewer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (controls_)
        controls_->setGeometry(0, 0, width(), controls_->sizeHint().height());
    if (video_)
        video_->setGeometry(contentRect());
    resetTemporaryZoom();
}

void MediaViewer::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
    resetTemporaryZoom();
}

void MediaViewer::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        resetTemporaryZoom();
        updateFullscreenUi();
    }
}
