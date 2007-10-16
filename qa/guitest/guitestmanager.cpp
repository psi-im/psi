#include <QDebug>

#include "guitestmanager.h"

GUITestManager::GUITestManager()
{
}

GUITestManager* GUITestManager::instance()
{
	if (!instance_) {
		instance_ = new GUITestManager();
	}
	return instance_;
}

void GUITestManager::registerTest(GUITest* test) 
{
	tests_ += test;
}

bool GUITestManager::runTest(const QString& name)
{
	foreach(GUITest* test, tests_) {
		if (test->name() == name) {
			return test->run();
		}
	}
	qWarning() << "Test not found: " << name;
	return false;
}

QStringList GUITestManager::getTestNames() const
{
	QStringList test_names;
	foreach(GUITest* test, tests_) {
		test_names += test->name();
	}
	return test_names;
}


GUITestManager* GUITestManager::instance_ = NULL;
