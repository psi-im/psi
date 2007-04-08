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

#include <q3cstring.h>
#include <qtimer.h>
#include <q3dns.h>
//Added by qt3to4:
#include <QList>
#include "safedelete.h"

#ifndef NO_NDNS
#include "ndns.h"
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
		tmp.remove(p);
	}
}

class SrvResolver::Private
{
public:
	Private() {}

	Q3Dns *qdns;
#ifndef NO_NDNS
	NDns ndns;
#endif

	bool failed;
	QHostAddress resultAddress;
	Q_UINT16 resultPort;

	bool srvonly;
	QString srv;
	QList<Q3Dns::Server> servers;
	bool aaaa;

	QTimer t;
	SafeDelete sd;
};

SrvResolver::SrvResolver(QObject *parent)
:QObject(parent)
{
	d = new Private;
	d->qdns = 0;

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
	d->t.start(15000, true);
	d->qdns = new Q3Dns;
	connect(d->qdns, SIGNAL(resultsReady()), SLOT(qdns_done()));
	d->qdns->setRecordType(Q3Dns::Srv);
	d->qdns->setLabel(d->srv);
}

void SrvResolver::resolveSrvOnly(const QString &server, const QString &type, const QString &proto)
{
	stop();

	d->failed = false;
	d->srvonly = true;
	d->srv = QString("_") + type + "._" + proto + '.' + server;
	d->t.start(15000, true);
	d->qdns = new Q3Dns;
	connect(d->qdns, SIGNAL(resultsReady()), SLOT(qdns_done()));
	d->qdns->setRecordType(Q3Dns::Srv);
	d->qdns->setLabel(d->srv);
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
	if(d->qdns) {
		d->qdns->disconnect(this);
		d->sd.deleteLater(d->qdns);
		d->qdns = 0;
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
	if(d->qdns || d->ndns.isBusy())
#else
	if(d->qdns)
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

Q_UINT16 SrvResolver::resultPort() const
{
	return d->resultPort;
}

void SrvResolver::tryNext()
{
#ifndef NO_NDNS
	d->ndns.resolve(d->servers.first().name);
#else
	d->qdns = new Q3Dns;
	connect(d->qdns, SIGNAL(resultsReady()), SLOT(ndns_done()));
	if(d->aaaa)
		d->qdns->setRecordType(Q3Dns::Aaaa); // IPv6
	else
		d->qdns->setRecordType(Q3Dns::A); // IPv4
	d->qdns->setLabel(d->servers.first().name);
#endif
}

void SrvResolver::qdns_done()
{
	if(!d->qdns)
		return;

	// apparently we sometimes get this signal even though the results aren't ready
	if(d->qdns->isWorking())
		return;
	d->t.stop();

	SafeDeleteLock s(&d->sd);

	// grab the server list and destroy the qdns object
	QList<Q3Dns::Server> list;
	if(d->qdns->recordType() == Q3Dns::Srv)
		list = d->qdns->servers();
	d->qdns->disconnect(this);
	d->sd.deleteLater(d->qdns);
	d->qdns = 0;

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

void SrvResolver::ndns_done()
{
#ifndef NO_NDNS
	SafeDeleteLock s(&d->sd);

	uint r = d->ndns.result();
	int port = d->servers.first().port;
	d->servers.remove(d->servers.begin());

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
#else
	if(!d->qdns)
		return;

	// apparently we sometimes get this signal even though the results aren't ready
	if(d->qdns->isWorking())
		return;

	SafeDeleteLock s(&d->sd);

	// grab the address list and destroy the qdns object
	QList<QHostAddress> list;
	if(d->qdns->recordType() == Q3Dns::A || d->qdns->recordType() == Q3Dns::Aaaa)
		list = d->qdns->addresses();
	d->qdns->disconnect(this);
	d->sd.deleteLater(d->qdns);
	d->qdns = 0;

	if(!list.isEmpty()) {
		int port = d->servers.first().port;
		d->servers.remove(d->servers.begin());
		d->aaaa = true;

		d->resultAddress = list.first();
		d->resultPort = port;
		resultsReady();
	}
	else {
		if(!d->aaaa)
			d->servers.remove(d->servers.begin());
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
#endif
}

void SrvResolver::t_timeout()
{
	SafeDeleteLock s(&d->sd);

	stop();
	resultsReady();
}

// CS_NAMESPACE_END
