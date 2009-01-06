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

// FIXME: remove this code once qca cert host checking works ...
using namespace QCA;
#include <qurl.h>

// ip address string to binary (msb), adapted from jdns (adapted from qt)
// return: size 4 = ipv4, size 16 = ipv6, size 0 = error
static QByteArray ipaddr_str2bin(const QString &str)
{
	// ipv6
	if(str.contains(':'))
	{
		QStringList parts = str.split(':', QString::KeepEmptyParts);
		if(parts.count() < 3 || parts.count() > 8)
			return QByteArray();

		QByteArray ipv6(16, 0);
		int at = 16;
		int fill = 9 - parts.count();
		for(int n = parts.count() - 1; n >= 0; --n)
		{
			if(at <= 0)
				return QByteArray();

			if(parts[n].isEmpty())
			{
				if(n == parts.count() - 1)
				{
					if(!parts[n - 1].isEmpty())
						return QByteArray();
					ipv6[--at] = 0;
					ipv6[--at] = 0;
				}
				else if(n == 0)
				{
					if(!parts[n + 1].isEmpty())
						return QByteArray();
					ipv6[--at] = 0;
					ipv6[--at] = 0;
				}
				else
				{
					for(int i = 0; i < fill; ++i)
					{
						if(at <= 0)
							return QByteArray();
						ipv6[--at] = 0;
						ipv6[--at] = 0;
					}
				}
			}
			else
			{
				if(parts[n].indexOf('.') == -1)
				{
					bool ok;
					int x = parts[n].toInt(&ok, 16);
					if(!ok || x < 0 || x > 0xffff)
						return QByteArray();
					ipv6[--at] = x & 0xff;
					ipv6[--at] = (x >> 8) & 0xff;
				}
				else
				{
					if(n != parts.count() - 1)
						return QByteArray();

					QByteArray buf = ipaddr_str2bin(parts[n]);
					if(buf.isEmpty())
						return QByteArray();

					ipv6[--at] = buf[3];
					ipv6[--at] = buf[2];
					ipv6[--at] = buf[1];
					ipv6[--at] = buf[0];
					--fill;
				}
			}
		}

		return ipv6;
	}
	else if(str.contains('.'))
	{
		QStringList parts = str.split('.', QString::KeepEmptyParts);
		if(parts.count() != 4)
			return QByteArray();

		QByteArray out(4, 0);
		for(int n = 0; n < 4; ++n)
		{
			bool ok;
			int x = parts[n].toInt(&ok);
			if(!ok || x < 0 || x > 0xff)
				return QByteArray();
			out[n] = (unsigned char)x;
		}
		return out;
	}
	else
		return QByteArray();
}

// acedomain must be all lowercase, with no trailing dot or wildcards
static bool cert_match_domain(const QString &certname, const QString &acedomain)
{
	// KSSL strips start/end whitespace, even though such whitespace is
	//   probably not legal anyway. (compat)
	QString name = certname.trimmed();

	// KSSL strips trailing dot, even though the dot is probably not
	//   legal anyway. (compat)
	if(name.length() > 0 && name[name.length()-1] == '.')
		name.truncate(name.length()-1);

	// after our compatibility modifications, make sure the name isn't
	//   empty.
	if(name.isEmpty())
		return false;

	// lowercase, for later performing case insensitive matching
	name = name.toLower();

	// ensure the cert field contains valid characters only
	if(QRegExp("[^a-z0-9\\.\\*\\-]").indexIn(name) >= 0)
		return false;

	// hack into parts, and require at least 1 part
	QStringList parts_name = name.split('.', QString::KeepEmptyParts);
	if(parts_name.isEmpty())
		return false;

	// KSSL checks to make sure the last two parts don't contain
	//   wildcards.  I don't know where it is written that this
	//   should be done, but for compat sake we'll do it.
	if(parts_name[parts_name.count()-1].contains('*'))
		return false;
	if(parts_name.count() >= 2 && parts_name[parts_name.count()-2].contains('*'))
		return false;

	QStringList parts_compare = acedomain.split('.', QString::KeepEmptyParts);
	if(parts_compare.isEmpty())
		return false;

	// don't allow empty parts
	foreach(const QString &s, parts_name)
	{
		if(s.isEmpty())
			return false;
	}
	foreach(const QString &s, parts_compare)
	{
		if(s.isEmpty())
			return false;
	}

	// RFC2818: "Names may contain the wildcard character * which is
	//   considered to match any single domain name component or
	//   component fragment. E.g., *.a.com matches foo.a.com but not
	//   bar.foo.a.com. f*.com matches foo.com but not bar.com."
	//
	// This means that for the domain to match it must have the
	//   same number of components, wildcards or not.  If there are
	//   wildcards, their scope must only be within the component
	//   they reside in.
	//
	// First, make sure the number of parts is equal.
	if(parts_name.count() != parts_compare.count())
		return false;

	// Now compare each part
	for(int n = 0; n < parts_name.count(); ++n)
	{
		const QString &p1 = parts_name[n];
		const QString &p2 = parts_compare[n];

		if(!QRegExp(p1, Qt::CaseSensitive, QRegExp::Wildcard).exactMatch(p2))
			return false;
	}

	return true;
}

// ipaddress must be an ipv4 or ipv6 address in binary format
static bool cert_match_ipaddress(const QString &certname, const QByteArray &ipaddress)
{
	// KSSL strips start/end whitespace, even though such whitespace is
	//   probably not legal anyway. (compat)
	QString name = certname.trimmed();

	// KSSL accepts IPv6 in brackets, which is usually done for URIs, but
	//   IMO sounds very strange for a certificate.  We'll follow this
	//   behavior anyway. (compat)
	if(name.length() >= 2 && name[0] == '[' && name[name.length()-1] == ']')
		name = name.mid(1, name.length() - 2); // chop off brackets

	// after our compatibility modifications, make sure the name isn't
	//   empty.
	if(name.isEmpty())
		return false;

	// convert to binary form
	QByteArray addr = ipaddr_str2bin(name);
	if(addr.isEmpty())
		return false;

	// not the same?
	if(addr != ipaddress)
		return false;

	return true;
}

static bool matchesHostName(const QCA::Certificate &cert, const QString &host)
{
	QByteArray ipaddr = ipaddr_str2bin(host);
	if(!ipaddr.isEmpty()) // ip address
	{
		// check iPAddress
		foreach(const QString &s, cert.subjectInfo().values(IPAddress))
		{
			if(cert_match_ipaddress(s, ipaddr))
				return true;
		}

		// check dNSName
		foreach(const QString &s, cert.subjectInfo().values(DNS))
		{
			if(cert_match_ipaddress(s, ipaddr))
				return true;
		}

		// check commonName
		foreach(const QString &s, cert.subjectInfo().values(CommonName))
		{
			if(cert_match_ipaddress(s, ipaddr))
				return true;
		}
	}
	else // domain
	{
		// lowercase
		QString name = host.toLower();

		// ACE
		name = QString::fromLatin1(QUrl::toAce(name));

		// don't allow wildcards in the comparison host
		if(name.contains('*'))
			return false;

		// strip out trailing dot
		if(name.length() > 0 && name[name.length()-1] == '.')
			name.truncate(name.length()-1);

		// make sure the name is not empty after our modifications
		if(name.isEmpty())
			return false;

		// check dNSName
		foreach(const QString &s, cert.subjectInfo().values(DNS))
		{
			if(cert_match_domain(s, name))
				return true;
		}

		// check commonName
		foreach(const QString &s, cert.subjectInfo().values(CommonName))
		{
			if(cert_match_domain(s, name))
				return true;
		}
	}

	return false;
}

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
	QString host;
	bool internalHostMatch;
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
	d->internalHostMatch = false;
}

QCATLSHandler::~QCATLSHandler()
{
	delete d;
}

void QCATLSHandler::setXMPPCertCheck(bool enable)
{
	d->internalHostMatch = enable;
}
bool QCATLSHandler::XMPPCertCheck()
{
	return d->internalHostMatch;
}
bool QCATLSHandler::certMatchesHostname()
{
	if (!d->internalHostMatch) return false;
	QCA::CertificateChain peerCert = d->tls->peerCertificateChain();

	if (matchesHostName(peerCert.primary(), d->host))
		return true;
	
	Jid host(d->host);

	foreach( const QString &idOnXmppAddr, peerCert.primary().subjectInfo().values(QCA::XMPP) ) {
		if (host.compare(Jid(idOnXmppAddr)))
			return true;
	}

	return false;
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
	if (d->internalHostMatch) d->host = host;
	d->tls->startClient(d->internalHostMatch ? QString() : host);
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
		d->tls->continueAfterStep();
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
