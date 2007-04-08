/*
 * servsock.cpp - simple wrapper to QServerSocket
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

#include "servsock.h"

// CS_NAMESPACE_BEGIN

//----------------------------------------------------------------------------
// ServSock
//----------------------------------------------------------------------------
class ServSock::Private
{
public:
	Private() {}

	ServSockSignal *serv;
};

ServSock::ServSock(QObject *parent)
:QObject(parent)
{
	d = new Private;
	d->serv = 0;
}

ServSock::~ServSock()
{
	stop();
	delete d;
}

bool ServSock::isActive() const
{
	return (d->serv ? true: false);
}

bool ServSock::listen(Q_UINT16 port)
{
	stop();

	d->serv = new ServSockSignal(port);
	if(!d->serv->ok()) {
		delete d->serv;
		d->serv = 0;
		return false;
	}
	connect(d->serv, SIGNAL(connectionReady(int)), SLOT(sss_connectionReady(int)));

	return true;
}

void ServSock::stop()
{
	delete d->serv;
	d->serv = 0;
}

int ServSock::port() const
{
	if(d->serv)
		return d->serv->port();
	else
		return -1;
}

QHostAddress ServSock::address() const
{
	if(d->serv)
		return d->serv->address();
	else
		return QHostAddress();
}

void ServSock::sss_connectionReady(int s)
{
	connectionReady(s);
}


//----------------------------------------------------------------------------
// ServSockSignal
//----------------------------------------------------------------------------
ServSockSignal::ServSockSignal(int port)
:Q3ServerSocket(port, 16)
{
}

void ServSockSignal::newConnection(int x)
{
	connectionReady(x);
}

// CS_NAMESPACE_END
