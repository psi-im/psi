/*
 * Copyright (c) 2005 by SilverSoft.Net
 * All rights reserved
 *
 * $Id: mainwindow.cpp,v 0.1 2005/01/08 12:20:13 denis Exp $
 *
 * Author: Denis Kozadaev (denis@silversoft.net)
 * Description:
 *
 * See also: style(9)
 *
 * Hacked by:
 */

#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QPixmap>
#include <QValidator>
#include <QMessageBox>
//Added by qt3to4:
#include <QResizeEvent>
#include <QLabel>
#include <Q3PopupMenu>

#include "mainwindow.h"
#include "gameboard.h"

#include "xpm/chess.xpm"
#include "xpm/quit.xpm"
#include "xpm/new_game.xpm"

extern QColor	cw, cb;

MainWindow::MainWindow(QWidget *parent, const char *name)
	:Q3MainWindow(parent, name)
{
	QPixmap	xpm(chess_xpm);

	cw = QColor(0x8F, 0xDF, 0xF0);
	cb = QColor(0x70, 0x9F, 0xDF);
	setIcon(xpm);
	wrk = new QWorkspace(this);
	sock = new GameSocket(this);
	game = new Q3PopupMenu(this);
	game->insertItem(QPixmap((const char **)new_game), tr("New"),
		this, SLOT(newGame()), Qt::CTRL + Qt::Key_N);
	id = game->insertItem(xpm, tr("Save image"), this,
		SLOT(saveImage()), Qt::CTRL + Qt::Key_S);
	game->setItemEnabled(id, false);
	game->insertSeparator();
	game->insertItem(QPixmap((const char **)quit_xpm), tr("Quit"), qApp,
		SLOT(quit()), Qt::CTRL + Qt::Key_Q);
	help = new Q3PopupMenu(this);
	help->insertItem(xpm, tr("About the game"), this, SLOT(about()));

	menuBar()->insertItem(tr("Game"), game);
	menuBar()->insertSeparator();
	menuBar()->insertItem(tr("Help"), help);

	setCentralWidget(wrk);
	ready_txt = tr("Ready");
	showStatus(ready_txt);

	QObject::connect(sock, SIGNAL(acceptConnection(int)),
		this, SLOT(newGame(int)));
	QObject::connect(wrk, SIGNAL(windowActivated(QWidget *)),
		this, SLOT(activated(QWidget *)));
}

MainWindow::~MainWindow()
{

	delete help;
	delete game;
	delete sock;
	delete wrk;
}


void
MainWindow::showStatus(const QString &txt)
{

	statusBar()->message(txt);
}


void
MainWindow::newGame()
{
	SelectGame	*dlg;
	GameBoard	*brd;
	QString		hst;

	dlg = new SelectGame(this);
	dlg->setHosts(hosts);
	if (dlg->exec()) {
		hosts = dlg->hosts();
		hst = dlg->host();
		brd = new GameBoard(dlg->gameType(), hst, wrk);
		showStatus(brd->status());
		QObject::connect(brd, SIGNAL(showStatus(const QString&)),
			this, SLOT(showStatus(const QString&)));
		brd->show();
	}
	delete dlg;
}


void
MainWindow::newGame(int sock)
{
	GameBoard	*brd;

	brd = new GameBoard(sock, wrk);
	showStatus(brd->status());
	QObject::connect(brd, SIGNAL(showStatus(const QString&)),
		this, SLOT(showStatus(const QString&)));
	brd->show();
	game->setItemEnabled(id, true);
}


void
MainWindow::about()
{

	QMessageBox::about(this, tr("About") + " QNetChess", "QNetChess " + tr(
		"is a network game chess for two players.\n"
		"It has a client and a server in the same program.\n"
		"You can modify and redistribute the source code\n"
		"because it is under GPL.\n\n"
		"Russia, Tambov, 2005 (denis@silversoft.net)"
	));
}


void
MainWindow::activated(QWidget *w)
{
	GameBoard	*brd = (GameBoard *)w;

	game->setItemEnabled(id, brd != NULL);
	if (brd != NULL)
		showStatus(brd->status());
	else
		showStatus(ready_txt);
}


void
MainWindow::saveImage()
{
	GameBoard	*brd = (GameBoard *)wrk->activeWindow();

	if (brd != NULL)
		brd->saveImage();
}

//-----------------------------------------------------------------------------

SelectGame::SelectGame(QWidget *parent, const char *name)
	:QDialog(parent, name)
{

	setCaption(tr("New game with..."));
	l1 = new QLabel(tr("To play with "), this);
	hst = new QComboBox(true, this);
	hst->setValidator(new QRegExpValidator(
		QRegExp("([a-zA-Z0-9]*\\.)*[a-zA-Z]"), hst));
	btn = new Q3ButtonGroup(tr("Choose your game"), this);
	wg = new QRadioButton(tr("White game"), btn);
	bg = new QRadioButton(tr("Black game"), btn);
	box = new Q3GroupBox(this);
	Ok = new QPushButton(tr("Play!"), box);
	Cancel = new QPushButton(tr("Cancel"), box);

	resize(400, QFontMetrics(font()).lineSpacing() * 7);
	setMinimumSize(size());
	setMaximumSize(size());

	QObject::connect(Ok, SIGNAL(clicked()), this, SLOT(accept()));
	QObject::connect(Cancel, SIGNAL(clicked()), this, SLOT(reject()));
	QObject::connect(wg, SIGNAL(clicked()), this, SLOT(checkParams()));
	QObject::connect(bg, SIGNAL(clicked()), this, SLOT(checkParams()));
	QObject::connect(hst, SIGNAL(textChanged(const QString&)),
		this, SLOT(checkParams(const QString&)));

	checkParams();
}

SelectGame::~SelectGame()
{

	delete Cancel;
	delete Ok;
	delete box;
	delete bg;
	delete wg;
	delete btn;
	delete hst;
	delete l1;
}


void
SelectGame::resizeEvent(QResizeEvent *e)
{
	QFontMetrics	fm(font());
	int		w = e->size().width(),
			h = e->size().height(),
			fh = fm.lineSpacing() + 2,
			bl;

	QDialog::resizeEvent(e);
	l1->setGeometry(0, 0, fm.width(l1->text()) + 20, fh);
	hst->move(l1->x() + l1->width(), l1->y());
	hst->resize(w - hst->x(), l1->height());
	btn->move(l1->x(), l1->y() + l1->height());
	btn->resize(w, l1->height() * 3 + 2);
	wg->setGeometry(2, fh, btn->width() - 4, fh);
	bg->setGeometry(wg->x(), wg->y() + wg->height(),
		wg->width(), wg->height());
	box->move(btn->x(), btn->y() + btn->height());
	box->resize(w, h - box->y());
	bl = box->width() / 5;
	Ok->move(bl, 4);
	Ok->resize(bl, box->height() - Ok->y() * 2);
	Cancel->setGeometry(Ok->x() + Ok->width() + bl, Ok->y(),
		Ok->width(), Ok->height());
}


void
SelectGame::checkParams()
{
	bool	res;

	res = !hst->currentText().isEmpty() &&
		(bg->isChecked() || wg->isChecked());
	Ok->setEnabled(res);
}


void
SelectGame::checkParams(const QString&)
{

	checkParams();
}


QString
SelectGame::host()
{
	QString	h(hst->currentText());

	return h.left(h.findRev(':'));
}


QStringList SelectGame::hosts()
{
	QStringList	lst;
	int		i, cnt;

	cnt = hst->count();
	lst += host();
	for (i = 0; i < cnt; i++)
		lst += hst->text(i);

	return (lst);
}


void SelectGame::setHosts(const QStringList &h)
{
	hst->insertStringList(h);
}


GameBoard::GameType SelectGame::gameType()
{
	GameBoard::GameType	gt;

	if (wg->isChecked())
		gt = GameBoard::WHITE;
	else if (bg->isChecked())
		gt = GameBoard::BLACK;
	else
		gt = GameBoard::NOGAME;

	return (gt);
}

