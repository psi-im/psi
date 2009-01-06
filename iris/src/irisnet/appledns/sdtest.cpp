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

#include <QtCore>
#include <QHostAddress>
#include "qdnssd.h"

// for ntohl
#ifdef Q_OS_WIN
# include <windows.h>
#else
# include <netinet/in.h>
#endif

class Command
{
public:
	enum Type
	{
		Query,
		Browse,
		Resolve,
		Reg
	};

	Type type;

	QString name; // query, resolve, reg
	int rtype; // query
	QString stype; // browse, resolve, reg
	QString domain; // browse, resolve, reg
	int port; // reg
	QByteArray txtRecord; // reg

	int id;
	int dnsId;
	bool error;
	bool done; // for resolve

	Command() :
		error(false),
		done(false)
	{
	}
};

static QString nameToString(const QByteArray &in)
{
	QStringList parts;
	int at = 0;
	while(at < in.size())
	{
		int len = in[at++];
		parts += QString::fromUtf8(in.mid(at, len));
		at += len;
	}
	return parts.join(".");
}

static QString recordToDesc(const QDnsSd::Record &rec)
{
	QString desc;

	if(rec.rrtype == 1) // A
	{
		quint32 *p = (quint32 *)rec.rdata.data();
		desc = QHostAddress(ntohl(*p)).toString();
	}
	else if(rec.rrtype == 28) // AAAA
	{
		desc = QHostAddress((quint8 *)rec.rdata.data()).toString();
	}
	else if(rec.rrtype == 12) // PTR
	{
		desc = QString("[%1]").arg(nameToString(rec.rdata));
	}
	else
		desc = QString("%1 bytes").arg(rec.rdata.size());

	return desc;
}

static QStringList txtRecordToStringList(const QByteArray &rdata)
{
	QList<QByteArray> txtEntries = QDnsSd::parseTxtRecord(rdata);
	if(txtEntries.isEmpty())
		return QStringList();

	QStringList out;
	foreach(const QByteArray &entry, txtEntries)
		out += QString::fromUtf8(entry);
	return out;
}

static void printIndentedTxt(const QByteArray &txtRecord)
{
	QStringList list = txtRecordToStringList(txtRecord);
	if(!list.isEmpty())
	{
		foreach(const QString &s, list)
			printf("   %s\n", qPrintable(s));
	}
	else
		printf("   (TXT parsing error)\n");
}

class App : public QObject
{
	Q_OBJECT
public:
	QList<Command> commands;
	QDnsSd *dns;

	App()
	{
		dns = new QDnsSd(this);
		connect(dns, SIGNAL(queryResult(int, const QDnsSd::QueryResult &)), SLOT(dns_queryResult(int, const QDnsSd::QueryResult &)));
		connect(dns, SIGNAL(browseResult(int, const QDnsSd::BrowseResult &)), SLOT(dns_browseResult(int, const QDnsSd::BrowseResult &)));
		connect(dns, SIGNAL(resolveResult(int, const QDnsSd::ResolveResult &)), SLOT(dns_resolveResult(int, const QDnsSd::ResolveResult &)));
		connect(dns, SIGNAL(regResult(int, const QDnsSd::RegResult &)), SLOT(dns_regResult(int, const QDnsSd::RegResult &)));
	}

public slots:
	void start()
	{
		for(int n = 0; n < commands.count(); ++n)
		{
			Command &c = commands[n];

			c.id = n;
			if(c.type == Command::Query)
			{
				printf("%2d: Query name=[%s], type=%d ...\n", c.id, qPrintable(c.name), c.rtype);
				c.dnsId = dns->query(c.name.toUtf8(), c.rtype);
			}
			else if(c.type == Command::Browse)
			{
				printf("%2d: Browse type=[%s]", c.id, qPrintable(c.stype));
				if(!c.domain.isEmpty())
					printf(", domain=[%s]", qPrintable(c.domain));
				printf(" ...\n");
				c.dnsId = dns->browse(c.stype.toUtf8(), c.domain.toUtf8());
			}
			else if(c.type == Command::Resolve)
			{
				printf("%2d: Resolve name=[%s], type=[%s], domain=[%s] ...\n", c.id, qPrintable(c.name), qPrintable(c.stype), qPrintable(c.domain));
				c.dnsId = dns->resolve(c.name.toUtf8(), c.stype.toUtf8(), c.domain.toUtf8());
			}
			else if(c.type == Command::Reg)
			{
				printf("%2d: Register name=[%s], type=[%s]", c.id, qPrintable(c.name), qPrintable(c.stype));
				if(!c.domain.isEmpty())
					printf(", domain=[%s]", qPrintable(c.domain));
				printf(", port=%d ...\n", c.port);
				if(!c.txtRecord.isEmpty())
					printIndentedTxt(c.txtRecord);

				c.dnsId = dns->reg(c.name.toUtf8(), c.stype.toUtf8(), c.domain.toUtf8(), c.port, c.txtRecord);
			}
		}
	}

signals:
	void quit();

private:
	int cmdIdToCmdIndex(int cmdId)
	{
		for(int n = 0; n < commands.count(); ++n)
		{
			const Command &c = commands[n];
			if(c.id == cmdId)
				return n;
		}
		return -1;
	}

	int dnsIdToCmdIndex(int dnsId)
	{
		for(int n = 0; n < commands.count(); ++n)
		{
			const Command &c = commands[n];
			if(c.dnsId == dnsId)
				return n;
		}
		return -1;
	}

	void tryQuit()
	{
		// quit if there are nothing but errors or completed resolves
		bool doQuit = true;
		foreach(const Command &c, commands)
		{
			if(c.error || (c.type == Command::Resolve && c.done))
				continue;

			doQuit = false;
			break;
		}

		if(doQuit)
			emit quit();
	}

private slots:
	void dns_queryResult(int id, const QDnsSd::QueryResult &result)
	{
		int at = dnsIdToCmdIndex(id);
		Command &c = commands[at];

		if(!result.success)
		{
			printf("%2d: Error.", c.id);
			if(!result.lowLevelError.func.isEmpty())
				printf(" (%s, %d)", qPrintable(result.lowLevelError.func), result.lowLevelError.code);
			printf("\n");
			c.error = true;
			tryQuit();
			return;
		}

		foreach(const QDnsSd::Record &rec, result.records)
		{
			if(rec.added)
			{
				printf("%2d: Added:   %s, ttl=%d\n", c.id, qPrintable(recordToDesc(rec)), rec.ttl);
				if(rec.rrtype == 16)
					printIndentedTxt(rec.rdata);
			}
			else
				printf("%2d: Removed: %s, ttl=%d\n", c.id, qPrintable(recordToDesc(rec)), rec.ttl);
		}
	}

	void dns_browseResult(int id, const QDnsSd::BrowseResult &result)
	{
		int at = dnsIdToCmdIndex(id);
		Command &c = commands[at];

		if(!result.success)
		{
			printf("%2d: Error.", c.id);
			if(!result.lowLevelError.func.isEmpty())
				printf(" (%s, %d)", qPrintable(result.lowLevelError.func), result.lowLevelError.code);
			printf("\n");
			c.error = true;
			tryQuit();
			return;
		}

		foreach(const QDnsSd::BrowseEntry &e, result.entries)
		{
			if(e.added)
				printf("%2d: Added:   [%s] [%s] [%s]\n", c.id, qPrintable(QString::fromUtf8(e.serviceName)), qPrintable(QString::fromUtf8(e.serviceType)), qPrintable(QString::fromUtf8(e.replyDomain)));
			else
				printf("%2d: Removed: [%s]\n", c.id, qPrintable(QString::fromUtf8(e.serviceName)));
		}
	}

	void dns_resolveResult(int id, const QDnsSd::ResolveResult &result)
	{
		int at = dnsIdToCmdIndex(id);
		Command &c = commands[at];

		if(!result.success)
		{
			printf("%2d: Error.", c.id);
			if(!result.lowLevelError.func.isEmpty())
				printf(" (%s, %d)", qPrintable(result.lowLevelError.func), result.lowLevelError.code);
			printf("\n");
			c.error = true;
			tryQuit();
			return;
		}

		printf("%2d: Result: host=[%s] port=%d\n", c.id, qPrintable(QString::fromUtf8(result.hostTarget)), result.port);
		if(!result.txtRecord.isEmpty())
			printIndentedTxt(result.txtRecord);

		c.done = true;
		tryQuit();
	}

	void dns_regResult(int id, const QDnsSd::RegResult &result)
	{
		int at = dnsIdToCmdIndex(id);
		Command &c = commands[at];

		if(!result.success)
		{
			QString errstr;
			if(result.errorCode == QDnsSd::RegResult::ErrorConflict)
				errstr = "Conflict";
			else
				errstr = "Generic";
			printf("%2d: Error (%s).", c.id, qPrintable(errstr));
			if(!result.lowLevelError.func.isEmpty())
				printf(" (%s, %d)", qPrintable(result.lowLevelError.func), result.lowLevelError.code);
			printf("\n");
			c.error = true;
			tryQuit();
			return;
		}

		printf("%2d: Registered: domain=[%s]\n", c.id, qPrintable(QString::fromUtf8(result.domain)));
	}
};

#include "sdtest.moc"

void usage()
{
	printf("usage: sdtest [[command] (command) ...]\n");
	printf(" options: --txt=str0,...,strn\n");
	printf("\n");
	printf(" q=name,type#                   query for a record\n");
	printf(" b=type(,domain)                browse for services\n");
	printf(" r=name,type,domain             resolve a service\n");
	printf(" e=name,type,port(,domain)      register a service\n");
	printf("\n");
}

int main(int argc, char **argv)
{
	QCoreApplication qapp(argc, argv);

	QStringList args = qapp.arguments();
	args.removeFirst();

	if(args.count() < 1)
	{
		usage();
		return 1;
	}

	// options
	QStringList txt;

	for(int n = 0; n < args.count(); ++n)
	{
		QString s = args[n];
		if(!s.startsWith("--"))
			continue;
		QString var;
		QString val;
		int x = s.indexOf('=');
		if(x != -1)
		{
			var = s.mid(2, x - 2);
			val = s.mid(x + 1);
		}
		else
		{
			var = s.mid(2);
		}

		bool known = true;

		if(var == "txt")
		{
			txt = val.split(',');
		}
		else
			known = false;

		if(known)
		{
			args.removeAt(n);
			--n; // adjust position
		}
	}

	// commands
	QList<Command> commands;

	for(int n = 0; n < args.count(); ++n)
	{
		QString str = args[n];
		int n = str.indexOf('=');
		if(n == -1)
		{
			printf("Error: bad format of command.\n");
			return 1;
		}

		QString type = str.mid(0, n);
		QString rest = str.mid(n + 1);
		QStringList parts = rest.split(',');

		if(type == "q")
		{
			if(parts.count() < 2)
			{
				usage();
				return 1;
			}

			Command c;
			c.type = Command::Query;
			c.name = parts[0];
			c.rtype = parts[1].toInt();
			commands += c;
		}
		else if(type == "b")
		{
			if(parts.count() < 1)
			{
				usage();
				return 1;
			}

			Command c;
			c.type = Command::Browse;
			c.stype = parts[0];
			if(parts.count() >= 2)
				c.domain = parts[1];
			commands += c;
		}
		else if(type == "r")
		{
			if(parts.count() < 3)
			{
				usage();
				return 1;
			}

			Command c;
			c.type = Command::Resolve;
			c.name = parts[0];
			c.stype = parts[1];
			c.domain = parts[2];
			commands += c;
		}
		else if(type == "e")
		{
			if(parts.count() < 3)
			{
				usage();
				return 1;
			}

			Command c;
			c.type = Command::Reg;
			c.name = parts[0];
			c.stype = parts[1];
			c.port = parts[2].toInt();
			if(parts.count() >= 4)
				c.domain = parts[3];

			if(!txt.isEmpty())
			{
				QList<QByteArray> strings;
				foreach(const QString &str, txt)
					strings += str.toUtf8();

				QByteArray txtRecord = QDnsSd::createTxtRecord(strings);
				if(txtRecord.isEmpty())
				{
					printf("Error: failed to create TXT record, input too large or invalid\n");
					return 1;
				}

				c.txtRecord = txtRecord;
			}

			commands += c;
		}
		else
		{
			printf("Error: unknown command type '%s'.\n", qPrintable(type));
			return 1;
		}
	}

	App app;
	app.commands = commands;
	QObject::connect(&app, SIGNAL(quit()), &qapp, SLOT(quit()));
	QTimer::singleShot(0, &app, SLOT(start()));
	qapp.exec();
	return 0;
}
