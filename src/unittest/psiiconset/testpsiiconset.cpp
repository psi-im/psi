#include <QtTest/QtTest>

#include <QStringList>
#include <Q3PtrList>

#include "iconset.h"
#include "psiiconset.h"

class TestPsiIconset: public QObject
{
	Q_OBJECT
private:
	PsiIconset *is;

private slots:
	void initTestCase()
	{
		is = new PsiIconset();
		
		g.pathBase = "../../tools/iconset/unittest";
		g.pathHome = getHomeDir();
		g.pathProfiles = g.pathHome + "/profiles";
	}

	void cleanupTestCase()
	{
		delete is;
	}

	void testLoadSystem()
	{
		QVERIFY(is->loadSystem());
	}

	// this mustn't produce any valgrind errors, so
	// it's pretty safe to generate suppressions 
	// from this test: just run "./testpsiiconset testLoadImage"
	void testLoadImage()
	{
		QImage *img = new QImage(":iconsets/system/default/psimain.png");
		delete img;
	}
		
	void testLoadAll()
	{
		// checking for memory leaks this way
		for (int i = 0; i < 3; i++)
			QVERIFY(is->loadAll());
	}
		
	void testBasePath()
	{
		Iconset *iconset = new Iconset();
		QVERIFY(iconset->load(g.pathBase + "/iconsets/roster/default.jisp"));
		delete iconset;
	}
		
	void testChangeOptions()
	{
		Options oldOptions = option;
		option.systemIconset = "crystal_system.jisp";
		option.emoticons = (QStringList() << "puz.jisp");
		option.defaultRosterIconset = "default.jisp";
		
		QVERIFY(is->optionsChanged(&oldOptions));
		
		QCOMPARE(is->system().name(),        QString("Crystal (System)"));   
		QCOMPARE(is->system().description(), QString("Crystal System Iconset"));   
		QCOMPARE(is->system().version(),     QString("0.3"));   
		QCOMPARE(is->emoticons.first()->name(), QString("puzazBox"));
		
		option = oldOptions;
	}
};

QTEST_MAIN(TestPsiIconset)
#include "testpsiiconset.moc"
