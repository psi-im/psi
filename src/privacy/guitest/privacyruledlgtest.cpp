#include "guitest.h"
#include "guitestmanager.h"
#include "privacyruledlg.h"
#include <QDebug>

class PrivacyRuleDlgTest : public GUITest
{
public:
	PrivacyRuleDlgTest();

	QString name() { return "PrivacyRuleDlgTest"; }
	bool run();
};

PrivacyRuleDlgTest::PrivacyRuleDlgTest()
{
	GUITestManager::instance()->registerTest(this);
}

bool PrivacyRuleDlgTest::run() 
{
	PrivacyRuleDlg dlg;
	dlg.exec();
	return false;
}

static PrivacyRuleDlgTest* privacyRuleDlgTestInstance = new PrivacyRuleDlgTest();
