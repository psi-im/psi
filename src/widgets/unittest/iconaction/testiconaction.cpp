#include "iconaction.h"
#include "icontoolbutton.h"

#include <QToolBar>
#include <QToolButton>

#include <QtTest/QtTest>
#include <QSignalSpy>

class TestIconAction : public QObject
{
	Q_OBJECT
	
private:
	IconAction *action, *checkableAction;
	
	IconToolButton *findToolButton(QToolBar *toolBar) {
		IconToolButton *toolButton = toolBar->findChild<IconToolButton*>();
		if ( !toolButton )
			throw "toolButton == 0!";
		
		return toolButton;
	}
	
private slots:
	void initTestCase() {
		action = new IconAction("statusTip", "foobar/icon", "text", 0, this, "action");
		
		QCOMPARE(action->statusTip(), QString("statusTip"));
		QCOMPARE(action->text(), QString("text"));
		QCOMPARE(action->psiIconName(), QString("foobar/icon"));
		QCOMPARE(action->isCheckable(), false);
		QVERIFY(action->shortcut().isEmpty());
		QCOMPARE(action->objectName(), QString("action"));
		
		checkableAction = new IconAction("statusTip2", "foobar/icon2", "text2", 0, this, "checkableAction", true);
		QCOMPARE(checkableAction->isCheckable(), true);
	}
	
	void cleanupTestCase() {
		delete action;
		delete checkableAction;
	}

	void testToolButtonClick() {
		QToolBar toolBar;
		action->addTo(&toolBar);
		QSignalSpy spy(action, SIGNAL(triggered()));
		
		IconToolButton *button = findToolButton(&toolBar);
		QCOMPARE(button->isCheckable(), false);
		
		button->click();
		QCOMPARE(spy.count(), 1);
	}
	
	void testToolButtonChecked() {
		QToolBar toolBar;
		checkableAction->addTo(&toolBar);
		QSignalSpy spy(checkableAction, SIGNAL(toggled(bool)));
		
		IconToolButton *button = findToolButton(&toolBar);
		QCOMPARE(button->isCheckable(), true);
		
		QCOMPARE(checkableAction->isChecked(), false);
		button->click();
		QCOMPARE(checkableAction->isChecked(), true);
		button->click();
		QCOMPARE(checkableAction->isChecked(), false);
		
		QCOMPARE(spy.count(), 2);
	}
};

QTEST_MAIN(TestIconAction)
#include "testiconaction.moc"

