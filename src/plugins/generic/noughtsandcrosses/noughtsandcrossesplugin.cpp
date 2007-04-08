/*
 * noughtsandcrossesplugin.cpp - Psi plugin to play noughts and crosses
 * Copyright (C) 2006  Kevin Smith
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * You can also redistribute and/or modify this program under the
 * terms of the Psi License, specified in the accompanied COPYING
 * file, as published by the Psi Project; either dated January 1st,
 * 2005, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <QtCore>
#include <QDebug>

#include "psiplugin.h"
#include "tictac.h"

class NoughtsAndCrossesPlugin : public PsiPlugin
{
	Q_OBJECT
	Q_INTERFACES(PsiPlugin)

public:
	NoughtsAndCrossesPlugin();
	virtual QString name() const; 
	virtual void message( const PsiAccount* account, const QString& message, const QString& fromJid, const QString& fromDisplay); 
	virtual QString shortName() const;
	virtual QString version() const;

private slots:
	void stopGame();
	void myTurn(int space);
	void theirTurn(int space);
	void gameOver(TicTacGameBoard::State state);
	
private:
	void startGame(QString jid, int size, bool meFirst, const PsiAccount* account);
	

	
	TicTacToe* game;
	QString playingWith;
	PsiAccount* account_;
};

Q_EXPORT_PLUGIN(NoughtsAndCrossesPlugin);

NoughtsAndCrossesPlugin::NoughtsAndCrossesPlugin() : PsiPlugin()
{
	game = NULL;
}

QString NoughtsAndCrossesPlugin::name() const
{
	return "NoughtsAndCrosses Plugin";
}

QString NoughtsAndCrossesPlugin::shortName() const
{
	return "noughtsandcrosses";
}

QString NoughtsAndCrossesPlugin::version() const
{
	return "0.0.1";
}

void NoughtsAndCrossesPlugin::message( const PsiAccount* account, const QString& message, const QString& fromJid, const QString& fromDisplay)
{
	QString reply;
	qDebug("naughtsandcrosses message");
	if (!message.startsWith("noughtsandcrosses"))
		return;
	qDebug("message for us in noughtsandcrosses");
	if (game && fromJid != playingWith)
	{
		reply=QString("<message to=\"%1\" type=\"chat\"><body>already playing with %2, sorry</body></message>").arg(fromJid).arg(playingWith);
		emit sendStanza(account, reply);
		return;
	}
	QString command = QString(message);
	command.remove(0,18);
	qDebug(qPrintable(QString("noughtsandcrosses command string %1").arg(command)));
	if (command == QString("start"))
	{
		if (game)
			return;
		qWarning(qPrintable(QString("Received message '%1', launching nac with %2").arg(message).arg(fromDisplay)));
		QString reply;
		reply=QString("<message to=\"%1\" type=\"chat\"><body>noughtsandcrosses starting</body></message>").arg(fromJid);
		emit sendStanza(account, reply);
		startGame(fromJid, 3, false, account);
	}
	else if (command == QString("starting"))
	{
		if (game)
			return;
		qWarning(qPrintable(QString("Received message '%1', launching nac with %2").arg(message).arg(fromDisplay)));
		QString reply;
		reply=QString("<message to=\"%1\" type=\"chat\"><body>starting noughts and crosses, I go first :)</body></message>").arg(fromJid);
		emit sendStanza(account, reply);
		startGame(fromJid, 3, true, account);
	}
	else if (!game)
	{
		return;
	}
	else if (command.startsWith("move"))
	{
		command.remove(0,5);
		
		int space=command.toInt();
		qDebug(qPrintable(QString("noughtsandcrosses move to space %1").arg(space)));
		theirTurn(space);
	}
}	

void NoughtsAndCrossesPlugin::startGame(QString jid, int size, bool meFirst, const PsiAccount* account)
{
	game = new TicTacToe( meFirst, size );
	game->setCaption(QString("Noughts and Crosses with %1").arg(jid));
	playingWith=jid;
    game->show();
	account_=(PsiAccount*)account;
	connect(game, SIGNAL(closing()), this, SLOT(stopGame()));
	connect(game, SIGNAL(myMove(int)), this, SLOT(myTurn(int)));
	connect(game, SIGNAL(gameOverSignal(TicTacGameBoard::State)), this, SLOT(gameOver(TicTacGameBoard::State)));
}

void NoughtsAndCrossesPlugin::stopGame()
{
	delete game;
	game=NULL;
}

void NoughtsAndCrossesPlugin::gameOver(TicTacGameBoard::State state)
{
	QString reply;
	QString winner;
	switch (state)
	{
		case TicTacGameBoard::HumanWon:
			winner="I";
			break;
		case TicTacGameBoard::ComputerWon:
			winner="You";
			break;
		case TicTacGameBoard::NobodyWon:
			winner="It was a draw, no-one";
			break;
		default:
			winner="ERROR!!!";
	}
	reply=QString("<message to=\"%1\" type=\"chat\"><body>%2 won. Good game.</body></message>").arg(playingWith).arg(winner);
	emit sendStanza(account_, reply);
}

void NoughtsAndCrossesPlugin::myTurn(int space)
{
	qDebug(qPrintable(QString("my turn: %1").arg(space)));
	if (!game)
		return;
	QString reply;
	reply=QString("<message to=\"%1\" type=\"chat\"><body>noughtsandcrosses move %2</body></message>").arg(playingWith).arg(space);
	emit sendStanza(account_, reply);
}

void NoughtsAndCrossesPlugin::theirTurn(int space)
{
	qDebug(qPrintable(QString("their turn: %1").arg(space)));
	if (!game)
		return;
	game->theirMove(space);
}


#include "noughtsandcrossesplugin.moc"
