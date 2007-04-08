#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QLineEdit>
#include <QtGui/QMainWindow>
#include <QtGui/QMenu>
#include <QtGui/QMenuBar>
#include <QtGui/QPushButton>
#include <QtGui/QSpinBox>
#include <QtGui/QStatusBar>
#include <QtGui/QWidget>
#include "svgstreamwidget.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT
public:
	MainWindow();
public slots:
	void openFile(const QString &path = QString());
	void setElement();
	void moveElement();
	void removeElement();
private:
	void setupUi();
	void retranslateUi();
	QAction *actionOpen;
	QWidget *centralwidget;
	QPushButton *pBMove;
	QSpinBox *sBMove;
	QLineEdit *lESet;
	QCheckBox *cBSet;
	QPushButton *pBSet;
	QPushButton *pBDelete;
	QLineEdit *lEMove;
	QLineEdit *lEDelete;
	QMenuBar *menubar;
	QMenu *menuFile;
	QStatusBar *statusbar;
	SvgStreamWidget *sSView;
};

#endif // MAINWINDOW_H
