#include <QCoreApplication>

#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include <cppunit/XmlOutputter.h>
#include <cppunit/TextTestResult.h>

int main(int argc, char* argv[])
{
	QCoreApplication a(argc, argv);

	CppUnit::TestFactoryRegistry& registry = CppUnit::TestFactoryRegistry::getRegistry();
	CppUnit::TextUi::TestRunner runner;
	runner.addTest( registry.makeTest() );
	if (argc >= 2) {
		if (!strcmp(argv[1],"--xml")) {
			runner.setOutputter(new CppUnit::XmlOutputter(&runner.result(), std::cout));
		}
	}
	if (argc >= 2 && !strcmp(argv[1],"--xml")) {
		runner.setOutputter(new CppUnit::XmlOutputter(&runner.result(), std::cout));
	}
	return (runner.run("") ? 0 : 1);
}
