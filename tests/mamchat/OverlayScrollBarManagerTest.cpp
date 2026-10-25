#include <QtTest>

#include <QAbstractScrollArea>
#include <QApplication>
#include <QCursor>
#include <QGraphicsOpacityEffect>
#include <QScrollBar>
#include <QToolButton>

#include "widgets/OverlayScrollBarManager.h"

namespace {

void settleEvents(int rounds = 6)
{
    for (int i = 0; i < rounds; ++i) {
        QCoreApplication::processEvents();
    }
}

} // namespace

class OverlayScrollBarManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        Mattermost::OverlayScrollBarManager::install(*qApp);
    }

    void leavingDuringRevealCancelsTheRevealAnimation()
    {
        QAbstractScrollArea area;
        area.setProperty("psiOverlayScrollBar", true);
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
        area.setProperty("psiOverlayScrollBar", true);
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
        area.setProperty("psiOverlayScrollBar", true);
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
        area.setProperty("psiOverlayScrollBar", true);
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
        area.setProperty("psiOverlayScrollBar", true);
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
};

QTEST_MAIN(OverlayScrollBarManagerTest)
#include "OverlayScrollBarManagerTest.moc"
