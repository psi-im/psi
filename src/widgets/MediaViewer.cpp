/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "MediaViewer.h"
#include "fileutil.h"

#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImageWriter>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPainter>
#include <QPointer>
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
constexpr int   Margin   = 16;
constexpr qreal ZoomStep = 1.2;

MediaViewer::SizeLoader imageViewerSizeLoader;
MediaViewer::SizeSaver  imageViewerSizeSaver;
MediaViewer::PathLoader imageViewerSavePathLoader;
MediaViewer::PathSaver  imageViewerSavePathSaver;

QPointer<MediaViewer> &currentMediaViewer()
{
    static QPointer<MediaViewer> viewer;
    return viewer;
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
    controls_->setVisible(source != nullptr);
    if (source) {
        source->setParent(this);
        // An explicit open starts a fresh selection containing that resource.
        // Subsequent checkbox changes apply to both navigation and counts.
        for (auto check : controls_->findChildren<QCheckBox *>()) {
            const QSignalBlocker blocker(check);
            check->setChecked(true);
        }
        connect(source, &MediaNavigationSource::changed, this, &MediaViewer::applyView);
        connect(source, &QObject::destroyed, this, [this] { navigationPending_ = false; });
        setMinimumSize(680, 360);
        resize(size().expandedTo(minimumSize()));
        controls_->setGeometry(0, 0, width(), controls_->sizeHint().height());
        applyFilter();
        source->start();
    }
}

QRect MediaViewer::contentRect() const
{
    auto area = rect();
    if (controls_ && !controls_->isHidden())
        area.setTop(controls_->height());
    return area;
}

void MediaViewer::setupControls()
{
    controls_ = new QWidget(this);
    controls_->setObjectName(QStringLiteral("mediaViewerControls"));
    auto rows = new QVBoxLayout(controls_);
    rows->setContentsMargins(8, 4, 8, 4);
    auto                             filters = new QHBoxLayout;
    const QList<QPair<QString, int>> types { { tr("Images"), 1 }, { tr("Videos"), 2 }, { tr("Other files"), 4 } };
    const QList<QPair<QString, int>> states { { tr("Downloaded"), 4 }, { tr("Partial"), 2 }, { tr("Remote"), 1 } };
    for (int group = 0; group < 2; ++group) {
        if (group)
            filters->addSpacing(12);
        for (const auto &value : group ? states : types) {
            auto check = new QCheckBox(value.first, controls_);
            check->setProperty(group ? "availabilityBit" : "typeBit", value.second);
            check->setObjectName(QStringLiteral("mediaFilter%1_%2").arg(group).arg(value.second));
            check->setChecked(true);
            filters->addWidget(check);
            connect(check, &QCheckBox::toggled, this, [this] { applyFilter(); });
        }
    }
    filters->addStretch();
    rows->addLayout(filters);
    auto       buttons = new QHBoxLayout;
    const auto button
        = [&](const QString &name, const QString &text, QStyle::StandardPixmap icon, std::function<void()> action) {
              auto result = new QToolButton(controls_);
              result->setObjectName(name);
              result->setToolTip(text);
              result->setAccessibleName(text);
              result->setIcon(style()->standardIcon(icon));
              connect(result, &QToolButton::clicked, this, std::move(action));
              buttons->addWidget(result);
              return result;
          };
    button("mediaPrevious", tr("Previous"), QStyle::SP_ArrowBack, [this] { showAdjacentImage(-1); });
    button("mediaNext", tr("Next"), QStyle::SP_ArrowForward, [this] { showAdjacentImage(1); });
    position_ = new QLabel("0 / 0", controls_);
    position_->setObjectName("mediaPosition");
    buttons->addWidget(position_);
    loading_ = new QProgressBar(controls_);
    loading_->setObjectName("mediaLoading");
    loading_->setRange(0, 0);
    loading_->setFixedSize(48, 12);
    loading_->setTextVisible(false);
    loading_->hide();
    buttons->addWidget(loading_);
    status_ = new QLabel(controls_);
    status_->setObjectName("mediaStatus");
    status_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    buttons->addWidget(status_, 1);
    button("mediaRetry", tr("Retry"), QStyle::SP_BrowserReload, [this] {
        if (navigationSource_)
            navigationSource_->retry();
    });
    button("mediaRotate", tr("Rotate clockwise"), QStyle::SP_BrowserReload, [this] {
        if (image_.isNull())
            return;
        image_ = image_.transformed(QTransform().rotate(90));
        resetTemporaryZoom();
        update();
    });
    button("mediaCopy", tr("Copy image"), QStyle::SP_FileIcon, [this] {
        if (!image_.isNull())
            QGuiApplication::clipboard()->setImage(image_);
    });
    button("mediaSave", tr("Save as…"), QStyle::SP_DialogSaveButton, [this] { saveImageAs(); });
    button("mediaFullscreen", tr("Full screen"), QStyle::SP_TitleBarMaxButton, [this] { toggleFullscreen(); });
    rows->addLayout(buttons);
    controls_->hide();
    auto fullscreen = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(fullscreen, &QShortcut::activated, this, [this] { toggleFullscreen(); });
    auto escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escape, &QShortcut::activated, this, [this] {
        if (isFullScreen())
            showNormal();
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
}
void MediaViewer::applyFilter()
{
    if (!navigationSource_)
        return;
    MediaFilter filter;
    filter.types        = 0;
    filter.availability = 0;
    for (const auto check : controls_->findChildren<QCheckBox *>()) {
        if (!check->isChecked())
            continue;
        filter.types |= check->property("typeBit").toInt();
        filter.availability |= check->property("availabilityBit").toInt();
    }
    navigationSource_->setFilter(filter);
}
void MediaViewer::applyView(const MediaNavigationSource::View &view)
{
    navigationPending_ = view.navigating;
    if (currentKey_ != view.key) {
        if (video_)
            delete video_.data();
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
    status_->setText(view.state == MediaNavigationSource::State::Loading ? tr("Loading…") : view.error);
    status_->setToolTip(view.error);
    loading_->setVisible(view.state == MediaNavigationSource::State::Loading || view.navigating);
    controls_->findChild<QToolButton *>("mediaRetry")->setEnabled(view.state == MediaNavigationSource::State::Failed);
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
            video_->setGeometry(contentRect());
            video_->show();
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
    if (event->type() == QEvent::WindowStateChange)
        resetTemporaryZoom();
}
