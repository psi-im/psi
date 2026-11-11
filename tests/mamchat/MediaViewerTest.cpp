/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/MediaViewer.h"

#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QWheelEvent>
#include <QWindowStateChangeEvent>
#include <QtTest/QtTest>
#include <cmath>

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
    void filtersAndPositionShareTheSourceSelection()
    {
        QImage image(80, 60, QImage::Format_RGB32);
        image.fill(Qt::red);
        MediaViewer viewer(image);
        auto        source = new TestNavigationSource;
        viewer.setNavigationSource(source);
        viewer.show();
        QCoreApplication::processEvents();
        auto videos  = viewer.findChild<QCheckBox *>("mediaFilter0_2");
        auto partial = viewer.findChild<QCheckBox *>("mediaFilter1_2");
        QVERIFY(videos && partial);
        videos->setChecked(false);
        partial->setChecked(false);
        QCOMPARE(source->filter.types, int(MediaFilter::Images | MediaFilter::Files));
        QCOMPARE(source->filter.availability, int(MediaFilter::Remote | MediaFilter::Complete));
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
