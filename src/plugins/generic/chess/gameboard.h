/*
 * Copyright (c) 2005 by SilverSoft.Net
 * All rights reserved
 *
 * $Id: gameboard.h,v 0.1 2005/01/08 13:00:57 denis Exp $
 *
 * Author: Denis Kozadaev (denis@silversoft.net)
 * Description:
 *
 * See also: style(9)
 *
 * Hacked by:
 */

#ifndef	__GAME_BOARD_H__
#define	__GAME_BOARD_H__

#include <QWidget>
#include <QPainter>
#include <QPixmap>
#include <QDialog>
#include <QLineEdit>
#include <QTimer>
//Added by qt3to4:
#include <QCloseEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QFocusEvent>
#include <QMouseEvent>
#include <stdlib.h>

#define	MAX(a, b)	(((a) > (b))?(a):(b))
#define	SEP		' '
#define	EOL		'\n'
#define	LONG_XCHG	"@-@"
#define	SHORT_XCHG	"o-o"
#define	SOCK_WAIT	900
#define	GAMEOVER_TXT	"****"

class GameBoard;
class Drawer;
class Figure;
class GameProtocol;

class GameBoard:public QWidget
{
	Q_OBJECT
public:
	enum GameType {
		NOGAME	= 0x0,
		BLACK	= 0x1,
		WHITE	= 0x2
	};

	enum FigureType {
		NONE		= 0x00,
		WHITE_PAWN	= 0x01,
		WHITE_CASTLE	= 0x02,
		WHITE_BISHOP	= 0x03,
		WHITE_KING	= 0x04,
		WHITE_QUEEN	= 0x05,
		WHITE_KNIGHT	= 0x06,
		BLACK_PAWN	= 0x11,
		BLACK_CASTLE	= 0x12,
		BLACK_BISHOP	= 0x13,
		BLACK_KING	= 0x14,
		BLACK_QUEEN	= 0x15,
		BLACK_KNIGHT	= 0x16,
		DUMMY		= 0xFF
	};

	GameBoard(GameType, const QString &, QWidget *parent = NULL,
		const char *name = NULL);
	GameBoard(int, QWidget *parent = NULL, const char *name = NULL);
	~GameBoard();

	void		saveImage();

	GameType	type()const{return (gt);}
	QString		status()const{return (my_stat);}

public slots:
	void receiveData(const QString& data);
	
private:
	Drawer		*drw;
	GameType	gt;
	FigureType	*map;
	QString		hst, my_stat;
	Q3Socket		*sock;
	Q3GroupBox	*box, *hist;
	QListWidget	*lst, *hw, *hb;
	QLineEdit	*edt;
	QTimer		*tmr, *tmr2;
	int		sock_tout;
	GameProtocol* protocol;

	void	initMap();
	void	parseString(const QString&);
	void	updateChat(const QString&);
	void	updateHistory(const QString&, bool);
	void	updateHistory(int, bool);

protected:
	void	resizeEvent(QResizeEvent *);
	void	closeEvent(QCloseEvent *);
	void	focusInEvent(QFocusEvent *);

private slots:
	void	showHostFound();
	void	sockConnected();
	void	sockRead(const QString& data);
	void	sockClosed();
	void	sendMove(const QString&);
	void	sendText();
	void	sendFigure(const QString&, GameBoard::FigureType);
	void	sockTest();
	void	sockError(int);
	void	gameover(int);

signals:
	void	showStatus(const QString&);
	void sendData(const QString& data);
};

//-----------------------------------------------------------------------------

class Drawer:public QWidget
{
	Q_OBJECT
public:
	Drawer(GameBoard::FigureType *, GameBoard::GameType *,
		QWidget *parent = NULL, const char *name = NULL);
	~Drawer();

	void	makeMove(const QString&);
	void	newFigure(const QString&, int);

private:
	int			top_margin, left_margin, hl;
	int			x_brd, y_brd, cs;
	int			tfx, tfy;
	QPixmap			fig[12];
	GameBoard::FigureType	*map;
	GameBoard::GameType	*gt;
	bool			km, lcm, rcm, kk;

	void	drawBoard(QPainter *, int, int);
	void	drawMap(QPainter *, int, int);
	void	win2map(int&, int&);
	void	map2win(int, int, int&, int&);
	void	takeFigure(int, int);
	void	makeMove(GameBoard::GameType, int, int, int, int, bool, bool);
	bool	xchg(GameBoard::FigureType, GameBoard::FigureType,
			int, int, int, int);
	bool	checkWhiteCastle(int, int, int, int, bool);
	bool	checkBlackCastle(int, int, int, int, bool);

	bool	canTake(int, int);
	bool	hasTakenFigure();
	bool	makeXchg();

protected:
	void	paintEvent(QPaintEvent *);
	void	mousePressEvent(QMouseEvent *);

signals:
	void	touchFigure(int, int);
	void	moved(const QString&);
	void	newFigure(const QString&, GameBoard::FigureType);
	void	gameover(int);
};

//-----------------------------------------------------------------------------

class FigureDialog:public QDialog
{
	Q_OBJECT
public:
	FigureDialog(const QPixmap *, const GameBoard::GameType,
		QWidget *parent = NULL, const char *name = NULL);
	~FigureDialog();

	GameBoard::FigureType	figure()const{return (fr);}

private:
	GameBoard::GameType	gt;
	const QPixmap		*fig;
	QString			str;
	int			step, fh;
	GameBoard::FigureType	fr;

protected:
	void	paintEvent(QPaintEvent *);
	void	mousePressEvent(QMouseEvent *);
};

//-----------------------------------------------------------------------------

class Figure
{
public:

	static bool	hasMyFigure(GameBoard::GameType,
				GameBoard::FigureType *, int, int, bool);
	static int	hasEnemyFigure(GameBoard::GameType,
				GameBoard::FigureType *, int, int, bool);
	static bool	hasFigure(GameBoard::GameType,
				GameBoard::FigureType *, int, int, bool);
	static int	map2map(GameBoard::GameType, int, int, bool);
	static int	validMove(GameBoard::GameType, GameBoard::FigureType *,
				int, int, int, int, bool);

	static void	moveList(Q3PointArray&, GameBoard::GameType,
				GameBoard::FigureType *, int, int, bool);
	static void	moveListWhitePawn(Q3PointArray&, GameBoard::GameType,
				GameBoard::FigureType *, int, int, bool);
	static void	moveListBlackPawn(Q3PointArray&, GameBoard::GameType,
				GameBoard::FigureType *, int, int, bool);
	static void	moveListCastle(Q3PointArray&, GameBoard::GameType,
				GameBoard::FigureType *, int, int, bool);
	static void	moveListBishop(Q3PointArray&, GameBoard::GameType,
				GameBoard::FigureType *, int, int, bool);
	static void	moveListKing(Q3PointArray&, GameBoard::GameType,
				GameBoard::FigureType *, int, int, bool);
	static void	moveListQueen(Q3PointArray&, GameBoard::GameType,
				GameBoard::FigureType *, int, int, bool);
	static void	moveListKnight(Q3PointArray&, GameBoard::GameType,
				GameBoard::FigureType *, int, int, bool);
	static bool	hasPoint(const Q3PointArray&, int, int);
	static bool	hasKingsMeeting(GameBoard::GameType,
				GameBoard::FigureType *, int, int, bool);
	static bool	validPoint(GameBoard::GameType,
				GameBoard::FigureType *, int, int, bool);
	static QString	map2str(int, int);
	static void	str2map(const QString&, int *, int *);
	static int	checkKing(GameBoard::GameType, GameBoard::FigureType *,
				bool, Q3PointArray&, bool);
};

//-----------------------------------------------------------------------------

class GameProtocol:public QDialog
{
	Q_OBJECT
public:
	void	send(Q3Socket *, const QString&);
	void	setGameType(Q3Socket *, GameBoard::GameType);
	void	acceptGame(Q3Socket *);
	void	sendMove(Q3Socket *, const QString&);
	void	sendQuit(Q3Socket *);
	void	sendText(Q3Socket *, const QString&);
	void	sendFigure(Q3Socket *, const QString&, int);
	void	sendGameover(Q3Socket *, const QString&);
signals:
	void sendData(const QString& data);
};

#endif	/* __GAME_BOARD_H__ */
