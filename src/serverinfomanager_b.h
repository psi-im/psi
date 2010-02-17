/*
 * serverinfomanager.h
 * Copyright (C) 2006  Remko Troncon
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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

#ifndef SERVERINFOMANAGER_H
#define SERVERINFOMANAGER_H

#include <QObject>
#include <QString>

namespace XMPP {
	class Client;
}

class ServerInfoManager : public QObject
{
	Q_OBJECT

public:
	ServerInfoManager(XMPP::Client* client);

	const QString& multicastService() const;
	bool hasPEP() const;

	QString mucService() const;

	QString stunHost() const;
	int stunPort() const;

	QString udpTurnHost() const;
	int udpTurnPort() const;
	QString udpTurnUser() const;
	QString udpTurnPass() const;

	QString tcpTurnHost() const;
	int tcpTurnPort() const;
	QString tcpTurnUser() const;
	QString tcpTurnPass() const;

signals:
	void featuresChanged();

private slots:
	void disco_finished();
	void initialize();
	void deinitialize();
	void reset();

private:
	class Private;
	friend class Private;
	Private *d;
	//XMPP::Client* client_;
	QString multicastService_;
	bool featuresRequested_;
	bool hasPEP_;
};

#endif
