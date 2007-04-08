/*
 * chessplugin.cpp - Psi plugin to play chess
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
#include "gameboard.h"

extern QColor	cw, cb;

class ChessPlugin : public PsiPlugin
{
	Q_OBJECT
	Q_INTERFACES(PsiPlugin)

public:
	ChessPlugin();
	virtual QString name() const; 
	virtual bool processEvent( const PsiAccount* account, QDomNode &event );
	virtual void message( const PsiAccount* account, const QString& message, const QString& fromJid, const QString& fromDisplay); 
	virtual QString shortName() const;
	virtual QString version() const;

private slots:
	void sendData(const QString& data);
	void receiveData(const QString& data);

	
private:
	void startGame(const QString& jid, bool meWhite, const PsiAccount* account);
	void stopGame();
	

	
	GameBoard* game_;
	QString playingWith_;
	PsiAccount* account_;
};

Q_EXPORT_PLUGIN(ChessPlugin);

ChessPlugin::ChessPlugin() : PsiPlugin()
{
	game_ = NULL;
}

QString ChessPlugin::name() const
{
	return "Chess Plugin";
}

QString ChessPlugin::shortName() const
{
	return "chess";
}

QString ChessPlugin::version() const
{
	return "0.0.1";
}

bool ChessPlugin::processEvent( const PsiAccount* account, QDomNode &event )
{
	return true;
}


void ChessPlugin::message( const PsiAccount* account, const QString& message, const QString& fromJid, const QString& fromDisplay)
{
	QString reply;
	qDebug("chess message");
	if (!message.startsWith("chess"))
		return;
	qDebug("message for us in chess");
	if (game_ && fromJid != playingWith_)
	{
		reply=QString("<message to=\"%1\" type=\"chat\"><body>already playing chess with %2, sorry</body></message>").arg(fromJid).arg(playingWith_);
		emit sendStanza(account, reply);
		return;
	}
	QString command = QString(message);
	command.remove(0,6);
	qDebug() << (qPrintable(QString("chess command string %1").arg(command)));
	if (command == QString("start"))
	{
		if (game_)
			return;
		qWarning(qPrintable(QString("Received message '%1', launching chess with %2").arg(message).arg(fromDisplay)));
		QString reply;
		reply=QString("<message to=\"%1\" type=\"chat\"><body>chess starting</body></message>").arg(fromJid);
		emit sendStanza(account, reply);
		startGame(fromJid, false, account);
	}
	else if (command == QString("starting"))
	{
		if (game_)
			return;
		qWarning(qPrintable(QString("Received message '%1', launching chess with %2").arg(message).arg(fromDisplay)));
		QString reply;
		reply=QString("<message to=\"%1\" type=\"chat\"><body>starting chess, I go first :)</body></message>").arg(fromJid);
		emit sendStanza(account, reply);
		startGame(fromJid, true, account);
	}
	else if (!game_)
	{
		return;
	}
	else if (command.startsWith("command"))
	{
		command.remove(0,8);
		
		qDebug() << (qPrintable(QString("chess command %1").arg(command)));
		receiveData(command);
	}
}

void ChessPlugin::startGame(const QString& jid, bool meFirst, const PsiAccount* account)
{
/*	game = new TicTacToe( meFirst, size );
	game->setCaption(QString("Noughts and Crosses with %1").arg(jid));
	playingWith=jid;
    game->show();
	account_=(PsiAccount*)account;
	connect(game, SIGNAL(closing()), this, SLOT(stopGame()));
	connect(game, SIGNAL(myMove(int)), this, SLOT(myTurn(int)));
	connect(game, SIGNAL(gameOverSignal(TicTacGameBoard::State)), this, SLOT(gameOver(TicTacGameBoard::State)));
*/
	playingWith_=jid;
	account_=(PsiAccount*)account;
	cw = QColor(0x8F, 0xDF, 0xF0);
	cb = QColor(0x70, 0x9F, 0xDF);
	if (meFirst)
	{
		GameBoard::GameType type=GameBoard::WHITE;
		game_ = new GameBoard(type, jid, NULL);
	} else {
		game_ = new GameBoard(1);
	}
	
	//showStatus(game_->status());
	//QObject::connect(brd, SIGNAL(showStatus(const QString&)), this, SLOT(showStatus(const QString&)));
	connect(game_, SIGNAL(sendData(const QString&)), this, SLOT(sendData(const QString &)));
	game_->show();
 }

void ChessPlugin::stopGame()
{
	delete game_;
	game_=NULL;
}

/*void ChessPlugin::gameOver(TicTacGameBoard::State state)
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
}*/

void ChessPlugin::sendData(const QString& data)
{
	qDebug() << (qPrintable(QString("sendingData turn: %1").arg(data)));
	if (!game_)
		return;
	QString reply;
	QString stanzaId="aaaa";
	reply=QString("<message to=\"%1\" type=\"chat\" id=\"%2\"><body>chess command %3</body></message>").arg(playingWith_).arg(stanzaId).arg(data);
	qDebug() << (qPrintable(QString("sendingData stanza: %1").arg(reply)));
	emit sendStanza(account_, reply);
}

void ChessPlugin::receiveData(const QString& data)
{
	qDebug() << (qPrintable(QString("received data: %1").arg(data)));
	if (!game_)
		return;
	//game->theirMove(space);
	game_->receiveData(data);
}


#include "chessplugin.moc"
