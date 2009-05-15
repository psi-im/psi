#include <QApplication>
#include <QDesktopWidget>
#include <QMoveEvent>
#include <QMouseEvent>
#include "../advwidget.h"

class MyWidget : public AdvancedWidget<QWidget>
{
	Q_OBJECT
public:
	MyWidget()
	{
		mouseDown = false;
	}

	void mousePressEvent(QMouseEvent *e)
	{
		mouseDown = true;
		oldPos = e->globalPos();
	}
	
	void mouseReleaseEvent(QMouseEvent *)
	{
		mouseDown = false;
	}

	void mouseMoveEvent(QMouseEvent *e)
	{
		if (mouseDown) {
			QPoint dp = e->globalPos() - oldPos;
			oldPos = e->globalPos();
			move(pos() + dp);
		}
	}

	QPoint oldPos;
	bool mouseDown;
};

int main (int argc, char *argv[])
{
	QApplication app(argc, argv);
	MyWidget mw;
	mw.show();
	return app.exec();
}

#include "main.moc"

