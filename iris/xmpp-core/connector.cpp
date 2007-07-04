/*
 * connector.cpp - establish a connection to an XMPP server
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

/*
  TODO:

  - Test and analyze all possible branches

  XMPP::AdvancedConnector is "good for now."  The only real issue is that
  most of what it provides is just to work around the old Jabber/XMPP 0.9
  connection behavior.  When XMPP 1.0 has taken over the world, we can
  greatly simplify this class.  - Sep 3rd, 2003.
*/

#include "xmpp.h"

#include <qpointer.h>
#include <qca.h>
#include <QList>
#include <QUrl>
#include "safedelete.h"
#include <libidn/idna.h>

#ifdef NO_NDNS
#include <q3dns.h>
#else
#include "ndns.h"
#endif

#include "bsocket.h"
#include "httpconnect.h"
#include "httppoll.h"
#include "socks.h"
#include "srvresolver.h"

//#define XMPP_DEBUG

using namespace XMPP;

//----------------------------------------------------------------------------
// Connector
//----------------------------------------------------------------------------
Connector::Connector(QObject *parent)
:QObject(parent)
{
	setUseSSL(false);
	setPeerAddressNone();
}

Connector::~Connector()
{
}

bool Connector::useSSL() const
{
	return ssl;
}

bool Connector::havePeerAddress() const
{
	return haveaddr;
}

QHostAddress Connector::peerAddress() const
{
	return addr;
}

Q_UINT16 Connector::peerPort() const
{
	return port;
}

void Connector::setUseSSL(bool b)
{
	ssl = b;
}

void Connector::setPeerAddressNone()
{
	haveaddr = false;
	addr = QHostAddress();
	port = 0;
}

void Connector::setPeerAddress(const QHostAddress &_addr, Q_UINT16 _port)
{
	haveaddr = true;
	addr = _addr;
	port = _port;
}


//----------------------------------------------------------------------------
// AdvancedConnector::Proxy
//----------------------------------------------------------------------------
AdvancedConnector::Proxy::Proxy()
{
	t = None;
	v_poll = 30;
}

AdvancedConnector::Proxy::~Proxy()
{
}

int AdvancedConnector::Proxy::type() const
{
	return t;
}

QString AdvancedConnector::Proxy::host() const
{
	return v_host;
}

Q_UINT16 AdvancedConnector::Proxy::port() const
{
	return v_port;
}

QString AdvancedConnector::Proxy::url() const
{
	return v_url;
}

QString AdvancedConnector::Proxy::user() const
{
	return v_user;
}

QString AdvancedConnector::Proxy::pass() const
{
	return v_pass;
}

int AdvancedConnector::Proxy::pollInterval() const
{
	return v_poll;
}

void AdvancedConnector::Proxy::setHttpConnect(const QString &host, Q_UINT16 port)
{
	t = HttpConnect;
	v_host = host;
	v_port = port;
}

void AdvancedConnector::Proxy::setHttpPoll(const QString &host, Q_UINT16 port, const QString &url)
{
	t = HttpPoll;
	v_host = host;
	v_port = port;
	v_url = url;
}

void AdvancedConnector::Proxy::setSocks(const QString &host, Q_UINT16 port)
{
	t = Socks;
	v_host = host;
	v_port = port;
}

void AdvancedConnector::Proxy::setUserPass(const QString &user, const QString &pass)
{
	v_user = user;
	v_pass = pass;
}

void AdvancedConnector::Proxy::setPollInterval(int secs)
{
	v_poll = secs;
}


//----------------------------------------------------------------------------
// AdvancedConnector
//----------------------------------------------------------------------------
enum { Idle, Connecting, Connected };
class AdvancedConnector::Private
{
public:
	int mode;
	ByteStream *bs;
#ifdef NO_NDNS
	Q3Dns *qdns;
#else
	NDns dns;
#endif
	SrvResolver srv;

	QString server;
	QString opt_host;
	int opt_port;
	bool opt_probe, opt_ssl;
	Proxy proxy;

	QString host;
	int port;
	QList<Q3Dns::Server> servers;
	int errorCode;

	bool multi, using_srv;
	bool will_be_ssl;
	int probe_mode;

	bool aaaa;
	SafeDelete sd;
};

AdvancedConnector::AdvancedConnector(QObject *parent)
:Connector(parent)
{
	d = new Private;
	d->bs = 0;
#ifdef NO_NDNS
	d->qdns = 0;
#else
	connect(&d->dns, SIGNAL(resultsReady()), SLOT(dns_done()));
#endif
	connect(&d->srv, SIGNAL(resultsReady()), SLOT(srv_done()));
	d->opt_probe = false;
	d->opt_ssl = false;
	cleanup();
	d->errorCode = 0;
}

AdvancedConnector::~AdvancedConnector()
{
	cleanup();
	delete d;
}

void AdvancedConnector::cleanup()
{
	d->mode = Idle;

	// stop any dns
#ifdef NO_NDNS
	if(d->qdns) {
		d->qdns->disconnect(this);
		d->qdns->deleteLater();
		//d->sd.deleteLater(d->qdns);
		d->qdns = 0;
	}
#else
	if(d->dns.isBusy())
		d->dns.stop();
#endif
	if(d->srv.isBusy())
		d->srv.stop();

	// destroy the bytestream, if there is one
	delete d->bs;
	d->bs = 0;

	d->multi = false;
	d->using_srv = false;
	d->will_be_ssl = false;
	d->probe_mode = -1;

	setUseSSL(false);
	setPeerAddressNone();
}

void AdvancedConnector::setProxy(const Proxy &proxy)
{
	if(d->mode != Idle)
		return;
	d->proxy = proxy;
}

void AdvancedConnector::setOptHostPort(const QString &host, Q_UINT16 _port)
{
	if(d->mode != Idle)
		return;
	d->opt_host = host;
	d->opt_port = _port;
}

void AdvancedConnector::setOptProbe(bool b)
{
	if(d->mode != Idle)
		return;
	d->opt_probe = b;
}

void AdvancedConnector::setOptSSL(bool b)
{
	if(d->mode != Idle)
		return;
	d->opt_ssl = b;
}

void AdvancedConnector::connectToServer(const QString &server)
{
	if(d->mode != Idle)
		return;
	if(server.isEmpty())
		return;

	d->errorCode = 0;
	d->mode = Connecting;
	d->aaaa = true;

	// Encode the servername
	d->server = QUrl::toAce(server);
	//char* server_encoded;
	//if (!idna_to_ascii_8z(server.utf8().data(), &server_encoded, 0)) {
	//	d->server = QString(server_encoded);
	//	free(server_encoded);
	//}
	//else {
	//	d->server = server;
	//}

	if(d->proxy.type() == Proxy::HttpPoll) {
		// need SHA1 here
		//if(!QCA::isSupported(QCA::CAP_SHA1))
		//	QCA::insertProvider(createProviderHash());

		HttpPoll *s = new HttpPoll;
		d->bs = s;
		connect(s, SIGNAL(connected()), SLOT(bs_connected()));
		connect(s, SIGNAL(syncStarted()), SLOT(http_syncStarted()));
		connect(s, SIGNAL(syncFinished()), SLOT(http_syncFinished()));
		connect(s, SIGNAL(error(int)), SLOT(bs_error(int)));
		if(!d->proxy.user().isEmpty())
			s->setAuth(d->proxy.user(), d->proxy.pass());
		s->setPollInterval(d->proxy.pollInterval());

		if(d->proxy.host().isEmpty())
			s->connectToUrl(d->proxy.url());
		else
			s->connectToHost(d->proxy.host(), d->proxy.port(), d->proxy.url());
	}
	else if (d->proxy.type() == Proxy::HttpConnect) {
		if(!d->opt_host.isEmpty()) {
			d->host = d->opt_host;
			d->port = d->opt_port;
		}
		else {
			d->host = server;
			d->port = 5222;
		}
		do_connect();
	}
	else {
		if(!d->opt_host.isEmpty()) {
			d->host = d->opt_host;
			d->port = d->opt_port;
			do_resolve();
		}
		else {
			d->multi = true;

			QPointer<QObject> self = this;
			srvLookup(d->server);
			if(!self)
				return;

			d->srv.resolveSrvOnly(d->server, "xmpp-client", "tcp");
		}
	}
}

void AdvancedConnector::changePollInterval(int secs)
{
	if(d->bs && (d->bs->inherits("XMPP::HttpPoll") || d->bs->inherits("HttpPoll"))) {
		HttpPoll *s = static_cast<HttpPoll*>(d->bs);
		s->setPollInterval(secs);
	}
}

ByteStream *AdvancedConnector::stream() const
{
	if(d->mode == Connected)
		return d->bs;
	else
		return 0;
}

void AdvancedConnector::done()
{
	cleanup();
}

int AdvancedConnector::errorCode() const
{
	return d->errorCode;
}

void AdvancedConnector::do_resolve()
{
#ifdef NO_NDNS
	printf("resolving (aaaa=%d)\n", d->aaaa);
	d->qdns = new Q3Dns;
	connect(d->qdns, SIGNAL(resultsReady()), SLOT(dns_done()));
	if(d->aaaa)
		d->qdns->setRecordType(Q3Dns::Aaaa); // IPv6
	else
		d->qdns->setRecordType(Q3Dns::A); // IPv4
	d->qdns->setLabel(d->host);
#else
	d->dns.resolve(d->host);
#endif
}

void AdvancedConnector::dns_done()
{
	bool failed = false;
	QHostAddress addr;

#ifdef NO_NDNS
	//if(!d->qdns)
	//	return;

	// apparently we sometimes get this signal even though the results aren' t ready
	//if(d->qdns->isWorking())
	//	return;

	//SafeDeleteLock s(&d->sd);

        // grab the address list and destroy the qdns object
	QList<QHostAddress> list = d->qdns->addresses();
	d->qdns->disconnect(this);
	d->qdns->deleteLater();
	//d->sd.deleteLater(d->qdns);
	d->qdns = 0;

	if(list.isEmpty()) {
		if(d->aaaa) {
			d->aaaa = false;
			do_resolve();
			return;
		}
		//do_resolve();
		//return;
		failed = true;
	}
	else
		addr = list.first();
#else
	if(d->dns.result().isNull ())
		failed = true;
	else
		addr = QHostAddress(d->dns.result());
#endif

	if(failed) {
#ifdef XMPP_DEBUG
		printf("dns1\n");
#endif
		// using proxy?  then try the unresolved host through the proxy
		if(d->proxy.type() != Proxy::None) {
#ifdef XMPP_DEBUG
			printf("dns1.1\n");
#endif
			do_connect();
		}
		else if(d->using_srv) {
#ifdef XMPP_DEBUG
			printf("dns1.2\n");
#endif
			if(d->servers.isEmpty()) {
#ifdef XMPP_DEBUG
				printf("dns1.2.1\n");
#endif
				cleanup();
				d->errorCode = ErrConnectionRefused;
				error();
			}
			else {
#ifdef XMPP_DEBUG
				printf("dns1.2.2\n");
#endif
				tryNextSrv();
				return;
			}
		}
		else {
#ifdef XMPP_DEBUG
			printf("dns1.3\n");
#endif
			cleanup();
			d->errorCode = ErrHostNotFound;
			error();
		}
	}
	else {
#ifdef XMPP_DEBUG
		printf("dns2\n");
#endif
		d->host = addr.toString();
		do_connect();
	}
}

void AdvancedConnector::do_connect()
{
#ifdef XMPP_DEBUG
	printf("trying %s:%d\n", d->host.latin1(), d->port);
#endif
	int t = d->proxy.type();
	if(t == Proxy::None) {
#ifdef XMPP_DEBUG
		printf("do_connect1\n");
#endif
		BSocket *s = new BSocket;
		d->bs = s;
		connect(s, SIGNAL(connected()), SLOT(bs_connected()));
		connect(s, SIGNAL(error(int)), SLOT(bs_error(int)));
		s->connectToHost(d->host, d->port);
	}
	else if(t == Proxy::HttpConnect) {
#ifdef XMPP_DEBUG
		printf("do_connect2\n");
#endif
		HttpConnect *s = new HttpConnect;
		d->bs = s;
		connect(s, SIGNAL(connected()), SLOT(bs_connected()));
		connect(s, SIGNAL(error(int)), SLOT(bs_error(int)));
		if(!d->proxy.user().isEmpty())
			s->setAuth(d->proxy.user(), d->proxy.pass());
		s->connectToHost(d->proxy.host(), d->proxy.port(), d->host, d->port);
	}
	else if(t == Proxy::Socks) {
#ifdef XMPP_DEBUG
		printf("do_connect3\n");
#endif
		SocksClient *s = new SocksClient;
		d->bs = s;
		connect(s, SIGNAL(connected()), SLOT(bs_connected()));
		connect(s, SIGNAL(error(int)), SLOT(bs_error(int)));
		if(!d->proxy.user().isEmpty())
			s->setAuth(d->proxy.user(), d->proxy.pass());
		s->connectToHost(d->proxy.host(), d->proxy.port(), d->host, d->port);
	}
}

void AdvancedConnector::tryNextSrv()
{
#ifdef XMPP_DEBUG
	printf("trying next srv\n");
#endif
	d->host = d->servers.first().name;
	d->port = d->servers.first().port;
	d->servers.remove(d->servers.begin());
	do_resolve();
}

void AdvancedConnector::srv_done()
{
	QPointer<QObject> self = this;
#ifdef XMPP_DEBUG
	printf("srv_done1\n");
#endif
	d->servers = d->srv.servers();
	if(d->servers.isEmpty()) {
		srvResult(false);
		if(!self)
			return;

#ifdef XMPP_DEBUG
		printf("srv_done1.1\n");
#endif
		// fall back to A record
		d->using_srv = false;
		d->host = d->server;
		if(d->opt_probe) {
#ifdef XMPP_DEBUG
			printf("srv_done1.1.1\n");
#endif
			d->probe_mode = 0;
			d->port = 5223;
			d->will_be_ssl = true;
		}
		else {
#ifdef XMPP_DEBUG
			printf("srv_done1.1.2\n");
#endif
			d->probe_mode = 1;
			d->port = 5222;
		}
		do_resolve();
		return;
	}

	srvResult(true);
	if(!self)
		return;

	d->using_srv = true;
	tryNextSrv();
}

void AdvancedConnector::bs_connected()
{
	if(d->proxy.type() == Proxy::None) {
		QHostAddress h = (static_cast<BSocket*>(d->bs))->peerAddress();
		int p = (static_cast<BSocket*>(d->bs))->peerPort();
		setPeerAddress(h, p);
	}

	// only allow ssl override if proxy==poll or host:port
	if((d->proxy.type() == Proxy::HttpPoll || !d->opt_host.isEmpty()) && d->opt_ssl)
		setUseSSL(true);
	else if(d->will_be_ssl)
		setUseSSL(true);

	d->mode = Connected;
	connected();
}

void AdvancedConnector::bs_error(int x)
{
	if(d->mode == Connected) {
		d->errorCode = ErrStream;
		error();
		return;
	}

	bool proxyError = false;
	int err = ErrConnectionRefused;
	int t = d->proxy.type();

#ifdef XMPP_DEBUG
	printf("bse1\n");
#endif

	// figure out the error
	if(t == Proxy::None) {
		if(x == BSocket::ErrHostNotFound)
			err = ErrHostNotFound;
		else
			err = ErrConnectionRefused;
	}
	else if(t == Proxy::HttpConnect) {
		if(x == HttpConnect::ErrConnectionRefused)
			err = ErrConnectionRefused;
		else if(x == HttpConnect::ErrHostNotFound)
			err = ErrHostNotFound;
		else {
			proxyError = true;
			if(x == HttpConnect::ErrProxyAuth)
				err = ErrProxyAuth;
			else if(x == HttpConnect::ErrProxyNeg)
				err = ErrProxyNeg;
			else
				err = ErrProxyConnect;
		}
	}
	else if(t == Proxy::HttpPoll) {
		if(x == HttpPoll::ErrConnectionRefused)
			err = ErrConnectionRefused;
		else if(x == HttpPoll::ErrHostNotFound)
			err = ErrHostNotFound;
		else {
			proxyError = true;
			if(x == HttpPoll::ErrProxyAuth)
				err = ErrProxyAuth;
			else if(x == HttpPoll::ErrProxyNeg)
				err = ErrProxyNeg;
			else
				err = ErrProxyConnect;
		}
	}
	else if(t == Proxy::Socks) {
		if(x == SocksClient::ErrConnectionRefused)
			err = ErrConnectionRefused;
		else if(x == SocksClient::ErrHostNotFound)
			err = ErrHostNotFound;
		else {
			proxyError = true;
			if(x == SocksClient::ErrProxyAuth)
				err = ErrProxyAuth;
			else if(x == SocksClient::ErrProxyNeg)
				err = ErrProxyNeg;
			else
				err = ErrProxyConnect;
		}
	}

	// no-multi or proxy error means we quit
	if(!d->multi || proxyError) {
		cleanup();
		d->errorCode = err;
		error();
		return;
	}

	if(d->using_srv && !d->servers.isEmpty()) {
#ifdef XMPP_DEBUG
		printf("bse1.1\n");
#endif
		tryNextSrv();
	}
	else if(!d->using_srv && d->opt_probe && d->probe_mode == 0) {
#ifdef XMPP_DEBUG
		printf("bse1.2\n");
#endif
		d->probe_mode = 1;
		d->port = 5222;
		d->will_be_ssl = false;
		do_connect();
	}
	else {
#ifdef XMPP_DEBUG
		printf("bse1.3\n");
#endif
		cleanup();
		d->errorCode = ErrConnectionRefused;
		error();
	}
}

void AdvancedConnector::http_syncStarted()
{
	httpSyncStarted();
}

void AdvancedConnector::http_syncFinished()
{
	httpSyncFinished();
}
