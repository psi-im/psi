#include <QApplication>
#include <QDebug>

#include <iostream>

#include "guitestmanager.h"

int main(int argc, char** argv)
{
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <test>" << std::endl << std::endl;
		std::cerr << "The following tests are available: " << std::endl;
		foreach(QString test, GUITestManager::instance()->getTestNames()) {
			std::cerr << "    " << (std::string) test << std::endl;
		}
		return -1;
	}

	QApplication a(argc, argv);
	if (GUITestManager::instance()->runTest(argv[1])) {
		return a.exec();
	}
	return 0;
}
