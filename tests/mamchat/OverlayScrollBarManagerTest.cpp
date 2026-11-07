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

    void suppressingAnAxisKeepsNativeChromeHidden_data()
    {
        QTest::addColumn<bool>("horizontal");
        QTest::newRow("horizontal") << true;
        QTest::newRow("vertical") << false;
    }

    void suppressingAnAxisKeepsNativeChromeHidden()
    {
        QFETCH(bool, horizontal);
        const Qt::Orientation orientation = horizontal ? Qt::Horizontal : Qt::Vertical;
        QAbstractScrollArea area;
        area.resize(320, 240);
        area.horizontalScrollBar()->setRange(0, 100);
        area.verticalScrollBar()->setRange(0, 100);
        auto *source = horizontal ? area.horizontalScrollBar() : area.verticalScrollBar();
        // Suppression also works before the overlays are attached.
        Mattermost::OverlayScrollBarManager::setScrollBarEnabled(area, orientation, false);
        Mattermost::OverlayScrollBarManager::attach(area);
        area.show();
        settleEvents();

        auto *overlay = area.findChild<QScrollBar *>(horizontal
            ? "mattermostOverlayHorizontalScrollBar" : "mattermostOverlayVerticalScrollBar");
        auto *other = area.findChild<QScrollBar *>(horizontal
            ? "mattermostOverlayVerticalScrollBar" : "mattermostOverlayHorizontalScrollBar");
        QVERIFY(overlay && other);
        QVERIFY(!overlay->isVisible());
        QVERIFY(other->isVisible());

        for (int cycle = 0; cycle < 3; ++cycle) {
            Mattermost::OverlayScrollBarManager::setScrollBarEnabled(area, orientation, true);
            settleEvents();
            QVERIFY(overlay->isVisible());
            QVERIFY(!area.horizontalScrollBar()->isVisible());
            QVERIFY(!area.verticalScrollBar()->isVisible());
            QCOMPARE(area.horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
            QCOMPARE(area.verticalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
            overlay->triggerAction(QAbstractSlider::SliderToMaximum);
            QCOMPARE(source->value(), 100);

            Mattermost::OverlayScrollBarManager::setScrollBarEnabled(area, orientation, false);
            // Range/value changes and deferred layout/reveal callbacks must
            // not resurrect an explicitly suppressed overlay or native bar.
            source->setRange(0, 120 + cycle);
            source->setValue(20 + cycle);
            source->setRange(0, 100);
            area.resize(330 + cycle, 250 + cycle);
            settleEvents();
            QVERIFY(!overlay->isVisible());
            QVERIFY(other->isVisible());
            QVERIFY(!source->isVisible());
        }
        QCOMPARE(horizontal ? area.horizontalScrollBar() : area.verticalScrollBar(), source);
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

    void bottomBadgeAndGuardRemainGenericAndClickable()
    {
        QAbstractScrollArea area;
        area.resize(320, 240);
        area.verticalScrollBar()->setRange(0, 100);
        area.verticalScrollBar()->setPageStep(20);
        Mattermost::OverlayScrollBarManager::attach(area);
        area.show();
        settleEvents();

        auto *button = area.findChild<QToolButton *>(
            QStringLiteral("mattermostOverlayScrollToEndButton"));
        auto *badge = area.findChild<QWidget *>(
            QStringLiteral("mattermostOverlayBottomBadge"));
        auto *guard = area.findChild<QWidget *>(
            QStringLiteral("mattermostOverlayScrollToEndGuard"));
        QVERIFY(button && badge && guard);
        QVERIFY(button->isVisible());
        QVERIFY(!badge->isVisible());
        QVERIFY(guard->isVisible());
        QCOMPARE(guard->parentWidget(), static_cast<QWidget *>(&area));
        QCOMPARE(guard->size(), QSize(button->width() * 3, button->height() * 3));
        QCOMPARE(button->pos(), guard->pos() + QPoint(button->width(), button->height()));
        QVERIFY(Mattermost::OverlayScrollBarManager::isEdgeNavigationExclusion(guard));
        QVERIFY(Mattermost::OverlayScrollBarManager::isEdgeNavigationExclusion(button));
        QVERIFY(!Mattermost::OverlayScrollBarManager::isEdgeNavigationExclusion(area.viewport()));

        area.setProperty("mattermostOverlayBottomBadgeText", QStringLiteral("42"));
        settleEvents();
        QVERIFY(badge->isVisible());
        QVERIFY(badge->testAttribute(Qt::WA_TransparentForMouseEvents));
        QVERIFY(badge->geometry().bottom() <
                button->geometry().top() - guard->geometry().top());
        QCOMPARE(badge->parentWidget(), guard);

        // The real arrow retains click priority over the invisible halo.
        QCOMPARE(area.childAt(button->geometry().center()), static_cast<QWidget *>(button));
        QTest::mouseClick(button, Qt::LeftButton);
        settleEvents();
        QCOMPARE(area.verticalScrollBar()->value(), 100);
        QVERIFY(!button->isVisible());
        QVERIFY(!guard->isVisible());
        QVERIFY(!badge->isVisible());

        area.verticalScrollBar()->setValue(40);
        area.setProperty("mattermostOverlayBottomBadgeText", QString());
        settleEvents();
        QVERIFY(button->isVisible());
        QVERIFY(!badge->isVisible());
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
