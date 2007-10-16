#include <QDebug>

#include "guitest.h"
#include "guitestmanager.h"
#include "mockprivacymanager.h"
#include "privacydlg.h"

class PrivacyDlgTest : public GUITest
{
public:
	PrivacyDlgTest();

	QString name() { return "PrivacyDlgTest"; }
	bool run();
};

PrivacyDlgTest::PrivacyDlgTest()
{
	GUITestManager::instance()->registerTest(this);
}

bool PrivacyDlgTest::run() 
{
	PrivacyDlg* dlg = new PrivacyDlg("MyAccount", new MockPrivacyManager());
	dlg->exec();
	return false;
}

static PrivacyDlgTest* privacyDlgTestInstance = new PrivacyDlgTest();
