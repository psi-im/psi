#include "mainwindow.h"
#include <QFileDialog>

MainWindow::MainWindow() {
	setupUi();
}

void MainWindow::setupUi() {
	this->setObjectName(QString::fromUtf8("MainWindow"));
	this->resize(QSize(770, 620).expandedTo(this->minimumSizeHint()));
	actionOpen = new QAction(this);
	actionOpen->setObjectName(QString::fromUtf8("actionOpen"));
	centralwidget = new QWidget(this);
	centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
	pBMove = new QPushButton(centralwidget);
	pBMove->setObjectName(QString::fromUtf8("pBMove"));
	pBMove->setGeometry(QRect(10, 70, 71, 21));
	sBMove = new QSpinBox(centralwidget);
	sBMove->setObjectName(QString::fromUtf8("sBMove"));
	sBMove->setGeometry(QRect(90, 100, 42, 22));
	sBMove->setWrapping(false);
	sBMove->setMinimum(-99);
	sBMove->setValue(1);
	lESet = new QLineEdit(centralwidget);
	lESet->setObjectName(QString::fromUtf8("lESet"));
	lESet->setGeometry(QRect(90, 10, 631, 25));
	cBSet = new QCheckBox(centralwidget);
	cBSet->setObjectName(QString::fromUtf8("cBSet"));
	cBSet->setGeometry(QRect(90, 40, 141, 22));
	pBSet = new QPushButton(centralwidget);
	pBSet->setObjectName(QString::fromUtf8("pBSet"));
	pBSet->setGeometry(QRect(10, 10, 71, 21));
	pBDelete = new QPushButton(centralwidget);
	pBDelete->setObjectName(QString::fromUtf8("pBDelete"));
	pBDelete->setGeometry(QRect(370, 70, 71, 21));
	lEMove = new QLineEdit(centralwidget);
	lEMove->setObjectName(QString::fromUtf8("lEMove"));
	lEMove->setGeometry(QRect(90, 70, 251, 25));
	lEDelete = new QLineEdit(centralwidget);
	lEDelete->setObjectName(QString::fromUtf8("lEDelete"));
	lEDelete->setGeometry(QRect(450, 70, 271, 25));
	this->setCentralWidget(centralwidget);
	menubar = new QMenuBar(this);
	menubar->setObjectName(QString::fromUtf8("menubar"));
	menubar->setGeometry(QRect(0, 0, 776, 29));
	menuFile = new QMenu(menubar);
	menuFile->setObjectName(QString::fromUtf8("menuFile"));
	this->setMenuBar(menubar);
	statusbar = new QStatusBar(this);
	statusbar->setObjectName(QString::fromUtf8("statusbar"));
	statusbar->setGeometry(QRect(0, 595, 776, 22));
	this->setStatusBar(statusbar);
	sSView = new SvgStreamWidget(this);
	sSView->setObjectName(QString::fromUtf8("sAView"));
	sSView->setGeometry(QRect(10, 160, 750, 450));
	menubar->addAction(menuFile->menuAction());
	menuFile->addAction(actionOpen);
	retranslateUi();
	QObject::connect(actionOpen, SIGNAL(triggered()), this, SLOT(openFile()));
	QObject::connect(pBSet, SIGNAL(clicked()), this, SLOT(setElement()));
	QObject::connect(pBMove, SIGNAL(clicked()), this, SLOT(moveElement()));
	QObject::connect(pBDelete, SIGNAL(clicked()), this, SLOT(removeElement()));
} // setupUi

void MainWindow::retranslateUi() {
	this->setWindowTitle(QApplication::translate("MainWindow", "SVG Stream Viewer", 0, QApplication::UnicodeUTF8));
	actionOpen->setText(QApplication::translate("MainWindow", "Open...", 0, QApplication::UnicodeUTF8));
	pBMove->setText(QApplication::translate("MainWindow", "Move", 0, QApplication::UnicodeUTF8));
	sBMove->setSpecialValueText(QApplication::translate("MainWindow", "", 0, QApplication::UnicodeUTF8));
	sBMove->setSuffix(QApplication::translate("MainWindow", "", 0, QApplication::UnicodeUTF8));
	cBSet->setText(QApplication::translate("MainWindow", "Force new element", 0, QApplication::UnicodeUTF8));
	pBSet->setText(QApplication::translate("MainWindow", "Set", 0, QApplication::UnicodeUTF8));
	pBDelete->setText(QApplication::translate("MainWindow", "Delete", 0, QApplication::UnicodeUTF8));
	menuFile->setTitle(QApplication::translate("MainWindow", "File", 0, QApplication::UnicodeUTF8));
} // retranslateUi

void MainWindow::openFile(const QString &path)
{
    QString fileName;
    if (path.isNull())
        fileName = QFileDialog::getOpenFileName(this, tr("Open SVG File"), "./", "*.svg");
    else
        fileName = path;

    if (!fileName.isEmpty()) {
        sSView->load(fileName);
        if (!fileName.startsWith(":/")) {
            setWindowTitle(tr("%1 - SVGViewer").arg(fileName));
        }
    }
}

void MainWindow::setElement() {
	sSView->setElement(lESet->text(), cBSet->isChecked());
	lESet->setText(QString());
}

void MainWindow::moveElement() {
	sSView->moveElement(lEMove->text(), sBMove->value());
	lEMove->setText(QString());
	sBMove->setValue(1);
}

void MainWindow::removeElement() {
	sSView->removeElement(lEDelete->text());
	lEDelete->setText(QString());
}
