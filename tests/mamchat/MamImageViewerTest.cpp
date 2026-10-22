/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/MamImageViewer.h"

#include <QApplication>
#include <QMoveEvent>
#include <QWheelEvent>
#include <QtTest/QtTest>
#include <cmath>

namespace {
void zoom(MamImageViewer &viewer, int notches)
{
    const QPoint centre = viewer.rect().center();
    QWheelEvent wheel(QPointF(centre), QPointF(viewer.mapToGlobal(centre)),
                      QPoint(), QPoint(0, notches * 120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&viewer, &wheel);
    QVERIFY(wheel.isAccepted());
}
}

class MamImageViewerTest final : public QObject {
    Q_OBJECT

private slots:
    void smallImageKeepsOriginalSizeAndCompactWindow()
    {
        QImage image(80, 60, QImage::Format_RGB32);
        image.fill(Qt::red);
        MamImageViewer viewer(image);
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
        MamImageViewer viewer(image);
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

    void wheelCanPassOriginalSizeAndWindowManipulationResets()
    {
        QImage image(1000, 800, QImage::Format_RGB32);
        image.fill(Qt::green);
        MamImageViewer viewer(image);
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
        QEvent stateChanged(QEvent::WindowStateChange);
        QApplication::sendEvent(&viewer, &stateChanged);
        QVERIFY(!viewer.hasTemporaryZoom());
    }
};

QTEST_MAIN(MamImageViewerTest)
#include "MamImageViewerTest.moc"
