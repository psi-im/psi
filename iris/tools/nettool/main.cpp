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

#include <stdio.h>
#include <iris/processquit.h>
#include <iris/netinterface.h>
#include <iris/netavailability.h>
#include <iris/netnames.h>

using namespace XMPP;

class NetMonitor : public QObject
{
	Q_OBJECT
public:
	NetInterfaceManager *man;
	QList<NetInterface*> ifaces;
	NetAvailability *netavail;

	~NetMonitor()
	{
		delete netavail;
		qDeleteAll(ifaces);
		delete man;
	}

signals:
	void quit();

public slots:
	void start()
	{
		connect(ProcessQuit::instance(), SIGNAL(quit()), SIGNAL(quit()));

		man = new NetInterfaceManager;
		connect(man, SIGNAL(interfaceAvailable(const QString &)),
			SLOT(here(const QString &)));
		QStringList list = man->interfaces();
		for(int n = 0; n < list.count(); ++n)
			here(list[n]);

		netavail = new NetAvailability;
		connect(netavail, SIGNAL(changed(bool)), SLOT(avail(bool)));
		avail(netavail->isAvailable());
	}

	void here(const QString &id)
	{
		NetInterface *iface = new NetInterface(id, man);
		connect(iface, SIGNAL(unavailable()), SLOT(gone()));
		printf("HERE: %s name=[%s]\n", qPrintable(iface->id()), qPrintable(iface->name()));
		QList<QHostAddress> addrs = iface->addresses();
		for(int n = 0; n < addrs.count(); ++n)
			printf("  address: %s\n", qPrintable(addrs[n].toString()));
		if(!iface->gateway().isNull())
			printf("  gateway: %s\n", qPrintable(iface->gateway().toString()));
		ifaces += iface;
	}

	void gone()
	{
		NetInterface *iface = (NetInterface *)sender();
		printf("GONE: %s\n", qPrintable(iface->id()));
		ifaces.removeAll(iface);
		delete iface;
	}

	void avail(bool available)
	{
		if(available)
			printf("** Network available\n");
		else
			printf("** Network unavailable\n");
	}
};

static QString dataToString(const QByteArray &buf)
{
	QString out;
	for(int n = 0; n < buf.size(); ++n)
	{
		unsigned char c = (unsigned char)buf[n];
		if(c == '\\')
			out += "\\\\";
		else if(c >= 0x20 || c < 0x7f)
			out += c;
		else
			out += QString("\\x%1").arg((uint)c, 2, 16);
	}
	return out;
}

static void print_record(const NameRecord &r)
{
	switch(r.type())
	{
		case NameRecord::A:
			printf("A: [%s] (ttl=%d)\n", qPrintable(r.address().toString()), r.ttl());
			break;
		case NameRecord::Aaaa:
			printf("AAAA: [%s] (ttl=%d)\n", qPrintable(r.address().toString()), r.ttl());
			break;
		case NameRecord::Mx:
			printf("MX: [%s] priority=%d (ttl=%d)\n", r.name().data(), r.priority(), r.ttl());
			break;
		case NameRecord::Srv:
			printf("SRV: [%s] port=%d priority=%d weight=%d (ttl=%d)\n", r.name().data(), r.port(), r.priority(), r.weight(), r.ttl());
			break;
		case NameRecord::Ptr:
			printf("PTR: [%s] (ttl=%d)\n", r.name().data(), r.ttl());
			break;
		case NameRecord::Txt:
		{
			QList<QByteArray> texts = r.texts();
			printf("TXT: count=%d (ttl=%d)\n", texts.count(), r.ttl());
			for(int n = 0; n < texts.count(); ++n)
				printf("  len=%d [%s]\n", texts[n].size(), qPrintable(dataToString(texts[n])));
			break;
		}
		case NameRecord::Hinfo:
			printf("HINFO: [%s] [%s] (ttl=%d)\n", r.cpu().data(), r.os().data(), r.ttl());
			break;
		case NameRecord::Null:
			printf("NULL: %d bytes (ttl=%d)\n", r.rawData().size(), r.ttl());
			break;
		default:
			printf("(Unknown): type=%d (ttl=%d)\n", r.type(), r.ttl());
			break;
	}
}

static int str2rtype(const QString &in)
{
	QString str = in.toLower();
	if(str == "a")
		return NameRecord::A;
	else if(str == "aaaa")
		return NameRecord::Aaaa;
	else if(str == "ptr")
		return NameRecord::Ptr;
	else if(str == "srv")
		return NameRecord::Srv;
	else if(str == "mx")
		return NameRecord::Mx;
	else if(str == "txt")
		return NameRecord::Txt;
	else if(str == "hinfo")
		return NameRecord::Hinfo;
	else if(str == "null")
		return NameRecord::Null;
	else
		return -1;
}

class ResolveName : public QObject
{
	Q_OBJECT
public:
	QString name;
	NameRecord::Type type;
	bool longlived;
	NameResolver dns;
	bool null_dump;

	ResolveName()
	{
		null_dump = false;
	}

public slots:
	void start()
	{
		connect(ProcessQuit::instance(), SIGNAL(quit()), SIGNAL(quit()));

		connect(&dns, SIGNAL(resultsReady(const QList<XMPP::NameRecord> &)),
			SLOT(dns_resultsReady(const QList<XMPP::NameRecord> &)));
		connect(&dns, SIGNAL(error(XMPP::NameResolver::Error)),
			SLOT(dns_error(XMPP::NameResolver::Error)));

		dns.start(name.toLatin1(), type, longlived ? NameResolver::LongLived : NameResolver::Single);
	}

signals:
	void quit();

private slots:
	void dns_resultsReady(const QList<XMPP::NameRecord> &list)
	{
		if(null_dump && list[0].type() == NameRecord::Null)
		{
			QByteArray buf = list[0].rawData();
			fwrite(buf.data(), buf.size(), 1, stdout);
		}
		else
		{
			for(int n = 0; n < list.count(); ++n)
				print_record(list[n]);
		}
		if(!longlived)
		{
			dns.stop();
			emit quit();
		}
	}

	void dns_error(XMPP::NameResolver::Error e)
	{
		QString str;
		if(e == NameResolver::ErrorNoName)
			str = "ErrorNoName";
		else if(e == NameResolver::ErrorTimeout)
			str = "ErrorTimeout";
		else if(e == NameResolver::ErrorNoLocal)
			str = "ErrorNoLocal";
		else if(e == NameResolver::ErrorNoLongLived)
			str = "ErrorNoLongLived";
		else // ErrorGeneric, or anything else
			str = "ErrorGeneric";

		printf("Error: %s\n", qPrintable(str));
		emit quit();
	}
};

class BrowseServices : public QObject
{
	Q_OBJECT
public:
	QString type, domain;
	ServiceBrowser browser;

public slots:
	void start()
	{
		connect(ProcessQuit::instance(), SIGNAL(quit()), SIGNAL(quit()));

		connect(&browser, SIGNAL(instanceAvailable(const XMPP::ServiceInstance &)),
			SLOT(browser_instanceAvailable(const XMPP::ServiceInstance &)));
		connect(&browser, SIGNAL(instanceUnavailable(const XMPP::ServiceInstance &)),
			SLOT(browser_instanceUnavailable(const XMPP::ServiceInstance &)));
		connect(&browser, SIGNAL(error()), SLOT(browser_error()));

		browser.start(type, domain);
	}

signals:
	void quit();

private slots:
	void browser_instanceAvailable(const XMPP::ServiceInstance &i)
	{
		printf("HERE: [%s] (%d attributes)\n", qPrintable(i.instance()), i.attributes().count());
		QMap<QString,QByteArray> attribs = i.attributes();
		QMapIterator<QString,QByteArray> it(attribs);
		while(it.hasNext())
		{
			it.next();
			printf("  [%s] = [%s]\n", qPrintable(it.key()), qPrintable(dataToString(it.value())));
		}
	}

	void browser_instanceUnavailable(const XMPP::ServiceInstance &i)
	{
		printf("GONE: [%s]\n", qPrintable(i.instance()));
	}

	void browser_error()
	{
	}
};

class ResolveService : public QObject
{
	Q_OBJECT
public:
	int mode;
	QString instance;
	QString type;
	QString domain;
	int port;

	ServiceResolver dns;

public slots:
	void start()
	{
		connect(ProcessQuit::instance(), SIGNAL(quit()), SIGNAL(quit()));

		connect(&dns, SIGNAL(resultsReady(const QHostAddress &, int)),
			SLOT(dns_resultsReady(const QHostAddress &, int)));
		connect(&dns, SIGNAL(finished()), SLOT(dns_finished()));
		connect(&dns, SIGNAL(error()), SLOT(dns_error()));

		if(mode == 0)
			dns.startFromInstance(instance.toLatin1() + '.' + type.toLatin1() + ".local.");
		else if(mode == 1)
			dns.startFromDomain(domain, type);
		else // 2
			dns.startFromPlain(domain, port);
	}

signals:
	void quit();

private slots:
	void dns_resultsReady(const QHostAddress &addr, int port)
	{
		printf("[%s] port=%d\n", qPrintable(addr.toString()), port);
		dns.tryNext();
	}

	void dns_finished()
	{
		emit quit();
	}

	void dns_error()
	{
	}
};

class PublishService : public QObject
{
	Q_OBJECT
public:
	QString instance;
	QString type;
	int port;
	QMap<QString,QByteArray> attribs;
	QByteArray extra_null;

	ServiceLocalPublisher pub;

public slots:
	void start()
	{
		//NetInterfaceManager::instance();

		connect(ProcessQuit::instance(), SIGNAL(quit()), SIGNAL(quit()));

		connect(&pub, SIGNAL(published()), SLOT(pub_published()));
		connect(&pub, SIGNAL(error(XMPP::ServiceLocalPublisher::Error)),
			SLOT(pub_error(XMPP::ServiceLocalPublisher::Error)));

		pub.publish(instance, type, port, attribs);
	}

signals:
	void quit();

private slots:
	void pub_published()
	{
		printf("Published\n");
		if(!extra_null.isEmpty())
		{
			NameRecord rec;
			rec.setNull(extra_null);
			pub.addRecord(rec);
		}
	}

	void pub_error(XMPP::ServiceLocalPublisher::Error e)
	{
		printf("Error: [%d]\n", e);
		emit quit();
	}
};

#include "main.moc"

void usage()
{
	printf("nettool: simple testing utility\n");
	printf("usage: nettool [command]\n");
	printf("\n");
	printf(" netmon                                          monitor network interfaces\n");
	printf(" rname (-r) [domain] (record type)               look up record (default = a)\n");
	printf(" rnamel [domain] [record type]                   look up record (long-lived)\n");
	printf(" browse [service type]                           browse for local services\n");
	printf(" rservi [instance] [service type]                look up browsed instance\n");
	printf(" rservd [domain] [service type]                  look up normal SRV\n");
	printf(" rservp [domain] [port]                          look up non-SRV\n");
	printf(" pserv  [inst] [type] [port] (attr) (-a [rec])   publish service instance\n");
	printf("\n");
	printf("record types: a aaaa ptr srv mx txt hinfo null\n");
	printf("service types: _service._proto format (e.g. \"_xmpp-client._tcp\")\n");
	printf("attributes: var0[=val0],...,varn[=valn]\n");
	printf("rname -r: for null type, dump raw record data to stdout\n");
	printf("pub -a: add extra record.  format: null:filename.dat\n");
	printf("\n");
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	if(argc < 2)
	{
		usage();
		return 1;
	}

	QStringList args;
	for(int n = 1; n < argc; ++n)
		args += argv[n];

	if(args[0] == "netmon")
	{
		NetMonitor a;
		QObject::connect(&a, SIGNAL(quit()), &app, SLOT(quit()));
		QTimer::singleShot(0, &a, SLOT(start()));
		app.exec();
	}
	else if(args[0] == "rname" || args[0] == "rnamel")
	{
		bool null_dump = false;
		for(int n = 1; n < args.count(); ++n)
		{
			if(args[n] == "-r")
			{
				null_dump = true;
				args.removeAt(n);
				--n;
			}
		}

		if(args.count() < 2)
		{
			usage();
			return 1;
		}
		if(args[0] == "rnamel" && args.count() < 3)
		{
			usage();
			return 1;
		}
		int x = NameRecord::A;
		if(args.count() >= 3)
		{
			x = str2rtype(args[2]);
			if(x == -1)
			{
				usage();
				return 1;
			}
		}
		ResolveName a;
		a.name = args[1];
		a.type = (NameRecord::Type)x;
		a.longlived = (args[0] == "rnamel") ? true : false;
		if(args[0] == "rname" && null_dump)
			a.null_dump = true;
		QObject::connect(&a, SIGNAL(quit()), &app, SLOT(quit()));
		QTimer::singleShot(0, &a, SLOT(start()));
		app.exec();
	}
	else if(args[0] == "browse")
	{
		if(args.count() < 2)
		{
			usage();
			return 1;
		}

		BrowseServices a;
		a.type = args[1];
		QObject::connect(&a, SIGNAL(quit()), &app, SLOT(quit()));
		QTimer::singleShot(0, &a, SLOT(start()));
		app.exec();
	}
	else if(args[0] == "rservi" || args[0] == "rservd" || args[0] == "rservp")
	{
		// they all take 2 params
		if(args.count() < 3)
		{
			usage();
			return 1;
		}

		ResolveService a;
		if(args[0] == "rservi")
		{
			a.mode = 0;
			a.instance = args[1];
			a.type = args[2];
		}
		else if(args[0] == "rservd")
		{
			a.mode = 1;
			a.domain = args[1];
			a.type = args[2];
		}
		else // rservp
		{
			a.mode = 2;
			a.domain = args[1];
			a.port = args[2].toInt();
		}
		QObject::connect(&a, SIGNAL(quit()), &app, SLOT(quit()));
		QTimer::singleShot(0, &a, SLOT(start()));
		app.exec();
	}
	else if(args[0] == "pserv")
	{
		QStringList addrecs;
		for(int n = 1; n < args.count(); ++n)
		{
			if(args[n] == "-a")
			{
				if(n + 1 < args.count())
				{
					addrecs += args[n + 1];
					args.removeAt(n);
					args.removeAt(n);
					--n;
				}
				else
				{
					usage();
					return 1;
				}
			}
		}

		QByteArray extra_null;
		for(int n = 0; n < addrecs.count(); ++n)
		{
			const QString &str = addrecs[n];
			int x = str.indexOf(':');
			if(x == -1 || str.mid(0, x) != "null")
			{
				usage();
				return 1;
			}

			QString null_file = str.mid(x + 1);

			if(!null_file.isEmpty())
			{
				QFile f(null_file);
				if(!f.open(QFile::ReadOnly))
				{
					printf("can't read file\n");
					return 1;
				}
				extra_null = f.readAll();
			}
		}

		if(args.count() < 4)
		{
			usage();
			return 1;
		}

		QMap<QString,QByteArray> attribs;
		if(args.count() > 4)
		{
			QStringList parts = args[4].split(',');
			for(int n = 0; n < parts.count(); ++n)
			{
				const QString &str = parts[n];
				int x = str.indexOf('=');
				if(x != -1)
					attribs.insert(str.mid(0, x), str.mid(x + 1).toUtf8());
				else
					attribs.insert(str, QByteArray());
			}
		}

		PublishService a;
		a.instance = args[1];
		a.type = args[2];
		a.port = args[3].toInt();
		a.attribs = attribs;
		a.extra_null = extra_null;
		QObject::connect(&a, SIGNAL(quit()), &app, SLOT(quit()));
		QTimer::singleShot(0, &a, SLOT(start()));
		app.exec();
	}
	else
	{
		usage();
		return 1;
	}
	return 0;
}
