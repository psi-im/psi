#include <QApplication>
#include <QLabel>

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	QLabel l("This is AutoUpdater Test App 2.0");
	l.show();
	return app.exec();
}
