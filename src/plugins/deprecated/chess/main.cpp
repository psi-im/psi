/*
 * Copyright (C) 2005  SilverSoft.Net
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

#include "mainwindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QTextCodec>

const int
    XSize = 800,
    YSize = 600;

int
main(int argc, const char *argv[])
{
    QApplication    *app;
    MainWindow    *mw;
    int        result = 0;

    app = new QApplication(argc, (char **)argv);
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
    delete app;

    return (result);
}

