#include <QtTest/QtTest>

#include "iconset.h"
#include "anim.h"

class TestIconset: public QObject
{
	Q_OBJECT
private:
	Iconset *iconset;

private slots:
	void initTestCase()
	{
		iconset = new Iconset();
		QVERIFY(iconset->load("iconsets/roster/default.jisp"));
		iconset->addToFactory();
	}

	void cleanupTestCase()
	{
		iconset->clear();
		QCOMPARE(iconset->count(), 0);

		delete iconset;
	}

	void testIconsetData()
	{
		// verify metadata
		QCOMPARE(iconset->name(),            QString("Stellar (default)"));
		QCOMPARE(iconset->version(),         QString("1.0"));
		QCOMPARE(iconset->authors().count(), 2);
		QCOMPARE(iconset->authors()[0],      QString("Jason Kim<br>&nbsp;&nbsp;Email: <a href='mailto:jmkim@uci.edu'>jmkim@uci.edu</a>"));
		QCOMPARE(iconset->authors()[1],      QString("Michail Pishchagin (icondef.xml)<br>&nbsp;&nbsp;Email: <a href='mailto:mblsha@users.sourceforge.net'>mblsha@users.sourceforge.net</a><br>&nbsp;&nbsp;JID: <a href='jabber:mblsha@jabber.ru'>mblsha@jabber.ru</a><br>&nbsp;&nbsp;WWW: <a href='http://maz.sf.net'>http://maz.sf.net</a>"));
		QCOMPARE(iconset->creation(),        QString("2003-07-08"));
		QCOMPARE(iconset->description(),     QString("Default Psi 0.9.1 iconset"));
		
		// verify icons
		QCOMPARE(iconset->count(), 19);
	}

	void testIterator() 
	{
		QStringList iconNames;
		QListIterator<PsiIcon *> it = iconset->iterator();
		while ( it.hasNext() ) {
			PsiIcon *icon = it.next();
			iconNames << icon->name();
		}
		QCOMPARE(iconNames.count(), iconset->count());
	}
	
	void testHeadlineIcon() 
	{
		const PsiIcon *messageHeadline = IconsetFactory::iconPtr("psi/headline");
		QVERIFY(messageHeadline != 0);
		QCOMPARE(messageHeadline->name(),        QString("psi/headline"));
		QCOMPARE(messageHeadline->isAnimated(),  false);
		QCOMPARE(messageHeadline->frameNumber(), 0);
		QCOMPARE(messageHeadline->anim(),        (const Anim *)0);
		QVERIFY(!messageHeadline->impix().isNull());
		QVERIFY(!messageHeadline->pixmap().isNull());
		QCOMPARE(messageHeadline->impix().image().width(), 16);
		QCOMPARE(messageHeadline->impix().image().height(), 16);
		QCOMPARE(messageHeadline->impix().pixmap().width(), 16);
		QCOMPARE(messageHeadline->impix().pixmap().height(), 16);
		QCOMPARE(messageHeadline->pixmap().width(),  16);
		QCOMPARE(messageHeadline->pixmap().height(), 16);
	}
	
	void testAnim()
	{
		const PsiIcon *chat = IconsetFactory::iconPtr("psi/chat");
		QVERIFY(chat->anim() != 0);

		const Anim *anim = chat->anim();
		Anim *copy1 = new Anim(*anim);
		Anim *copy2 = new Anim(*copy1);
		Anim *copy3 = new Anim(*copy2);
		
		// at first, all animations should contain 15 frames
		QCOMPARE(copy3->numFrames(), 15);
		
		copy3->stripFirstFrame();
		QCOMPARE(copy3->numFrames(), 14);
		QCOMPARE(copy2->numFrames(), 15);
		QCOMPARE(copy1->numFrames(), 15);
		QCOMPARE(anim->numFrames(),  15);
		
		int i;
		for (i = 0; i < 2; i++)
			copy2->stripFirstFrame();
		QCOMPARE(copy3->numFrames(), 14);
		QCOMPARE(copy2->numFrames(), 13);
		QCOMPARE(copy1->numFrames(), 15);
		QCOMPARE(anim->numFrames(),  15);

		for (i = 0; i < 5; i++)
			copy1->stripFirstFrame();
		QCOMPARE(copy3->numFrames(), 14);
		QCOMPARE(copy2->numFrames(), 13);
		QCOMPARE(copy1->numFrames(), 10);
		QCOMPARE(anim->numFrames(),  15);
		
		delete copy3;
		delete copy2;
		delete copy1;
	}
		
	void testIconStripping()
	{
		const PsiIcon *chat = IconsetFactory::iconPtr("psi/chat");
		QVERIFY(chat != 0);

		QVERIFY(chat->isAnimated() == true);
		QCOMPARE(chat->anim()->numFrames(), 15);
		
		PsiIcon *stripping = new PsiIcon(*chat);
		QCOMPARE(chat->name(),      QString("psi/chat"));
		QCOMPARE(stripping->name(), QString("psi/chat"));

		stripping->stripFirstAnimFrame();
		QCOMPARE(stripping->anim()->numFrames(), 14);
		QCOMPARE(chat->anim()->numFrames(),      15);

		stripping->setName("yohimbo");
		QCOMPARE(chat->name(),      QString("psi/chat"));
		QCOMPARE(stripping->name(), QString("yohimbo"));

		delete stripping;
	}
	
	void testIconsetFactory()
	{
		QCOMPARE(IconsetFactory::icons().count(), 19);
		QVERIFY(IconsetFactory::iconPtr("psi/message"));
	}

	void testCombineIconsets()
	{
		Iconset *is = new Iconset();
		QVERIFY(is->load("iconsets/roster/small.jisp"));
		
		Iconset *combined = new Iconset(*is);
		QCOMPARE(combined->count(), is->count());

		*combined += *is;
		QCOMPARE(combined->count(), is->count());
		
		QListIterator<PsiIcon*> it = is->iterator();
		while (it.hasNext()) {
			PsiIcon *icon = new PsiIcon(*it.next());
			icon->setName(icon->name() + '2');
			combined->setIcon(icon->name(), *icon);
			delete icon;
		}
		QCOMPARE(combined->count(), 4);
		
		combined->clear();
		QCOMPARE(combined->count(), 0);
		QCOMPARE(is->count(),       2);
		
		delete combined;
		delete is;
	}
	
	void testLoadGifIconset()
	{
		Iconset *is = new Iconset();
		QVERIFY(is->load("iconsets/emoticons/puz.jisp"));
		QVERIFY(is->count() > 0);
		delete is;
	}
		
	void testMultipleIconTextStrings()
	{
		// all puz iconset icons contain multiple
		// text strings. we need to verify that.
		Iconset *is = new Iconset();
		QVERIFY(is->load("iconsets/emoticons/puz.jisp"));
		QVERIFY(is->count() > 0);
		
		QListIterator<PsiIcon*> it = is->iterator();
		while (it.hasNext()) {
			const QList<PsiIcon::IconText> text = it.next()->text();
			QVERIFY(text.count() > 1);
		}
		
		delete is;
	}
		
	void testCreateQIcon()
	{
		const PsiIcon *chat = IconsetFactory::iconPtr("psi/chat");
		QVERIFY(chat != 0);
		
		QIcon icon = chat->icon();
		QVERIFY(!icon.isNull());
	}
};

QTEST_MAIN(TestIconset)
#include "testiconset.moc"
