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
#include "xmpp_tasks.h"

using namespace XMPP;

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
	JT_DiscoInfo *curDisco;

	Private(ServerInfoManager *parent = 0) : QObject(parent), q(parent), curDisco(0)
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
