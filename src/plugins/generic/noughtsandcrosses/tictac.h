
/****************************************************************************
** $Id: $
**
** Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#ifndef TICTAC_H
#define TICTAC_H


#include <QPushButton>
#include <Q3PtrVector>
//Added by qt3to4:
#include <Q3MemArray>
#include <QLabel>

class QComboBox;
class QLabel;


// --------------------------------------------------------------------------
// TicTacButton implements a single tic-tac-toe button
//

class TicTacButton : public QPushButton
{
    Q_OBJECT
public:
    TicTacButton( QWidget *parent );
    enum Type { Blank, Circle, Cross };
    Type        type() const            { return t; }
    void        setType( Type type )    
	{ 
		t = type; 
		QString mark=""; 
		if (t==Circle)
			mark="0";
		if (t==Cross)
			mark="X";
		setText(mark); repaint(); }
    QSizePolicy sizePolicy() const
    { return QSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred ); }
    QSize sizeHint() const { return QSize( 32, 32 ); }
    QSize minimumSizeHint() const { return QSize( 10, 10 ); }
//protected:
//    void        drawButtonLabel( QPainter * );
private:
    Type t;
};

// Using template vector to make vector-class of TicTacButton.
// This vector is used by the TicTacGameBoard class defined below.

typedef Q3PtrVector<TicTacButton>        TicTacButtons;
typedef Q3MemArray<int>          TicTacArray;


// --------------------------------------------------------------------------
// TicTacGameBoard implements the tic-tac-toe game board.
// TicTacGameBoard is a composite widget that contains N x N TicTacButtons.
// N is specified in the constructor.
//

class TicTacGameBoard : public QWidget
{
    Q_OBJECT
public:
    TicTacGameBoard( bool meFirst, int n, QWidget *parent=0, const char *name=0 );
   ~TicTacGameBoard();
    enum        State { Init, HumansTurn, ComputersTurn, HumanWon, ComputerWon, NobodyWon };
    State       state() const           { return st; }
    void        computerStarts( bool v );
    void        newGame();

signals:
    void        finished();                     // game finished
	void myMove(int space);
	void stateChanged();
public slots:
	void theirMove(int space);
private slots:
    void        buttonClicked();
private:
    void        setState( State state ) { st = state; }
    void        updateButtons();
    int checkBoard( TicTacArray * );
    void        computerMove();
    State       st;
    int         nBoard;
    bool        comp_starts;
    TicTacArray *btArray;
    TicTacButtons *buttons;
};


// --------------------------------------------------------------------------
// TicTacToe implements the complete game.
// TicTacToe is a composite widget that contains a TicTacGameBoard and
// two push buttons for starting the game and quitting.
//

class TicTacToe : public QWidget
{
    Q_OBJECT
public:
    TicTacToe( bool meFirst, int boardSize=3, QWidget *parent=0, const char *name=0 );

signals:
	void	closing();
	void myMove(int space);
	void gameOverSignal(TicTacGameBoard::State state);
public slots:
	void theirMove(int space);
private slots:
    void        newGameClicked();
    void        gameOver();
    void        newState();
private:

    QComboBox   *whoStarts;
    QPushButton *newGame;
    QPushButton *quit;
    QLabel      *message;
    TicTacGameBoard *board;
};


#endif // TICTAC_H
