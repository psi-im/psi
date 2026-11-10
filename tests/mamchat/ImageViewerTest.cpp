/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/ImageViewer.h"

#include <QApplication>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QWheelEvent>
#include <QWindowStateChangeEvent>
#include <QtTest/QtTest>
#include <cmath>

namespace {
void zoom(ImageViewer &viewer, int notches)
{
    const QPoint centre = viewer.rect().center();
    QWheelEvent wheel(QPointF(centre), QPointF(viewer.mapToGlobal(centre)),
                      QPoint(), QPoint(0, notches * 120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&viewer, &wheel);
    QVERIFY(wheel.isAccepted());
}

void drag(ImageViewer &viewer, const QPoint &from, const QPoint &to)
{
    QTest::mousePress(&viewer, Qt::LeftButton, Qt::NoModifier, from);
    QMouseEvent moved(QEvent::MouseMove, QPointF(to), QPointF(viewer.mapToGlobal(to)),
                      Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&viewer, &moved);
    QTest::mouseRelease(&viewer, Qt::LeftButton, Qt::NoModifier, to);
}
}

class ImageViewerTest final : public QObject {
    Q_OBJECT

private slots:
    void smallImageKeepsOriginalSizeAndCompactWindow()
    {
        QImage image(80, 60, QImage::Format_RGB32);
        image.fill(Qt::red);
        ImageViewer viewer(image);
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
        ImageViewer viewer(image);
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
        ImageViewer viewer(image);
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
        QMouseEvent moved(QEvent::MouseMove, QPointF(position), QPointF(viewer.mapToGlobal(position)),
                          Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
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
        ImageViewer viewer(image);
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
        ImageViewer viewer(image);
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

QTEST_MAIN(ImageViewerTest)
#include "ImageViewerTest.moc"
