/*
 * socks.h - SOCKS5 TCP proxy client/server
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

#ifndef CS_SOCKS_H
#define CS_SOCKS_H

#include "bytestream.h"

// CS_NAMESPACE_BEGIN

class QHostAddress;
class SocksClient;
class SocksServer;

class SocksUDP : public QObject
{
	Q_OBJECT
public:
	~SocksUDP();

	void change(const QString &host, int port);
	void write(const QByteArray &data);

signals:
	void packetReady(const QByteArray &data);

private slots:
	void sd_activated();

private:
	class Private;
	Private *d;

	friend class SocksClient;
	SocksUDP(SocksClient *sc, const QString &host, int port, const QHostAddress &routeAddr, int routePort);
};

class SocksClient : public ByteStream
{
	Q_OBJECT
public:
	enum Error { ErrConnectionRefused = ErrCustom, ErrHostNotFound, ErrProxyConnect, ErrProxyNeg, ErrProxyAuth };
	enum Method { AuthNone=0x0001, AuthUsername=0x0002 };
	enum Request { ReqConnect, ReqUDPAssociate };
	SocksClient(QObject *parent=0);
	SocksClient(int, QObject *parent=0);
	~SocksClient();

	bool isIncoming() const;

	// outgoing
	void setAuth(const QString &user, const QString &pass="");
	void connectToHost(const QString &proxyHost, int proxyPort, const QString &host, int port, bool udpMode=false);

	// incoming
	void chooseMethod(int);
	void authGrant(bool);
	void requestDeny();
	void grantConnect();
	void grantUDPAssociate(const QString &relayHost, int relayPort);

	// from ByteStream
	bool isOpen() const;
	void close();
	void write(const QByteArray &);
	QByteArray read(int bytes=0);
	int bytesAvailable() const;
	int bytesToWrite() const;

	// remote address
	QHostAddress peerAddress() const;
	quint16 peerPort() const;

	// udp
	QString udpAddress() const;
	quint16 udpPort() const;
	SocksUDP *createUDP(const QString &host, int port, const QHostAddress &routeAddr, int routePort);

signals:
	// outgoing
	void connected();

	// incoming
	void incomingMethods(int);
	void incomingAuth(const QString &user, const QString &pass);
	void incomingConnectRequest(const QString &host, int port);
	void incomingUDPAssociateRequest();

private slots:
	void sock_connected();
	void sock_connectionClosed();
	void sock_delayedCloseFinished();
	void sock_readyRead();
	void sock_bytesWritten(int);
	void sock_error(int);
	void serve();

private:
	class Private;
	Private *d;

	void init();
	void reset(bool clear=false);
	void do_request();
	void processOutgoing(const QByteArray &);
	void processIncoming(const QByteArray &);
	void continueIncoming();
	void writeData(const QByteArray &a);
};

class SocksServer : public QObject
{
	Q_OBJECT
public:
	SocksServer(QObject *parent=0);
	~SocksServer();

	bool isActive() const;
	bool listen(quint16 port, bool udp=false);
	void stop();
	int port() const;
	QHostAddress address() const;
	SocksClient *takeIncoming();

	void writeUDP(const QHostAddress &addr, int port, const QByteArray &data);

signals:
	void incomingReady();
	void incomingUDP(const QString &host, int port, const QHostAddress &addr, int sourcePort, const QByteArray &data);

private slots:
	void connectionReady(int);
	void connectionError();
	void sd_activated();

private:
	class Private;
	Private *d;
};

// CS_NAMESPACE_END

#endif
