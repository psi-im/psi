/*
 * Copyright (C) 2006  Justin Karneges
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 */

#include "jdnsshared.h"

namespace XMPP {

//----------------------------------------------------------------------------
// JDnsSharedDebug
//----------------------------------------------------------------------------
JDnsSharedDebug::JDnsSharedDebug(QObject *parent)
:QObject(parent)
{
}

JDnsSharedDebug::~JDnsSharedDebug()
{
}

void JDnsSharedDebug::addDebug(const QString &name, const QStringList &lines)
{
	QMutexLocker locker(&m);
	for(int n = 0; n < lines.count(); ++n)
		_lines += name + ": " + lines[n];
	QMetaObject::invokeMethod(this, "doUpdate", Qt::QueuedConnection);
}

void JDnsSharedDebug::doUpdate()
{
	QMutexLocker locker(&m);
	if(_lines.isEmpty())
		return;
	QStringList lines = _lines;
	_lines.clear();
	emit debug(lines);
}

//----------------------------------------------------------------------------
// JDnsSharedRequest
//----------------------------------------------------------------------------
static int jsr_refs = 0;
JDnsSharedRequest::JDnsSharedRequest(JDnsShared *owner, QObject *parent)
:QObject(parent)
{
	_owner = owner;
	++jsr_refs;
	//printf("create.  refs=%d\n", jsr_refs);
}

JDnsSharedRequest::~JDnsSharedRequest()
{
	--jsr_refs;
	//printf("destroy.  refs=%d\n", jsr_refs);
	cancel();
}

JDnsSharedRequest::Type JDnsSharedRequest::type()
{
	return _type;
}

void JDnsSharedRequest::query(const QByteArray &name, int type)
{
	if(!_handles.isEmpty())
		return;
	_owner->queryStart(this, name, type);
}

void JDnsSharedRequest::publish(QJDns::PublishMode m, const QJDns::Record &record)
{
	if(!_handles.isEmpty())
		return;
	_owner->publishStart(this, m, record);
}

void JDnsSharedRequest::publishUpdate(const QJDns::Record &record)
{
	if(_handles.isEmpty())
		return;
	_owner->publishUpdate(this, record);
}

void JDnsSharedRequest::cancel()
{
	if(_handles.isEmpty())
		return;
	if(_type == Query)
		_owner->queryCancel(this);
	else
		_owner->publishCancel(this);
}

bool JDnsSharedRequest::success() const
{
	return _success;
}

JDnsSharedRequest::Error JDnsSharedRequest::error() const
{
	return _error;
}

QList<QJDns::Record> JDnsSharedRequest::results() const
{
	return _results;
}

//----------------------------------------------------------------------------
// JDnsShared
//----------------------------------------------------------------------------
JDnsShared::JDnsShared(Mode _mode, JDnsSharedDebug *_db, const QString &_dbname, QObject *parent)
:QObject(parent)
{
	mode = _mode;
	shutting_down = false;
	db = _db;
	dbname = _dbname;
}

JDnsShared::~JDnsShared()
{
	for(int n = 0; n < instances.count(); ++n)
	{
		delete instances[n]->jdns;
		delete instances[n];
	}
}

bool JDnsShared::addInterface(const QHostAddress &addr)
{
	if(shutting_down)
		return false;

	QString str;
	int index = instances.count();

	str = QString("attempting to use interface %1").arg(addr.toString());
	if(db)
		db->addDebug(dbname + QString::number(index), QStringList() << str);

	QJDns *jdns;

	if(mode == UnicastInternet || mode == UnicastLocal)
	{
		jdns = new QJDns(this);
		jdns_link(jdns);
		if(!jdns->init(QJDns::Unicast, addr))
		{
			doDebug(jdns, index);
			delete jdns;
			return false;
		}

		if(mode == UnicastLocal)
		{
			QJDns::NameServer host;
			host.address = QHostAddress("224.0.0.251");
			host.port = 5353;
			jdns->setNameServers(QList<QJDns::NameServer>() << host);
		}
	}
	else
	{
		// only one instance supported for now (see more in the
		//   comment below)
		if(!instances.isEmpty())
			return false;

		jdns = new QJDns(this);
		jdns_link(jdns);

		// instead of binding to the requested address, we'll bind
		//   to "Any".  this is because QJDns doesn't support
		//   multiple interfaces.  however, if that ever changes,
		//   we can update the code here and things should be mostly
		//   transparent to the user of JDnsShared.
		//if(!jdns->init(QJDns::Multicast, addr))
		if(!jdns->init(QJDns::Multicast, QHostAddress::Any))
		{
			doDebug(jdns, index);
			delete jdns;
			return false;
		}
	}

	Instance *i = new Instance;
	i->jdns = jdns;
	i->addr = addr;
	instances += i;

	if(mode == UnicastInternet)
		applyNameServers(i);

	str = QString("interface ready");
	if(db)
		db->addDebug(dbname + QString::number(index), QStringList() << str);

	return true;
}

void JDnsShared::removeInterface(const QHostAddress &addr)
{
	Instance *i = 0;
	int index;
	for(int n = 0; n < instances.count(); ++n)
	{
		if(instances[n]->addr == addr)
		{
			i = instances[n];
			index = n;
			break;
		}
	}
	if(!i)
		return;

	// no need to shutdown jdns when this happens, since there is no interface to send on.
	instances.removeAll(i);
	delete i->jdns;
	delete i;

	if(instances.isEmpty())
	{
		// TODO: invalidate any active requests
	}

	QString str = QString("removing from %1").arg(addr.toString());
	if(db)
		db->addDebug(dbname + QString::number(index), QStringList() << str);
}

void JDnsShared::setNameServers(const QList<QJDns::NameServer> &_nameServers)
{
	nameServers = _nameServers;
	for(int n = 0; n < instances.count(); ++n)
		applyNameServers(instances[n]);
}

void JDnsShared::shutdown()
{
	shutting_down = true;
	for(int n = 0; n < instances.count(); ++n)
		instances[n]->jdns->shutdown();
}

void JDnsShared::jdns_link(QJDns *jdns)
{
	connect(jdns, SIGNAL(resultsReady(int, const QJDns::Response &)), SLOT(jdns_resultsReady(int, const QJDns::Response &)));
	connect(jdns, SIGNAL(published(int)), SLOT(jdns_published(int)));
	connect(jdns, SIGNAL(error(int, QJDns::Error)), SLOT(jdns_error(int, QJDns::Error)));
	connect(jdns, SIGNAL(shutdownFinished()), SLOT(jdns_shutdownFinished()));
	connect(jdns, SIGNAL(debugLinesReady()), SLOT(jdns_debugLinesReady()));
}

int JDnsShared::indexByJDns(const QJDns *jdns)
{
	for(int n = 0; n < instances.count(); ++n)
	{
		if(instances[n]->jdns == jdns)
			return n;
	}
	return -1;
}

void JDnsShared::applyNameServers(Instance *i)
{
	QList<QJDns::NameServer> usable;
	for(int n = 0; n < nameServers.count(); ++n)
	{
		if(nameServers[n].address.protocol() == i->addr.protocol())
			usable += nameServers[n];
	}
	i->jdns->setNameServers(usable);
}

void JDnsShared::doDebug(QJDns *jdns, int index)
{
	QStringList lines = jdns->debugLines();
	if(db)
		db->addDebug(dbname + QString::number(index), lines);
}

JDnsSharedRequest *JDnsShared::findRequest(QJDns *jdns, int id)
{
	for(int n = 0; n < requests.count(); ++n)
	{
		JDnsSharedRequest *req = requests[n];
		for(int k = 0; k < req->_handles.count(); ++k)
		{
			const JDnsSharedRequest::Handle &h = req->_handles[k];
			if(h.jdns == jdns && h.id == id)
				return req;
		}
	}
	return 0;
}

void JDnsShared::queryStart(JDnsSharedRequest *obj, const QByteArray &name, int qType)
{
	obj->_type = JDnsSharedRequest::Query;
	obj->_success = false;
	if(instances.isEmpty())
	{
		obj->_error = JDnsSharedRequest::ErrorNoNet;
		QMetaObject::invokeMethod(obj, "resultsReady", Qt::QueuedConnection);
		return;
	}

	// query on all jdns instances
	for(int n = 0; n < instances.count(); ++n)
	{
		JDnsSharedRequest::Handle h;
		h.jdns = instances[n]->jdns;
		h.id = instances[n]->jdns->queryStart(name, qType);
		obj->_handles += h;
	}

	requests += obj;
}

void JDnsShared::queryCancel(JDnsSharedRequest *obj)
{
	if(!requests.contains(obj))
		return;

	for(int n = 0; n < obj->_handles.count(); ++n)
	{
		JDnsSharedRequest::Handle &h = obj->_handles[n];
		h.jdns->queryCancel(h.id);
	}
	obj->_handles.clear();
	requests.removeAll(obj);
}

void JDnsShared::publishStart(JDnsSharedRequest *obj, QJDns::PublishMode m, const QJDns::Record &record)
{
	obj->_type = JDnsSharedRequest::Publish;
	obj->_success = false;
	if(instances.isEmpty())
	{
		obj->_error = JDnsSharedRequest::ErrorNoNet;
		QMetaObject::invokeMethod(obj, "resultsReady", Qt::QueuedConnection);
		return;
	}

	// only one instance
	JDnsSharedRequest::Handle h;
	h.jdns = instances[0]->jdns;
	h.id = instances[0]->jdns->publishStart(m, record);
	obj->_handles += h;

	requests += obj;
}

void JDnsShared::publishUpdate(JDnsSharedRequest *obj, const QJDns::Record &record)
{
	if(!requests.contains(obj))
		return;

	// fill in the right A record
	QJDns::Record rec_copy = record;
	if(rec_copy.type == QJDns::A && rec_copy.address.isNull())
		rec_copy.address = instances[0]->addr;

	// only one instance
	obj->_handles[0].jdns->publishUpdate(obj->_handles[0].id, record);
}

void JDnsShared::publishCancel(JDnsSharedRequest *obj)
{
	if(!requests.contains(obj))
		return;

	// only one instance
	obj->_handles[0].jdns->publishCancel(obj->_handles[0].id);

	obj->_handles.clear();
	requests.removeAll(obj);
}

void JDnsShared::jdns_resultsReady(int id, const QJDns::Response &results)
{
	QJDns *jdns = (QJDns *)sender();
	JDnsSharedRequest *r = findRequest(jdns, id);
	r->_success = true;
	r->_results.clear();
	for(int n = 0; n < results.answerRecords.count(); ++n)
		r->_results += results.answerRecords[n];
	if(mode == UnicastInternet || mode == UnicastLocal)
	{
		// only one response, so "cancel" it
		for(int n = 0; n < r->_handles.count(); ++n)
		{
			JDnsSharedRequest::Handle &h = r->_handles[n];
			if(h.jdns == jdns && h.id == id)
			{
				r->_handles.removeAt(n);
				if(r->_handles.isEmpty())
					requests.removeAll(r);
				break;
			}
		}
	}
	emit r->resultsReady();
}

void JDnsShared::jdns_published(int id)
{
	QJDns *jdns = (QJDns *)sender();
	JDnsSharedRequest *r = findRequest(jdns, id);

	r->_success = true;
	emit r->resultsReady();
}

void JDnsShared::jdns_error(int id, QJDns::Error e)
{
	QJDns *jdns = (QJDns *)sender();
	JDnsSharedRequest *r = findRequest(jdns, id);

	// "cancel" it
	for(int n = 0; n < r->_handles.count(); ++n)
	{
		JDnsSharedRequest::Handle &h = r->_handles[n];
		if(h.jdns == jdns && h.id == id)
		{
			r->_handles.removeAt(n);
			if(r->_handles.isEmpty())
				requests.removeAll(r);
			break;
		}
	}

	// for queries, ignore the error if it is not the last error
	if(r->_type == JDnsSharedRequest::Query && !r->_handles.isEmpty())
		return;

	r->_success = false;
	JDnsSharedRequest::Error x = JDnsSharedRequest::ErrorGeneric;
	if(e == QJDns::ErrorNXDomain)
		x = JDnsSharedRequest::ErrorNXDomain;
	else if(e == QJDns::ErrorTimeout)
		x = JDnsSharedRequest::ErrorTimeout;
	else // ErrorGeneric
		x = JDnsSharedRequest::ErrorGeneric;
	r->_error = x;
	emit r->resultsReady();
}

void JDnsShared::jdns_shutdownFinished()
{
	QJDns *jdns = (QJDns *)sender();
	//printf("shutdownFinished: [%p]\n", jdns);

	int x = indexByJDns(jdns);
	instances[x]->shutdown_done = true;

	bool alldone = true;
	for(int n = 0; n < instances.count(); ++n)
	{
		if(!instances[n]->shutdown_done)
			alldone = false;
	}
	if(alldone)
		emit shutdownFinished();
}

void JDnsShared::jdns_debugLinesReady()
{
	QJDns *jdns = (QJDns *)sender();
	int x = indexByJDns(jdns);
	if(x != -1)
		doDebug(jdns, x);
}

}
