/*
 * ndns.cpp - native DNS resolution
 * Copyright (C) 2001, 2002  Justin Karneges
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

//! \class NDns ndns.h
//! \brief Simple DNS resolution using native system calls
//!
//! This class is to be used when Qt's QDns is not good enough.  Because QDns
//! does not use threads, it cannot make a system call asyncronously.  Thus,
//! QDns tries to imitate the behavior of each platform's native behavior, and
//! generally falls short.
//!
//! NDns uses a thread to make the system call happen in the background.  This
//! gives your program native DNS behavior, at the cost of requiring threads
//! to build.
//!
//! \code
//! #include "ndns.h"
//!
//! ...
//!
//! NDns dns;
//! dns.resolve("psi.affinix.com");
//!
//! // The class will emit the resultsReady() signal when the resolution
//! // is finished. You may then retrieve the results:
//!
//! uint ip_address = dns.result();
//!
//! // or if you want to get the IP address as a string:
//!
//! QString ip_address = dns.resultString();
//! \endcode

#include "ndns.h"

#include "netnames.h"

// CS_NAMESPACE_BEGIN

//----------------------------------------------------------------------------
// NDns
//----------------------------------------------------------------------------
//! \fn void NDns::resultsReady()
//! This signal is emitted when the DNS resolution succeeds or fails.

//!
//! Constructs an NDns object with parent \a parent.
NDns::NDns(QObject *parent)
:QObject(parent)
{
	busy = false;

	connect(&dns, SIGNAL(resultsReady(const QList<XMPP::NameRecord> &)), SLOT(dns_resultsReady(const QList<XMPP::NameRecord> &)));
	connect(&dns, SIGNAL(error(XMPP::NameResolver::Error)), SLOT(dns_error(XMPP::NameResolver::Error)));
}

//!
//! Destroys the object and frees allocated resources.
NDns::~NDns()
{
	stop();
}

//!
//! Resolves hostname \a host (eg. psi.affinix.com)
void NDns::resolve(const QString &host)
{
	stop();
	busy = true;
	dns.start(host.toLatin1());
}

//!
//! Cancels the lookup action.
//! \note This will not stop the underlying system call, which must finish before the next lookup will proceed.
void NDns::stop()
{
	dns.stop();
	busy = false;
}

//!
//! Returns the IP address as a 32-bit integer in host-byte-order.  This will be 0 if the lookup failed.
//! \sa resultsReady()
QHostAddress NDns::result() const
{
	return addr;
}

//!
//! Returns the IP address as a string.  This will be an empty string if the lookup failed.
//! \sa resultsReady()
QString NDns::resultString() const
{
	return addr.toString();
}

//!
//! Returns TRUE if busy resolving a hostname.
bool NDns::isBusy() const
{
	return busy;
}

void NDns::dns_resultsReady(const QList<XMPP::NameRecord> &results)
{
	addr = results[0].address();
	busy = false;
	emit resultsReady();
}

void NDns::dns_error(XMPP::NameResolver::Error)
{
	addr = QHostAddress();
	busy = false;
	emit resultsReady();
}

// CS_NAMESPACE_END

