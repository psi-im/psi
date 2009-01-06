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

#include "netnames.h"

//#include <idna.h>
#include "irisnetplugin.h"
#include "irisnetglobal_p.h"

namespace XMPP {

//----------------------------------------------------------------------------
// NameRecord
//----------------------------------------------------------------------------
class NameRecord::Private : public QSharedData
{
public:
	QByteArray owner;
	NameRecord::Type type;
	int ttl;

	QHostAddress address;
	QByteArray name;
	int priority, weight, port;
	QList<QByteArray> texts;
	QByteArray cpu, os;
	QByteArray rawData;
};

#define ENSURE_D { if(!d) d = new Private; }

NameRecord::NameRecord()
{
	d = 0;
}

NameRecord::NameRecord(const QByteArray &owner, int ttl)
{
	d = 0;
	setOwner(owner);
	setTtl(ttl);
}

NameRecord::NameRecord(const NameRecord &from)
{
	d = 0;
	*this = from;
}

NameRecord::~NameRecord()
{
}

NameRecord & NameRecord::operator=(const NameRecord &from)
{
	d = from.d;
	return *this;
}

bool NameRecord::isNull() const
{
	return (d ? false : true);
}

QByteArray NameRecord::owner() const
{
	Q_ASSERT(d);
	return d->owner;
}

int NameRecord::ttl() const
{
	Q_ASSERT(d);
	return d->ttl;
}

NameRecord::Type NameRecord::type() const
{
	Q_ASSERT(d);
	return d->type;
}

QHostAddress NameRecord::address() const
{
	Q_ASSERT(d);
	return d->address;
}

QByteArray NameRecord::name() const
{
	Q_ASSERT(d);
	return d->name;
}

int NameRecord::priority() const
{
	Q_ASSERT(d);
	return d->priority;
}

int NameRecord::weight() const
{
	Q_ASSERT(d);
	return d->weight;
}

int NameRecord::port() const
{
	Q_ASSERT(d);
	return d->port;
}

QList<QByteArray> NameRecord::texts() const
{
	Q_ASSERT(d);
	return d->texts;
}

QByteArray NameRecord::cpu() const
{
	Q_ASSERT(d);
	return d->cpu;
}

QByteArray NameRecord::os() const
{
	Q_ASSERT(d);
	return d->os;
}

QByteArray NameRecord::rawData() const
{
	Q_ASSERT(d);
	return d->rawData;
}

void NameRecord::setOwner(const QByteArray &name)
{
	ENSURE_D
	d->owner = name;
}

void NameRecord::setTtl(int seconds)
{
	ENSURE_D
	d->ttl = seconds;
}

void NameRecord::setAddress(const QHostAddress &a)
{
	ENSURE_D
	if(a.protocol() == QAbstractSocket::IPv6Protocol)
		d->type = NameRecord::Aaaa;
	else
		d->type = NameRecord::A;
	d->address = a;
}

void NameRecord::setMx(const QByteArray &name, int priority)
{
	ENSURE_D
	d->type = NameRecord::Mx;
	d->name = name;
	d->priority = priority;
}

void NameRecord::setSrv(const QByteArray &name, int port, int priority, int weight)
{
	ENSURE_D
	d->type = NameRecord::Srv;
	d->name = name;
	d->port = port;
	d->priority = priority;
	d->weight = weight;
}

void NameRecord::setCname(const QByteArray &name)
{
	ENSURE_D
	d->type = NameRecord::Cname;
	d->name = name;
}

void NameRecord::setPtr(const QByteArray &name)
{
	ENSURE_D
	d->type = NameRecord::Ptr;
	d->name = name;
}

void NameRecord::setTxt(const QList<QByteArray> &texts)
{
	ENSURE_D
	d->type = NameRecord::Txt;
	d->texts = texts;
}

void NameRecord::setHinfo(const QByteArray &cpu, const QByteArray &os)
{
	ENSURE_D
	d->type = NameRecord::Hinfo;
	d->cpu = cpu;
	d->os = os;
}

void NameRecord::setNs(const QByteArray &name)
{
	ENSURE_D
	d->type = NameRecord::Ns;
	d->name = name;
}

void NameRecord::setNull(const QByteArray &rawData)
{
	ENSURE_D
	d->type = NameRecord::Null;
	d->rawData = rawData;
}

//----------------------------------------------------------------------------
// ServiceInstance
//----------------------------------------------------------------------------
class ServiceInstance::Private : public QSharedData
{
public:
	QString instance, type, domain;
	QMap<QString,QByteArray> attribs;
	QByteArray name;
};

ServiceInstance::ServiceInstance()
{
	d = new Private;
}

ServiceInstance::ServiceInstance(const QString &instance, const QString &type, const QString &domain, const QMap<QString,QByteArray> &attribs)
{
	d = new Private;
	d->instance = instance;
	d->type = type;
	d->domain = domain;
	d->attribs = attribs;

	// FIXME: escape the items
	d->name = instance.toLatin1() + '.' + type.toLatin1() + '.' + domain.toLatin1();
}

ServiceInstance::ServiceInstance(const ServiceInstance &from)
{
	d = 0;
	*this = from;
}

ServiceInstance::~ServiceInstance()
{
}

ServiceInstance & ServiceInstance::operator=(const ServiceInstance &from)
{
	d = from.d;
	return *this;
}

QString ServiceInstance::instance() const
{
	return d->instance;
}

QString ServiceInstance::type() const
{
	return d->type;
}

QString ServiceInstance::domain() const
{
	return d->domain;
}

QMap<QString,QByteArray> ServiceInstance::attributes() const
{
	return d->attribs;
}

QByteArray ServiceInstance::name() const
{
	return d->name;
}

//----------------------------------------------------------------------------
// NameManager
//----------------------------------------------------------------------------
class NameManager;

Q_GLOBAL_STATIC(QMutex, nman_mutex)
static NameManager *g_nman = 0;

class NameResolver::Private
{
public:
	NameResolver *q;

	int type;
	bool longLived;
	int id;

	Private(NameResolver *_q) : q(_q)
	{
	}
};

class ServiceBrowser::Private
{
public:
	ServiceBrowser *q;

	int id;

	Private(ServiceBrowser *_q) : q(_q)
	{
	}
};

class ServiceResolver::Private : public QObject
{
	Q_OBJECT
public:
	ServiceResolver *q;

	int id;

	int mode;
	NameResolver dns;
	int port;

	class Server
	{
	public:
		QByteArray host;
		int port;
		int priority;
		int weight;
	};

	QList<Server> servers;
	QList<QHostAddress> addrs;

	Private(ServiceResolver *_q) : q(_q)
	{
		mode = 3;
		connect(&dns, SIGNAL(resultsReady(const QList<XMPP::NameRecord> &)), SLOT(dns_resultsReady(const QList<XMPP::NameRecord> &)));
		connect(&dns, SIGNAL(error(XMPP::NameResolver::Error)), SLOT(dns_error(XMPP::NameResolver::Error)));
	}

	void tryNext()
	{
		if(mode == 3)
		{
			QMetaObject::invokeMethod(q, "finished", Qt::QueuedConnection);
		}
		if(mode == 2)
		{
			if(!addrs.isEmpty())
			{
				QHostAddress addr = addrs.takeFirst();
				QMetaObject::invokeMethod(q, "resultsReady", Qt::QueuedConnection, Q_ARG(QHostAddress, addr), Q_ARG(int, port));
				return;
			}

			if(servers.isEmpty())
			{
				QMetaObject::invokeMethod(q, "finished", Qt::QueuedConnection);
				return;
			}

			Server serv = servers.takeFirst();
			port = serv.port;
			dns.start(serv.host, NameRecord::A); // TODO: ipv6!
		}
		else
		{
			if(addrs.isEmpty())
			{
				QMetaObject::invokeMethod(q, "finished", Qt::QueuedConnection);
				return;
			}

			QHostAddress addr = addrs.takeFirst();
			QMetaObject::invokeMethod(q, "resultsReady", Qt::QueuedConnection, Q_ARG(QHostAddress, addr), Q_ARG(int, port));
		}
	}

private slots:
	void dns_resultsReady(const QList<XMPP::NameRecord> &results)
	{
		if(mode == 0)
		{
			mode = 2;
			servers.clear();
			for(int n = 0; n < results.count(); ++n)
			{
				Server serv;
				serv.host = results[n].name();
				serv.port = results[n].port();
				serv.priority = results[n].priority();
				serv.weight = results[n].weight();
				servers += serv;
			}
			tryNext();
		}
		else if(mode == 1)
		{
			addrs.clear();

			// TODO: don't forget about ipv6
			for(int n = 0; n < results.count(); ++n)
				addrs += results[n].address();

			tryNext();
		}
		else
		{
			QList<QHostAddress> tmp;
			for(int n = 0; n < results.count(); ++n)
				tmp += results[n].address();

			addrs += tmp;
			tryNext();
		}
	}

	void dns_error(XMPP::NameResolver::Error)
	{
		if(mode == 0 || mode == 1)
			emit q->error();
		else
			tryNext(); // FIXME: probably shouldn't share this
	}
};

class ServiceLocalPublisher::Private
{
public:
	ServiceLocalPublisher *q;

	int id;

	Private(ServiceLocalPublisher *_q) : q(_q)
	{
	}
};

class NameManager : public QObject
{
	Q_OBJECT
public:
	NameProvider *p_net, *p_local;
	ServiceProvider *p_serv;
	QHash<int,NameResolver::Private*> res_instances;
	QHash<int,int> res_sub_instances;

	QHash<int,ServiceBrowser::Private*> br_instances;
	QHash<int,ServiceResolver::Private*> sres_instances;
	QHash<int,ServiceLocalPublisher::Private*> slp_instances;

	NameManager(QObject *parent = 0) : QObject(parent)
	{
		p_net = 0;
		p_local = 0;
		p_serv = 0;
	}

	~NameManager()
	{
		delete p_net;
		delete p_local;
		delete p_serv;
	}

	static NameManager *instance()
	{
		QMutexLocker locker(nman_mutex());
		if(!g_nman)
		{
			g_nman = new NameManager;
			irisNetAddPostRoutine(NetNames::cleanup);
		}
		return g_nman;
	}

	static void cleanup()
	{
		delete g_nman;
		g_nman = 0;
	}

	void resolve_start(NameResolver::Private *np, const QByteArray &name, int qType, bool longLived)
	{
		QMutexLocker locker(nman_mutex());

		np->type = qType;
		np->longLived = longLived;
		if(!p_net)
		{
			NameProvider *c = 0;
			QList<IrisNetProvider*> list = irisNetProviders();
			for(int n = 0; n < list.count(); ++n)
			{
				IrisNetProvider *p = list[n];
				c = p->createNameProviderInternet();
				if(c)
					break;
			}
			Q_ASSERT(c); // we have built-in support, so this should never fail
			p_net = c;

			// use queued connections
			qRegisterMetaType< QList<XMPP::NameRecord> >("QList<XMPP::NameRecord>");
			qRegisterMetaType<XMPP::NameResolver::Error>("XMPP::NameResolver::Error");
			connect(p_net, SIGNAL(resolve_resultsReady(int, const QList<XMPP::NameRecord> &)), SLOT(provider_resolve_resultsReady(int, const QList<XMPP::NameRecord> &)), Qt::QueuedConnection);
			connect(p_net, SIGNAL(resolve_error(int, XMPP::NameResolver::Error)), SLOT(provider_resolve_error(int, XMPP::NameResolver::Error)), Qt::QueuedConnection);
			connect(p_net, SIGNAL(resolve_useLocal(int, const QByteArray &)), SLOT(provider_resolve_useLocal(int, const QByteArray &)), Qt::QueuedConnection);
		}

		np->id = p_net->resolve_start(name, qType, longLived);

		//printf("assigning %d to %p\n", req_id, np);
		res_instances.insert(np->id, np);
	}

	void browse_start(ServiceBrowser::Private *np, const QString &type, const QString &domain)
	{
		QMutexLocker locker(nman_mutex());

		if(!p_serv)
		{
			ServiceProvider *c = 0;
			QList<IrisNetProvider*> list = irisNetProviders();
			for(int n = 0; n < list.count(); ++n)
			{
				IrisNetProvider *p = list[n];
				c = p->createServiceProvider();
				if(c)
					break;
			}
			Q_ASSERT(c); // we have built-in support, so this should never fail
			p_serv = c;

			// use queued connections
			qRegisterMetaType<XMPP::ServiceInstance>("XMPP::ServiceInstance");
			qRegisterMetaType<XMPP::ServiceBrowser::Error>("XMPP::ServiceBrowser::Error");

			connect(p_serv, SIGNAL(browse_instanceAvailable(int, const XMPP::ServiceInstance &)), SLOT(provider_browse_instanceAvailable(int, const XMPP::ServiceInstance &)), Qt::QueuedConnection);
			connect(p_serv, SIGNAL(browse_instanceUnavailable(int, const XMPP::ServiceInstance &)), SLOT(provider_browse_instanceUnavailable(int, const XMPP::ServiceInstance &)), Qt::QueuedConnection);
			connect(p_serv, SIGNAL(browse_error(int, XMPP::ServiceBrowser::Error)), SLOT(provider_browse_error(int, XMPP::ServiceBrowser::Error)), Qt::QueuedConnection);
		}

		/*np->id = */

		np->id = p_serv->browse_start(type, domain);

		br_instances.insert(np->id, np);
	}

	void resolve_instance_start(ServiceResolver::Private *np, const QByteArray &name)
	{
		QMutexLocker locker(nman_mutex());

		if(!p_serv)
		{
			ServiceProvider *c = 0;
			QList<IrisNetProvider*> list = irisNetProviders();
			for(int n = 0; n < list.count(); ++n)
			{
				IrisNetProvider *p = list[n];
				c = p->createServiceProvider();
				if(c)
					break;
			}
			Q_ASSERT(c); // we have built-in support, so this should never fail
			p_serv = c;

			// use queued connections
			qRegisterMetaType<QHostAddress>("QHostAddress");
			qRegisterMetaType< QList<XMPP::ServiceProvider::ResolveResult> >("QList<XMPP::ServiceProvider::ResolveResult>");
			connect(p_serv, SIGNAL(resolve_resultsReady(int, const QList<XMPP::ServiceProvider::ResolveResult> &)), SLOT(provider_resolve_resultsReady(int, const QList<XMPP::ServiceProvider::ResolveResult> &)), Qt::QueuedConnection);
		}

		/*np->id = */

		np->id = p_serv->resolve_start(name);

		sres_instances.insert(np->id, np);
	}

	void publish_start(ServiceLocalPublisher::Private *np, const QString &instance, const QString &type, int port, const QMap<QString,QByteArray> &attribs)
	{
		QMutexLocker locker(nman_mutex());

		if(!p_serv)
		{
			ServiceProvider *c = 0;
			QList<IrisNetProvider*> list = irisNetProviders();
			for(int n = 0; n < list.count(); ++n)
			{
				IrisNetProvider *p = list[n];
				c = p->createServiceProvider();
				if(c)
					break;
			}
			Q_ASSERT(c); // we have built-in support, so this should never fail
			p_serv = c;

			// use queued connections
			qRegisterMetaType<XMPP::ServiceLocalPublisher::Error>("XMPP::ServiceLocalPublisher::Error");
			connect(p_serv, SIGNAL(publish_published(int)), SLOT(provider_publish_published(int)), Qt::QueuedConnection);
			connect(p_serv, SIGNAL(publish_extra_published(int)), SLOT(provider_publish_extra_published(int)), Qt::QueuedConnection);
		}

		/*np->id = */

		np->id = p_serv->publish_start(instance, type, port, attribs);

		slp_instances.insert(np->id, np);
	}

	void publish_extra_start(ServiceLocalPublisher::Private *np, const NameRecord &rec)
	{
		np->id = p_serv->publish_extra_start(np->id, rec);
	}

private slots:
	void provider_resolve_resultsReady(int id, const QList<XMPP::NameRecord> &results)
	{
		// is it a sub-request?
		if(res_sub_instances.contains(id))
		{
			int par_id = res_sub_instances.value(id);
			res_sub_instances.remove(id);
			p_net->resolve_localResultsReady(par_id, results);
			return;
		}

		NameResolver::Private *np = res_instances.value(id);
		emit np->q->resultsReady(results);
	}

	void provider_resolve_error(int id, XMPP::NameResolver::Error e)
	{
		// is it a sub-request?
		if(res_sub_instances.contains(id))
		{
			int par_id = res_sub_instances.value(id);
			res_sub_instances.remove(id);
			p_net->resolve_localError(par_id, e);
			return;
		}

		NameResolver::Private *np = res_instances.value(id);
		emit np->q->error(e);
	}

	void provider_resolve_useLocal(int id, const QByteArray &name)
	{
		// transfer to local
		if(!p_local)
		{
			NameProvider *c = 0;
			QList<IrisNetProvider*> list = irisNetProviders();
			for(int n = 0; n < list.count(); ++n)
			{
				IrisNetProvider *p = list[n];
				c = p->createNameProviderLocal();
				if(c)
					break;
			}
			Q_ASSERT(c); // we have built-in support, so this should never fail
			// FIXME: not true, binding can fail
			p_local = c;

			// use queued connections
			qRegisterMetaType< QList<XMPP::NameRecord> >("QList<XMPP::NameRecord>");
			qRegisterMetaType<XMPP::NameResolver::Error>("XMPP::NameResolver::Error");
			connect(p_local, SIGNAL(resolve_resultsReady(int, const QList<XMPP::NameRecord> &)), SLOT(provider_resolve_resultsReady(int, const QList<XMPP::NameRecord> &)), Qt::QueuedConnection);
			connect(p_local, SIGNAL(resolve_error(int, XMPP::NameResolver::Error)), SLOT(provider_resolve_error(int, XMPP::NameResolver::Error)), Qt::QueuedConnection);
		}

		NameResolver::Private *np = res_instances.value(id);

		// transfer to local only
		if(np->longLived)
		{
			res_instances.remove(np->id);

			np->id = p_local->resolve_start(name, np->type, true);
			res_instances.insert(np->id, np);
		}
		// sub request
		else
		{
			int req_id = p_local->resolve_start(name, np->type, false);

			res_sub_instances.insert(req_id, np->id);
		}
	}

	void provider_browse_instanceAvailable(int id, const XMPP::ServiceInstance &i)
	{
		ServiceBrowser::Private *np = br_instances.value(id);
		emit np->q->instanceAvailable(i);
	}

	void provider_browse_instanceUnavailable(int id, const XMPP::ServiceInstance &i)
	{
		ServiceBrowser::Private *np = br_instances.value(id);
		emit np->q->instanceUnavailable(i);
	}

	void provider_browse_error(int id, XMPP::ServiceBrowser::Error e)
	{
		Q_UNUSED(e);
		ServiceBrowser::Private *np = br_instances.value(id);
		// TODO
		emit np->q->error();
	}

	void provider_resolve_resultsReady(int id, const QList<XMPP::ServiceProvider::ResolveResult> &results)
	{
		ServiceResolver::Private *np = sres_instances.value(id);
		emit np->q->resultsReady(results[0].address, results[0].port);
	}

	void provider_publish_published(int id)
	{
		ServiceLocalPublisher::Private *np = slp_instances.value(id);
		emit np->q->published();
	}

	void provider_publish_extra_published(int id)
	{
		Q_UNUSED(id);
		//ServiceLocalPublisher::Private *np = slp_instances.value(id);
		//emit np->q->published();
	}
};

//----------------------------------------------------------------------------
// NameResolver
//----------------------------------------------------------------------------

// copied from JDNS
#define JDNS_RTYPE_A         1
#define JDNS_RTYPE_AAAA     28
#define JDNS_RTYPE_MX       15
#define JDNS_RTYPE_SRV      33
#define JDNS_RTYPE_CNAME     5
#define JDNS_RTYPE_PTR      12
#define JDNS_RTYPE_TXT      16
#define JDNS_RTYPE_HINFO    13
#define JDNS_RTYPE_NS        2
#define JDNS_RTYPE_ANY     255

static int recordType2Rtype(NameRecord::Type type)
{
	switch(type)
	{
		case NameRecord::A:     return JDNS_RTYPE_A;
		case NameRecord::Aaaa:  return JDNS_RTYPE_AAAA;
		case NameRecord::Mx:    return JDNS_RTYPE_MX;
		case NameRecord::Srv:   return JDNS_RTYPE_SRV;
		case NameRecord::Cname: return JDNS_RTYPE_CNAME;
		case NameRecord::Ptr:   return JDNS_RTYPE_PTR;
		case NameRecord::Txt:   return JDNS_RTYPE_TXT;
		case NameRecord::Hinfo: return JDNS_RTYPE_HINFO;
		case NameRecord::Ns:    return JDNS_RTYPE_NS;
		case NameRecord::Null:  return 10;
		case NameRecord::Any:   return JDNS_RTYPE_ANY;
	}
	return -1;
}

NameResolver::NameResolver(QObject *parent)
:QObject(parent)
{
	d = new Private(this);
}

NameResolver::~NameResolver()
{
	delete d;
}

void NameResolver::start(const QByteArray &name, NameRecord::Type type, Mode mode)
{
	int qType = recordType2Rtype(type);
	if(qType == -1)
		qType = JDNS_RTYPE_A;
	NameManager::instance()->resolve_start(d, name, qType, mode == NameResolver::LongLived ? true : false);
}

void NameResolver::stop()
{
	// TODO
}

//----------------------------------------------------------------------------
// ServiceBrowser
//----------------------------------------------------------------------------
ServiceBrowser::ServiceBrowser(QObject *parent)
:QObject(parent)
{
	d = new Private(this);
}

ServiceBrowser::~ServiceBrowser()
{
	delete d;
}

void ServiceBrowser::start(const QString &type, const QString &domain)
{
	NameManager::instance()->browse_start(d, type, domain);
}

void ServiceBrowser::stop()
{
}

//----------------------------------------------------------------------------
// ServiceResolver
//----------------------------------------------------------------------------
ServiceResolver::ServiceResolver(QObject *parent)
:QObject(parent)
{
	qRegisterMetaType<QHostAddress>("QHostAddress");
	d = new Private(this);
}

ServiceResolver::~ServiceResolver()
{
	delete d;
}

void ServiceResolver::startFromInstance(const QByteArray &name)
{
	NameManager::instance()->resolve_instance_start(d, name);
}

void ServiceResolver::startFromDomain(const QString &domain, const QString &type)
{
	d->mode = 0;
	d->dns.start(type.toLatin1() + '.' + domain.toLatin1(), NameRecord::Srv);
}

void ServiceResolver::startFromPlain(const QString &host, int port)
{
	d->mode = 1;
	d->port = port;
	d->dns.start(host.toLatin1(), NameRecord::A); // TODO: try Aaaa first, fallback to A
}

void ServiceResolver::tryNext()
{
	d->tryNext();
}

void ServiceResolver::stop()
{
}

//----------------------------------------------------------------------------
// ServiceLocalPublisher
//----------------------------------------------------------------------------
ServiceLocalPublisher::ServiceLocalPublisher(QObject *parent)
:QObject(parent)
{
	d = new Private(this);
}

ServiceLocalPublisher::~ServiceLocalPublisher()
{
	delete d;
}

void ServiceLocalPublisher::publish(const QString &instance, const QString &type, int port, const QMap<QString,QByteArray> &attributes)
{
	NameManager::instance()->publish_start(d, instance, type, port, attributes);
}

void ServiceLocalPublisher::updateAttributes(const QMap<QString,QByteArray> &attributes)
{
	Q_UNUSED(attributes);
}

void ServiceLocalPublisher::addRecord(const NameRecord &rec)
{
	NameManager::instance()->publish_extra_start(d, rec);
}

void ServiceLocalPublisher::cancel()
{
}

//----------------------------------------------------------------------------
// NetNames
//----------------------------------------------------------------------------
void NetNames::cleanup()
{
	NameManager::cleanup();
}

QString NetNames::diagnosticText()
{
	// TODO
	return QString();
}

QByteArray NetNames::idnaFromString(const QString &in)
{
	// TODO
	Q_UNUSED(in);
	return QByteArray();
}

QString NetNames::idnaToString(const QByteArray &in)
{
	// TODO
	Q_UNUSED(in);
	return QString();
}

QByteArray NetNames::escapeDomain(const QByteArray &in)
{
	// TODO
	Q_UNUSED(in);
	return QByteArray();
}

QByteArray NetNames::unescapeDomain(const QByteArray &in)
{
	// TODO
	Q_UNUSED(in);
	return QByteArray();
}

}

#include "netnames.moc"
