#include <QtTest>

#include <QAbstractScrollArea>
#include <QApplication>
#include <QCursor>
#include <QGraphicsOpacityEffect>
#include <QScrollBar>
#include <QToolButton>
#include <QStandardItemModel>
#include <QTreeView>
#include <QWheelEvent>

#include "widgets/OverlayScrollBarManager.h"

namespace {

void settleEvents(int rounds = 6)
{
    for (int i = 0; i < rounds; ++i) {
        QCoreApplication::processEvents();
    }
}

class WheelCountingArea : public QAbstractScrollArea
{
public:
    int wheels = 0;
    QPoint lastAngleDelta;

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        ++wheels;
        lastAngleDelta = event->angleDelta();
        event->accept();
    }
};

} // namespace

class OverlayScrollBarManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void attachIsIdempotentAndKeepsTheOriginalScrollModel()
    {
        QAbstractScrollArea area;
        QScrollBar *source = area.verticalScrollBar();
        source->setRange(0, 100);
        Mattermost::OverlayScrollBarManager::attach(area);
        Mattermost::OverlayScrollBarManager::attach(area);

        auto overlays = area.findChildren<QScrollBar *>(
            QStringLiteral("mattermostOverlayVerticalScrollBar"));
        QCOMPARE(overlays.size(), 1);
        QCOMPARE(area.verticalScrollBar(), source);
        QCOMPARE(area.verticalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);

        source->setValue(42);
        QCOMPARE(overlays.first()->value(), 42);
    }

    void attachAfterAreaIsAlreadyVisible()
    {
        QAbstractScrollArea area;
        area.resize(240, 140);
        area.verticalScrollBar()->setRange(0, 100);
        area.show();
        settleEvents();
        QVERIFY(!area.findChild<QScrollBar *>(
            QStringLiteral("mattermostOverlayVerticalScrollBar")));

        Mattermost::OverlayScrollBarManager::attach(area);
        auto *overlay = area.findChild<QScrollBar *>(
            QStringLiteral("mattermostOverlayVerticalScrollBar"));
        QVERIFY(overlay);
        QCOMPARE(overlay->maximum(), 100);
        QVERIFY(overlay->isVisible());
    }

    void optedInRostersGetOverlayWithoutAffectingOtherScrollAreas()
    {
        QTreeView roster;
        QStandardItemModel model;
        for (int row = 0; row < 50; ++row)
            model.appendRow(new QStandardItem(QString::number(row)));
        roster.setModel(&model);
        roster.resize(220, 170);
        roster.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        Mattermost::OverlayScrollBarManager::attach(roster);
        roster.show();

        QTreeView unrelated;
        unrelated.setModel(&model);
        unrelated.resize(220, 170);
        unrelated.show();
        settleEvents();

        auto *overlay = roster.findChild<QScrollBar *>(
            QStringLiteral("mattermostOverlayVerticalScrollBar"));
        QVERIFY(overlay);
        QCOMPARE(roster.verticalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
        QVERIFY(roster.verticalScrollBar()->maximum() > 0);
        QVERIFY(!unrelated.findChild<QScrollBar *>(
            QStringLiteral("mattermostOverlayVerticalScrollBar")));

        roster.verticalScrollBar()->setValue(10);
        settleEvents();
        QCOMPARE(overlay->value(), 10);
    }

    void leavingDuringRevealCancelsTheRevealAnimation()
    {
        QAbstractScrollArea area;
        Mattermost::OverlayScrollBarManager::attach(area);
        area.resize(320, 240);
        area.move(200, 200);
        QCursor::setPos(50, 50);
        area.verticalScrollBar()->setRange(0, 100);
        area.verticalScrollBar()->setPageStep(20);
        area.show();
        settleEvents();
        auto *overlay = area.findChild<QScrollBar *>(QStringLiteral("mattermostOverlayVerticalScrollBar"));
        QVERIFY(overlay);
        auto *opacity = qobject_cast<QGraphicsOpacityEffect *>(overlay->graphicsEffect());
        QVERIFY(opacity);
        area.verticalScrollBar()->setValue(1);
        // Leave before the first reveal animation frame. A fade to the current
        // zero opacity must still stop the pending animation towards one.
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(area.viewport(), &leave);
        QTest::qWait(300);
        QVERIFY(opacity->opacity() < 0.01);
    }

    void lostLeaveDoesNotKeepOverlayVisible()
    {
        QAbstractScrollArea area;
        Mattermost::OverlayScrollBarManager::attach(area);
        area.resize(320, 240);
        area.move(200, 200);
        area.verticalScrollBar()->setRange(0, 100);
        area.verticalScrollBar()->setPageStep(20);
        area.show();
        settleEvents();
        auto *overlay = area.findChild<QScrollBar *>(QStringLiteral("mattermostOverlayVerticalScrollBar"));
        QVERIFY(overlay);
        QCursor::setPos(overlay->mapToGlobal(overlay->rect().center()));
        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(overlay, &enter);
        auto *opacity = qobject_cast<QGraphicsOpacityEffect *>(overlay->graphicsEffect());
        QVERIFY(opacity);
        QTRY_VERIFY(opacity->opacity() > 0.9);
        // Simulate the missing Leave notification that can occur on geometry
        // changes or desktop/window transitions, rather than changing focus policy.
        class DropLeave : public QObject {
            bool eventFilter(QObject *, QEvent *event) override { return event->type() == QEvent::Leave; }
        } filter;
        qApp->installEventFilter(&filter);
        QCursor::setPos(area.mapToGlobal(QPoint(-50, -50)));
        QTRY_VERIFY_WITH_TIMEOUT(opacity->opacity() < 0.01, 4000);
        qApp->removeEventFilter(&filter);
    }

    void edgeButtonsTrackPositionAndTriggerRealSliderActions()
    {
        QAbstractScrollArea area;
        Mattermost::OverlayScrollBarManager::attach(area);
        area.resize(320, 240);
        area.verticalScrollBar()->setRange(0, 100);
        area.verticalScrollBar()->setPageStep(20);
        area.show();
        settleEvents();

        auto* toStart = area.findChild<QToolButton*>(
            QStringLiteral("mattermostOverlayScrollToStartButton"));
        auto* toEnd = area.findChild<QToolButton*>(
            QStringLiteral("mattermostOverlayScrollToEndButton"));
        QVERIFY(toStart);
        QVERIFY(toEnd);

        QVERIFY(!toStart->isVisible());
        QVERIFY(toEnd->isVisible());

        area.verticalScrollBar()->setValue(50);
        settleEvents();
        QVERIFY(toStart->isVisible());
        QVERIFY(toEnd->isVisible());

        QTest::mouseClick(toEnd, Qt::LeftButton);
        settleEvents();
        QCOMPARE(area.verticalScrollBar()->value(),
                 area.verticalScrollBar()->maximum());
        QVERIFY(toStart->isVisible());
        QVERIFY(!toEnd->isVisible());

        area.verticalScrollBar()->setValue(50);
        settleEvents();
        QTest::mouseClick(toStart, Qt::LeftButton);
        settleEvents();
        QCOMPARE(area.verticalScrollBar()->value(),
                 area.verticalScrollBar()->minimum());
    }

    void edgeNavigationButtonsCanBeDisabledTogether()
    {
        QAbstractScrollArea area;
        Mattermost::OverlayScrollBarManager::attach(area);
        area.resize(320, 240);
        area.verticalScrollBar()->setRange(0, 100);
        area.verticalScrollBar()->setValue(50);
        Mattermost::OverlayScrollBarManager::setEdgeNavigationButtonsEnabled(
            area, false);
        area.show();
        settleEvents();

        auto* toStart = area.findChild<QToolButton*>(
            QStringLiteral("mattermostOverlayScrollToStartButton"));
        auto* toEnd = area.findChild<QToolButton*>(
            QStringLiteral("mattermostOverlayScrollToEndButton"));
        QVERIFY(toStart);
        QVERIFY(toEnd);
        QVERIFY(!toStart->isVisible());
        QVERIFY(!toEnd->isVisible());

        auto* overlay = area.findChild<QScrollBar*>(
            QStringLiteral("mattermostOverlayVerticalScrollBar"));
        QVERIFY(overlay);
        QVERIFY(overlay->isVisible());

        Mattermost::OverlayScrollBarManager::setEdgeNavigationButtonsEnabled(
            area, true);
        settleEvents();
        QVERIFY(toStart->isVisible());
        QVERIFY(toEnd->isVisible());
    }

    void startActionCanBeDisabledWithoutAffectingEndAction()
    {
        QAbstractScrollArea area;
        Mattermost::OverlayScrollBarManager::attach(area);
        area.resize(320, 240);
        area.verticalScrollBar()->setRange(0, 100);
        area.verticalScrollBar()->setValue(50);
        Mattermost::OverlayScrollBarManager::setScrollToStartButtonEnabled(area, false);
        area.show();
        settleEvents();

        auto* toStart = area.findChild<QToolButton*>(
            QStringLiteral("mattermostOverlayScrollToStartButton"));
        auto* toEnd = area.findChild<QToolButton*>(
            QStringLiteral("mattermostOverlayScrollToEndButton"));
        QVERIFY(toStart);
        QVERIFY(toEnd);
        QVERIFY(!toStart->isVisible());
        QVERIFY(toEnd->isVisible());

        Mattermost::OverlayScrollBarManager::setScrollToStartButtonEnabled(area, true);
        settleEvents();
        QVERIFY(toStart->isVisible());
    }

    void wheelOverOverlayIsHandledByTheAreaItself()
    {
        WheelCountingArea area;
        area.resize(240, 140);
        area.verticalScrollBar()->setRange(0, 1000);
        Mattermost::OverlayScrollBarManager::attach(area);
        area.show();
        settleEvents();
        auto *overlay = area.findChild<QScrollBar *>(
            QStringLiteral("mattermostOverlayVerticalScrollBar"));
        QVERIFY(overlay);
        QVERIFY(overlay->isVisible());

        const QPoint local(overlay->width() / 2, overlay->height() / 2);
        QWheelEvent wheel(QPointF(local), QPointF(overlay->mapToGlobal(local)), QPoint(),
                          QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(overlay, &wheel);

        // The area's own wheel handling decides the distance; the overlay's
        // QScrollBar wheel step (singleStep * wheelScrollLines) is bypassed.
        QCOMPARE(area.wheels, 1);
        QCOMPARE(area.lastAngleDelta, QPoint(0, -120));
        QCOMPARE(area.verticalScrollBar()->value(), 0);
        QVERIFY(wheel.isAccepted());
    }
};

QTEST_MAIN(OverlayScrollBarManagerTest)
#include "OverlayScrollBarManagerTest.moc"
