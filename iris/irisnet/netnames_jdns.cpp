/*
 * Copyright (C) 2005  Justin Karneges
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

#include "irisnetplugin.h"

#include "jdnsshared.h"
#include "netinterface.h"

namespace XMPP {

NameRecord importJDNSRecord(const QJDns::Record &in)
{
	NameRecord out;
	switch(in.type)
	{
		case QJDns::A:     out.setAddress(in.address); break;
		case QJDns::Aaaa:  out.setAddress(in.address); break;
		case QJDns::Mx:    out.setMx(in.name, in.priority); break;
		case QJDns::Srv:   out.setSrv(in.name, in.port, in.priority, in.weight); break;
		case QJDns::Cname: out.setCname(in.name); break;
		case QJDns::Ptr:   out.setPtr(in.name); break;
		case QJDns::Txt:   out.setTxt(in.texts); break;
		case QJDns::Hinfo: out.setHinfo(in.cpu, in.os); break;
		case QJDns::Ns:    out.setNs(in.name); break;
		case 10:           out.setNull(in.rdata); break;
		default:
			return out;
	}
	out.setOwner(in.owner);
	out.setTTL(in.ttl);
	return out;
}

QJDns::Record exportJDNSRecord(const NameRecord &in)
{
	QJDns::Record out;
	switch(in.type())
	{
		case NameRecord::A:
			out.type = QJDns::A;
			out.haveKnown = true;
			out.address = in.address();
			break;
		case NameRecord::Aaaa:
			out.type = QJDns::Aaaa;
			out.haveKnown = true;
			out.address = in.address();
			break;
		case NameRecord::Mx:
			out.type = QJDns::Mx;
			out.haveKnown = true;
			out.name = in.name();
			out.priority = in.priority();
			break;
		case NameRecord::Srv:
			out.type = QJDns::Srv;
			out.haveKnown = true;
			out.name = in.name();
			out.port = in.port();
			out.priority = in.priority();
			out.weight = in.weight();
			break;
		case NameRecord::Cname:
			out.type = QJDns::Cname;
			out.haveKnown = true;
			out.name = in.name();
			break;
		case NameRecord::Ptr:
			out.type = QJDns::Ptr;
			out.haveKnown = true;
			out.name = in.name();
			break;
		case NameRecord::Txt:
			out.type = QJDns::Txt;
			out.haveKnown = true;
			out.texts = in.texts();
			break;
		case NameRecord::Hinfo:
			out.type = QJDns::Hinfo;
			out.haveKnown = true;
			out.cpu = in.cpu();
			out.os = in.os();
			break;
		case NameRecord::Ns:
			out.type = QJDns::Ns;
			out.haveKnown = true;
			out.name = in.name();
			break;
		case NameRecord::Null:
			out.type = 10;
			out.rdata = in.rawData();
			break;
		default:
			return out;
	}
	out.owner = in.owner();
	out.ttl = in.ttl();
	return out;
}

//----------------------------------------------------------------------------
// JDnsGlobal
//----------------------------------------------------------------------------
class JDnsGlobal : public QObject
{
	Q_OBJECT
public:
	JDnsSharedDebug db;
	JDnsShared *uni_net, *uni_local, *mul;
	QHostAddress mul_addr4, mul_addr6;

	JDnsGlobal()
	{
		uni_net = 0;
		uni_local = 0;
		mul = 0;

		connect(&db, SIGNAL(readyRead()), SLOT(jdns_debugReady()));
	}

	~JDnsGlobal()
	{
		QList<JDnsShared*> list;
		if(uni_net)
			list += uni_net;
		if(uni_local)
			list += uni_local;
		if(mul)
			list += mul;

		// calls shutdown on the list, waits for shutdownFinished, deletes
		JDnsShared::waitForShutdown(list);

		// get final debug
		jdns_debugReady();
	}

	JDnsShared *ensure_uni_net()
	{
		if(!uni_net)
		{
			uni_net = new JDnsShared(JDnsShared::UnicastInternet, this);
			uni_net->setDebug(&db, "U");
			bool ok4 = uni_net->addInterface(QHostAddress::Any);
			bool ok6 = uni_net->addInterface(QHostAddress::AnyIPv6);
			if(!ok4 && !ok6)
			{
				delete uni_net;
				uni_net = 0;
			}
		}
		return uni_net;
	}

	JDnsShared *ensure_uni_local()
	{
		if(!uni_local)
		{
			uni_local = new JDnsShared(JDnsShared::UnicastLocal, this);
			uni_local->setDebug(&db, "L");
			bool ok4 = uni_local->addInterface(QHostAddress::Any);
			bool ok6 = uni_local->addInterface(QHostAddress::AnyIPv6);
			if(!ok4 && !ok6)
			{
				delete uni_local;
				uni_local = 0;
			}
		}
		return uni_local;
	}

	JDnsShared *ensure_mul()
	{
		if(!mul)
		{
			mul = new JDnsShared(JDnsShared::Multicast, this);
			mul->setDebug(&db, "M");

			updateMulticastInterfaces();
		}
		return mul;
	}

signals:
	void debug(const QStringList &lines);

public slots:
	// TODO: call this when the network changes
	void updateMulticastInterfaces()
	{
		QHostAddress addr4 = QJDns::detectPrimaryMulticast(QHostAddress::Any);
		QHostAddress addr6 = QJDns::detectPrimaryMulticast(QHostAddress::AnyIPv6);

		if(!(addr4 == mul_addr4))
		{
			if(!mul_addr4.isNull())
				mul->removeInterface(mul_addr4);
			mul_addr4 = addr4;
			if(!mul_addr4.isNull())
			{
				if(!mul->addInterface(mul_addr4))
					mul_addr4 = QHostAddress();
			}
		}

		if(!(addr6 == mul_addr6))
		{
			if(!mul_addr6.isNull())
				mul->removeInterface(mul_addr6);
			mul_addr6 = addr6;
			if(!mul_addr6.isNull())
			{
				if(!mul->addInterface(mul_addr6))
					mul_addr6 = QHostAddress();
			}
		}
	}

private slots:
	void jdns_debugReady()
	{
		QStringList lines = db.readDebugLines();
		Q_UNUSED(lines);
		//for(int n = 0; n < lines.count(); ++n)
		//	printf("jdns: %s\n", qPrintable(lines[n]));
		//emit debug(lines);
	}
};

//----------------------------------------------------------------------------
// JDnsNameProvider
//----------------------------------------------------------------------------
static int next_id = 1;
class JDnsNameProvider : public NameProvider
{
	Q_OBJECT
	Q_INTERFACES(XMPP::NameProvider);
public:
	enum Mode { Internet, Local };

	JDnsGlobal *global;

	Mode mode;
	class Item
	{
	public:
		JDnsSharedRequest *req;
		QByteArray name;
		int type;
		bool longLived;
		int id;

		Item()
		{
			req = 0;
		}

		~Item()
		{
			delete req;
		}
	};
	QList<Item*> items;

	static JDnsNameProvider *create(JDnsGlobal *global, Mode mode, QObject *parent = 0)
	{
		if(mode == Internet)
		{
			if(!global->ensure_uni_net())
				return 0;
		}
		else
		{
			if(!global->ensure_uni_local())
				return 0;
		}

		return new JDnsNameProvider(global, mode, parent);
	}

	JDnsNameProvider(JDnsGlobal *_global, Mode _mode, QObject *parent = 0) : NameProvider(parent)
	{
		global = _global;
		mode = _mode;
	}

	~JDnsNameProvider()
	{
	}

	virtual int resolve_start(const QByteArray &name, int qType, bool longLived)
	{
		if(mode == Internet)
		{
			if(name.right(6) == ".local" || name.right(7) == ".local.")
			{
				Item *i = new Item;
				i->id = next_id++;
				i->name = name;
				i->longLived = longLived;
				items += i;
				QMetaObject::invokeMethod(this, "do_local", Qt::QueuedConnection, Q_ARG(int, i->id));
				return i->id;
			}

			if(longLived)
			{
				Item *i = new Item;
				i->id = next_id++;
				items += i;
				QMetaObject::invokeMethod(this, "do_error", Qt::QueuedConnection, Q_ARG(int, i->id));
				return i->id;
			}

			Item *i = new Item;
			i->req = new JDnsSharedRequest(global->uni_net);
			connect(i->req, SIGNAL(resultsReady()), SLOT(req_resultsReady()));
			i->longLived = false;
			i->id = next_id++;
			items += i;
			i->req->query(name, qType);
			return i->id;
		}
		else
		{
			Item *i = new Item;
			if(longLived)
			{
				if(!global->ensure_mul())
				{
					Item *i = new Item;
					i->id = next_id++;
					items += i;
					QMetaObject::invokeMethod(this, "do_nolocal", Qt::QueuedConnection, Q_ARG(int, i->id));
					return i->id;
				}

				i->req = new JDnsSharedRequest(global->mul);
				i->longLived = true;
			}
			else
			{
				i->req = new JDnsSharedRequest(global->uni_local);
				i->longLived = false;
			}
			connect(i->req, SIGNAL(resultsReady()), SLOT(req_resultsReady()));
			i->id = next_id++;
			items += i;
			i->req->query(name, qType);
			return i->id;
		}
	}

	virtual void resolve_stop(int id)
	{
		Item *i = 0;
		for(int n = 0; n < items.count(); ++n)
		{
			if(items[n]->id == id)
			{
				i = items[n];
				break;
			}
		}
		if(!i)
			return;

		i->req->cancel();

		items.removeAll(i);
		delete i;
	}

	virtual void resolve_localResultsReady(int id, const QList<XMPP::NameRecord> &results)
	{
		Item *i = 0;
		for(int n = 0; n < items.count(); ++n)
		{
			if(items[n]->id == id)
			{
				i = items[n];
				break;
			}
		}
		if(!i)
			return;

		// not long-lived, so delete it (long-lived doesn't get looped through here)
		items.removeAll(i);
		delete i;

		QMetaObject::invokeMethod(this, "resolve_resultsReady", Qt::QueuedConnection,
			Q_ARG(int, id), Q_ARG(QList<XMPP::NameRecord>, results));
	}

	virtual void resolve_localError(int id, XMPP::NameResolver::Error e)
	{
		Item *i = 0;
		for(int n = 0; n < items.count(); ++n)
		{
			if(items[n]->id == id)
			{
				i = items[n];
				break;
			}
		}
		if(!i)
			return;

		items.removeAll(i);
		delete i;

		QMetaObject::invokeMethod(this, "resolve_error", Qt::QueuedConnection,
			Q_ARG(int, id), Q_ARG(XMPP::NameResolver::Error, e));
	}

private slots:
	void req_resultsReady()
	{
		JDnsSharedRequest *req = (JDnsSharedRequest *)sender();

		Item *i = 0;
		for(int n = 0; n < items.count(); ++n)
		{
			if(items[n]->req == req)
			{
				i = items[n];
				break;
			}
		}
		if(!i)
			return;

		int id = i->id;

		if(req->success())
		{
			QList<NameRecord> out;
			QList<QJDns::Record> results = req->results();
			for(int n = 0; n < results.count(); ++n)
				out += importJDNSRecord(results[n]);
			if(!i->longLived)
			{
				items.removeAll(i);
				delete i;
			}
			emit resolve_resultsReady(id, out);
		}
		else
		{
			JDnsSharedRequest::Error e = req->error();
			items.removeAll(i);
			delete i;

			NameResolver::Error error = NameResolver::ErrorGeneric;
			if(e == JDnsSharedRequest::ErrorNXDomain)
				error = NameResolver::ErrorNoName;
			else if(e == JDnsSharedRequest::ErrorTimeout)
				error = NameResolver::ErrorTimeout;
			else // ErrorGeneric or ErrorNoNet
				error = NameResolver::ErrorGeneric;
			emit resolve_error(id, error);
		}
	}

	void do_local(int id)
	{
		Item *i = 0;
		for(int n = 0; n < items.count(); ++n)
		{
			if(items[n]->id == id)
			{
				i = items[n];
				break;
			}
		}
		if(!i)
			return;

		QByteArray name = i->name;
		if(i->longLived) // longlived is a handoff, so delete our instance
		{
			items.removeAll(i);
			delete i;
		}

		emit resolve_useLocal(id, name);
	}

	void do_error(int id)
	{
		Item *i = 0;
		for(int n = 0; n < items.count(); ++n)
		{
			if(items[n]->id == id)
			{
				i = items[n];
				break;
			}
		}
		if(!i)
			return;

		items.removeAll(i);
		delete i;

		emit resolve_error(id, NameResolver::ErrorNoLongLived);
	}

	void do_nolocal(int id)
	{
		Item *i = 0;
		for(int n = 0; n < items.count(); ++n)
		{
			if(items[n]->id == id)
			{
				i = items[n];
				break;
			}
		}
		if(!i)
			return;

		items.removeAll(i);
		delete i;

		emit resolve_error(id, NameResolver::ErrorNoLocal);
	}
};

//----------------------------------------------------------------------------
// JDnsServiceProvider
//----------------------------------------------------------------------------
class JDnsBrowseLookup : public QObject
{
	Q_OBJECT
public:
	JDnsShared *jdns;
	JDnsSharedRequest *req;

	bool success; // TODO: use this variable
	bool mode2;
	QByteArray name;
	QByteArray instance;
	bool haveSrv;
	QByteArray srvhost;
	int srvport;
	QList<QByteArray> attribs;
	QHostAddress addr;

	JDnsBrowseLookup(JDnsShared *_jdns)
	{
		req = 0;
		jdns = _jdns;
	}

	~JDnsBrowseLookup()
	{
		delete req;
	}

	void start(const QByteArray &_name)
	{
		success = false;
		mode2 = false;
		name = _name;
		haveSrv = false;

		req = new JDnsSharedRequest(jdns);
		connect(req, SIGNAL(resultsReady()), SLOT(jdns_resultsReady()));
		req->query(name, QJDns::Srv);
	}

	void start2(const QByteArray &_name)
	{
		success = false;
		mode2 = true;
		name = _name;
		haveSrv = false;

		req = new JDnsSharedRequest(jdns);
		connect(req, SIGNAL(resultsReady()), SLOT(jdns_resultsReady()));
		req->query(name, QJDns::Srv);
	}

signals:
	void finished();

private slots:
	void jdns_resultsReady()
	{
		if(!haveSrv)
		{
			QJDns::Record rec = req->results().first();

			haveSrv = true;
			srvhost = rec.name;
			srvport = rec.port;

			//printf("  Server: [%s] port=%d\n", srvhost.data(), srvport);
			req->cancel();

			if(mode2)
				req->query(srvhost, QJDns::A); // TODO: ipv6?
			else
				req->query(name, QJDns::Txt);
		}
		else
		{
			if(mode2)
			{
				QJDns::Record rec = req->results().first();

				addr = rec.address;

				delete req;
				req = 0;

				//printf("resolve done\n");
				emit finished();
				return;
			}

			QJDns::Record rec = req->results().first();

			attribs.clear();
			if(!rec.texts.isEmpty())
			{
				if(rec.texts.count() != 1 || !rec.texts[0].isEmpty())
					attribs = rec.texts;
			}

			/*if(attribs.isEmpty())
			{
				printf("  No attributes\n");
			}
			else
			{
				printf("  Attributes:\n", attribs.count());
				for(int n = 0; n < attribs.count(); ++n)
					printf("    [%s]\n", attribs[n].data());
			}*/

			delete req;
			req = 0;

			//printf("Instance Available!\n");
			emit finished();
		}
	}
};

class JDnsBrowseInfo
{
public:
	QByteArray name;
	QByteArray instance;
	QByteArray srvhost;
	int srvport;
	QList<QByteArray> attribs;
};

class JDnsBrowse : public QObject
{
	Q_OBJECT
public:
	int id;

	JDnsShared *jdns;
	JDnsSharedRequest *req;
	QByteArray type;
	QList<JDnsBrowseLookup*> lookups;

	JDnsBrowse(JDnsShared *_jdns)
	{
		req = 0;
		jdns = _jdns;
	}

	~JDnsBrowse()
	{
		qDeleteAll(lookups);
		delete req;
	}

	void start(const QByteArray &_type)
	{
		type = _type;
		req = new JDnsSharedRequest(jdns);
		connect(req, SIGNAL(resultsReady()), SLOT(jdns_resultsReady()));
		req->query(type + ".local.", QJDns::Ptr);
	}

signals:
	void available(const JDnsBrowseInfo &i);
	void unavailable(const QByteArray &instance);

private slots:
	void jdns_resultsReady()
	{
		QJDns::Record rec = req->results().first();
		QByteArray name = rec.name;

		// FIXME: this is wrong, it should search backwards
		int x = name.indexOf('.');
		QByteArray instance = name.mid(0, x);

		if(rec.ttl == 0)
		{
			// stop any lookups
			JDnsBrowseLookup *bl = 0;
			for(int n = 0; n < lookups.count(); ++n)
			{
				if(lookups[n]->name == name)
				{
					bl = lookups[n];
					break;
				}
			}
			if(bl)
			{
				lookups.removeAll(bl);
				delete bl;
			}

			//printf("Instance Gone: [%s]\n", instance.data());
			emit unavailable(instance);
			return;
		}

		//printf("Instance Found: [%s]\n", instance.data());

		//printf("Lookup starting\n");
		JDnsBrowseLookup *bl = new JDnsBrowseLookup(jdns);
		connect(bl, SIGNAL(resultsReady()), SLOT(bl_resultsReady()));
		lookups += bl;
		bl->instance = instance;
		bl->start(name);
	}

	void bl_resultsReady()
	{
		JDnsBrowseLookup *bl = (JDnsBrowseLookup *)sender();

		JDnsBrowseInfo i;
		i.name = bl->name;
		i.instance = bl->instance;
		i.srvhost = bl->srvhost;
		i.srvport = bl->srvport;
		i.attribs = bl->attribs;

		lookups.removeAll(bl);
		delete bl;

		//printf("Lookup finished\n");
		emit available(i);
	}
};

/*void JDnsShared::net_available(const QString &id)
{
	NetInterface *iface = new NetInterface(id);
	connect(iface, SIGNAL(unavailable()), SLOT(net_unavailable()));
	ifaces += iface;

	QList<QHostAddress> addrlist = iface->addresses();

	if(!instances.isEmpty())
		return;

	// prefer using just ipv4
	QHostAddress addr;
	for(int n = 0; n < addrlist.count(); ++n)
	{
		if(addrlist[n].protocol() == QAbstractSocket::IPv4Protocol)
		{
			addr = addrlist[n];
			break;
		}
	}

	if(addr.isNull())
		return;

	addr = QHostAddress("192.168.1.150");
}

void JDnsShared::net_unavailable()
{
	// TODO
}*/

class JDnsServiceProvider : public ServiceProvider
{
	Q_OBJECT
public:
	JDnsGlobal *global;

	QList<JDnsBrowse*> list;
	QHash<QByteArray,ServiceInstance> items;

	QList<JDnsSharedRequest*> pubitems;
	QByteArray _servname;

	static JDnsServiceProvider *create(JDnsGlobal *global, QObject *parent = 0)
	{
		JDnsServiceProvider *p = new JDnsServiceProvider(global, parent);
		return p;
	}

	JDnsServiceProvider(JDnsGlobal *_global, QObject *parent = 0) : ServiceProvider(parent)
	{
		global = _global;
	}

	~JDnsServiceProvider()
	{
		qDeleteAll(pubitems);
	}

	virtual int browse_start(const QString &type, const QString &domain)
	{
		// no support for non-local domains
		if(!domain.isEmpty() && (domain != ".local." && domain != ".local" && domain != "."))
		{
			// TODO
		}

		if(!global->ensure_mul())
		{
			// TODO
		}

		JDnsBrowse *b = new JDnsBrowse(global->mul);
		connect(b, SIGNAL(available(const JDnsBrowseInfo &)), SLOT(jb_available(const JDnsBrowseInfo &)));
		connect(b, SIGNAL(unavailable(const QByteArray &)), SLOT(jb_unavailable(const QByteArray &)));
		b->start(type.toLatin1());

		return 1;
	}

	virtual void browse_stop(int id)
	{
		// TODO
		Q_UNUSED(id);
	}

	virtual int resolve_start(const QByteArray &name)
	{
		if(!global->ensure_mul())
		{
			// TODO
		}

		JDnsBrowseLookup *bl = new JDnsBrowseLookup(global->mul);
		connect(bl, SIGNAL(finished()), SLOT(bl_finished()));
		bl->start2(name);

		return 1;
	}

	virtual void resolve_stop(int id)
	{
		// TODO
		Q_UNUSED(id);
	}

	virtual int publish_start(const QString &instance, const QString &type, int port, const QMap<QString,QByteArray> &attributes)
	{
		if(!global->ensure_mul())
		{
			// TODO
		}

		QString me = QHostInfo::localHostName();
		//QHostInfo hi = QHostInfo::fromName(me);
		QByteArray melocal = me.toLatin1() + ".local.";
		QByteArray servname = instance.toLatin1() + '.' + type.toLatin1() + ".local.";

		JDnsSharedRequest *req = new JDnsSharedRequest(global->mul);
		QJDns::Record rec;
		rec.type = QJDns::A;
		rec.owner = melocal;
		rec.ttl = 120;
		rec.haveKnown = true;
		rec.address = QHostAddress(); // null address, will be filled in
		req->publish(QJDns::Unique, rec);
		pubitems += req;

		/*JDnsSharedRequest *req = new JDnsSharedRequest(global->mul);
		QJDns::Record rec;
		rec.type = QJDns::Aaaa;
		rec.owner = melocal;
		rec.ttl = 120;
		rec.haveKnown = true;
		rec.address = QHostAddress(); // null address, will be filled in
		req->publish(QJDns::Unique, rec);
		pubitems += req;*/

		req = new JDnsSharedRequest(global->mul);
		rec = QJDns::Record();
		rec.type = QJDns::Srv;
		rec.owner = servname;
		rec.ttl = 120;
		rec.haveKnown = true;
		rec.name = melocal;
		rec.port = port;
		rec.priority = 0;
		rec.weight = 0;
		req->publish(QJDns::Unique, rec);
		pubitems += req;

		req = new JDnsSharedRequest(global->mul);
		rec = QJDns::Record();
		rec.type = QJDns::Txt;
		rec.owner = servname;
		rec.ttl = 4500;
		rec.haveKnown = true;
		QMapIterator<QString,QByteArray> it(attributes);
		while(it.hasNext())
		{
			it.next();
			rec.texts += it.key().toLatin1() + '=' + it.value();
		}
		if(rec.texts.isEmpty())
			rec.texts += QByteArray();
		req->publish(QJDns::Unique, rec);
		pubitems += req;

		req = new JDnsSharedRequest(global->mul);
		rec = QJDns::Record();
		rec.type = QJDns::Ptr;
		rec.owner = type.toLatin1() + ".local.";
		rec.ttl = 4500;
		rec.haveKnown = true;
		rec.name = servname;
		req->publish(QJDns::Shared, rec);
		pubitems += req;

		_servname = servname;

		QMetaObject::invokeMethod(this, "publish_published", Qt::QueuedConnection, Q_ARG(int, 1));

		return 1;
	}

	virtual int publish_update(const QMap<QString,QByteArray> &attributes)
	{
		// TODO
		Q_UNUSED(attributes);
		return 0;
	}

	virtual void publish_cancel(int id)
	{
		// TODO
		Q_UNUSED(id);
	}

	virtual int publish_extra_start(int pub_id, const NameRecord &name)
	{
		// TODO
		Q_UNUSED(pub_id);

		JDnsSharedRequest *req = new JDnsSharedRequest(global->mul);
		QJDns::Record rec;
		rec.type = 10;
		rec.owner = _servname;
		rec.ttl = 4500;
		rec.rdata = name.rawData();
		req->publish(QJDns::Unique, rec);
		pubitems += req;

		QMetaObject::invokeMethod(this, "publish_extra_published", Qt::QueuedConnection, Q_ARG(int, 2));

		return 2;
	}

	virtual int publish_extra_update(int id, const NameRecord &name)
	{
		// TODO
		Q_UNUSED(id);
		Q_UNUSED(name);
		return 0;
	}

	virtual void publish_extra_cancel(int id)
	{
		// TODO
		Q_UNUSED(id);
	}

private slots:
	void jb_available(const JDnsBrowseInfo &i)
	{
		//printf("jb_available: [%s]\n", i.instance.data());
		JDnsBrowse *b = (JDnsBrowse *)sender();
		QMap<QString,QByteArray> map;
		for(int n = 0; n < i.attribs.count(); ++n)
		{
			const QByteArray &a = i.attribs[n];
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

			map.insert(key, value);
		}
		ServiceInstance si(QString::fromLatin1(i.instance), QString::fromLatin1(b->type), "local.", map);
		items.insert(i.name, si);
		emit browse_instanceAvailable(1, si);
	}

	void jb_unavailable(const QByteArray &instance)
	{
		//printf("jb_unavailable: [%s]\n", instance.data());
		JDnsBrowse *b = (JDnsBrowse *)sender();
		QByteArray name = instance + '.' + b->type + ".local.";
		if(!items.contains(name))
			return;

		ServiceInstance si = items.value(name);
		items.remove(name);
		emit browse_instanceUnavailable(1, si);
	}

	void bl_finished()
	{
		JDnsBrowseLookup *bl = (JDnsBrowseLookup *)sender();
		QHostAddress addr = bl->addr;
		int port = bl->srvport;
		delete bl;
		emit resolve_resultsReady(1, addr, port);
	}

		//connect(&jdns, SIGNAL(published(int)), SLOT(jdns_published(int)));

	/*virtual int publish_start(NameLocalPublisher::Mode pmode, const NameRecord &name)
	{
		if(mode == Unicast)
			return -1;

		QJDns::Response response;
		QJDns::Record record = exportJDNSRecord(name);
		response.answerRecords += record;
		QJDns::PublishMode m = (pmode == NameLocalPublisher::Unique ? QJDns::Unique : QJDns::Shared);
		return jdns.publishStart(m, record.owner, record.type, response);
	}

	virtual void publish_update(int id, const NameRecord &name)
	{
		QJDns::Response response;
		QJDns::Record record = exportJDNSRecord(name);
		response.answerRecords += record;
		return jdns.publishUpdate(id, response);
	}

	virtual void publish_stop(int id)
	{
		jdns.publishCancel(id);
	}*/

		//else if(e == QJDns::ErrorConflict)
		//	error = NameResolver::ErrorConflict;

		//if(mode == Multicast)
		//	jdns.queryCancel(id);
};

//----------------------------------------------------------------------------
// JDnsProvider
//----------------------------------------------------------------------------
class JDnsProvider : public IrisNetProvider
{
	Q_OBJECT
	Q_INTERFACES(XMPP::IrisNetProvider);
public:
	JDnsGlobal *global;

	JDnsProvider()
	{
		global = 0;
	}

	~JDnsProvider()
	{
		delete global;
	}

	virtual NameProvider *createNameProviderInternet()
	{
		if(!global)
			global = new JDnsGlobal;
		return JDnsNameProvider::create(global, JDnsNameProvider::Internet);
	}

	virtual NameProvider *createNameProviderLocal()
	{
		if(!global)
			global = new JDnsGlobal;
		return JDnsNameProvider::create(global, JDnsNameProvider::Local);
	}

	virtual ServiceProvider *createServiceProvider()
	{
		if(!global)
			global = new JDnsGlobal;
		return JDnsServiceProvider::create(global);
	}
};

IrisNetProvider *irisnet_createJDnsProvider()
{
        return new JDnsProvider;
}

}

#include "netnames_jdns.moc"
