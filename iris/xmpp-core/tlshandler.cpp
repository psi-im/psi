/*
 * tlshandler.cpp - abstract wrapper for TLS
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

#include "xmpp.h"

#include <qtimer.h>
#include "qca.h"

using namespace XMPP;

//----------------------------------------------------------------------------
// TLSHandler
//----------------------------------------------------------------------------
TLSHandler::TLSHandler(QObject *parent)
:QObject(parent)
{
}

TLSHandler::~TLSHandler()
{
}


//----------------------------------------------------------------------------
// QCATLSHandler
//----------------------------------------------------------------------------
class QCATLSHandler::Private
{
public:
	QCA::TLS *tls;
	int state, err;
};

QCATLSHandler::QCATLSHandler(QCA::TLS *parent)
:TLSHandler(parent)
{
	d = new Private;
	d->tls = parent;
	connect(d->tls, SIGNAL(handshaken()), SLOT(tls_handshaken()));
	connect(d->tls, SIGNAL(readyRead()), SLOT(tls_readyRead()));
	connect(d->tls, SIGNAL(readyReadOutgoing()), SLOT(tls_readyReadOutgoing()));
	connect(d->tls, SIGNAL(closed()), SLOT(tls_closed()));
	connect(d->tls, SIGNAL(error()), SLOT(tls_error()));
	d->state = 0;
	d->err = -1;
}

QCATLSHandler::~QCATLSHandler()
{
	delete d;
}

QCA::TLS *QCATLSHandler::tls() const
{
	return d->tls;
}

int QCATLSHandler::tlsError() const
{
	return d->err;
}

void QCATLSHandler::reset()
{
	d->tls->reset();
	d->state = 0;
}

void QCATLSHandler::startClient(const QString &host)
{
	d->state = 0;
	d->err = -1;
	d->tls->startClient(host);
}

void QCATLSHandler::write(const QByteArray &a)
{
	d->tls->write(a);
}

void QCATLSHandler::writeIncoming(const QByteArray &a)
{
	d->tls->writeIncoming(a);
}

void QCATLSHandler::continueAfterHandshake()
{
	if(d->state == 2) {
		success();
		d->state = 3;
	}
}

void QCATLSHandler::tls_handshaken()
{
	d->state = 2;
	tlsHandshaken();
}

void QCATLSHandler::tls_readyRead()
{
	readyRead(d->tls->read());
}

void QCATLSHandler::tls_readyReadOutgoing()
{
	int plainBytes;
	QByteArray buf = d->tls->readOutgoing(&plainBytes);
	readyReadOutgoing(buf, plainBytes);
}

void QCATLSHandler::tls_closed()
{
	closed();
}

void QCATLSHandler::tls_error()
{
	d->err = d->tls->errorCode();
	d->state = 0;
	fail();
}
