/*
 * Copyright (c) 2005 by SilverSoft.Net
 * All rights reserved
 *
 * $Id: gameboard.cpp,v 1.1 2005/03/26 11:24:13 denis Exp $
 *
 * Author: Denis Kozadaev (denis@silversoft.net)
 * Description:
 *
 * See also: style(9)
 *
 * Hacked by:
 *	Fixed the mate checker (big thanks to knyaz@RusNet)
 */

#include <QPainter>
#include <QFontMetrics>
#include <QMessageBox>
#include <QCursor>
#include <Q3FileDialog>
//Added by qt3to4:
#include <Q3PointArray>
#include <QPixmap>
#include <QResizeEvent>
#include <QFocusEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QCloseEvent>
#include <QDebug>
#include <stdlib.h>

#include "gameboard.h"
#include "gamesocket.h"

#include "xpm/black_bishop.xpm"
#include "xpm/black_castle.xpm"
#include "xpm/black_king.xpm"
#include "xpm/black_knight.xpm"
#include "xpm/black_pawn.xpm"
#include "xpm/black_queen.xpm"
#include "xpm/white_bishop.xpm"
#include "xpm/white_castle.xpm"
#include "xpm/white_king.xpm"
#include "xpm/white_knight.xpm"
#include "xpm/white_pawn.xpm"
#include "xpm/white_queen.xpm"

const int 	cell_size = 40,
			XSize = 640,
			YSize = 480;

QColor	cb, cw;

bool Figure::hasMyFigure(GameBoard::GameType gt, GameBoard::FigureType *map,
	int x, int y, bool mirror)
{
	int	n;
	bool	res;

	n = map2map(gt, x, y, mirror);

	if (gt == GameBoard::WHITE)
		switch (map[n]) {
			case GameBoard::DUMMY:
			case GameBoard::WHITE_PAWN:
			case GameBoard::WHITE_CASTLE:
			case GameBoard::WHITE_BISHOP:
			case GameBoard::WHITE_KING:
			case GameBoard::WHITE_QUEEN:
			case GameBoard::WHITE_KNIGHT:
				res = true;
				break;
			default:
				res = false;
		}
	else if (gt == GameBoard::BLACK)
		switch (map[n]) {
			case GameBoard::DUMMY:
			case GameBoard::BLACK_PAWN:
			case GameBoard::BLACK_CASTLE:
			case GameBoard::BLACK_BISHOP:
			case GameBoard::BLACK_KING:
			case GameBoard::BLACK_QUEEN:
			case GameBoard::BLACK_KNIGHT:
				res = true;
				break;
			default:
				res = false;
		}
	else
		res = false;

	return (res);
}


int Figure::hasEnemyFigure(GameBoard::GameType gt, GameBoard::FigureType *map,
	int x, int y, bool mirror)
{
	int	n;
	int	res;

	n = map2map(gt, x, y, mirror);

	if (gt == GameBoard::BLACK)
		switch (map[n]) {
			case GameBoard::WHITE_PAWN:
			case GameBoard::WHITE_CASTLE:
			case GameBoard::WHITE_BISHOP:
			case GameBoard::WHITE_QUEEN:
			case GameBoard::WHITE_KNIGHT:
				res = 1;
				break;
			case GameBoard::WHITE_KING:
				res = 2;
				break;
			case GameBoard::DUMMY:
			default:
				res = 0;
		}
	else if (gt == GameBoard::WHITE)
		switch (map[n]) {
			case GameBoard::BLACK_PAWN:
			case GameBoard::BLACK_CASTLE:
			case GameBoard::BLACK_BISHOP:
			case GameBoard::BLACK_QUEEN:
			case GameBoard::BLACK_KNIGHT:
				res = 1;
				break;
			case GameBoard::BLACK_KING:
				res = 2;
				break;
			case GameBoard::DUMMY:
			default:
				res = 0;
		}
	else
		res = 0;

	return (res);
}


bool Figure::hasFigure(GameBoard::GameType gt, GameBoard::FigureType *map,
	int x, int y, bool mirror)
{
	int	n;

	n = map2map(gt, x, y, mirror);

	return (map[n] != GameBoard::NONE);
}


int Figure::map2map(GameBoard::GameType gt, int x, int y, bool mirror)
{
	int	n = -1;

	if (gt == GameBoard::WHITE)
		if (mirror)
			n = (y - 1) * 8 + (8 - x);
		else
			n = (8 - y) * 8 + (x - 1);
	else if (gt == GameBoard::BLACK)
		if (mirror)
			n = (8 - y) * 8 + (x - 1);
		else
			n = (y - 1) * 8 + (8 - x);

	return (n);
}


QString Figure::map2str(int x, int y)
{
	QString	s;

	s = QString(QChar('a' + x - 1)) + QString::number(y);
	return (s);
}


void Figure::str2map(const QString &coo, int *x, int *y)
{
	*x = coo.at(0).toAscii() - 'a' + 1;
	*y = coo.at(1).toAscii() - '0';
}


int
Figure::validMove(GameBoard::GameType gt, GameBoard::FigureType *map,
	int fx, int fy, int tx, int ty, bool mirror)
{
	Q3PointArray	vl;
	int		res, f, t;

	moveList(vl, gt, map, fx, fy, mirror);
	res = hasPoint(vl, tx, ty);
	f = map2map(gt, fx, fy, mirror);
	switch (map[f]) {
		case GameBoard::WHITE_PAWN:
			if (res && (ty == 8))
				res++;
			break;
		case GameBoard::BLACK_PAWN:
			if (res && (ty == 1))
				res++;
			break;
		default:;
	}
	if (res) {
		t = map2map(gt, tx, ty, mirror);
		map[t] = map[f];
		map[f] = GameBoard::NONE;
		if (mirror) {
			vl.resize(0);
			t = checkKing(gt, map, mirror, vl, false);
			switch (t) {
				case 1:
					res |= 0x10;
					break;
				case 2:
					res |= 0x20;
					break;
				case 3:
					res |= 0x40;
					break;
				default:;
			}
		}
	}

	return (res);
}


/*
 *	0 - nothing
 *	1 - check
 *	2 - mate
 *	3 - stalemate
 */
int
Figure::checkKing(GameBoard::GameType gt, GameBoard::FigureType *map,
	bool mirror, Q3PointArray &vl, bool co)
{
	Q3PointArray		tmp;
	GameBoard::FigureType	myking, map1[64];
	GameBoard::GameType	mytype;
	int			res, x, y, p, xk, yk, pk;
	bool			hp;

	if (gt == GameBoard::WHITE) {
		myking = GameBoard::BLACK_KING;
		mytype = GameBoard::BLACK;
	} else if (gt == GameBoard::BLACK) {
		myking = GameBoard::WHITE_KING;
		mytype = GameBoard::WHITE;
	} else {
		myking = GameBoard::NONE;
		mytype = GameBoard::NOGAME;
	}
	xk = yk = -1;
	res = 0; p = -1;

	for (y = 1; y < 9; ++y)
		for (x = 1; x < 9; ++x)
			/* check enemy figures */
			if (hasMyFigure(gt, map, x, y, mirror))
				moveList(vl, gt, map, x, y, mirror);
			else if (p == -1) {
				p = map2map(mytype, x, y, !mirror);
				if (map[p] == myking) {
					xk = x;
					yk = y;
				} else
					p = -1;
			}

	hp = hasPoint(vl, xk, yk);
	if (hp) {
		res++;
		if (!co) {
			vl.resize(0);
			for (y = 1; y < 9; ++y)
				for (x = 1; x < 9; ++x)
					if (hasMyFigure(mytype, map,
						x, y, !mirror))
						moveList(vl, mytype, map,
							x, y, !mirror);
			memmove(map1, map, sizeof(map1));
			pk = map2map(mytype, xk, yk, !mirror);
			for (x = vl.size() - 1; x >= 0; --x) {
				p = map2map(mytype, vl.point(x).x(),
					vl.point(x).y(), !mirror);
				if (p != pk)
					map1[p] = GameBoard::DUMMY;
			}
			if (checkKing(gt, map1, mirror, vl, true) != 0) {
				vl.resize(0);
				moveListKing(vl, mytype, map, xk, yk, !mirror);
				memmove(map1, map, sizeof(map1));
				for (y = 0, x = vl.size() - 1; x >= 0; --x) {
					p = map2map(mytype, vl.point(x).x(),
						vl.point(x).y(), !mirror);
					map1[p] = myking;
					map1[pk] = GameBoard::NONE;
					if (checkKing(gt, map1, mirror,
						tmp, true) == 1)
						++y;
					map1[pk] = map[pk];
					map1[p] = map[p];
				}
				if (y == (int)vl.size())
					res++;
			}
		}
	} else if (!co) {
		vl.resize(0);
		for (y = 1; y < 9; ++y)
			for (x = 1; x < 9; ++x)
				if (hasMyFigure(mytype, map, x, y, !mirror))
					moveList(vl, mytype, map, x, y,
						!mirror);
		if (vl.size() == 0)
			res = 3;
	}

	return (res);
}


void
Figure::moveList(Q3PointArray &vl, GameBoard::GameType gt,
	GameBoard::FigureType *map, int x, int y, bool mirror)
{
	int		n;

	n = map2map(gt, x, y, mirror);
	switch (map[n]) {
		case GameBoard::WHITE_PAWN:
			moveListWhitePawn(vl, gt, map, x, y, mirror);
			break;

		case GameBoard::WHITE_CASTLE:
		case GameBoard::BLACK_CASTLE:
			moveListCastle(vl, gt, map, x, y, mirror);
			break;

		case GameBoard::WHITE_BISHOP:
		case GameBoard::BLACK_BISHOP:
			moveListBishop(vl, gt, map, x, y, mirror);
			break;

		case GameBoard::WHITE_KING:
		case GameBoard::BLACK_KING:
			moveListKing(vl, gt, map, x, y, mirror);
			break;

		case GameBoard::WHITE_QUEEN:
		case GameBoard::BLACK_QUEEN:
			moveListQueen(vl, gt, map, x, y, mirror);
			break;

		case GameBoard::WHITE_KNIGHT:
		case GameBoard::BLACK_KNIGHT:
			moveListKnight(vl, gt, map, x, y, mirror);
			break;

		case GameBoard::BLACK_PAWN:
			moveListBlackPawn(vl, gt, map, x, y, mirror);
			break;


		default:;
	}
}


void
Figure::moveListWhitePawn(Q3PointArray &vl, GameBoard::GameType gt,
	GameBoard::FigureType *map, int x, int y, bool mirror)
{

	if (validPoint(gt, map, x, y + 1, mirror) &&
		!hasFigure(gt, map, x, y + 1, mirror)) {
		vl.putPoints(vl.size(), 1, x, y + 1);
		if ((y == 2) && validPoint(gt, map, x, y + 2, mirror))
			vl.putPoints(vl.size(), 1, x, y + 2);
	}
	if (validPoint(gt, map, x + 1, y + 1, mirror) &&
		hasEnemyFigure(gt, map, x + 1, y + 1, mirror))
		vl.putPoints(vl.size(), 1, x + 1, y + 1);
	if (validPoint(gt, map, x - 1, y + 1, mirror) &&
		hasEnemyFigure(gt, map, x - 1, y + 1, mirror))
		vl.putPoints(vl.size(), 1, x - 1, y + 1);
}


void
Figure::moveListBlackPawn(Q3PointArray &vl, GameBoard::GameType gt,
	GameBoard::FigureType *map, int x, int y, bool mirror)
{

	if (validPoint(gt, map, x, y - 1, mirror) &&
		!hasFigure(gt, map, x, y - 1, mirror)) {
		vl.putPoints(vl.size(), 1, x, y - 1);
		if ((y == 7) && validPoint(gt, map, x, y - 2, mirror))
			vl.putPoints(vl.size(), 1, x, y - 2);
	}
	if (validPoint(gt, map, x + 1, y - 1, mirror) &&
		hasEnemyFigure(gt, map, x + 1, y - 1, mirror))
		vl.putPoints(vl.size(), 1, x + 1, y - 1);
	if (validPoint(gt, map, x - 1, y - 1, mirror) &&
		hasEnemyFigure(gt, map, x - 1, y - 1, mirror))
		vl.putPoints(vl.size(), 1, x - 1, y - 1);
}


void
Figure::moveListCastle(Q3PointArray &vl, GameBoard::GameType gt,
	GameBoard::FigureType *map, int x, int y, bool mirror)
{
	int	i;

	for (i = x + 1; i < 9; i++) {
		if (!hasFigure(gt, map, i, y, mirror)) {
			vl.putPoints(vl.size(), 1, i, y);
			continue;
		} else if (hasEnemyFigure(gt, map, i, y, mirror))
			vl.putPoints(vl.size(), 1, i, y);
		break;
	}
	for (i = x - 1; i > 0; i--) {
		if (!hasFigure(gt, map, i, y, mirror)) {
			vl.putPoints(vl.size(), 1, i, y);
			continue;
		} else if (hasEnemyFigure(gt, map, i, y, mirror))
			vl.putPoints(vl.size(), 1, i, y);
		break;
	}
	for (i = y + 1; i < 9; i++) {
		if (!hasFigure(gt, map, x, i, mirror)) {
			vl.putPoints(vl.size(), 1, x, i);
			continue;
		} else if (hasEnemyFigure(gt, map, x, i, mirror))
			vl.putPoints(vl.size(), 1, x, i);
		break;
	}
	for (i = y - 1; i > 0; i--) {
		if (!hasFigure(gt, map, x, i, mirror)) {
			vl.putPoints(vl.size(), 1, x, i);
			continue;
		} else if (hasEnemyFigure(gt, map, x, i, mirror))
			vl.putPoints(vl.size(), 1, x, i);
		break;
	}
}


void
Figure::moveListBishop(Q3PointArray &vl, GameBoard::GameType gt,
	GameBoard::FigureType *map, int x, int y, bool mirror)
{
	int	i, j;

	for (i = x + 1, j = y + 1; (i < 9) && (j < 9); i++, j++) {
		if (!hasFigure(gt, map, i, j, mirror)) {
			vl.putPoints(vl.size(), 1, i, j);
			continue;
		} else if (hasEnemyFigure(gt, map, i, j, mirror))
			vl.putPoints(vl.size(), 1, i, j);
		break;
	}
	for (i = x - 1, j = y + 1; (i > 0) && (j < 9); i--, j++) {
		if (!hasFigure(gt, map, i, j, mirror)) {
			vl.putPoints(vl.size(), 1, i, j);
			continue;
		} else if (hasEnemyFigure(gt, map, i, j, mirror))
			vl.putPoints(vl.size(), 1, i, j);
		break;
	}
	for (i = x - 1, j = y - 1; (i > 0) && (j > 0); i--, j--) {
		if (!hasFigure(gt, map, i, j, mirror)) {
			vl.putPoints(vl.size(), 1, i, j);
			continue;
		} else if (hasEnemyFigure(gt, map, i, j, mirror))
			vl.putPoints(vl.size(), 1, i, j);
		break;
	}
	for (i = x + 1, j = y - 1; (i < 9) && (j > 0); i++, j--) {
		if (!hasFigure(gt, map, i, j, mirror)) {
			vl.putPoints(vl.size(), 1, i, j);
			continue;
		} else if (hasEnemyFigure(gt, map, i, j, mirror))
			vl.putPoints(vl.size(), 1, i, j);
		break;
	}
}


void
Figure::moveListKing(Q3PointArray &vl, GameBoard::GameType gt,
	GameBoard::FigureType *map, int x, int y, bool mirror)
{
	int	x1, x2, y1, y2;

	x1 = x - 1; x2 = x + 1;
	y1 = y - 1; y2 = y + 1;
	if (validPoint(gt, map, x1, y2, mirror) &&
		!hasKingsMeeting(gt, map, x1, y2, mirror))
		vl.putPoints(vl.size(), 1, x1, y2);
	if (validPoint(gt, map, x, y2, mirror) &&
		!hasKingsMeeting(gt, map, x, y2, mirror))
		vl.putPoints(vl.size(), 1, x, y2);
	if (validPoint(gt, map, x2, y2, mirror) &&
		!hasKingsMeeting(gt, map, x2, y2, mirror))
		vl.putPoints(vl.size(), 1, x2, y2);
	if (validPoint(gt, map, x1, y, mirror) &&
		!hasKingsMeeting(gt, map, x1, y, mirror))
		vl.putPoints(vl.size(), 1, x1, y);
	if (validPoint(gt, map, x2, y, mirror) &&
		!hasKingsMeeting(gt, map, x2, y, mirror))
		vl.putPoints(vl.size(), 1, x2, y);
	if (validPoint(gt, map, x1, y1, mirror) &&
		!hasKingsMeeting(gt, map, x1, y1, mirror))
		vl.putPoints(vl.size(), 1, x1, y1);
	if (validPoint(gt, map, x, y1, mirror) &&
		!hasKingsMeeting(gt, map, x, y1, mirror))
		vl.putPoints(vl.size(), 1, x, y1);
	if (validPoint(gt, map, x2, y1, mirror) &&
		!hasKingsMeeting(gt, map, x2, y1, mirror))
		vl.putPoints(vl.size(), 1, x2, y1);
}


void
Figure::moveListQueen(Q3PointArray &vl, GameBoard::GameType gt,
	GameBoard::FigureType *map, int x, int y, bool mirror)
{

	moveListBishop(vl, gt, map, x, y, mirror);
	moveListCastle(vl, gt, map, x, y, mirror);
}


void
Figure::moveListKnight(Q3PointArray &vl, GameBoard::GameType gt,
	GameBoard::FigureType *map, int x, int y, bool mirror)
{
	int	x1, x2, x3, x4,
		y1, y2, y3, y4;

	x1 = x + 1;
	x2 = x1 + 1;
	x3 = x - 1;
	x4 = x3 - 1;
	y1 = y + 1;
	y2 = y1 + 1;
	y3 = y - 1;
	y4 = y3 - 1;
	if (validPoint(gt, map, x3, y2, mirror))
		vl.putPoints(vl.size(), 1, x3, y2);
	if (validPoint(gt, map, x1, y2, mirror))
		vl.putPoints(vl.size(), 1, x1, y2);
	if (validPoint(gt, map, x4, y1, mirror))
		vl.putPoints(vl.size(), 1, x4, y1);
	if (validPoint(gt, map, x2, y1, mirror))
		vl.putPoints(vl.size(), 1, x2, y1);
	if (validPoint(gt, map, x4, y3, mirror))
		vl.putPoints(vl.size(), 1, x4, y3);
	if (validPoint(gt, map, x2, y3, mirror))
		vl.putPoints(vl.size(), 1, x2, y3);
	if (validPoint(gt, map, x3, y4, mirror))
		vl.putPoints(vl.size(), 1, x3, y4);
	if (validPoint(gt, map, x1, y4, mirror))
		vl.putPoints(vl.size(), 1, x1, y4);
}


bool
Figure::hasKingsMeeting(GameBoard::GameType gt, GameBoard::FigureType *map,
	int x, int y, bool mirror)
{
	int	x1, x2, y1, y2;
	bool	res;

	x1 = x - 1; x2 = x + 1;
	y1 = y - 1; y2 = y + 1;
	res = false;

	if (validPoint(gt, map, x1, y2, mirror))
		res = (hasEnemyFigure(gt, map, x1, y2, mirror) == 2);
	if (! res && validPoint(gt, map, x, y2, mirror))
		res = (hasEnemyFigure(gt, map, x, y2, mirror) == 2);
	if (!res && validPoint(gt, map, x2, y2, mirror))
		res = (hasEnemyFigure(gt, map, x2, y2, mirror) == 2);
	if (!res && validPoint(gt, map, x1, y, mirror))
		res = (hasEnemyFigure(gt, map, x1, y, mirror) == 2);
	if (!res && validPoint(gt, map, x2, y, mirror))
		res = (hasEnemyFigure(gt, map, x2, y, mirror) == 2);
	if (!res && validPoint(gt, map, x1, y1, mirror))
		res = (hasEnemyFigure(gt, map, x1, y1, mirror) == 2);
	if (!res && validPoint(gt, map, x, y1, mirror))
		res = (hasEnemyFigure(gt, map, x, y1, mirror) == 2);
	if (!res && validPoint(gt, map, x2, y1, mirror))
		res = (hasEnemyFigure(gt, map, x2, y1, mirror) == 2);

	return (res);
}


bool
Figure::hasPoint(const Q3PointArray &vl, int x, int y)
{
	int	i, xp, yp, cnt;
	bool	res = false;

	cnt = vl.count();
	for (i = 0; i < cnt; ++i) {
		vl.point(i, &xp, &yp);
		if ((xp == x) && (yp == y)) {
			res = true;
			break;
		}
	}

	return (res);
}


bool
Figure::validPoint(GameBoard::GameType gt, GameBoard::FigureType *map,
	int x, int y, bool mirror)
{
	bool	res;

	res = ((x >0) && (x < 9) && (y >0) && (y < 9));
	if (res)
		res = !hasMyFigure(gt, map, x, y, mirror);

	return (res);
}

//-----------------------------------------------------------------------------

GameBoard::GameBoard(GameType g, const QString &h, QWidget *parent,
	const char *name)
	:QWidget(parent, name, Qt::WResizeNoErase | Qt::WNoAutoErase)
{
	setAttribute(Qt::WA_DeleteOnClose);
	QString	str;

	protocol = new GameProtocol();
	connect(protocol,SIGNAL(sendData(const QString&)), this, SIGNAL(sendData(const QString&)));

	gt = g; hst = h;
	setCursor(QCursor(Qt::WaitCursor));
	if (gt == WHITE)
		str = tr("White");
	else if (gt == BLACK)
		str = tr("Black");
	str += ' ' + tr("game with") + ' ';
	setCaption(str + hst);
	setIcon(QPixmap((const char **)white_knight));
	map = new FigureType[64];
	initMap();

	sock = new Q3Socket(this);
	drw = new Drawer(map, &gt, this);
	drw->setEnabled(false);
	drw->setFocusPolicy(Qt::NoFocus);
	box = new Q3GroupBox(tr("Game chat"), this);
	lst = new QListWidget(box);
	lst->setFocusPolicy(Qt::NoFocus);
	lst->setVScrollBarMode(Q3ScrollView::AlwaysOff);
	lst->setSelectionMode(QListWidget::NoSelection);
	edt = new QLineEdit(box);
	edt->setEnabled(false);
	setFocusProxy(edt);
	hist = new Q3GroupBox(tr("History"), this);
	hist->setAlignment(Qt::AlignHCenter);
	hist->setFocusPolicy(Qt::NoFocus);
	hw = new QListWidget(hist);
	hw->setSelectionMode(QListWidget::NoSelection);
	hw->setPaletteBackgroundColor(cw);
	hb = new QListWidget(hist);
	hb->setSelectionMode(QListWidget::NoSelection);
	hb->setPaletteBackgroundColor(cb);
	//tmr = new QTimer(this);
	//sock_tout = SOCK_WAIT;
	my_stat = tr("Looking up the host") + ' ' + hst + "...";
	/*QObject::connect(sock, SIGNAL(hostFound()),
		this, SLOT(showHostFound()));
	QObject::connect(sock, SIGNAL(connected()),
		this, SLOT(sockConnected()));
	QObject::connect(sock, SIGNAL(readyRead()),
		this, SLOT(sockRead()));
	QObject::connect(sock, SIGNAL(connectionClosed()),
		this, SLOT(sockClosed()));
	QObject::connect(sock, SIGNAL(error(int)),
		this, SLOT(sockError(int)));*/
	
		
	QObject::connect(drw, SIGNAL(moved(const QString&)),
		this, SLOT(sendMove(const QString&)));
	QObject::connect(drw, SIGNAL(newFigure(const QString&,
		GameBoard::FigureType)),
		this, SLOT(sendFigure(const QString&, GameBoard::FigureType)));
	QObject::connect(drw, SIGNAL(gameover(int)),
		this, SLOT(gameover(int)));
	QObject::connect(edt, SIGNAL(returnPressed()),
		this, SLOT(sendText()));
	//QObject::connect(tmr, SIGNAL(timeout()), this, SLOT(sockTest()));

	resize(XSize, YSize);
	setMinimumSize(size());
	setMaximumSize(size());
	//sock->connectToHost(hst, GAME_PORT);
	//tmr->start(1000);

	//hackyhackhack
	tmr = new QTimer(this);
	connect(tmr, SIGNAL(timeout()), this, SLOT(showHostFound()));
	tmr->start(1000);
	tmr2 = new QTimer(this);
	connect(tmr2, SIGNAL(timeout()), this, SLOT(sockConnected()));
	tmr2->start(2000);
	qDebug("GameBoard inited, type 1");
}

GameBoard::GameBoard(int sfd, QWidget *parent, const char *name)
	:QWidget(parent, name, Qt::WResizeNoErase | Qt::WNoAutoErase |
	Qt::WDestructiveClose)
{

	gt = NOGAME;
	setCursor(QCursor(Qt::WaitCursor));
	setIcon(QPixmap((const char **)white_knight));
	map = new FigureType[64];
	memset(map, NONE, 64 * sizeof(*map));

	protocol = new GameProtocol();
	connect(protocol,SIGNAL(sendData(const QString&)), this, SIGNAL(sendData(const QString&)));
	sock = new Q3Socket(this);
	drw = new Drawer(map, &gt, this);
	drw->setEnabled(false);
	drw->setFocusPolicy(Qt::NoFocus);
	box = new Q3GroupBox(tr("Game chat"), this);
	lst = new QListWidget(box);
	lst->setFocusPolicy(Qt::NoFocus);
	lst->setVScrollBarMode(Q3ScrollView::AlwaysOff);
	lst->setSelectionMode(QListWidget::NoSelection);
	edt = new QLineEdit(box);
	setFocusProxy(edt);
	hist = new Q3GroupBox(tr("History"), this);
	hist->setAlignment(Qt::AlignHCenter);
	hist->setFocusPolicy(Qt::NoFocus);
	hw = new QListWidget(hist);
	hw->setSelectionMode(QListWidget::NoSelection);
	hw->setPaletteBackgroundColor(cw);
	hb = new QListWidget(hist);
	hb->setSelectionMode(QListWidget::NoSelection);
	hb->setPaletteBackgroundColor(cb);
	
	//sock->setSocket(sfd);
	//sock_tout = SOCK_WAIT;
	my_stat = tr("Accepted a new connection");
	/*QObject::connect(sock, SIGNAL(hostFound()),
		this, SLOT(showHostFound()));
	QObject::connect(sock, SIGNAL(connected()),
		this, SLOT(sockConnected()));
	QObject::connect(sock, SIGNAL(readyRead()),
		this, SLOT(sockRead()));
	QObject::connect(sock, SIGNAL(connectionClosed()),
		this, SLOT(sockClosed()));
	QObject::connect(sock, SIGNAL(error(int)),
		this, SLOT(sockError(int)));*/
		
	
	QObject::connect(drw, SIGNAL(moved(const QString&)),
		this, SLOT(sendMove(const QString&)));
	QObject::connect(drw, SIGNAL(newFigure(const QString&,
		GameBoard::FigureType)),
		this, SLOT(sendFigure(const QString&, GameBoard::FigureType)));
	QObject::connect(drw, SIGNAL(gameover(int)),
		this, SLOT(gameover(int)));
	QObject::connect(edt, SIGNAL(returnPressed()),
		this, SLOT(sendText()));
	
	resize(XSize, YSize);
	setMinimumSize(size());
	setMaximumSize(size());
	

	//hackyhackhack
	tmr = new QTimer(this);
	//connect(tmr, SIGNAL(timeout()), this, SLOT(showHostFound()));
	//tmr->start(1000);
	tmr2 = new QTimer(this);
	//connect(tmr2, SIGNAL(timeout()), this, SLOT(sockConnected()));
	//tmr2->start(2000);
	
	qDebug("GameBoard initialised (type 2)");
}

GameBoard::~GameBoard()
{

	protocol->sendQuit(sock);
	delete tmr;
	delete tmr2;
	delete hb;
	delete hw;
	delete hist;
	delete edt;
	delete lst;
	delete box;
	delete drw;
	delete sock;
	delete[] map;
	delete protocol;
}


void
GameBoard::resizeEvent(QResizeEvent *e)
{
	QFontMetrics	fm(font());
	int		w = e->size().width(),
			h = e->size().height(),
			fh = fm.lineSpacing() + 4;

	QWidget::resizeEvent(e);
	drw->move(0, 0);
	box->move(drw->x(), drw->y() + drw->height());
	box->resize(w, h - box->y());
	edt->move(2, box->height() - fh - 2);
	edt->resize(box->width() - edt->x() * 2, fh);
	lst->move(edt->x(), fm.lineSpacing());
	lst->resize(edt->width(), edt->y() - lst->y());
	hist->move(drw->x() + drw->width(), drw->y());
	hist->resize(w - hist->x(), box->y());
	hw->move(2, QFontMetrics(hist->font()).lineSpacing());
	hw->resize((hist->width() - hw->x()) / 2,
		hist->height() - hw->y() - 2);
	hb->move(hw->x() + hw->width(), hw->y());
	hb->resize(hw->size());
}


void
GameBoard::focusInEvent(QFocusEvent *e)
{

	QWidget::focusInEvent(e);
	emit showStatus(my_stat);
}


void
GameBoard::initMap()
{

	memset(map, NONE, 64 * sizeof(*map));
	if (gt == WHITE) {
		map[0] = BLACK_CASTLE;
		map[1] = BLACK_KNIGHT;
		map[2] = BLACK_BISHOP;
		map[3] = BLACK_QUEEN;
		map[4] = BLACK_KING;
		map[5] = BLACK_BISHOP;
		map[6] = BLACK_KNIGHT;
		map[7] = BLACK_CASTLE;
		map[8] = BLACK_PAWN;
		map[9] = BLACK_PAWN;
		map[10] = BLACK_PAWN;
		map[11] = BLACK_PAWN;
		map[12] = BLACK_PAWN;
		map[13] = BLACK_PAWN;
		map[14] = BLACK_PAWN;
		map[15] = BLACK_PAWN;
		map[48] = WHITE_PAWN;
		map[49] = WHITE_PAWN;
		map[50] = WHITE_PAWN;
		map[51] = WHITE_PAWN;
		map[52] = WHITE_PAWN;
		map[53] = WHITE_PAWN;
		map[54] = WHITE_PAWN;
		map[55] = WHITE_PAWN;
		map[56] = WHITE_CASTLE;
		map[57] = WHITE_KNIGHT;
		map[58] = WHITE_BISHOP;
		map[59] = WHITE_QUEEN;
		map[60] = WHITE_KING;
		map[61] = WHITE_BISHOP;
		map[62] = WHITE_KNIGHT;
		map[63] = WHITE_CASTLE;
	} else {
		map[0] = WHITE_CASTLE;
		map[1] = WHITE_KNIGHT;
		map[2] = WHITE_BISHOP;
		map[3] = WHITE_KING;
		map[4] = WHITE_QUEEN;
		map[5] = WHITE_BISHOP;
		map[6] = WHITE_KNIGHT;
		map[7] = WHITE_CASTLE;
		map[8] = WHITE_PAWN;
		map[9] = WHITE_PAWN;
		map[10] = WHITE_PAWN;
		map[11] = WHITE_PAWN;
		map[12] = WHITE_PAWN;
		map[13] = WHITE_PAWN;
		map[14] = WHITE_PAWN;
		map[15] = WHITE_PAWN;
		map[48] = BLACK_PAWN;
		map[49] = BLACK_PAWN;
		map[50] = BLACK_PAWN;
		map[51] = BLACK_PAWN;
		map[52] = BLACK_PAWN;
		map[53] = BLACK_PAWN;
		map[54] = BLACK_PAWN;
		map[55] = BLACK_PAWN;
		map[56] = BLACK_CASTLE;
		map[57] = BLACK_KNIGHT;
		map[58] = BLACK_BISHOP;
		map[59] = BLACK_KING;
		map[60] = BLACK_QUEEN;
		map[61] = BLACK_BISHOP;
		map[62] = BLACK_KNIGHT;
		map[63] = BLACK_CASTLE;
	}
}


void
GameBoard::showHostFound()
{
	tmr->stop();
	my_stat = tr("The host found");
	emit showStatus(my_stat);
	qDebug("showHostFound");
}


void
GameBoard::sockConnected()
{
	tmr2->stop();
	my_stat = tr("Connected to the host");
	emit showStatus(my_stat);
	protocol->setGameType(sock, gt);
	edt->setEnabled(true);
	qDebug("sockConnected");
}

void GameBoard::receiveData(const QString& data)
{
	sockRead(data);
}

void GameBoard::sockRead(const QString& data)
{
	parseString(data);

}


void
GameBoard::sockClosed()
{

	close();
}


void
GameBoard::sockError(int err)
{
	QString	e;

	QMessageBox::critical(this, tr("Socket Error..."),
		tr("You have a socket error number") + ' ' +
		QString::number(err));
}


void
GameBoard::parseString(const QString &str)
{
	QStringList	lst(QStringList::split(SEP, str));
	QString		s(lst[0].lower());
	int		id;

	if (s == "game") {
		s = lst[1].lower();
		if (s == "mate") {
			updateHistory(GAMEOVER_TXT, true);
			gt = NOGAME;
			gameover(0);
			close();
		} else if (s == "stalemate") {
			gt = NOGAME;
			gameover(3);
			close();
		} else if (s != "accept") {
			if (s == "white") {
				gt = BLACK;
				s = tr("White");
			} else if (s == "black") {
				gt = WHITE;
				s = tr("Black");
				drw->setEnabled(true);
				setCursor(QCursor(Qt::ArrowCursor));
			}
			s += ' ' + tr("game from") + ' ';
			my_stat = tr("Accepted the") + ' ' + s;
			hst = sock->peerName();
			if (hst.isEmpty())
				hst = sock->peerAddress().toString() + ':' +
					QString::number(sock->peerPort());
			initMap();
			drw->repaint(true);
			protocol->acceptGame(sock);
			setCaption(s + hst);
			my_stat += hst;
			emit showStatus(my_stat);
		} else if (gt == WHITE) {
			drw->setEnabled(true);
			setCursor(QCursor(Qt::ArrowCursor));
		}
	} else if (s == "move") {
		if (!drw->isEnabled()) {
			drw->setEnabled(true);
			s = lst[1].lower();
			updateHistory(s, true);
			drw->makeMove(s);
			setCursor(QCursor(Qt::ArrowCursor));
			my_stat = tr("Your move...");
			emit showStatus(my_stat);
		}
	} else if (s == "quit") {
		gt = NOGAME;
		sockClosed();
	} else if (s == "chat") {
		s = str.right(str.length() - 5);
		updateChat('>' + s);
	} else if (s == "figure") {
		s = lst[1].lower();
		id = lst[2].toInt();
		drw->newFigure(s, id);
		updateHistory(id, true);
	}
}


void GameBoard::sendMove(const QString &str)
{

	protocol->sendMove(sock, str);
	emit sendData(str);
	drw->setEnabled(false);
	setCursor(QCursor(Qt::WaitCursor));
	updateHistory(str, false);
	sock_tout = SOCK_WAIT;
	my_stat = tr("Waiting a move...");
	emit showStatus(my_stat);
}


void GameBoard::closeEvent(QCloseEvent *e)
{
	int	res;

	if (gt != NOGAME) {
		res = QMessageBox::question(this, tr("End the game"),
			tr("Want you to end the game?\nYou will lose it"),
			tr("Yes, end"), tr("No, continue"), QString::null, 1);
		if (res == 0)
			QWidget::closeEvent(e);
	} else
		QWidget::closeEvent(e);
}


void
GameBoard::sendText()
{
	QString	s;

	s = edt->text().utf8();
	if (!s.isEmpty()) {
		updateChat(s);
		protocol->sendText(sock, s.ascii());
	}
	edt->clear();
}


void
GameBoard::updateChat(const QString &s)
{
	int	fh, h;

	lst->insertItem(QString::fromUtf8(s.ascii()));
	h = lst->height();
	fh = QFontMetrics(lst->font()).lineSpacing();
	if ((int)lst->count() * fh >= lst->visibleHeight())
		lst->removeItem(0);
}


void
GameBoard::updateHistory(const QString &st, bool t)
{
	QString	s;

	if (st.length() == 3) {
		if (st[0] == '@')
			s = "O-O";
		else
			s = st;
	} else
		s = st.left(2) + " - " + st.right(2);
	if (t) {
		if (gt == WHITE)
			hb->insertItem(s);
		else if (gt == BLACK)
			hw->insertItem(s);
	} else {
		if (gt == WHITE)
			hw->insertItem(s);
		else if (gt == BLACK)
			hb->insertItem(s);
	}
}


void
GameBoard::updateHistory(int id, bool t)
{
	QString	s("; "), s1;

	switch (id) {
		case 3:
			s += tr("B");
			break;
		case 4:
			s += tr("K");
			break;
		case 5:
			s += tr("C");
			break;
		case 10:
			s += tr("Q");
			break;
		default:
			s += tr("Error!");
	}
	if (t) {
		if (gt == WHITE) {
			id = hb->count() - 1;
			s1 = hb->text(id);
			hb->changeItem(s1 + s, id);
		} else if (gt == BLACK) {
			id = hw->count() - 1;
			s1 = hw->text(id);
			hw->changeItem(s1 + s, id);
		}
	} else {
		if (gt == WHITE) {
			id = hw->count() - 1;
			s1 = hw->text(id);
			hw->changeItem(s1 + s, id);
		} else if (gt == BLACK) {
			id = hb->count() - 1;
			s1 = hb->text(id);
			hb->changeItem(s1 + s, id);
		}
	}
}


void
GameBoard::sendFigure(const QString &coo, GameBoard::FigureType ft)
{
	int	id = -1;

	switch (ft) {
		case BLACK_CASTLE:
		case WHITE_CASTLE:
			id = 5;
			break;

		case BLACK_BISHOP:
		case WHITE_BISHOP:
			id = 3;
			break;

		case BLACK_KNIGHT:
		case WHITE_KNIGHT:
			id = 4;
			break;

		case BLACK_QUEEN:
		case WHITE_QUEEN:
			id = 10;
			break;
		default:
			id = -1;
	}
	if (id != -1) {
		protocol->sendFigure(sock, coo, id);
		updateHistory(id, false);
	}
}


void
GameBoard::sockTest()
{

	--sock_tout;
	if (sock_tout < 0) {
		tmr->stop();
		HAXEP:
		gt = NOGAME;
		sockClosed();
	} else if ((sock->state() == QAbstractSocket::HostLookupState) &&
		(sock_tout + 60 < SOCK_WAIT)) {
		tmr->stop();
		QMessageBox::critical(this, tr("Lookup Error"),
			tr("The host") + ' ' + hst + ' ' + tr("not found."));
		goto HAXEP;
	}

}


void
GameBoard::saveImage()
{
	QString	fn;

	fn = Q3FileDialog::getSaveFileName(QString::null, "*.png", this, NULL,
		tr("Save image"));

	if (!fn.isEmpty()) {
		if (fn.findRev(".png") < (int)(fn.length() - 4))
			fn += ".png";
		QPixmap::grabWidget(this).save(fn, "PNG");
	}
}


void
GameBoard::gameover(int type)
{
	bool	save = false;
	QString	s('\n' + tr("Do you want to save the image?")),
		yes(tr("Yes, save")),
		no(tr("No, don't save")),
		go(tr("Game over"));

	if (type == 0) {
		save = (QMessageBox::question(this, go,
			tr("You scored the game") + s, yes, no) == 0);
	} else if (type == 2) {
		updateHistory(GAMEOVER_TXT, false);
		protocol->sendGameover(sock, "MATE");
		save = (QMessageBox::question(this, go,
			tr("You have a mate.\nYou lost the game.") + s,
			yes, no) == 0);
	} else if (type == 3) {
		protocol->sendGameover(sock, "STALEMATE");
		save = (QMessageBox::question(this, go,
			tr("You have a stalemate") + s, yes, no) == 0);
	}

	if (save)
		saveImage();
}

//-----------------------------------------------------------------------------

Drawer::Drawer(GameBoard::FigureType *ft, GameBoard::GameType *g,
	QWidget *parent, const char *name)
	:QWidget(parent, name, Qt::WResizeNoErase | Qt::WNoAutoErase)
{
	QFontMetrics	fm(font());
	int		i;

	map = ft; gt = g;
	kk = rcm = lcm = km = false;
	cs = cell_size * 8;
	top_margin = 5;
	for (left_margin = 0, i = 0; i < 8; i++)
		left_margin = MAX(fm.width(QString::number(i)), left_margin);
	left_margin += top_margin;
	hl = fm.lineSpacing() + 2;
	setPaletteBackgroundColor(Qt::white);
	i = MAX(cs + left_margin + top_margin, cs + top_margin + hl);
	resize(i, i);
	x_brd = i - cs - 6;
	y_brd = 4;
	tfx = tfy = -1;
	fig[0] = QPixmap((const char **)black_bishop);
	fig[1] = QPixmap((const char **)black_castle);
	fig[2] = QPixmap((const char **)black_knight);
	fig[3] = QPixmap((const char **)black_pawn);
	fig[4] = QPixmap((const char **)black_king);
	fig[5] = QPixmap((const char **)black_queen);
	fig[6] = QPixmap((const char **)white_bishop);
	fig[7] = QPixmap((const char **)white_castle);
	fig[8] = QPixmap((const char **)white_knight);
	fig[9] = QPixmap((const char **)white_pawn);
	fig[10] = QPixmap((const char **)white_king);
	fig[11] = QPixmap((const char **)white_queen);
}

Drawer::~Drawer()
{
}


void
Drawer::paintEvent(QPaintEvent *e)
{
	QPainter	*p;
	int		w, y;

	w = width();
	y = w - 4;
	QWidget::paintEvent(e);
	p = new QPainter(this);
	p->setPen(Qt::black);
	p->drawRect(0, 0, w, w);
	p->drawRect(2, 2, y, y);
	p->drawLine(2, y, x_brd, cs + 4);
	drawBoard(p, x_brd, y_brd);
	drawMap(p, x_brd, y_brd);
	delete p;
}


void
Drawer::drawBoard(QPainter *p, int x, int y)
{
	int	i, j, cs, x1, r, k;
	char	c, st;

	cs = Drawer::cs + 2;
	p->setPen(Qt::black);
	p->drawRect(x, y, cs, cs);
	c = 'a'; st = 1;
	r = (*gt == GameBoard::BLACK);
	if (r) {
		c += 7;
		st = -st;
		k = 1;
		r ^= 1;
	} else
		k = 8;
	x1 = x + 1;
	for (j = 0, y++; j < 8; j++, y += cell_size, r ^= 1) {
		for (i = 0, x = x1; i < 8; i++, x += cell_size) {
			r ^= 1;
			if (r) {
				p->setPen(cw);
				p->setBrush(cw);
			} else {
				p->setPen(cb);
				p->setBrush(cb);
			}
			p->drawRect(x, y, cell_size, cell_size);
			if (j == 7) {
				p->setPen(Qt::black);
				p->drawText(x, cs + 2, cell_size, hl,
					Qt::AlignCenter, QChar(c));
				c += st;
			}
		}
		p->setPen(Qt::black);
		p->drawText(x1 - left_margin, y, left_margin, cell_size,
			Qt::AlignCenter, QString::number(k));
		k -= st;
	}
	if ((tfx != -1) && (tfy != -1)) {
		map2win(tfx, tfy, x, y);
		p->setPen(QPen(Qt::red, 2));
		p->setBrush(Qt::NoBrush);
		p->drawRect(x, y, cell_size, cell_size);
	}
}


void
Drawer::drawMap(QPainter *p, int x, int y)
{
	int	i, j, x1, n;
	QPixmap	*xpm;

	x1 = x + 1; y++;
	for (n = j = 0; j < 8; j++, y += cell_size) {
		for (i = 0, x = x1; i < 8; i++, x += cell_size) {
			switch (map[n++]) {
				case GameBoard::WHITE_PAWN:
					xpm = &fig[9];
					break;
				case GameBoard::WHITE_CASTLE:
					xpm = &fig[7];
					break;
				case GameBoard::WHITE_BISHOP:
					xpm = &fig[6];
					break;
				case GameBoard::WHITE_KING:
					xpm = &fig[10];
					break;
				case GameBoard::WHITE_QUEEN:
					xpm = &fig[11];
					break;
				case GameBoard::WHITE_KNIGHT:
					xpm = &fig[8];
					break;
				case GameBoard::BLACK_PAWN:
					xpm = &fig[3];
					break;
				case GameBoard::BLACK_CASTLE:
					xpm = &fig[1];
					break;
				case GameBoard::BLACK_BISHOP:
					xpm = &fig[0];
					break;
				case GameBoard::BLACK_KING:
					xpm = &fig[4];
					break;
				case GameBoard::BLACK_QUEEN:
					xpm = &fig[5];
					break;
				case GameBoard::BLACK_KNIGHT:
					xpm = &fig[2];
					break;
				default:
					xpm = NULL;
			}
			if (xpm != NULL)
				p->drawPixmap(x, y, *xpm);
		}
	}
}


void
Drawer::mousePressEvent(QMouseEvent *e)
{
	int	x = e->x() - x_brd,
		y = e->y() - y_brd;

	if ((x >= 0) && (x <= cs) && (y >= 0) && (y <= cs)) {
		win2map(x, y);
		if (hasTakenFigure()) {
			if ((tfx == x) && (tfy == y)) {
				tfx = tfy = -1;
				repaint(false);
			} else
				makeMove(*gt, tfx, tfy, x, y, false, false);
		} else if (canTake(x, y)) {
			takeFigure(x, y);
			emit touchFigure(x, y);
		}
	}
}


bool
Drawer::canTake(int x, int y)
{

	return (Figure::hasMyFigure(*gt, map, x, y, false));
}


void
Drawer::win2map(int &x, int &y)
{

	if (*gt == GameBoard::WHITE) {
		x /= cell_size;
		y = 8 - y / cell_size;
		x++;
	} else  if (*gt == GameBoard::BLACK) {
		x = 8 - x / cell_size;
		y /= cell_size;
		y++;
	}
}


void
Drawer::map2win(int mx, int my, int &x, int &y)
{

	if (*gt == GameBoard::WHITE) {
		x = (mx - 1) * cell_size + x_brd + 1;
		y = (8 - my) * cell_size + y_brd + 1;
	} else if (*gt == GameBoard::BLACK) {
		x = (8 - mx) * cell_size + x_brd + 1;
		y = (my - 1) * cell_size + y_brd + 1;
	} else {
		x = mx;
		y = my;
	}
}


void
Drawer::takeFigure(int x, int y)
{

	if ((tfx == x) && (tfy == y))
		tfx = tfy = -1;
	else {
		tfx = x;
		tfy = y;
	}
	repaint(false);
}


bool
Drawer::hasTakenFigure()
{

	return ((tfx != -1) && (tfy != -1));
}


void
Drawer::newFigure(const QString &coo, int id)
{
	GameBoard::FigureType	ft;
	int			x, y, n;

	ft = GameBoard::NONE; n = -1;
	Figure::str2map(coo, &x, &y);
	if (*gt == GameBoard::WHITE) {
		n = Figure::map2map(GameBoard::BLACK, x, y, true);
		switch (id) {
			case 3:
				ft = GameBoard::BLACK_BISHOP;
				break;
			case 4:
				ft = GameBoard::BLACK_KNIGHT;
				break;
			case 5:
				ft = GameBoard::BLACK_CASTLE;
				break;
			case 10:
				ft = GameBoard::BLACK_QUEEN;
				break;
			default:
				ft = GameBoard::NONE;
		}
	} else if (*gt == GameBoard::BLACK) {
		n = Figure::map2map(GameBoard::WHITE, x, y, true);
		switch (id) {
			case 3:
				ft = GameBoard::WHITE_BISHOP;
				break;
			case 4:
				ft = GameBoard::WHITE_KNIGHT;
				break;
			case 5:
				ft = GameBoard::WHITE_CASTLE;
				break;
			case 10:
				ft = GameBoard::WHITE_QUEEN;
				break;
			default:
				ft = GameBoard::NONE;
		}
	}

	if (ft != GameBoard::NONE) {
		map[n] = ft;
		repaint(false);
	}
}


void
Drawer::makeMove(const QString &txt)
{
	int	fx, fy, tx, ty;
	GameBoard::GameType	et;

	if (*gt == GameBoard::WHITE)
		et = GameBoard::BLACK;
	else if (*gt == GameBoard::BLACK)
		et = GameBoard::WHITE;
	else
		et = GameBoard::NOGAME;
	if (txt == LONG_XCHG) {
		if (et == GameBoard::BLACK)
			makeMove(et, 1, 8, 4, 8, true, true);
		else if (et == GameBoard::WHITE)
			makeMove(et, 1, 1, 4, 1, true, true);
	} else if (txt == SHORT_XCHG) {
		if (et == GameBoard::BLACK)
			makeMove(et, 8, 8, 6, 8, true, true);
		else if (et == GameBoard::WHITE)
			makeMove(et, 8, 1, 6, 1, true, true);
	} else {
		Figure::str2map(txt.left(2), &fx, &fy);
		Figure::str2map(txt.right(2), &tx, &ty);
		makeMove(et, fx, fy, tx, ty, true, false);
	}
}


void
Drawer::makeMove(GameBoard::GameType gt, int fx, int fy, int tx, int ty,
	bool mirror, bool xc)
{
	GameBoard::GameType	et;
	GameBoard::FigureType	fo, old;
	int			res, nf, nt;
	FigureDialog		*dlg;
	bool			x;
	Q3PointArray		vl;

	et = GameBoard::NOGAME;
	nf = Figure::map2map(gt, fx, fy, mirror);
	fo = map[nf];
	nt = Figure::map2map(gt, tx, ty, mirror);
	old = map[nt];
	res = Figure::validMove(gt, map, fx, fy, tx, ty, mirror);
	if (res) {
		if (!mirror) {
			x = false;
			if (gt == GameBoard::WHITE)
				et = GameBoard::BLACK;
			else if (gt == GameBoard::BLACK)
				et = GameBoard::WHITE;
			if (Figure::checkKing(et, map, mirror, vl, true) !=
				0) {
				map[nf] = map[nt];
				map[nt] = old;
				tfx = tfy = -1;
				QMessageBox::information(this,
					tr("Error moving"), tr("You cannot "
					"move this figure because the king "
					"is in check") + '.');
				goto HAXEP;
			} else
				kk = false;
			if (!km && (!lcm || !rcm) && !kk)
				x = xchg(fo, map[nt], fx, fy, tx, ty);
			else
				x = true;
			if (x)
				emit moved(Figure::map2str(fx, fy) +
					Figure::map2str(tx, ty));
			if ((res & 0xF) == 2) {
				dlg = new FigureDialog(fig, gt, this);
				dlg->exec();
				fo = dlg->figure();
				delete dlg;
				map[nt] = fo;
				emit newFigure(Figure::map2str(tx, ty), fo);
			}
			tfx = tfy = -1;
		} else if (xc) {
			if (gt == GameBoard::BLACK)
				checkBlackCastle(fx, fy, tx, ty, true);
			else if (gt == GameBoard::WHITE)
				checkWhiteCastle(fx, fy, tx, ty, true);
		}
		if (mirror && (res & 0x10)) {
			kk = true;
		} else if (res & 0x20) {
			repaint(false);
			emit gameover(2);
			return;
		} else if (res & 0x40) {
			repaint(false);
			emit gameover(3);
			return;
		}
		HAXEP:
		repaint(false);
	}
}


bool
Drawer::xchg(GameBoard::FigureType o, GameBoard::FigureType n,
	int fx, int fy, int tx, int ty)
{
	bool	ret = true;

	if (*gt == GameBoard::WHITE) {
		km = ((o == n) && (o == GameBoard::WHITE_KING));
		if (!km && ((o == n) && (o == GameBoard::WHITE_CASTLE)))
			ret = checkWhiteCastle(fx, fy, tx, ty, false);
	} else if (*gt == GameBoard::BLACK) {
		km = ((o == n) && (o == GameBoard::BLACK_KING));
		if (!km && ((o == n) && (o == GameBoard::BLACK_CASTLE)))
			ret = checkBlackCastle(fx, fy, tx, ty, false);
	}

	return (ret);
}


bool
Drawer::checkWhiteCastle(int fx, int fy, int tx, int ty, bool mirror)
{
	int	n1, n2;
	bool	ret = true;

	n1 = n2 = -1;
	if ((fx == 1) && (fy == 1)) {
		if ((tx == 4) && (ty == 1))
			if (mirror) {
				n1 = Figure::map2map(*gt, 5, 1, false);
				n2 = Figure::map2map(*gt, 3, 1, false);
			} else if (!lcm) {
				if (makeXchg()) {
					n1 = Figure::map2map(*gt, 5, 1, false);
					n2 = Figure::map2map(*gt, 3, 1, false);
					emit moved(LONG_XCHG);
					ret = false;
					rcm = true;
				}
				lcm = true;
			}
	} else if ((fx == 8) && (fy == 1)) {
		if ((tx == 6) && (ty == 1))
			if (mirror) {
				n1 = Figure::map2map(*gt, 5, 1, false);
				n2 = Figure::map2map(*gt, 7, 1, false);
			} else if (!rcm) {
				if (makeXchg()) {
					n1 = Figure::map2map(*gt, 5, 1, false);
					n2 = Figure::map2map(*gt, 7, 1, false);
					emit moved(SHORT_XCHG);
					ret = false;
					lcm = true;
				}
				rcm = true;
			}
	}
	if (n1 != n2) {
		map[n2] = map[n1];
		map[n1] = GameBoard::NONE;
	}

	return (ret);
}


bool
Drawer::checkBlackCastle(int fx, int fy, int tx, int ty, bool mirror)
{
	int	n1, n2;
	bool	ret = true;

	n1 = n2 = -1;
	if ((fx == 1) && (fy == 8)) {
		if ((tx == 4) && (ty == 8)) {
			if (mirror) {
				n1 = Figure::map2map(*gt, 5, 8, false);
				n2 = Figure::map2map(*gt, 3, 8, false);
			} else if (!rcm) {
				if (makeXchg()) {
					n1 = Figure::map2map(*gt, 5, 8, false);
					n2 = Figure::map2map(*gt, 3, 8, false);
					emit moved(LONG_XCHG);
					ret = false;
				}
				rcm = true;
			}
		}
	} else if ((fx == 8) && (fy == 8)) {
		if ((tx == 6) && (ty == 8))
			if (mirror) {
				n1 = Figure::map2map(*gt, 5, 8, false);
				n2 = Figure::map2map(*gt, 7, 8, false);
			} else if (!lcm) {
				if (makeXchg()) {
					n1 = Figure::map2map(*gt, 5, 8, false);
					n2 = Figure::map2map(*gt, 7, 8, false);
					emit moved(SHORT_XCHG);
					ret = false;
				}
				lcm = true;
			}
	}
	if (n1 != n2) {
		map[n2] = map[n1];
		map[n1] = GameBoard::NONE;
	}

	return (ret);
}


bool
Drawer::makeXchg()
{

	return (QMessageBox::question(this, tr("To castle"),
		tr("Do you want to castle?"), tr("Yes"), tr("No")) == 0);
}


//-----------------------------------------------------------------------------

FigureDialog::FigureDialog(const QPixmap *f, const GameBoard::GameType g,
	QWidget *parent, const char *name):QDialog(parent, name)
{
	QFontMetrics	fm(font());
	int		w, h;

	gt = g; fig = f;
	if (gt == GameBoard::WHITE)
		fr = GameBoard::WHITE_QUEEN;
	else if (gt == GameBoard::BLACK)
		fr = GameBoard::BLACK_QUEEN;
	str = tr("What figure should I set?");
	setCaption(str);
	fh = fm.lineSpacing() + 2;
	h = cell_size + fh;
	w = MAX(cell_size * 4, fm.width(str));
	step = (w - cell_size * 4) / 2;
	resize(w, h);
	setMinimumSize(size());
	setMaximumSize(size());
}

FigureDialog::~FigureDialog()
{
}


void
FigureDialog::paintEvent(QPaintEvent *e)
{
	QPainter	*p;
	int		x, f = -1;

	QDialog::paintEvent(e);
	p = new QPainter(this);

	p->setPen(Qt::black);
	p->drawText(0, 0, width(), fh, Qt::AlignCenter, str);
	x = step;
	if (gt == GameBoard::BLACK)
		f = 0;
	else if (gt == GameBoard::WHITE)
		f = 6;
	p->drawPixmap(x, fh, fig[f]); x += cell_size;
	p->drawPixmap(x, fh, fig[f + 1]); x += cell_size;
	p->drawPixmap(x, fh, fig[f + 2]); x += cell_size;
	p->drawPixmap(x, fh, fig[f + 5]);

	delete p;
}


void
FigureDialog::mousePressEvent(QMouseEvent *e)
{
	int	x = e->x(),
		y = e->y(),
		f = -1;

	if (e->button() == Qt::LeftButton) {
		if ((x >= step) && (x <= width() - step) && (y >= fh) &&
			(y <= height()))
			f = (x - step) / cell_size;
	}

	if (f != -1) {
		if (gt == GameBoard::WHITE)
			switch (f) {
				case 0:
					fr = GameBoard::WHITE_BISHOP;
					break;
				case 1:
					fr = GameBoard::WHITE_CASTLE;
					break;
				case 2:
					fr = GameBoard::WHITE_KNIGHT;
					break;
				case 3:
				default:
					fr = GameBoard::WHITE_QUEEN;
			}
		else if (gt == GameBoard::BLACK)
			switch (f) {
				case 0:
					fr = GameBoard::BLACK_BISHOP;
					break;
				case 1:
					fr = GameBoard::BLACK_CASTLE;
					break;
				case 2:
					fr = GameBoard::BLACK_KNIGHT;
					break;
				case 3:
				default:
					fr = GameBoard::BLACK_QUEEN;
			}
		accept();
	}
}


//-----------------------------------------------------------------------------

void
GameProtocol::send(Q3Socket *sock, const QString &dat)
{
	/*QString		s(dat + EOL);
	const char	*buf;

	if (sock->state() == Q3Socket::Connected) {
		buf = s.ascii();
		sock->writeBlock(buf, s.length());
		sock->flush();
	}*/
	qDebug() << (qPrintable(QString("GameProtocol::send(%1)").arg(dat)));
	emit sendData(dat);
}


void
GameProtocol::setGameType(Q3Socket *sock, GameBoard::GameType gt)
{
	QString		d("GAME");

	d += SEP;
	if (gt == GameBoard::WHITE)
		d += "WHITE";
	else if (gt == GameBoard::BLACK)
		d += "BLACK";
	else
		d += "NOGAME";
	send(sock, d);
}


void
GameProtocol::acceptGame(Q3Socket *sock)
{
	QString		d("GAME");

	d += SEP;
	d += "ACCEPT";
	send(sock, d);
}


void
GameProtocol::sendMove(Q3Socket *sock, const QString &coo)
{
	QString		d("MOVE");

	d += SEP;
	d += coo;
	send(sock, d);
}


void
GameProtocol::sendQuit(Q3Socket *sock)
{

	send(sock, "QUIT");
}


void
GameProtocol::sendText(Q3Socket *sock, const QString &txt)
{
	QString		d("CHAT");

	d += SEP;
	d += txt;
	send(sock, d);
}


void
GameProtocol::sendFigure(Q3Socket *sock, const QString &coo, int id)
{
	QString		d("FIGURE");

	d += SEP;
	d += coo;
	d += SEP;
	d += QString::number(id);
	send(sock, d);
}


void
GameProtocol::sendGameover(Q3Socket *sock, const QString &got)
{
	QString		d("GAME");

	d += SEP;
	d += got;
	send(sock, d);
}

