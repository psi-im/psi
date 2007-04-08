/*
 * httpconnect.h - HTTP "CONNECT" proxy
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

#ifndef CS_HTTPCONNECT_H
#define CS_HTTPCONNECT_H

#include "bytestream.h"

// CS_NAMESPACE_BEGIN

class HttpConnect : public ByteStream
{
	Q_OBJECT
public:
	enum Error { ErrConnectionRefused = ErrCustom, ErrHostNotFound, ErrProxyConnect, ErrProxyNeg, ErrProxyAuth };
	HttpConnect(QObject *parent=0);
	~HttpConnect();

	void setAuth(const QString &user, const QString &pass="");
	void connectToHost(const QString &proxyHost, int proxyPort, const QString &host, int port);

	// from ByteStream
	bool isOpen() const;
	void close();
	void write(const QByteArray &);
	QByteArray read(int bytes=0);
	int bytesAvailable() const;
	int bytesToWrite() const;

signals:
	void connected();

private slots:
	void sock_connected();
	void sock_connectionClosed();
	void sock_delayedCloseFinished();
	void sock_readyRead();
	void sock_bytesWritten(int);
	void sock_error(int);

private:
	class Private;
	Private *d;

	void reset(bool clear=false);
};

// CS_NAMESPACE_END

#endif
