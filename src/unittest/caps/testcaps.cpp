#include <QtTest/QtTest>

#include "xmpp_jid.h"
#include "capsregistry.h"
#include "capsmanager.h"

class TestCaps : public QObject
{
	Q_OBJECT

private slots:
	void registry();
	void manager();
};

void TestCaps::registry()
{
	QCOMPARE(true,true);
}

void TestCaps::manager()
{
	QCOMPARE(true,true);
}

QTEST_MAIN(TestCaps)

#include "testcaps.moc"
