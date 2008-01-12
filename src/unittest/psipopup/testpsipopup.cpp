#include <QtTest/QtTest>
#include <QApplication>
#include <QTimer>
#include <QtCrypto>

#include "psipopup.h"
#include "iconset.h"
#include "psicon.h" // for PsiCon
#include "psiaccount.h" // for PsiAccount
#include "profiles.h" // for UserAccount
#include "im.h" // for Jid
#include "userlist.h" // for UserListItem
#include "mainwin.h"

class TestPsiPopup: public QObject
{
	Q_OBJECT
private:
	PsiCon *psi;
	PsiAccount *account;
	bool timeout;
	QCA::Initializer *qca_init;

	void delay(int msecs)
	{
		timeout = false;
		QTimer::singleShot(msecs, this, SLOT(timedOut()));
		while (!timeout)
			qApp->processEvents();
	}

protected slots:
	void timedOut()
	{
		timeout = true;
	}

private slots:
	void initTestCase()
	{
		// initialize the minimal amount of stuff necessary
		// to show PsiPopups on screen
// 		is = new PsiIconset();
// 		QVERIFY(is->loadAll());
		
		qca_init = new QCA::Initializer();
		QCA::keyStoreManager()->start();
		QCA::keyStoreManager()->waitForBusyFinished();
		
		psi = new PsiCon();
		psi->init();
		UserAccount userAccount;
		account = new PsiAccount(userAccount, psi);
	}

	void cleanupTestCase()
	{
		delete psi;
		QCA::unloadAllPlugins();
		delete qca_init;
	}
	
	void testPsiPopup()
	{
// 		Q3MainWindow window;
// 		window.show();
		delay(10000);
		
		option.ppHideTime = 10 * 1000; // 10 seconds
		option.ppBorderColor = Qt::blue;
		option.ppIsOn = true;

		PsiPopup *popup = new PsiPopup(PsiPopup::AlertChat, account);
		Jid jid("mblsha@jabber.ru");
		Resource resource("PowerBook");
		UserListItem userListItem;
		MessageEvent psiEvent(account);
		popup->setData(jid, resource, &userListItem, &psiEvent);
		
		delay(15000);
	}
};

QTEST_MAIN(TestPsiPopup)
#include "testpsipopup.moc"
