/*
 * Copyright (C) 2005  SilverSoft.Net
 * All rights reserved
 *
 * $Id: mainwindow.h,v 0.1 2005/01/08 12:20:13 denis Exp $
 *
 * Author: Denis Kozadaev (denis@silversoft.net)
 * Description:
 *
 * See also: style(9)
 *
 * Hacked by:
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "gameboard.h"
#include "gamesocket.h"

#include <Q3ButtonGroup>
#include <Q3GroupBox>
#include <Q3MainWindow>
#include <Q3PopupMenu>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QResizeEvent>
#include <QWorkspace>
#include <stdlib.h>

class MainWindow:public Q3MainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = NULL, const char *name = NULL);
    ~MainWindow();

    bool    sockOk()const{return (sock->ok());}

private:
    int        id;
    QString        ready_txt;
    Q3PopupMenu    *game, *help;
    QWorkspace    *wrk;
    GameSocket    *sock;
    QStringList    hosts;

private slots:
    void    showStatus(const QString&);
    void    newGame();
    void    newGame(int);
    void    about();
    void    activated(QWidget *);
    void    saveImage();
};

//-----------------------------------------------------------------------------

class SelectGame:public QDialog
{
    Q_OBJECT
public:
    SelectGame(QWidget *parent = NULL, const char *name = NULL);
    ~SelectGame();

    void        setHosts(const QStringList &);

    QString        host();
    QStringList    hosts();
    GameBoard::GameType    gameType();

private:
    QLabel        *l1;
    QComboBox    *hst;
    Q3ButtonGroup    *btn;
    QRadioButton    *wg, *bg;
    Q3GroupBox    *box;
    QPushButton    *Ok, *Cancel;

protected:
    void    resizeEvent(QResizeEvent *);

private slots:
    void    checkParams();
    void    checkParams(const QString&);
};

#endif // MAINWINDOW_H
