/*
 * securestream.h - combines a ByteStream with TLS and SASL
 * Copyright (C) 2004  Justin Karneges
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

#ifndef SECURESTREAM_H
#define SECURESTREAM_H

#include <qca.h>
#include "bytestream.h"

#define USE_TLSHANDLER

#ifdef USE_TLSHANDLER
namespace XMPP
{
	class TLSHandler;
}
#endif

class CompressionHandler;

class SecureStream : public ByteStream
{
	Q_OBJECT
public:
	enum Error { ErrTLS = ErrCustom, ErrSASL };
	SecureStream(ByteStream *s);
	~SecureStream();

	void startTLSClient(QCA::TLS *t, const QByteArray &spare=QByteArray());
	void startTLSServer(QCA::TLS *t, const QByteArray &spare=QByteArray());
	void setLayerCompress(const QByteArray &spare=QByteArray());
	void setLayerSASL(QCA::SASL *s, const QByteArray &spare=QByteArray());
#ifdef USE_TLSHANDLER
	void startTLSClient(XMPP::TLSHandler *t, const QString &server, const QByteArray &spare=QByteArray());
#endif

	void closeTLS();
	int errorCode() const;

	// reimplemented
	bool isOpen() const;
	void write(const QByteArray &);
	int bytesToWrite() const;

signals:
	void tlsHandshaken();
	void tlsClosed();

private slots:
	void bs_readyRead();
	void bs_bytesWritten(int);

	void layer_tlsHandshaken();
	void layer_tlsClosed(const QByteArray &);
	void layer_readyRead(const QByteArray &);
	void layer_needWrite(const QByteArray &);
	void layer_error(int);

private:
	void linkLayer(QObject *);
	int calcPrebytes() const;
	void insertData(const QByteArray &a);
	void writeRawData(const QByteArray &a);
	void incomingData(const QByteArray &a);

	class Private;
	Private *d;
};

#endif
