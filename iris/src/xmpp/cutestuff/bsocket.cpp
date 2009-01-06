/*
 * bsocket.cpp - QSocket wrapper based on Bytestream with SRV DNS support
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

#include <QTcpSocket>
#include <QHostAddress>
#include <QMetaType>

#include "bsocket.h"

//#include "safedelete.h"
#include "ndns.h"
#include "srvresolver.h"

//#define BS_DEBUG

#ifdef BS_DEBUG
#include <stdio.h>
#endif

#define READBUFSIZE 65536

// CS_NAMESPACE_BEGIN

class QTcpSocketSignalRelay : public QObject
{
	Q_OBJECT
public:
	QTcpSocketSignalRelay(QTcpSocket *sock, QObject *parent = 0)
	:QObject(parent)
	{
		qRegisterMetaType<QAbstractSocket::SocketError>("QAbstractSocket::SocketError");
		connect(sock, SIGNAL(hostFound()), SLOT(sock_hostFound()), Qt::QueuedConnection);
		connect(sock, SIGNAL(connected()), SLOT(sock_connected()), Qt::QueuedConnection);
		connect(sock, SIGNAL(disconnected()), SLOT(sock_disconnected()), Qt::QueuedConnection);
		connect(sock, SIGNAL(readyRead()), SLOT(sock_readyRead()), Qt::QueuedConnection);
		connect(sock, SIGNAL(bytesWritten(qint64)), SLOT(sock_bytesWritten(qint64)), Qt::QueuedConnection);
		connect(sock, SIGNAL(error(QAbstractSocket::SocketError)), SLOT(sock_error(QAbstractSocket::SocketError)), Qt::QueuedConnection);
	}

signals:
	void hostFound();
	void connected();
	void disconnected();
	void readyRead();
	void bytesWritten(qint64);
	void error(QAbstractSocket::SocketError);

public slots:
	void sock_hostFound()
	{
		emit hostFound();
	}

	void sock_connected()
	{
		emit connected();
	}

	void sock_disconnected()
	{
		emit disconnected();
	}

	void sock_readyRead()
	{
		emit readyRead();
	}

	void sock_bytesWritten(qint64 x)
	{
		emit bytesWritten(x);
	}

	void sock_error(QAbstractSocket::SocketError x)
	{
		emit error(x);
	}
};

class BSocket::Private
{
public:
	Private()
	{
		qsock = 0;
		qsock_relay = 0;
	}

	QTcpSocket *qsock;
	QTcpSocketSignalRelay *qsock_relay;
	int state;

	NDns ndns;
	SrvResolver srv;
	QString host;
	int port;
	//SafeDelete sd;
};

BSocket::BSocket(QObject *parent)
:ByteStream(parent)
{
	d = new Private;
	connect(&d->ndns, SIGNAL(resultsReady()), SLOT(ndns_done()));
	connect(&d->srv, SIGNAL(resultsReady()), SLOT(srv_done()));

	reset();
}

BSocket::~BSocket()
{
	reset(true);
	delete d;
}

void BSocket::reset(bool clear)
{
	if(d->qsock) {
		delete d->qsock_relay;
		d->qsock_relay = 0;

		/*d->qsock->disconnect(this);

		if(!clear && d->qsock->isOpen() && d->qsock->isValid()) {*/
			// move remaining into the local queue
			QByteArray block(d->qsock->bytesAvailable(), 0);
			d->qsock->read(block.data(), block.size());
			appendRead(block);
		//}

		//d->sd.deleteLater(d->qsock);
    d->qsock->deleteLater();
		d->qsock = 0;
	}
	else {
		if(clear)
			clearReadBuffer();
	}

	if(d->srv.isBusy())
		d->srv.stop();
	if(d->ndns.isBusy())
		d->ndns.stop();
	d->state = Idle;
}

void BSocket::ensureSocket()
{
	if(!d->qsock) {
		d->qsock = new QTcpSocket;
#if QT_VERSION >= 0x030200
		d->qsock->setReadBufferSize(READBUFSIZE);
#endif
		d->qsock_relay = new QTcpSocketSignalRelay(d->qsock);
		connect(d->qsock_relay, SIGNAL(hostFound()), SLOT(qs_hostFound()));
		connect(d->qsock_relay, SIGNAL(connected()), SLOT(qs_connected()));
		connect(d->qsock_relay, SIGNAL(disconnected()), SLOT(qs_closed()));
		connect(d->qsock_relay, SIGNAL(readyRead()), SLOT(qs_readyRead()));
		connect(d->qsock_relay, SIGNAL(bytesWritten(qint64)), SLOT(qs_bytesWritten(qint64)));
		connect(d->qsock_relay, SIGNAL(error(QAbstractSocket::SocketError)), SLOT(qs_error(QAbstractSocket::SocketError)));
	}
}

void BSocket::connectToHost(const QString &host, quint16 port)
{
	reset(true);
	d->host = host;
	d->port = port;
	d->state = HostLookup;
	d->ndns.resolve(d->host);
}

void BSocket::connectToServer(const QString &srv, const QString &type)
{
	reset(true);
	d->state = HostLookup;
	d->srv.resolve(srv, type, "tcp");
}

int BSocket::socket() const
{
	if(d->qsock)
		return d->qsock->socketDescriptor();
	else
		return -1;
}

void BSocket::setSocket(int s)
{
	reset(true);
	ensureSocket();
	d->state = Connected;
	d->qsock->setSocketDescriptor(s);
}

int BSocket::state() const
{
	return d->state;
}

bool BSocket::isOpen() const
{
	if(d->state == Connected)
		return true;
	else
		return false;
}

void BSocket::close()
{
	if(d->state == Idle)
		return;

	if(d->qsock) {
		d->qsock->close();
		d->state = Closing;
		if(d->qsock->bytesToWrite() == 0)
			reset();
	}
	else {
		reset();
	}
}

void BSocket::write(const QByteArray &a)
{
	if(d->state != Connected)
		return;
#ifdef BS_DEBUG
	QString s = QString::fromUtf8(a);
	fprintf(stderr, "BSocket: writing [%d]: {%s}\n", a.size(), s.latin1());
#endif
	d->qsock->write(a.data(), a.size());
}

QByteArray BSocket::read(int bytes)
{
	QByteArray block;
	if(d->qsock) {
		int max = bytesAvailable();
		if(bytes <= 0 || bytes > max)
			bytes = max;
		block.resize(bytes);
		d->qsock->read(block.data(), block.size());
	}
	else
		block = ByteStream::read(bytes);

#ifdef BS_DEBUG
	QString s = QString::fromUtf8(block);
	fprintf(stderr, "BSocket: read [%d]: {%s}\n", block.size(), s.latin1());
#endif
	return block;
}

int BSocket::bytesAvailable() const
{
	if(d->qsock)
		return d->qsock->bytesAvailable();
	else
		return ByteStream::bytesAvailable();
}

int BSocket::bytesToWrite() const
{
	if(!d->qsock)
		return 0;
	return d->qsock->bytesToWrite();
}

QHostAddress BSocket::address() const
{
	if(d->qsock)
		return d->qsock->localAddress();
	else
		return QHostAddress();
}

quint16 BSocket::port() const
{
	if(d->qsock)
		return d->qsock->localPort();
	else
		return 0;
}

QHostAddress BSocket::peerAddress() const
{
	if(d->qsock)
		return d->qsock->peerAddress();
	else
		return QHostAddress();
}

quint16 BSocket::peerPort() const
{
	if(d->qsock)
		return d->qsock->peerPort();
	else
		return 0;
}

void BSocket::srv_done()
{
	if(d->srv.failed()) {
#ifdef BS_DEBUG
		fprintf(stderr, "BSocket: Error resolving hostname.\n");
#endif
		error(ErrHostNotFound);
		return;
	}

	d->host = d->srv.resultAddress().toString();
	d->port = d->srv.resultPort();
	do_connect();
	//QTimer::singleShot(0, this, SLOT(do_connect()));
	//hostFound();
}

void BSocket::ndns_done()
{
	if(!d->ndns.result().isNull()) {
		d->host = d->ndns.resultString();
		d->state = Connecting;
		do_connect();
		//QTimer::singleShot(0, this, SLOT(do_connect()));
		//hostFound();
	}
	else {
#ifdef BS_DEBUG
		fprintf(stderr, "BSocket: Error resolving hostname.\n");
#endif
		error(ErrHostNotFound);
	}
}

void BSocket::do_connect()
{
#ifdef BS_DEBUG
	fprintf(stderr, "BSocket: Connecting to %s:%d\n", d->host.latin1(), d->port);
#endif
	ensureSocket();
	d->qsock->connectToHost(d->host, d->port);
}

void BSocket::qs_hostFound()
{
	//SafeDeleteLock s(&d->sd);
}

void BSocket::qs_connected()
{
	d->state = Connected;
#ifdef BS_DEBUG
	fprintf(stderr, "BSocket: Connected.\n");
#endif
	//SafeDeleteLock s(&d->sd);
	connected();
}

void BSocket::qs_closed()
{
	if(d->state == Closing)
	{
#ifdef BS_DEBUG
		fprintf(stderr, "BSocket: Delayed Close Finished.\n");
#endif
		//SafeDeleteLock s(&d->sd);
		reset();
		delayedCloseFinished();
	}
}

void BSocket::qs_readyRead()
{
	//SafeDeleteLock s(&d->sd);
	readyRead();
}

void BSocket::qs_bytesWritten(qint64 x64)
{
	int x = x64;
#ifdef BS_DEBUG
	fprintf(stderr, "BSocket: BytesWritten [%d].\n", x);
#endif
	//SafeDeleteLock s(&d->sd);
	bytesWritten(x);
}

void BSocket::qs_error(QAbstractSocket::SocketError x)
{
	if(x == QTcpSocket::RemoteHostClosedError) {
#ifdef BS_DEBUG
		fprintf(stderr, "BSocket: Connection Closed.\n");
#endif
		//SafeDeleteLock s(&d->sd);
		reset();
		connectionClosed();
		return;
	}

#ifdef BS_DEBUG
	fprintf(stderr, "BSocket: Error.\n");
#endif
	//SafeDeleteLock s(&d->sd);

	// connection error during SRV host connect?  try next
	if(d->state == HostLookup && (x == QTcpSocket::ConnectionRefusedError || x == QTcpSocket::HostNotFoundError)) {
		d->srv.next();
		return;
	}

	reset();
	if(x == QTcpSocket::ConnectionRefusedError)
		error(ErrConnectionRefused);
	else if(x == QTcpSocket::HostNotFoundError)
		error(ErrHostNotFound);
	else
		error(ErrRead);
}

#include "bsocket.moc"

// CS_NAMESPACE_END
