/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/MediaViewer.h"

#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QToolButton>
#include <QVBoxLayout>
#include <QTimer>
#include <QLabel>
#include <QProgressBar>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QWheelEvent>
#include <QWindowStateChangeEvent>
#include <QtTest/QtTest>
#include <cmath>
#include <memory>

namespace {
void zoom(MediaViewer &viewer, int notches)
{
    const QPoint centre = viewer.rect().center();
    QWheelEvent  wheel(QPointF(centre), QPointF(viewer.mapToGlobal(centre)), QPoint(), QPoint(0, notches * 120),
                       Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&viewer, &wheel);
    QVERIFY(wheel.isAccepted());
}

void drag(MediaViewer &viewer, const QPoint &from, const QPoint &to)
{
    QTest::mousePress(&viewer, Qt::LeftButton, Qt::NoModifier, from);
    QMouseEvent moved(QEvent::MouseMove, QPointF(to), QPointF(viewer.mapToGlobal(to)), Qt::NoButton, Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&viewer, &moved);
    QTest::mouseRelease(&viewer, Qt::LeftButton, Qt::NoModifier, to);
}
}

class TestNavigationSource final : public MediaNavigationSource {
public:
    int                                        requests  = 0;
    int                                        direction = 0;
    int                                        queued = 0;
    std::function<void(QImage, QString, bool)> pending;
    MediaFilter                                filter;
    void                                       setFilter(MediaFilter value) override { filter = value; }

    bool enqueueAdjacent(int step) override
    {
        queued += step;
        return true;
    }
    void adjacent(int step) override
    {
        ++requests;
        direction = step;
        const QPointer<TestNavigationSource> guard(this);
        pending = [guard](QImage image, QString name, bool ok) {
            if (!guard)
                return;
            View view;
            view.key      = name;
            view.name     = name;
            view.image    = std::move(image);
            view.revision = 1;
            view.state    = ok ? State::Ready : State::Failed;
            emit guard->changed(view);
        };
    }
};

class MediaViewerTest final : public QObject {
    Q_OBJECT

private slots:
    void filtersAreRestoredAcrossViewerInstances()
    {
        auto preferences = std::make_shared<MediaFilter>(
            MediaFilter { MediaFilter::Images | MediaFilter::Videos,
                          MediaFilter::Complete | MediaFilter::Partial });
        MediaViewer::setFilterPersistence(
            [preferences] { return *preferences; },
            [preferences](MediaFilter value) { *preferences = value; });
        QImage image(80, 60, QImage::Format_RGB32);
        image.fill(Qt::red);
        {
            MediaViewer viewer(image);
            auto *source = new TestNavigationSource;
            viewer.setNavigationSource(source);
            QCOMPARE(source->filter.types, int(MediaFilter::Images | MediaFilter::Videos));
            QCOMPARE(source->filter.availability, int(MediaFilter::Complete | MediaFilter::Partial));
            auto *audioVideo = viewer.findChild<QToolButton *>(QStringLiteral("mediaFilter0_2"));
            auto *remote = viewer.findChild<QToolButton *>(QStringLiteral("mediaFilter1_1"));
            QVERIFY(audioVideo && remote);
            audioVideo->setChecked(false);
            remote->setChecked(true);
            QCOMPARE(preferences->types, int(MediaFilter::Images));
            QCOMPARE(preferences->availability,
                     int(MediaFilter::Complete | MediaFilter::Partial | MediaFilter::Remote));
        }
        {
            MediaViewer viewer(image);
            auto *source = new TestNavigationSource;
            viewer.setNavigationSource(source);
            auto *audioVideo = viewer.findChild<QToolButton *>(QStringLiteral("mediaFilter0_2"));
            auto *remote = viewer.findChild<QToolButton *>(QStringLiteral("mediaFilter1_1"));
            QVERIFY(audioVideo && remote);
            QVERIFY(!audioVideo->isChecked());
            QVERIFY(remote->isChecked());
            QCOMPARE(source->filter.types, preferences->types);
            QCOMPARE(source->filter.availability, preferences->availability);
        }
        MediaViewer::setFilterPersistence({}, {});
    }

    void openedRemoteMediaIsIncludedOnlyForThisViewer()
    {
        const MediaFilter saved { MediaFilter::Images, MediaFilter::Complete };
        MediaFilter persisted = saved;
        MediaViewer::setFilterPersistence([&persisted] { return persisted; },
                                          [&persisted](MediaFilter value) { persisted = value; });
        QImage image(80, 60, QImage::Format_RGB32);
        image.fill(Qt::red);
        {
            MediaViewer viewer(image);
            auto *source = new TestNavigationSource;
            viewer.setNavigationSource(source);
            QCOMPARE(source->filter.availability, int(MediaFilter::Complete));
            emit source->filterAdjustmentRequested(
                { MediaFilter::Images, MediaFilter::Complete | MediaFilter::Remote });
            const auto *remote = viewer.findChild<QToolButton *>(QStringLiteral("mediaFilter1_1"));
            QVERIFY(remote);
            QVERIFY(remote->isChecked());
            QCOMPARE(source->filter.availability, int(MediaFilter::Complete | MediaFilter::Remote));
            QCOMPARE(persisted.availability, int(MediaFilter::Complete));
        }
        {
            MediaViewer viewer(image);
            auto *source = new TestNavigationSource;
            viewer.setNavigationSource(source);
            QVERIFY(!viewer.findChild<QToolButton *>(QStringLiteral("mediaFilter1_1"))->isChecked());
        }
        MediaViewer::setFilterPersistence({}, {});
    }

    void filtersAndPositionShareTheSourceSelection()
    {
        QImage image(80, 60, QImage::Format_RGB32);
        image.fill(Qt::red);
        MediaViewer viewer(image);
        auto        source = new TestNavigationSource;
        viewer.setNavigationSource(source);
        viewer.show();
        QCoreApplication::processEvents();
        auto *videos  = viewer.findChild<QToolButton *>("mediaFilter0_2");
        auto *partial = viewer.findChild<QToolButton *>("mediaFilter1_2");
        auto *remote = viewer.findChild<QToolButton *>("mediaFilter1_1");
        auto *other = viewer.findChild<QToolButton *>("mediaFilter0_4");
        auto *previous = viewer.findChild<QToolButton *>("mediaPrevious");
        auto *rotate = viewer.findChild<QToolButton *>("mediaRotate");
        auto *retry = viewer.findChild<QToolButton *>("mediaRetry");
        QVERIFY(videos && partial && remote && other && previous && rotate && retry);
        QVERIFY(videos->isCheckable() && partial->isCheckable());
        QVERIFY(videos->isChecked() && partial->isChecked());
        QVERIFY(!remote->isChecked() && !other->isChecked());
        QCOMPARE(source->filter.types, int(MediaFilter::Images | MediaFilter::Videos));
        QCOMPARE(source->filter.availability, int(MediaFilter::Complete | MediaFilter::Partial));
        QVERIFY(!videos->icon().isNull());
        QVERIFY(!rotate->icon().isNull());
        QCOMPARE(videos->parentWidget(), previous->parentWidget());
        QCOMPARE(videos->geometry().center().y(), previous->geometry().center().y());
        QCOMPARE(rotate->geometry().center().y(), previous->geometry().center().y());
        QVERIFY(retry->toolTip().contains("download"));
        videos->setChecked(false);
        partial->setChecked(false);
        QCOMPARE(source->filter.types, int(MediaFilter::Images));
        QCOMPARE(source->filter.availability, int(MediaFilter::Complete));
        remote->setChecked(true);
        other->setChecked(true);
        QCOMPARE(source->filter.types, int(MediaFilter::Images | MediaFilter::Files));
        QCOMPARE(source->filter.availability, int(MediaFilter::Complete | MediaFilter::Remote));
        MediaNavigationSource::View view;
        view.key      = "resource";
        view.name     = "photo.png";
        view.image    = image;
        view.revision = 1;
        view.index    = 2;
        view.total    = 5;
        view.state    = MediaNavigationSource::State::Loading;
        emit source->changed(view);
        QCOMPARE(viewer.findChild<QLabel *>("mediaPosition")->text(), QStringLiteral("2 / 5"));
        QVERIFY(!viewer.findChild<QLabel *>("mediaStatus")->text().isEmpty());
        auto progress = viewer.findChild<QProgressBar *>("mediaLoading");
        QVERIFY(progress);
        QCOMPARE(progress->maximum(), 0); // Unknown-length transfer.
        view.progressPercent = 47;
        emit source->changed(view);
        QCOMPARE(progress->maximum(), 100);
        QCOMPARE(progress->value(), 47);
        QVERIFY(viewer.findChild<QLabel *>("mediaStatus")->text().contains("47%"));
        zoom(viewer, 1);
        const auto scale = viewer.currentScale();
        view.index       = 1;
        view.total       = 3;
        view.state       = MediaNavigationSource::State::Ready;
        emit source->changed(view);
        QCOMPARE(viewer.findChild<QLabel *>("mediaPosition")->text(), QStringLiteral("1 / 3"));
        QCOMPARE(viewer.currentScale(), scale);
        QTest::keyClick(&viewer, Qt::Key_F11);
        QTRY_VERIFY(viewer.isFullScreen());
        QTest::keyClick(&viewer, Qt::Key_Escape);
        QTRY_VERIFY(!viewer.isFullScreen());
        QVERIFY(viewer.isVisible());
    }
    void fullscreenVideoAutoHidesBothControlBars()
    {
        QImage image(80, 60, QImage::Format_RGB32);
        image.fill(Qt::red);
        MediaViewer viewer(image);
        auto *source = new TestNavigationSource;
        viewer.setNavigationSource(source);
        viewer.show();
        QCoreApplication::processEvents();

        MediaNavigationSource::View view;
        view.key = QStringLiteral("video");
        view.name = QStringLiteral("video.mp4");
        view.revision = 1;
        view.index = 1;
        view.total = 1;
        view.state = MediaNavigationSource::State::Ready;
        view.video = [](QWidget *parent, std::function<void()>) {
            auto *player = new QWidget(parent);
            auto *layout = new QVBoxLayout(player);
            layout->setContentsMargins(0, 0, 0, 0);
            auto *surface = new QWidget(player);
            layout->addWidget(surface, 1);
            auto *bottom = new QWidget(player);
            bottom->setObjectName(QStringLiteral("qite-video-controls"));
            bottom->setFixedHeight(32);
            layout->addWidget(bottom);
            return player;
        };
        emit source->changed(view);
        auto *top = viewer.findChild<QWidget *>("mediaViewerControls");
        auto *bottom = viewer.findChild<QWidget *>("qite-video-controls");
        auto *idle = viewer.findChild<QTimer *>("mediaVideoIdleTimer");
        QVERIFY(top && bottom && idle);
        auto *player = bottom->parentWidget();
        QVERIFY(player);
        QVERIFY(top->isVisible() && bottom->isVisible());

        QTest::keyClick(&viewer, Qt::Key_F11);
        QTRY_VERIFY(viewer.isFullScreen());
        QVERIFY(top->isVisible() && bottom->isVisible());
        QVERIFY(idle->isActive());
        // Exercise the timeout deterministically; no dependence on real sleep.
        QVERIFY(QMetaObject::invokeMethod(idle, "timeout", Qt::DirectConnection));
        auto *topEffect = qobject_cast<QGraphicsOpacityEffect *>(top->graphicsEffect());
        auto *bottomEffect = qobject_cast<QGraphicsOpacityEffect *>(bottom->graphicsEffect());
        QVERIFY(topEffect && bottomEffect);
        QTRY_VERIFY(!top->isVisible());
        QTRY_VERIFY(!bottom->isVisible());
        QCOMPARE(topEffect->opacity(), qreal(0));
        QCOMPARE(bottomEffect->opacity(), qreal(0));
        const QPoint local(25, 25);
        QMouseEvent movement(QEvent::MouseMove, QPointF(local), QPointF(player->mapToGlobal(local)),
                             Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(player, &movement);
        QVERIFY(top->isVisible());
        QVERIFY(bottom->isVisible());
        QVERIFY(idle->isActive());
        QVERIFY(QMetaObject::invokeMethod(idle, "timeout", Qt::DirectConnection));
        QTRY_VERIFY(!top->isVisible());
        QTest::keyClick(player, Qt::Key_Space);
        QVERIFY(top->isVisible());
        QVERIFY(bottom->isVisible());

        QTest::keyClick(&viewer, Qt::Key_Escape);
        QTRY_VERIFY(!viewer.isFullScreen());
        QVERIFY(top->isVisible());
        QVERIFY(bottom->isVisible());
        QCOMPARE(topEffect->opacity(), qreal(1));
        QCOMPARE(bottomEffect->opacity(), qreal(1));
        QVERIFY(!idle->isActive());
    }

    void rapidWheelBurstIsCoalescedAndMultiNotchInputIsPreserved()
    {
        QImage image(80, 60, QImage::Format_RGB32);
        image.fill(Qt::red);
        MediaViewer viewer(image);
        auto *source = new TestNavigationSource;
        viewer.setNavigationSource(source);
        viewer.show();
        QCoreApplication::processEvents();
        const QPoint center = viewer.rect().center();
        const auto wheel = [&](int notches) {
            QWheelEvent event(QPointF(center), QPointF(viewer.mapToGlobal(center)),
                              {}, QPoint(0, notches * 120), Qt::NoButton,
                              Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(&viewer, &event);
            QVERIFY(event.isAccepted());
        };
        wheel(5);
        QCOMPARE(source->requests, 1);
        QCOMPARE(source->direction, -5);
        wheel(3);
        wheel(-1);
        QCOMPARE(source->requests, 1);
        QCOMPARE(source->queued, -2);
        source->pending(image, QStringLiteral("next.png"), true);
        QCOMPARE(viewer.windowTitle(), QStringLiteral("next.png"));
        wheel(-4);
        QCOMPARE(source->requests, 2);
        QCOMPARE(source->direction, 4);
    }

    void navigationUsesInjectedSourceAndCancelsStaleResults()
    {
        QImage original(80, 60, QImage::Format_RGB32);
        original.fill(Qt::red);
        MediaViewer viewer(original, QStringLiteral("/cache/unrelated.png"));
        auto       *first = new TestNavigationSource;
        viewer.setNavigationSource(first);
        viewer.show();
        QCoreApplication::processEvents();

        const QPoint centre = viewer.rect().center();
        const auto   wheel  = [&](int turns, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
            QWheelEvent event(QPointF(centre), QPointF(viewer.mapToGlobal(centre)), {}, QPoint(0, turns * 120),
                                 Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
            QApplication::sendEvent(&viewer, &event);
            QVERIFY(event.isAccepted());
        };

        wheel(-1);
        QCOMPARE(first->requests, 1);
        QCOMPARE(first->direction, 1); // Next unique resource.
        wheel(-1);
        QCOMPARE(first->requests, 1); // Navigation is in flight.
        zoom(viewer, 1);
        QCOMPARE(first->requests, 1); // Ctrl+wheel still zooms, never browses.

        auto  oldResult = first->pending;
        auto *second    = new TestNavigationSource;
        viewer.setNavigationSource(second);
        QImage stale(90, 70, QImage::Format_RGB32);
        stale.fill(Qt::yellow);
        oldResult(stale, QStringLiteral("stale.png"), true);
        QCOMPARE(viewer.windowTitle(), QStringLiteral("unrelated.png"));

        wheel(1);
        QCOMPARE(second->requests, 1);
        QCOMPARE(second->direction, -1);
        QImage next(64, 48, QImage::Format_RGB32);
        next.fill(Qt::blue);
        second->pending(next, QStringLiteral("chat-photo.png"), true);
        QCOMPARE(viewer.windowTitle(), QStringLiteral("chat-photo.png"));
        QCOMPARE(viewer.displayedImageRect().size(), QSizeF(64, 48));
        QVERIFY(!viewer.hasTemporaryZoom());
    }
    void smallImageKeepsOriginalSizeAndCompactWindow()
    {
        QImage image(80, 60, QImage::Format_RGB32);
        image.fill(Qt::red);
        MediaViewer viewer(image);
        viewer.show();
        QCoreApplication::processEvents();

        QVERIFY(viewer.width() <= 400);
        QVERIFY(viewer.height() <= 300);
        QCOMPARE(viewer.currentScale(), qreal(1.0));
        QCOMPARE(viewer.displayedImageRect().size(), QSizeF(80, 60));
        QVERIFY(!viewer.hasTemporaryZoom());
    }

    void largeImageFitsAndPreservesAspectRatio()
    {
        QImage image(2000, 800, QImage::Format_RGB32);
        image.fill(Qt::blue);
        MediaViewer viewer(image);
        viewer.show();
        QCoreApplication::processEvents();

        QVERIFY(viewer.currentScale() > 0);
        QVERIFY(viewer.currentScale() < 1);
        const QRectF displayed = viewer.displayedImageRect();
        QVERIFY(displayed.width() <= viewer.width());
        QVERIFY(displayed.height() <= viewer.height());
        QVERIFY(std::abs(displayed.width() / displayed.height() - 2.5) < 0.001);
        QVERIFY(std::abs(displayed.center().x() - viewer.width() / 2.0) < 0.001);
    }

    void zoomedImageCanBeDraggedAndCannotLeaveEmptyEdges()
    {
        QImage image(1000, 800, QImage::Format_RGB32);
        image.fill(Qt::green);
        MediaViewer viewer(image);
        viewer.resize(350, 270);
        viewer.show();
        QCoreApplication::processEvents();

        for (int i = 0; i < 8; ++i)
            zoom(viewer, +1);
        const QRectF centred = viewer.displayedImageRect();
        QVERIFY(centred.width() > viewer.width() + 100);
        QVERIFY(centred.height() > viewer.height() + 100);
        QCOMPARE(viewer.cursor().shape(), Qt::OpenHandCursor);

        const QPoint centre = viewer.rect().center();
        QTest::mousePress(&viewer, Qt::LeftButton, Qt::NoModifier, centre);
        QCOMPARE(viewer.cursor().shape(), Qt::ClosedHandCursor);
        const QPoint position = centre + QPoint(45, -30);
        QMouseEvent  moved(QEvent::MouseMove, QPointF(position), QPointF(viewer.mapToGlobal(position)), Qt::NoButton,
                           Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&viewer, &moved);
        QTest::mouseRelease(&viewer, Qt::LeftButton, Qt::NoModifier, position);
        QCOMPARE(viewer.cursor().shape(), Qt::OpenHandCursor);
        QCOMPARE(viewer.displayedImageRect().topLeft(), centred.topLeft() + QPointF(45, -30));

        drag(viewer, centre, centre + QPoint(5000, 5000));
        const QRectF atTopLeft = viewer.displayedImageRect();
        QVERIFY(std::abs(atTopLeft.left()) < 0.001);
        QVERIFY(std::abs(atTopLeft.top()) < 0.001);

        drag(viewer, centre, centre - QPoint(5000, 5000));
        const QRectF atBottomRight = viewer.displayedImageRect();
        QVERIFY(std::abs(atBottomRight.right() - viewer.width()) < 0.001);
        QVERIFY(std::abs(atBottomRight.bottom() - viewer.height()) < 0.001);

        viewer.resize(420, 320);
        QCoreApplication::processEvents();
        QVERIFY(!viewer.hasTemporaryZoom());
        QCOMPARE(viewer.cursor().shape(), Qt::ArrowCursor);
        const QRectF fitted = viewer.displayedImageRect();
        QVERIFY(std::abs(fitted.center().x() - viewer.width() / 2.0) < 0.001);
        QVERIFY(std::abs(fitted.center().y() - viewer.height() / 2.0) < 0.001);
    }

    void panOnlyAlongAxisThatOverflowsAndStopWhenZoomedOut()
    {
        QImage image(2500, 100, QImage::Format_RGB32);
        image.fill(Qt::blue);
        MediaViewer viewer(image);
        viewer.resize(400, 300);
        viewer.show();
        QCoreApplication::processEvents();

        for (int i = 0; i < 7; ++i)
            zoom(viewer, +1);
        const QRectF centred = viewer.displayedImageRect();
        QVERIFY(centred.width() > viewer.width());
        QVERIFY(centred.height() < viewer.height());

        const QPoint centre = viewer.rect().center();
        drag(viewer, centre, centre + QPoint(80, 100));
        const QRectF moved = viewer.displayedImageRect();
        QVERIFY(moved.left() > centred.left());
        QCOMPARE(moved.center().y(), centred.center().y());

        for (int i = 0; i < 20; ++i)
            zoom(viewer, -1);
        const QRectF zoomedOut = viewer.displayedImageRect();
        QVERIFY(zoomedOut.width() < viewer.width());
        QCOMPARE(viewer.cursor().shape(), Qt::ArrowCursor);
        drag(viewer, centre, centre + QPoint(80, 100));
        QCOMPARE(viewer.displayedImageRect(), zoomedOut);
    }

    void wheelCanPassOriginalSizeAndWindowManipulationResets()
    {
        QImage image(1000, 800, QImage::Format_RGB32);
        image.fill(Qt::green);
        MediaViewer viewer(image);
        viewer.resize(350, 270);
        viewer.show();
        QCoreApplication::processEvents();

        const qreal fit = viewer.currentScale();
        QVERIFY(fit < 1);
        for (int i = 0; i < 12; ++i)
            zoom(viewer, +1);
        QVERIFY(viewer.hasTemporaryZoom());
        QVERIFY(viewer.currentScale() > 1);
        QVERIFY(viewer.displayedImageRect().width() > viewer.width());

        viewer.resize(470, 350);
        QCoreApplication::processEvents();
        QVERIFY(!viewer.hasTemporaryZoom());
        QVERIFY(viewer.currentScale() < 1);
        QVERIFY(std::abs(viewer.currentScale() - fit) > 0.001);

        const qreal newFit = viewer.currentScale();
        zoom(viewer, -4);
        QVERIFY(viewer.hasTemporaryZoom());
        QVERIFY(viewer.currentScale() < newFit);
        viewer.move(viewer.pos() + QPoint(20, 15));
        QTRY_VERIFY(!viewer.hasTemporaryZoom());
        QVERIFY(viewer.currentScale() <= 1.0);

        zoom(viewer, +1);
        QVERIFY(viewer.hasTemporaryZoom());
        QWindowStateChangeEvent stateChanged(Qt::WindowNoState);
        QApplication::sendEvent(&viewer, &stateChanged);
        QVERIFY(!viewer.hasTemporaryZoom());
    }
};

QTEST_MAIN(MediaViewerTest)
#include "MediaViewerTest.moc"
