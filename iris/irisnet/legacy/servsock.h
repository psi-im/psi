/*
 * servsock.h - simple wrapper to QServerSocket
 * Copyright (C) 2003  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef CS_SERVSOCK_H
#define CS_SERVSOCK_H

#include <QtCore>
#include <QtNetwork>

// CS_NAMESPACE_BEGIN

class ServSock : public QObject
{
	Q_OBJECT
public:
	ServSock(QObject *parent=0);
	~ServSock();

	bool isActive() const;
	bool listen(quint16 port);
	void stop();
	int port() const;
	QHostAddress address() const;

signals:
	void connectionReady(int);

private slots:
	void sss_connectionReady(int);

private:
	class Private;
	Private *d;
};

class ServSockSignal : public QTcpServer
{
	Q_OBJECT
public:
	ServSockSignal(QObject *parent = 0);

signals:
	void connectionReady(int);

protected:
	// reimplemented
	void incomingConnection(int socketDescriptor);
};

// CS_NAMESPACE_END

#endif
