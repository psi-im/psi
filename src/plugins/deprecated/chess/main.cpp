/*
 * Copyright (c) 2005 by SilverSoft.Net
 * All rights reserved
 *
 * $Id: main.cpp,v 0.1 2005/01/08 12:19:58 denis Exp $
 *
 * Author: Denis Kozadaev (denis@silversoft.net)
 * Description:
 *
 * See also: style(9)
 *
 * Hacked by:
 */

#include <QApplication>
#include <QMessageBox>
#include <QTextCodec>
//Added by qt3to4:
#include <QTranslator>

#include "mainwindow.h"

const int
	XSize = 800,
	YSize = 600;

int
main(int argc, const char *argv[])
{
	QApplication	*app;
	MainWindow	*mw;
	QTranslator	*qt;
	int		result = 0;

	app = new QApplication(argc, (char **)argv);
	/*qt = new QTranslator();
	if (qt->load(LOCALE_FILE))
		app->installTranslator(qt);*/
	mw = new MainWindow();

	if (mw->sockOk()) {
		app->setMainWidget(mw);
		mw->show();
		mw->resize(XSize, YSize);
		mw->setMinimumSize(mw->size());
		result = app->exec();
	} else
		QMessageBox::critical(NULL, QObject::tr("Socket Error"),
			QObject::tr("Cannot create a server socket!"));

	delete mw;
	delete qt;
	delete app;

	return (result);
}

