/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/MediaViewer.h"

#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QToolButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QTimer>
#include <QLabel>
#include <QProgressBar>
#include <QScreen>
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
    void fullscreenPhotoAutoHidesToolbarWithoutMovingImage()
    {
        QImage image(240, 140, QImage::Format_RGB32);
        image.fill(Qt::blue);
        MediaViewer viewer(image);
        auto *source = new TestNavigationSource;
        viewer.setNavigationSource(source);
        viewer.show();
        QCoreApplication::processEvents();
        auto *top = viewer.findChild<QWidget *>(QStringLiteral("mediaViewerControls"));
        auto *idle = viewer.findChild<QTimer *>(QStringLiteral("mediaFullscreenIdleTimer"));
        QVERIFY(top && idle);
        QVERIFY(top->isVisible());

        QTest::keyClick(&viewer, Qt::Key_F);
        QTRY_VERIFY(viewer.isFullScreen());
        // Offscreen QPA delivers the final fullscreen resize asynchronously;
        // capture geometry only after the window has reached screen size.
        QVERIFY(viewer.screen());
        QTRY_COMPARE(viewer.size(), viewer.screen()->geometry().size());
        QVERIFY(top->isVisible());
        QVERIFY(idle->isActive());
        const QRectF shown = viewer.displayedImageRect();

        QVERIFY(QMetaObject::invokeMethod(idle, "timeout", Qt::DirectConnection));
        QTRY_VERIFY(!top->isVisible());
        QCOMPARE(viewer.displayedImageRect(), shown);
        auto *effect = qobject_cast<QGraphicsOpacityEffect *>(top->graphicsEffect());
        QVERIFY(effect);
        QCOMPARE(effect->opacity(), qreal(0));

        const QPoint point(40, qMax(50, viewer.height() / 2));
        QMouseEvent movement(QEvent::MouseMove, QPointF(point), QPointF(viewer.mapToGlobal(point)),
                             Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&viewer, &movement);
        QVERIFY(top->isVisible());
        QVERIFY(idle->isActive());
        QCOMPARE(viewer.displayedImageRect(), shown);

        QTest::keyClick(&viewer, Qt::Key_F);
        QTRY_VERIFY(!viewer.isFullScreen());
        QVERIFY(top->isVisible());
        QVERIFY(!idle->isActive());
        QCOMPARE(effect->opacity(), qreal(1));
    }

    void borrowedPlayerReturnsToChatOnNavigationAndClose()
    {
        QWidget inlineHost;
        inlineHost.resize(320, 200);
        inlineHost.show();
        auto *player = new QWidget(&inlineHost);
        QPointer<QWidget> retained(player);
        int restored = 0;
        {
            MediaViewer viewer(QImage{});
            auto *source = new TestNavigationSource;
            viewer.setNavigationSource(source);
            viewer.show();

            auto loan = [&](const QString &name) {
                MediaNavigationSource::View video;
                video.key = name;
                video.name = QStringLiteral("video.mp4");
                video.revision = 1;
                video.state = MediaNavigationSource::State::Ready;
                video.video = [player](QWidget *parent, std::function<void()>) {
                    player->setParent(parent);
                    return player;
                };
                video.videoReturn = [&](QWidget *widget) {
                    if (widget != player)
                        return false;
                    ++restored;
                    widget->setParent(&inlineHost);
                    widget->show();
                    return true;
                };
                emit source->changed(video);
            };

            loan(QStringLiteral("first"));
            QCOMPARE(player->parentWidget(), &viewer);
            MediaNavigationSource::View image;
            image.key = QStringLiteral("photo");
            image.revision = 1;
            image.name = QStringLiteral("photo.png");
            image.state = MediaNavigationSource::State::Ready;
            image.image = QImage(20, 20, QImage::Format_RGB32);
            emit source->changed(image);
            QCOMPARE(restored, 1);
            QCOMPARE(player->parentWidget(), &inlineHost);
            QVERIFY(retained);
            loan(QStringLiteral("second"));
            QCOMPARE(player->parentWidget(), &viewer);
            QCOMPARE(restored, 1);
        }
        QCOMPARE(restored, 2);
        QCOMPARE(player->parentWidget(), &inlineHost);
        QVERIFY(retained);
    }

    void borrowedPlayerSurvivesIndexRefreshAndIsDeletedIfChatIsGone()
    {
        MediaViewer viewer(QImage{});
        auto *source = new TestNavigationSource;
        viewer.setNavigationSource(source);
        viewer.show();

        QPointer<QWidget> borrowed;
        int created = 0;
        int returned = 0;
        MediaNavigationSource::View initial;
        initial.key = QStringLiteral("borrowed:clip");
        initial.revision = 1;
        initial.name = QStringLiteral("clip.mp4");
        initial.video = [&](QWidget *parent, std::function<void()>) {
            ++created;
            borrowed = new QWidget(parent);
            return borrowed.data();
        };
        initial.videoReturn = [&](QWidget *) {
            ++returned;
            return false; // The chat/message was deleted while viewing.
        };
        emit source->changed(initial);
        QVERIFY(borrowed);
        QCOMPARE(created, 1);

        // Resolving the initial SQL/gallery index must not create a second
        // decoder for the same resource or change its playback state.
        MediaNavigationSource::View indexed = initial;
        indexed.revision = 2;
        indexed.index = 2;
        indexed.total = 6;
        indexed.video = [&](QWidget *parent, std::function<void()>) {
            ++created;
            return new QWidget(parent);
        };
        emit source->changed(indexed);
        QCOMPARE(created, 1);
        QVERIFY(borrowed);

        MediaNavigationSource::View next;
        next.key = QStringLiteral("next-image");
        next.name = QStringLiteral("photo.png");
        next.revision = 1;
        next.image = QImage(20, 20, QImage::Format_RGB32);
        emit source->changed(next);
        QCOMPARE(returned, 1);
        QVERIFY(borrowed.isNull());
    }

    void videoPlayPauseUsesSpaceAndSurfaceClick()
    {
        MediaViewer viewer(QImage{});
        auto *source = new TestNavigationSource;
        viewer.setNavigationSource(source);
        viewer.show();
        QCoreApplication::processEvents();
        int toggles = 0;

        MediaNavigationSource::View view;
        view.key = QStringLiteral("video");
        view.name = QStringLiteral("video.mp4");
        view.revision = 1;
        view.state = MediaNavigationSource::State::Ready;
        view.video = [&toggles](QWidget *parent, std::function<void()>) {
            auto *player = new QWidget(parent);
            auto *layout = new QVBoxLayout(player);
            layout->setContentsMargins(0, 0, 0, 0);
            auto *surface = new QWidget(player);
            surface->setObjectName(QStringLiteral("testPlaybackSurface"));
            layout->addWidget(surface, 1);
            auto *bottom = new QWidget(player);
            bottom->setObjectName(QStringLiteral("qite-video-controls"));
            auto *bottomLayout = new QVBoxLayout(bottom);
            auto *play = new QToolButton(bottom);
            play->setObjectName(QStringLiteral("qite-video-play"));
            bottomLayout->addWidget(play);
            auto *seek = new QSlider(Qt::Horizontal, bottom);
            seek->setObjectName(QStringLiteral("testSeekSlider"));
            seek->setRange(0, 100);
            bottomLayout->addWidget(seek);
            layout->addWidget(bottom);
            QObject::connect(play, &QToolButton::clicked, play, [&toggles] { ++toggles; });
            return player;
        };
        emit source->changed(view);
        auto *surface = viewer.findChild<QWidget *>(QStringLiteral("testPlaybackSurface"));
        auto *play = viewer.findChild<QToolButton *>(QStringLiteral("qite-video-play"));
        auto *seek = viewer.findChild<QSlider *>(QStringLiteral("testSeekSlider"));
        QVERIFY(surface && play && seek);

        QTest::keyClick(&viewer, Qt::Key_Space);
        QCOMPARE(toggles, 1);
        QTest::keyClick(surface, Qt::Key_Space);
        QCOMPARE(toggles, 2);
        QTest::mouseClick(surface, Qt::LeftButton);
        QCOMPARE(toggles, 3);
        // Native controls must not trigger two playback toggles.
        QTest::mouseClick(play, Qt::LeftButton);
        QCOMPARE(toggles, 4);
        // A seek bar click must not propagate into a pause, even if the
        // slider leaves a mouse release for its parent to handle.
        QTest::mouseClick(seek, Qt::LeftButton);
        QCOMPARE(toggles, 4);
        QWidget *player = seek->parentWidget()->parentWidget();
        QVERIFY(player);
        const QPoint controlPoint = player->mapFromGlobal(seek->mapToGlobal(seek->rect().center()));
        QTest::mouseClick(player, Qt::LeftButton, Qt::NoModifier, controlPoint);
        QCOMPARE(toggles, 4);
        QTest::mouseClick(&viewer, Qt::LeftButton);
        QCOMPARE(toggles, 5);
        // Only the player has a Space playback action, never still images.
        MediaNavigationSource::View still;
        still.key = QStringLiteral("still");
        still.name = QStringLiteral("still.png");
        still.revision = 1;
        emit source->changed(still);
        QTest::keyClick(&viewer, Qt::Key_Space);
        QCOMPARE(toggles, 5);
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
            surface->setObjectName(QStringLiteral("testVideoSurface"));
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
        auto *idle = viewer.findChild<QTimer *>("mediaFullscreenIdleTimer");
        QVERIFY(top && bottom && idle);
        auto *player = bottom->parentWidget();
        QVERIFY(player);
        auto *surface = player->findChild<QWidget *>(QStringLiteral("testVideoSurface"));
        QVERIFY(surface);
        QVERIFY(player->layout());
        QTRY_VERIFY(surface->height() < player->height()); // Normal window: in layout.
        QVERIFY(player->layout()->indexOf(bottom) >= 0);
        QVERIFY(top->isVisible() && bottom->isVisible());

        QTest::keyClick(&viewer, Qt::Key_F11);
        QTRY_VERIFY(viewer.isFullScreen());
        QVERIFY(viewer.screen());
        QTRY_COMPARE(viewer.size(), viewer.screen()->geometry().size());
        QVERIFY(top->isVisible() && bottom->isVisible());
        QVERIFY(idle->isActive());
        // Fullscreen: controls are layered above the player rather than
        // consuming height from its QVBoxLayout.
        QTRY_COMPARE(player->layout()->indexOf(bottom), -1);
        QTRY_COMPARE(surface->geometry(), player->rect());
        const QRect fullSurface = surface->geometry();
        QTRY_VERIFY(bottom->geometry().bottom() <= player->height());
        QVERIFY(bottom->geometry().top() >= 0);
        QVERIFY(bottom->geometry().width() > 0);
        // Exercise the timeout deterministically; no dependence on real sleep.
        QVERIFY(QMetaObject::invokeMethod(idle, "timeout", Qt::DirectConnection));
        auto *topEffect = qobject_cast<QGraphicsOpacityEffect *>(top->graphicsEffect());
        auto *bottomEffect = qobject_cast<QGraphicsOpacityEffect *>(bottom->graphicsEffect());
        QVERIFY(topEffect && bottomEffect);
        QTRY_VERIFY(!top->isVisible());
        QTRY_VERIFY(!bottom->isVisible());
        QCOMPARE(topEffect->opacity(), qreal(0));
        QCOMPARE(bottomEffect->opacity(), qreal(0));
        QCOMPARE(surface->geometry(), fullSurface); // Fading must not resize playback.
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
        QTRY_VERIFY(player->layout()->indexOf(bottom) >= 0);
        QTRY_VERIFY(surface->height() < player->height()); // Restored normal layout.
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
