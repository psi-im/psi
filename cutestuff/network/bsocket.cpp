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

#include "bsocket.h"

#include <q3cstring.h>
#include <q3socket.h>
#include <q3dns.h>
#include <qpointer.h>
#include "safedelete.h"
#ifndef NO_NDNS
#include "ndns.h"
#endif
#include "srvresolver.h"

#ifdef BS_DEBUG
#include <stdio.h>
#endif

#define READBUFSIZE 65536

// CS_NAMESPACE_BEGIN

class BSocket::Private
{
public:
	Private()
	{
		qsock = 0;
	}

	Q3Socket *qsock;
	int state;

#ifndef NO_NDNS
	NDns ndns;
#endif
	SrvResolver srv;
	QString host;
	int port;
	SafeDelete sd;
};

BSocket::BSocket(QObject *parent)
:ByteStream(parent)
{
	d = new Private;
#ifndef NO_NDNS
	connect(&d->ndns, SIGNAL(resultsReady()), SLOT(ndns_done()));
#endif
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
		d->qsock->disconnect(this);

		if(!clear && d->qsock->isOpen()) {
			// move remaining into the local queue
			QByteArray block(d->qsock->bytesAvailable());
			d->qsock->readBlock(block.data(), block.size());
			appendRead(block);
		}

		d->sd.deleteLater(d->qsock);
		d->qsock = 0;
	}
	else {
		if(clear)
			clearReadBuffer();
	}

	if(d->srv.isBusy())
		d->srv.stop();
#ifndef NO_NDNS
	if(d->ndns.isBusy())
		d->ndns.stop();
#endif
	d->state = Idle;
}

void BSocket::ensureSocket()
{
	if(!d->qsock) {
		d->qsock = new Q3Socket;
		d->qsock->setReadBufferSize(READBUFSIZE);
		connect(d->qsock, SIGNAL(hostFound()), SLOT(qs_hostFound()));
		connect(d->qsock, SIGNAL(connected()), SLOT(qs_connected()));
		connect(d->qsock, SIGNAL(connectionClosed()), SLOT(qs_connectionClosed()));
		connect(d->qsock, SIGNAL(delayedCloseFinished()), SLOT(qs_delayedCloseFinished()));
		connect(d->qsock, SIGNAL(readyRead()), SLOT(qs_readyRead()));
		connect(d->qsock, SIGNAL(bytesWritten(int)), SLOT(qs_bytesWritten(int)));
		connect(d->qsock, SIGNAL(error(int)), SLOT(qs_error(int)));
	}
}

void BSocket::connectToHost(const QString &host, Q_UINT16 port)
{
	reset(true);
	d->host = host;
	d->port = port;
#ifdef NO_NDNS
	d->state = Connecting;
	do_connect();
#else
	d->state = HostLookup;
	d->ndns.resolve(d->host);
#endif
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
		return d->qsock->socket();
	else
		return -1;
}

void BSocket::setSocket(int s)
{
	reset(true);
	ensureSocket();
	d->state = Connected;
	d->qsock->setSocket(s);
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
	Q3CString cs;
	cs.resize(a.size()+1);
	memcpy(cs.data(), a.data(), a.size());
	QString s = QString::fromUtf8(cs);
	fprintf(stderr, "BSocket: writing [%d]: {%s}\n", a.size(), cs.data());
#endif
	d->qsock->writeBlock(a.data(), a.size());
}

QByteArray BSocket::read(int bytes)
{
	QByteArray block;
	if(d->qsock) {
		int max = bytesAvailable();
		if(bytes <= 0 || bytes > max)
			bytes = max;
		block.resize(bytes);
		d->qsock->readBlock(block.data(), block.size());
	}
	else
		block = ByteStream::read(bytes);

#ifdef BS_DEBUG
	Q3CString cs;
	cs.resize(block.size()+1);
	memcpy(cs.data(), block.data(), block.size());
	QString s = QString::fromUtf8(cs);
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
		return d->qsock->address();
	else
		return QHostAddress();
}

Q_UINT16 BSocket::port() const
{
	if(d->qsock)
		return d->qsock->port();
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

Q_UINT16 BSocket::peerPort() const
{
	if(d->qsock)
		return d->qsock->port();
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
#ifndef NO_NDNS
	if(d->ndns.result()) {
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
#endif
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
	SafeDeleteLock s(&d->sd);
	connected();
}

void BSocket::qs_connectionClosed()
{
#ifdef BS_DEBUG
	fprintf(stderr, "BSocket: Connection Closed.\n");
#endif
	SafeDeleteLock s(&d->sd);
	reset();
	connectionClosed();
}

void BSocket::qs_delayedCloseFinished()
{
#ifdef BS_DEBUG
	fprintf(stderr, "BSocket: Delayed Close Finished.\n");
#endif
	SafeDeleteLock s(&d->sd);
	reset();
	delayedCloseFinished();
}

void BSocket::qs_readyRead()
{
	SafeDeleteLock s(&d->sd);
	readyRead();
}

void BSocket::qs_bytesWritten(int x)
{
#ifdef BS_DEBUG
	fprintf(stderr, "BSocket: BytesWritten [%d].\n", x);
#endif
	SafeDeleteLock s(&d->sd);
	bytesWritten(x);
}

void BSocket::qs_error(int x)
{
#ifdef BS_DEBUG
	fprintf(stderr, "BSocket: Error.\n");
#endif
	SafeDeleteLock s(&d->sd);

	// connection error during SRV host connect?  try next
	if(d->state == HostLookup && (x == Q3Socket::ErrConnectionRefused || x == Q3Socket::ErrHostNotFound)) {
		d->srv.next();
		return;
	}

	reset();
	if(x == Q3Socket::ErrConnectionRefused)
		error(ErrConnectionRefused);
	else if(x == Q3Socket::ErrHostNotFound)
		error(ErrHostNotFound);
	else if(x == Q3Socket::ErrSocketRead)
		error(ErrRead);
}

// CS_NAMESPACE_END
