/*
 * Copyright (C) 2007  Justin Karneges
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

#include "irisnetplugin.h"

#include <QtCore>
#include <QtNetwork>
#include "qdnssd.h"

// for ntohl
#ifdef Q_OS_WIN
# include <windows.h>
#else
# include <netinet/in.h>
#endif

static QByteArray nameToDottedString(const QByteArray &in)
{
	QByteArray out;
	int at = 0;
	while(at < in.size())
	{
		int len = in[at++];
		if(len > 0)
			out += in.mid(at, len);
		out += '.';
		at += len;
	}
	return out;
}

static QMap<QString,QByteArray> textsToAttribs(const QList<QByteArray> &texts)
{
	QMap<QString,QByteArray> out;
	foreach(const QByteArray &a, texts)
	{
		QString key;
		QByteArray value;
		int x = a.indexOf('=');
		if(x != -1)
		{
			key = QString::fromLatin1(a.mid(0, x));
			value = a.mid(x + 1);
		}
		else
		{
			key = QString::fromLatin1(a);
		}

		out.insert(key, value);
	}
	return out;
}

static QByteArray attribsToTxtRecord(const QMap<QString,QByteArray> &attribs)
{
	QList<QByteArray> texts;
	QMapIterator<QString,QByteArray> it(attribs);
	while(it.hasNext())
	{
		it.next();
		QByteArray line = it.key().toLatin1() + '=' + it.value();
		texts += line;
	}
	return QDnsSd::createTxtRecord(texts);
}

// returns a list of 3 items, or an empty list on error
static QList<QByteArray> nameToInstanceParts(const QByteArray &name)
{
	// FIXME: improve this parsing...  (what about escaping??)
	int at = name.indexOf('.');
	QByteArray sname = name.mid(0, at);
	++at;
	int next = name.indexOf('.', at);
	++next;
	next = name.indexOf('.', next);
	QByteArray stype = name.mid(at, next - at);
	at = next + 1;
	QByteArray sdomain = name.mid(at);

	QList<QByteArray> out;
	out += sname;
	out += stype;
	out += sdomain;
	return out;
}

static XMPP::NameRecord importQDnsSdRecord(const QDnsSd::Record &in)
{
	XMPP::NameRecord out;
	switch(in.rrtype)
	{
		case 1: // A
		{
			quint32 *p = (quint32 *)in.rdata.data();
			out.setAddress(QHostAddress(ntohl(*p)));
		}
		break;

		case 28: // AAAA
		{
			out.setAddress(QHostAddress((quint8 *)in.rdata.data()));
		}
		break;

		case 12: // PTR
		{
			out.setPtr(nameToDottedString(in.rdata));
		}
		break;

		case 10: // NULL
		{
			out.setNull(in.rdata);
		}
		break;

		case 16: // TXT
		{
			QList<QByteArray> txtEntries = QDnsSd::parseTxtRecord(in.rdata);
			if(txtEntries.isEmpty())
				return out;
			out.setTxt(txtEntries);
		}
		break;

		default: // unsupported
		{
			return out;
		}
	}

	out.setOwner(in.name);
	out.setTtl(in.ttl);
	return out;
}

namespace {

class QDnsSdDelegate
{
public:
	virtual ~QDnsSdDelegate()
	{
	}

	virtual void dns_queryResult(int id, const QDnsSd::QueryResult &result)
	{
		Q_UNUSED(id);
		Q_UNUSED(result);
	}

	virtual void dns_browseResult(int id, const QDnsSd::BrowseResult &result)
	{
		Q_UNUSED(id);
		Q_UNUSED(result);
	}

	virtual void dns_resolveResult(int id, const QDnsSd::ResolveResult &result)
	{
		Q_UNUSED(id);
		Q_UNUSED(result);
	}

	virtual void dns_regResult(int id, const QDnsSd::RegResult &result)
	{
		Q_UNUSED(id);
		Q_UNUSED(result);
	}
};

class IdManager
{
private:
	QSet<int> set;
	int at;

	inline void bump_at()
	{
		if(at == 0x7fffffff)
			at = 0;
		else
			++at;
	}

public:
	IdManager() :
		at(0)
	{
	}

	int reserveId()
	{
		while(1)
		{
			if(!set.contains(at))
			{
				int id = at;
				set.insert(id);
				bump_at();
				return id;
			}

			bump_at();
		}
	}

	void releaseId(int id)
	{
		set.remove(id);
	}
};

}

//----------------------------------------------------------------------------
// AppleProvider
//----------------------------------------------------------------------------
class AppleProvider : public XMPP::IrisNetProvider
{
	Q_OBJECT
	Q_INTERFACES(XMPP::IrisNetProvider);
public:
	QDnsSd dns;
	QHash<int,QDnsSdDelegate*> delegateById;

	AppleProvider() :
		dns(this)
	{
		connect(&dns, SIGNAL(queryResult(int, const QDnsSd::QueryResult &)), SLOT(dns_queryResult(int, const QDnsSd::QueryResult &)));
		connect(&dns, SIGNAL(browseResult(int, const QDnsSd::BrowseResult &)), SLOT(dns_browseResult(int, const QDnsSd::BrowseResult &)));
		connect(&dns, SIGNAL(resolveResult(int, const QDnsSd::ResolveResult &)), SLOT(dns_resolveResult(int, const QDnsSd::ResolveResult &)));
		connect(&dns, SIGNAL(regResult(int, const QDnsSd::RegResult &)), SLOT(dns_regResult(int, const QDnsSd::RegResult &)));
	}

	virtual XMPP::NameProvider *createNameProviderInternet();
	virtual XMPP::NameProvider *createNameProviderLocal();
	virtual XMPP::ServiceProvider *createServiceProvider();

	int query(QDnsSdDelegate *p, const QByteArray &name, int qType)
	{
		int id = dns.query(name, qType);
		delegateById[id] = p;
		return id;
	}

	int browse(QDnsSdDelegate *p, const QByteArray &serviceType, const QByteArray &domain)
	{
		int id = dns.browse(serviceType, domain);
		delegateById[id] = p;
		return id;
	}

	int resolve(QDnsSdDelegate *p, const QByteArray &serviceName, const QByteArray &serviceType, const QByteArray &domain)
	{
		int id = dns.resolve(serviceName, serviceType, domain);
		delegateById[id] = p;
		return id;
	}

	int reg(QDnsSdDelegate *p, const QByteArray &serviceName, const QByteArray &serviceType, const QByteArray &domain, int port, const QByteArray &txtRecord)
	{
		int id = dns.reg(serviceName, serviceType, domain, port, txtRecord);
		delegateById[id] = p;
		return id;
	}

	void stop(int id)
	{
		delegateById.remove(id);
		dns.stop(id);
	}

	void stop_all(QDnsSdDelegate *p)
	{
		QList<int> ids;
		QHashIterator<int,QDnsSdDelegate*> it(delegateById);
		while(it.hasNext())
		{
			it.next();
			if(it.value() == p)
				ids += it.key();
		}
		foreach(int id, ids)
			stop(id);
	}

private slots:
	void dns_queryResult(int id, const QDnsSd::QueryResult &result)
	{
		delegateById[id]->dns_queryResult(id, result);
	}

	void dns_browseResult(int id, const QDnsSd::BrowseResult &result)
	{
		delegateById[id]->dns_browseResult(id, result);
	}

	void dns_resolveResult(int id, const QDnsSd::ResolveResult &result)
	{
		delegateById[id]->dns_resolveResult(id, result);
	}

	void dns_regResult(int id, const QDnsSd::RegResult &result)
	{
		delegateById[id]->dns_regResult(id, result);
	}
};

//----------------------------------------------------------------------------
// AppleBrowseSession
//----------------------------------------------------------------------------
// only use this class for a single browse.  if you want to browse again,
//   create a new object.
class AppleBrowse : public QObject, public QDnsSdDelegate
{
	Q_OBJECT
public:
	AppleProvider *global;
	int browse_id;
	QList<XMPP::ServiceInstance> instances;
	QHash<int,QByteArray> pendingByQueryId; // waiting for TXT

	AppleBrowse(AppleProvider *_global, QObject *parent = 0) :
		QObject(parent),
		global(_global),
		browse_id(-1)
	{
		connect(this, SIGNAL(unavailable_p(const XMPP::ServiceInstance &)), SIGNAL(unavailable(const XMPP::ServiceInstance &)));
	}

	~AppleBrowse()
	{
		global->stop_all(this);
	}

	void browse(const QString &type, const QString &domain)
	{
		browse_id = global->browse(this, type.toUtf8(), domain.toUtf8());
	}

signals:
	void available(const XMPP::ServiceInstance &instance);
	void unavailable(const XMPP::ServiceInstance &instance);
	void error();

	// emit delayed
	void unavailable_p(const XMPP::ServiceInstance &instance);

protected:
	virtual void dns_browseResult(int id, const QDnsSd::BrowseResult &result)
	{
		Q_UNUSED(id);

		if(!result.success)
		{
			emit error();
			return;
		}

		foreach(const QDnsSd::BrowseEntry &e, result.entries)
		{
			XMPP::ServiceInstance si(e.serviceName, e.serviceType, e.replyDomain, QMap<QString,QByteArray>());

			if(e.added)
			{
				int query_id = global->query(this, si.name(), 16); // 16 == TXT
				pendingByQueryId[query_id] = si.name();
			}
			else // removed
			{
				// emit these queued for SS.  no worry of SR since
				//   the browse operation is not cancellable.
				for(int n = 0; n < instances.count(); ++n)
				{
					const XMPP::ServiceInstance &i = instances[n];
					if(i.name() == si.name())
					{
						emit unavailable_p(i);
						instances.removeAt(n);
						--n; // adjust position
					}
				}
			}
		}
	}

	virtual void dns_queryResult(int id, const QDnsSd::QueryResult &result)
	{
		if(!result.success)
		{
			// if we get here, then it means we received a browse
			//   entry, but could not fetch its TXT record.  if
			//   that happens, cancel the query and drop the
			//   browse entry.
			global->stop(id);
			pendingByQueryId.remove(id);
			return;
		}

		// qdnssd guarantees at least one answer
		Q_ASSERT(!result.records.isEmpty());

		// only the first entry matters, and it must be an added TXT
		if(!result.records[0].added || result.records[0].rrtype != 16)
			return;

		// we only care about one answer
		QByteArray name = pendingByQueryId[id];
		QList<QByteArray> parts = nameToInstanceParts(name);
		if(parts.isEmpty())
		{
			// TODO: error
			Q_ASSERT(0);
		}

		global->stop(id);
		pendingByQueryId.remove(id);

		XMPP::NameRecord rec = importQDnsSdRecord(result.records[0]);

		// bad answer?
		if(rec.isNull())
			return;

		QMap<QString,QByteArray> attribs = textsToAttribs(rec.texts());
		// FIXME: conversion/escaping?
		XMPP::ServiceInstance si(QString::fromUtf8(parts[0]), QString::fromUtf8(parts[1]), QString::fromUtf8(parts[2]), attribs);

		// does qdnssd guarantee we won't receive dups?
		bool found = false;
		foreach(const XMPP::ServiceInstance &i, instances)
		{
			if(i.name() == si.name())
			{
				found = true;
				break;
			}
		}
		Q_ASSERT(!found);

		instances += si;
		emit available(si);
	}
};

//----------------------------------------------------------------------------
// AppleBrowseLookup
//----------------------------------------------------------------------------
// only use this class for a single lookup.  if you want to lookup again,
//   create a new object.
class AppleBrowseLookup : public QObject, public QDnsSdDelegate
{
	Q_OBJECT
public:
	AppleProvider *global;
	int resolve_id;
	XMPP::NameResolver nameResolverAaaa;
	XMPP::NameResolver nameResolverA;
	bool activeAaaa;
	bool activeA;
	QTimer waitTimer;
	QByteArray host;
	QHostAddress addr4;
	QHostAddress addr6;
	int port;

	AppleBrowseLookup(AppleProvider *_global, QObject *parent = 0) :
		QObject(parent),
		global(_global),
		resolve_id(-1),
		nameResolverAaaa(this),
		nameResolverA(this),
		activeAaaa(false),
		activeA(false),
		waitTimer(this)
	{
		connect(&nameResolverAaaa, SIGNAL(resultsReady(const QList<XMPP::NameRecord> &)),
			SLOT(nameAaaa_resultsReady(const QList<XMPP::NameRecord> &)));
		connect(&nameResolverAaaa, SIGNAL(error(XMPP::NameResolver::Error)),
			SLOT(nameAaaa_error(XMPP::NameResolver::Error)));

		connect(&nameResolverA, SIGNAL(resultsReady(const QList<XMPP::NameRecord> &)),
			SLOT(nameA_resultsReady(const QList<XMPP::NameRecord> &)));
		connect(&nameResolverA, SIGNAL(error(XMPP::NameResolver::Error)),
			SLOT(nameA_error(XMPP::NameResolver::Error)));

		connect(&waitTimer, SIGNAL(timeout()), SLOT(waitTimer_timeout()));
		waitTimer.setSingleShot(true);
	}

	~AppleBrowseLookup()
	{
		global->stop_all(this);
	}

	void resolve(const QByteArray &instance, const QByteArray &type, const QByteArray &domain)
	{
		resolve_id = global->resolve(this, instance, type, domain);
	}

signals:
	// emits at least 1 and at most 2
	void finished(const QList<QHostAddress> &addrs, int port);
	void error();

protected:
	void dns_resolveResult(int id, const QDnsSd::ResolveResult &result)
	{
		// there is only one response, so deregister
		global->stop(id);

		if(!result.success)
		{
			emit error();
			return;
		}

		host = result.hostTarget;
		port = result.port;

		activeAaaa = true;
		activeA = true;
		nameResolverAaaa.start(host, XMPP::NameRecord::Aaaa);
		nameResolverA.start(host, XMPP::NameRecord::A);
		waitTimer.start(500); // 500ms cut-off time, take what we have and run
	}

private slots:
	void nameAaaa_resultsReady(const QList<XMPP::NameRecord> &results)
	{
		// nameresolver guarantees at least one result, and we only
		//   care about the first
		addr6 = results[0].address();
		activeAaaa = false;
		tryDone();
	}

	void nameAaaa_error(XMPP::NameResolver::Error e)
	{
		Q_UNUSED(e);
		activeAaaa = false;
		tryDone();
	}

	void nameA_resultsReady(const QList<XMPP::NameRecord> &results)
	{
		// nameresolver guarantees at least one result, and we only
		//   care about the first
		addr4 = results[0].address();
		activeA = false;
		tryDone();
	}

	void nameA_error(XMPP::NameResolver::Error e)
	{
		Q_UNUSED(e);
		activeA = false;
		tryDone();
	}

	void waitTimer_timeout()
	{
		tryDone();
	}

private:
	void tryDone()
	{
		// we're done if both resolves are inactive and we have no
		//   results, or if the wait timer ends and we have at least
		//   one result

		if(!activeAaaa && !activeA && addr6.isNull() && addr4.isNull())
		{
			nameResolverAaaa.stop();
			nameResolverA.stop();
			waitTimer.stop();

			emit error();
			return;
		}

		if(!waitTimer.isActive() && (!addr6.isNull() || !addr4.isNull()))
		{
			nameResolverAaaa.stop();
			nameResolverA.stop();

			QList<QHostAddress> out;
			if(!addr4.isNull())
				out += addr4;
			if(!addr6.isNull())
				out += addr6;
			emit finished(out, port);
		}
	}
};

//----------------------------------------------------------------------------
// AppleNameProvider
//----------------------------------------------------------------------------
class AppleNameProvider : public XMPP::NameProvider, public QDnsSdDelegate
{
	Q_OBJECT
public:
	AppleProvider *global;

	AppleNameProvider(AppleProvider *parent) :
		NameProvider(parent),
		global(parent)
	{
	}

	~AppleNameProvider()
	{
		global->stop_all(this);
	}

	virtual bool supportsLongLived() const
	{
		return true;
	}

	virtual bool supportsRecordType(int type) const
	{
		// all record types supported
		Q_UNUSED(type);
		return true;
	}

	virtual int resolve_start(const QByteArray &name, int qType, bool longLived)
	{
		Q_UNUSED(longLived); // query is always long lived
		return global->query(this, name, qType);
	}

	virtual void resolve_stop(int id)
	{
		global->stop(id);
	}

protected:
	virtual void dns_queryResult(int id, const QDnsSd::QueryResult &result)
	{
		if(!result.success)
		{
			emit resolve_error(id, XMPP::NameResolver::ErrorGeneric);
			return;
		}

		QList<XMPP::NameRecord> results;
		foreach(const QDnsSd::Record &rec, result.records)
		{
			XMPP::NameRecord nr = importQDnsSdRecord(rec);

			// unsupported type
			if(nr.isNull())
				continue;

			// if removed, ensure ttl is 0
			if(!rec.added)
				nr.setTtl(0);

			results += nr;
		}

		emit resolve_resultsReady(id, results);
	}
};

//----------------------------------------------------------------------------
// AppleServiceProvider
//----------------------------------------------------------------------------
class AppleServiceProvider : public XMPP::ServiceProvider, public QDnsSdDelegate
{
	Q_OBJECT
public:
	class Browse
	{
	public:
		AppleServiceProvider *parent;
		int id;
		AppleBrowse *browse;

		Browse(AppleServiceProvider *_parent) :
			parent(_parent),
			id(-1),
			browse(0)
		{
		}

		~Browse()
		{
			delete browse;
			parent->idManager.releaseId(id);
		}
	};

	class Resolve
	{
	public:
		AppleServiceProvider *parent;
		int id;
		AppleBrowseLookup *resolve;

		Resolve(AppleServiceProvider *_parent) :
			parent(_parent),
			id(-1),
			resolve(0)
		{
		}

		~Resolve()
		{
			delete resolve;
			parent->idManager.releaseId(id);
		}
	};

	AppleProvider *global;
	QList<Browse*> browseList;
	QList<Resolve*> resolveList;
	IdManager idManager;

	AppleServiceProvider(AppleProvider *parent) :
		ServiceProvider(parent),
		global(parent)
	{
	}

	~AppleServiceProvider()
	{
		qDeleteAll(resolveList);
		qDeleteAll(browseList);
		global->stop_all(this);
	}

	int indexOfBrowseByBrowse(AppleBrowse *browse) const
	{
		for(int n = 0; n < browseList.count(); ++n)
		{
			if(browseList[n]->browse == browse)
				return n;
		}
		return -1;
	}

	int indexOfBrowseById(int id) const
	{
		for(int n = 0; n < browseList.count(); ++n)
		{
			if(browseList[n]->id == id)
				return n;
		}
		return -1;
	}

	int indexOfResolveByResolve(AppleBrowseLookup *resolve) const
	{
		for(int n = 0; n < resolveList.count(); ++n)
		{
			if(resolveList[n]->resolve == resolve)
				return n;
		}
		return -1;
	}

	int indexOfResolveById(int id) const
	{
		for(int n = 0; n < resolveList.count(); ++n)
		{
			if(resolveList[n]->id == id)
				return n;
		}
		return -1;
	}

	virtual int browse_start(const QString &type, const QString &domain)
	{
		Browse *b = new Browse(this);
		b->id = idManager.reserveId();
		b->browse = new AppleBrowse(global, this);
		connect(b->browse, SIGNAL(available(const XMPP::ServiceInstance &)), SLOT(browse_available(const XMPP::ServiceInstance &)));
		connect(b->browse, SIGNAL(unavailable(const XMPP::ServiceInstance &)), SLOT(browse_unavailable(const XMPP::ServiceInstance &)));
		connect(b->browse, SIGNAL(error()), SLOT(browse_error()));
		browseList += b;
		b->browse->browse(type, domain);
		return b->id;
	}

	virtual void browse_stop(int id)
	{
		int at = indexOfBrowseById(id);
		if(at == -1)
			return;

		Browse *b = browseList[at];
		browseList.removeAt(at);
		delete b;
	}

	virtual int resolve_start(const QByteArray &name)
	{
		QList<QByteArray> parts = nameToInstanceParts(name);
		if(parts.isEmpty())
		{
			// TODO: signal error rather than die
			Q_ASSERT(0);
		}

		Resolve *r = new Resolve(this);
		r->id = idManager.reserveId();
		r->resolve = new AppleBrowseLookup(global, this);
		connect(r->resolve, SIGNAL(finished(const QList<QHostAddress> &)), SLOT(resolve_finished(const QList<QHostAddress> &)));
		connect(r->resolve, SIGNAL(error()), SLOT(resolve_error()));
		resolveList += r;
		r->resolve->resolve(parts[0], parts[1], parts[2]);
		return r->id;
	}

	virtual void resolve_stop(int id)
	{
		int at = indexOfResolveById(id);
		if(at == -1)
			return;

		Resolve *r = resolveList[at];
		resolveList.removeAt(at);
		delete r;
	}

	virtual int publish_start(const QString &instance, const QString &type, int port, const QMap<QString,QByteArray> &attributes)
	{
		QByteArray txtRecord = attribsToTxtRecord(attributes);
		if(txtRecord.isEmpty())
		{
			// TODO: signal error rather than die
			Q_ASSERT(0);
		}

		QString domain = "local";

		// FIXME: conversion/escaping is probably wrong?
		return global->reg(this, instance.toUtf8(), type.toUtf8(), domain.toUtf8(), port, txtRecord);
	}

	virtual void publish_update(int id, const QMap<QString,QByteArray> &attributes)
	{
		// TODO: verify 'id' is valid.  if not valid, then assert/return (don't do anything or signal error)

		QByteArray txtRecord = attribsToTxtRecord(attributes);
		if(txtRecord.isEmpty())
		{
			// TODO: signal error rather than die
			Q_ASSERT(0);
		}

		if(global->dns.recordUpdateTxt(id, txtRecord, 4500))
		{
			// FIXME: SR
			QMetaObject::invokeMethod(this, "publish_published", Qt::QueuedConnection, Q_ARG(int, id));
		}
		else
		{
			// TODO: unpublish

			// FIXME: register meta type, SR
			QMetaObject::invokeMethod(this, "publish_error", Qt::QueuedConnection,
				Q_ARG(int, id),
				Q_ARG(XMPP::ServiceLocalPublisher::Error, XMPP::ServiceLocalPublisher::ErrorGeneric));
		}
	}

	virtual void publish_stop(int id)
	{
		global->stop(id);
	}

	virtual int publish_extra_start(int pub_id, const XMPP::NameRecord &name)
	{
		// TODO
		Q_UNUSED(pub_id);
		Q_UNUSED(name);
		return 0;
	}

	virtual void publish_extra_update(int id, const XMPP::NameRecord &name)
	{
		// TODO
		Q_UNUSED(id);
		Q_UNUSED(name);
	}

	virtual void publish_extra_stop(int id)
	{
		// TODO
		Q_UNUSED(id);
	}

	// called by AppleProvider

	void dns_regResult(int id, const QDnsSd::RegResult &result)
	{
		// TODO
		Q_UNUSED(id);
		Q_UNUSED(result);
	}

private slots:
	void browse_available(const XMPP::ServiceInstance &instance)
	{
		int at = indexOfBrowseByBrowse((AppleBrowse *)sender());
		Q_ASSERT(at != -1);

		emit browse_instanceAvailable(browseList[at]->id, instance);
	}

	void browse_unavailable(const XMPP::ServiceInstance &instance)
	{
		int at = indexOfBrowseByBrowse((AppleBrowse *)sender());
		Q_ASSERT(at != -1);

		emit browse_instanceUnavailable(browseList[at]->id, instance);
	}

	void browse_error()
	{
		int at = indexOfBrowseByBrowse((AppleBrowse *)sender());
		Q_ASSERT(at != -1);

		Browse *b = browseList[at];
		browseList.removeAt(at);
		int id = b->id;
		delete b;

		// FIXME: this looks weird, we should probably rename our
		//   local function
		emit ServiceProvider::browse_error(id, XMPP::ServiceBrowser::ErrorGeneric);
	}

	void resolve_finished(const QList<QHostAddress> &addrs, int port)
	{
		int at = indexOfResolveByResolve((AppleBrowseLookup *)sender());
		Q_ASSERT(at != -1);

		Resolve *r = resolveList[at];
		resolveList.removeAt(at);
		int id = r->id;
		delete r;

		QList<ResolveResult> results;
		foreach(const QHostAddress &addr, addrs)
		{
			ResolveResult r;
			r.address = addr;
			r.port = port;
			results += r;
		}

		emit resolve_resultsReady(id, results);
	}

	void resolve_error()
	{
		int at = indexOfResolveByResolve((AppleBrowseLookup *)sender());
		Q_ASSERT(at != -1);

		Resolve *r = resolveList[at];
		resolveList.removeAt(at);
		int id = r->id;
		delete r;

		// FIXME: this looks weird, we should probably rename our
		//   local function
		emit ServiceProvider::resolve_error(id, XMPP::ServiceResolver::ErrorGeneric);
	}
};

// AppleProvider
XMPP::NameProvider *AppleProvider::createNameProviderInternet()
{
	return new AppleNameProvider(this);
}

XMPP::NameProvider *AppleProvider::createNameProviderLocal()
{
	return new AppleNameProvider(this);
}

XMPP::ServiceProvider *AppleProvider::createServiceProvider()
{
	return new AppleServiceProvider(this);
}

#ifdef APPLEDNS_STATIC
XMPP::IrisNetProvider *irisnet_createAppleProvider()
{
        return new AppleProvider;
}
#else
Q_EXPORT_PLUGIN2(appledns, AppleProvider)
#endif

#include "appledns.moc"
