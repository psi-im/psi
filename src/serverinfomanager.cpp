/*
 * serverinfomanager.cpp
 * Copyright (C) 2006  Remko Troncon
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "serverinfomanager_b.h"
#include "xmpp_xmlcommon.h"
#include "xmpp_tasks.h"

using namespace XMPP;

class JT_ExDisco : public XMPP::Task
{
	Q_OBJECT

public:
	class Service
	{
	public:
		QString type;
		QString name;

		QString host;
		int port;
		QString transport;

		QString username;
		QString password;
	};

	JT_ExDisco(XMPP::Task *parent) :
		XMPP::Task(parent)
	{
	}

	~JT_ExDisco()
	{
	}

	void get(const XMPP::Jid &to, const QString &type = QString())
	{
		services_.clear();
		to_ = to;
		iq_ = createIQ(doc(), "get", to.full(), id());
		QDomElement query = doc()->createElement("services");
		query.setAttribute("xmlns", "urn:xmpp:extdisco:0");
		if(!type.isEmpty())
			query.setAttribute("type", type);
		iq_.appendChild(query);
	}

	QList<Service> services() const
	{
		return services_;
	}

	virtual void onGo()
	{
		send(iq_);
	}

	virtual bool take(const QDomElement &x)
	{
		if(!iqVerify(x, to_, id()))
			return false;

		if(x.attribute("type") == "result")
		{
			QDomElement se;
			for(QDomNode n = x.firstChild(); !n.isNull(); n = n.nextSibling())
			{
				if(!n.isElement())
					continue;

				QDomElement e = n.toElement();
				if(e.tagName() == "services" && e.attribute("xmlns") == "urn:xmpp:extdisco:0")
				{
					se = e;
					break;
				}
			}

			if(se.isNull())
				return true;

			for(QDomNode n = se.firstChild(); !n.isNull(); n = n.nextSibling())
			{
				if(!n.isElement())
					continue;

				QDomElement e = n.toElement();
				if(e.tagName() == "service")
				{
					Service s;
					s.type = e.attribute("type");
					s.name = e.attribute("name");
					s.host = e.attribute("host");
					s.port = e.attribute("port").toInt();
					s.transport = e.attribute("transport");
					s.username = e.attribute("username");
					s.password = e.attribute("password");
					services_ += s;
				}
			}

			setSuccess();
		}
		else
			setError(x);

		return true;
	}

private:
	QDomElement iq_;
	XMPP::Jid to_;
	QList<Service> services_;
};

// use qobject for the private object so we can destruct it without having to
//   define ~ServerInfoManager
class ServerInfoManager::Private : public QObject
{
	Q_OBJECT

public:
	ServerInfoManager *q;

	Client *client_;
	QStringList items;
	QString muc;
	QString stunHost, udpTurnHost, tcpTurnHost;
	int stunPort, udpTurnPort, tcpTurnPort;
	QString udpTurnUser, udpTurnPass, tcpTurnUser, tcpTurnPass;
	JT_DiscoInfo *curDisco;
	JT_ExDisco *jte;

	Private(ServerInfoManager *parent = 0) :
		QObject(parent),
		q(parent),
		stunPort(-1),
		udpTurnPort(-1),
		tcpTurnPort(-1),
		curDisco(0),
		jte(0)
	{
	}

	void tryDiscoEachItem()
	{
		if(items.isEmpty())
			return;

		Jid jid = items.takeFirst();

		curDisco = new JT_DiscoInfo(client_->rootTask());
		connect(curDisco, SIGNAL(finished()), q, SLOT(disco_finished()));
		curDisco->get(jid);
		curDisco->go(true);
	}

private slots:
	void exdisco_finished()
	{
		JT_ExDisco *jt = jte;
		jte = 0;

		QList<JT_ExDisco::Service> list = jt->services();
		foreach(const JT_ExDisco::Service &s, list)
		{
			if(s.type == "stun" && s.transport == "udp" && stunHost.isEmpty())
			{
				stunHost = s.host;
				stunPort = s.port;
			}
			else if(s.type == "turn" && s.transport == "udp" && udpTurnHost.isEmpty())
			{
				udpTurnHost = s.host;
				udpTurnPort = s.port;
				udpTurnUser = s.username;
				udpTurnPass = s.password;
			}
			else if(s.type == "turn" && s.transport == "tcp" && tcpTurnHost.isEmpty())
			{
				tcpTurnHost = s.host;
				tcpTurnPort = s.port;
				tcpTurnUser = s.username;
				tcpTurnPass = s.password;
			}
		}

		emit q->featuresChanged();
	}
};

ServerInfoManager::ServerInfoManager(Client* client)
{
	d = new Private(this);
	d->client_ = client;

	deinitialize();
	connect(d->client_, SIGNAL(rosterRequestFinished(bool, int, const QString &)), SLOT(initialize()));
	connect(d->client_, SIGNAL(disconnected()), SLOT(deinitialize()));
}

void ServerInfoManager::reset()
{
	hasPEP_ = false;
	multicastService_ = QString();

	d->stunHost.clear();
	d->stunPort = -1;
	d->udpTurnHost.clear();
	d->udpTurnPort = -1;
	d->udpTurnUser.clear();
	d->udpTurnPass.clear();
	d->tcpTurnHost.clear();
	d->tcpTurnPort = -1;
	d->tcpTurnUser.clear();
	d->tcpTurnPass.clear();
}

void ServerInfoManager::initialize()
{
	JT_DiscoInfo *jt = new JT_DiscoInfo(d->client_->rootTask());
	connect(jt, SIGNAL(finished()), SLOT(disco_finished()));
	jt->get(d->client_->jid().domain());
	jt->go(true);

	JT_DiscoItems *jti = new JT_DiscoItems(d->client_->rootTask());
	connect(jti, SIGNAL(finished()), SLOT(disco_finished()));
	jti->get(d->client_->jid().domain());
	jti->go(true);

	refreshExDisco();
}

void ServerInfoManager::deinitialize()
{
	reset();
	emit featuresChanged();
}

const QString& ServerInfoManager::multicastService() const
{
	return multicastService_;
}

bool ServerInfoManager::hasPEP() const
{
	return hasPEP_;
}

QString ServerInfoManager::mucService() const
{
	return d->muc;
}

void ServerInfoManager::refreshExDisco()
{
	delete d->jte;

	d->jte = new JT_ExDisco(d->client_->rootTask());
	connect(d->jte, SIGNAL(finished()), d, SLOT(exdisco_finished()));
	d->jte->get(d->client_->jid().domain());
	d->jte->go(true);
}

QString ServerInfoManager::stunHost() const
{
	return d->stunHost;
}

int ServerInfoManager::stunPort() const
{
	return d->stunPort;
}

QString ServerInfoManager::udpTurnHost() const
{
	return d->udpTurnHost;
}

int ServerInfoManager::udpTurnPort() const
{
	return d->udpTurnPort;
}

QString ServerInfoManager::udpTurnUser() const
{
	return d->udpTurnUser;
}

QString ServerInfoManager::udpTurnPass() const
{
	return d->udpTurnPass;
}

QString ServerInfoManager::tcpTurnHost() const
{
	return d->tcpTurnHost;
}

int ServerInfoManager::tcpTurnPort() const
{
	return d->tcpTurnPort;
}

QString ServerInfoManager::tcpTurnUser() const
{
	return d->tcpTurnUser;
}

QString ServerInfoManager::tcpTurnPass() const
{
	return d->tcpTurnPass;
}

void ServerInfoManager::disco_finished()
{
	JT_DiscoItems *jti = qobject_cast<JT_DiscoItems *>(sender());
	if(jti) {
		if(jti->success()) {
			DiscoList items = jti->items();
			d->items.clear();
			foreach(const DiscoItem &i, items)
				d->items += i.jid().full();

			d->tryDiscoEachItem();
		}

		return;
	}

	JT_DiscoInfo *jt = qobject_cast<JT_DiscoInfo *>(sender());
	if (jt == d->curDisco) {
		d->curDisco = 0;

		// Identities
		DiscoItem item = jt->item();
		DiscoItem::Identities is = item.identities();
		bool isMuc = false;
		bool isGateway = false;
		foreach(DiscoItem::Identity i, is) {
			//printf("item: jid=[%s], category=[%s], type=[%s]\n", qPrintable(item.jid().full()), qPrintable(i.category), qPrintable(i.type));
			if (i.category == "conference" && i.type == "text")
				isMuc = true;
			else if (i.category == "gateway")
				isGateway = true;
		}

		if(isMuc && !isGateway)
			d->muc = item.jid().full();

		d->tryDiscoEachItem();

		return;
	}

	if (jt->success()) {
		// Features
		Features f = jt->item().features();
		if (f.canMulticast())
			multicastService_ = d->client_->jid().domain();
		// TODO: Remove this, this is legacy
		if (f.test(QStringList("http://jabber.org/protocol/pubsub#pep")))
			hasPEP_ = true;

		// Identities
		DiscoItem::Identities is = jt->item().identities();
		foreach(DiscoItem::Identity i, is) {
			if (i.category == "pubsub" && i.type == "pep")
				hasPEP_ = true;
		}

		emit featuresChanged();
	}
}

#include "serverinfomanager.moc"
