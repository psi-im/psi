/*
 * srvresolver.cpp - class to simplify SRV lookups
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

#include "srvresolver.h"

#ifndef NO_NDNS
# include "ndns.h"
#endif

// CS_NAMESPACE_BEGIN

static void sortSRVList(QList<Q3Dns::Server> &list)
{
	QList<Q3Dns::Server> tmp = list;
	list.clear();

	while(!tmp.isEmpty()) {
		QList<Q3Dns::Server>::Iterator p = tmp.end();
		for(QList<Q3Dns::Server>::Iterator it = tmp.begin(); it != tmp.end(); ++it) {
			if(p == tmp.end())
				p = it;
			else {
				int a = (*it).priority;
				int b = (*p).priority;
				int j = (*it).weight;
				int k = (*p).weight;
				if(a < b || (a == b && j < k))
					p = it;
			}
		}
		list.append(*p);
		tmp.erase(p);
	}
}

class SrvResolver::Private
{
public:
	Private() {}

	XMPP::NameResolver nndns;
	XMPP::NameRecord::Type nntype;
	bool nndns_busy;

#ifndef NO_NDNS
	NDns ndns;
#endif

	bool failed;
	QHostAddress resultAddress;
	quint16 resultPort;

	bool srvonly;
	QString srv;
	QList<Q3Dns::Server> servers;
	bool aaaa;

	QTimer t;
};

SrvResolver::SrvResolver(QObject *parent)
:QObject(parent)
{
	d = new Private;
	d->nndns_busy = false;

	connect(&d->nndns, SIGNAL(resultsReady(const QList<XMPP::NameRecord> &)), SLOT(nndns_resultsReady(const QList<XMPP::NameRecord> &)));
	connect(&d->nndns, SIGNAL(error(XMPP::NameResolver::Error)), SLOT(nndns_error(XMPP::NameResolver::Error)));

#ifndef NO_NDNS
	connect(&d->ndns, SIGNAL(resultsReady()), SLOT(ndns_done()));
#endif
	connect(&d->t, SIGNAL(timeout()), SLOT(t_timeout()));
	stop();
}

SrvResolver::~SrvResolver()
{
	stop();
	delete d;
}

void SrvResolver::resolve(const QString &server, const QString &type, const QString &proto)
{
	stop();

	d->failed = false;
	d->srvonly = false;
	d->srv = QString("_") + type + "._" + proto + '.' + server;
	d->t.setSingleShot(true);
	d->t.start(15000);
	d->nndns_busy = true;
	d->nntype = XMPP::NameRecord::Srv;
	d->nndns.start(d->srv.toLatin1(), d->nntype);
}

void SrvResolver::resolveSrvOnly(const QString &server, const QString &type, const QString &proto)
{
	stop();

	d->failed = false;
	d->srvonly = true;
	d->srv = QString("_") + type + "._" + proto + '.' + server;
	d->t.setSingleShot(true);
	d->t.start(15000);
	d->nndns_busy = true;
	d->nntype = XMPP::NameRecord::Srv;
	d->nndns.start(d->srv.toLatin1(), d->nntype);
}

void SrvResolver::next()
{
	if(d->servers.isEmpty())
		return;

	tryNext();
}

void SrvResolver::stop()
{
	if(d->t.isActive())
		d->t.stop();
	if(d->nndns_busy) {
		d->nndns.stop();
		d->nndns_busy = false;
	}
#ifndef NO_NDNS
	if(d->ndns.isBusy())
		d->ndns.stop();
#endif
	d->resultAddress = QHostAddress();
	d->resultPort = 0;
	d->servers.clear();
	d->srv = "";
	d->failed = true;
}

bool SrvResolver::isBusy() const
{
#ifndef NO_NDNS
	if(d->nndns_busy || d->ndns.isBusy())
#else
	if(d->nndns_busy)
#endif
		return true;
	else
		return false;
}

QList<Q3Dns::Server> SrvResolver::servers() const
{
	return d->servers;
}

bool SrvResolver::failed() const
{
	return d->failed;
}

QHostAddress SrvResolver::resultAddress() const
{
	return d->resultAddress;
}

quint16 SrvResolver::resultPort() const
{
	return d->resultPort;
}

void SrvResolver::tryNext()
{
#ifndef NO_NDNS
	d->ndns.resolve(d->servers.first().name);
#else
	d->nndns_busy = true;
	d->nntype = d->aaaa ? XMPP::NameRecord::Aaaa : XMPP::NameRecord::A;
	d->nndns.start(d->servers.first().name.toLatin1(), d->nntype);
#endif
}

void SrvResolver::nndns_resultsReady(const QList<XMPP::NameRecord> &results)
{
	if(!d->nndns_busy)
		return;

	d->t.stop();

	if(d->nntype == XMPP::NameRecord::Srv) {
		// grab the server list and destroy the qdns object
		QList<Q3Dns::Server> list;
		for(int n = 0; n < results.count(); ++n)
		{
			list += Q3Dns::Server(QString::fromLatin1(results[n].name()), results[n].priority(), results[n].weight(), results[n].port());
		}

		d->nndns_busy = false;
		d->nndns.stop();

		if(list.isEmpty()) {
			stop();
			resultsReady();
			return;
		}
		sortSRVList(list);
		d->servers = list;

		if(d->srvonly)
			resultsReady();
		else {
			// kick it off
			d->aaaa = true;
			tryNext();
		}
	}
	else {
		// grab the address list and destroy the qdns object
		QList<QHostAddress> list;
		if(d->nntype == XMPP::NameRecord::A || d->nntype == XMPP::NameRecord::Aaaa)
		{
			for(int n = 0; n < results.count(); ++n)
			{
				list += results[n].address();
			}
		}
		d->nndns_busy = false;
		d->nndns.stop();

		if(!list.isEmpty()) {
			int port = d->servers.first().port;
			d->servers.removeFirst();
			d->aaaa = true;

			d->resultAddress = list.first();
			d->resultPort = port;
			resultsReady();
		}
		else {
			if(!d->aaaa)
				d->servers.removeFirst();
			d->aaaa = !d->aaaa;

			// failed?  bail if last one
			if(d->servers.isEmpty()) {
				stop();
				resultsReady();
				return;
			}

			// otherwise try the next
			tryNext();
		}
	}
}

void SrvResolver::nndns_error(XMPP::NameResolver::Error)
{
	nndns_resultsReady(QList<XMPP::NameRecord>());
}

void SrvResolver::ndns_done()
{
#ifndef NO_NDNS
	uint r = d->ndns.result();
	int port = d->servers.first().port;
	d->servers.removeFirst();

	if(r) {
		d->resultAddress = QHostAddress(d->ndns.result());
		d->resultPort = port;
		resultsReady();
	}
	else {
		// failed?  bail if last one
		if(d->servers.isEmpty()) {
			stop();
			resultsReady();
			return;
		}

		// otherwise try the next
		tryNext();
	}
#endif
}

void SrvResolver::t_timeout()
{
	stop();
	resultsReady();
}

// CS_NAMESPACE_END
