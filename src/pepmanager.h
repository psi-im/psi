/*
 * pepmanager.h - Classes for PEP
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

#ifndef PEPMANAGER_H
#define PEPMANAGER_H

#include <QObject>

namespace XMPP {
	class Client;
	class Jid;
	class Message;
	class PubSubItem;
	class PubSubRetraction;
}
class PubSubSubscription;
class QString;
class ServerInfoManager;

using namespace XMPP;


class PEPManager : public QObject
{
	Q_OBJECT
	
public:
	enum Access {
		DefaultAccess,
		PresenceAccess,
		PublicAccess
	};

	PEPManager(XMPP::Client* client, ServerInfoManager* serverInfo);

	//void registerNode(const QString&);
	//void registerNodes(const QStringList&);
	//bool canPublish(const QString&) const;
	
	//void subscribe(const QString&, const QString&);
	//void unsubscribe(const QString&, const QString&);

	void publish(const QString& node, const PubSubItem&, Access = DefaultAccess);
	void retract(const QString& node, const QString& id);
	void get(const Jid& jid, const QString& node, const QString& id);

	//void getSubscriptions(const Jid& jid);

signals:
	void publish_success(const QString&, const PubSubItem&);
	void publish_error(const QString&, const PubSubItem&);
	void itemPublished(const Jid& jid, const QString& node, const PubSubItem&);
	void itemRetracted(const Jid& jid, const QString& node, const PubSubRetraction&);
	//void ready(const QString& node);
	//void getSubscriptions_success(const Jid& jid, const QList<PubSubSubscription>& subscriptions);
	//void getSubscriptions_error(const Jid&, int, const QString&);
	//void available(bool);

protected slots:
	void messageReceived(const Message&);
	void getFinished();
	//void serverFeaturesChanged();
	//void getSelfSubscriptionsTaskFinished();
	//void getSubscriptionsTaskFinished();
	void publishFinished();
	//void subscribeFinished();
	//void unsubscribeFinished();
	//void createFinished();

protected: 
	//void createNode(const QString& node);
	//void saveSubscriptions();

private:
	XMPP::Client* client_;
	ServerInfoManager* serverInfo_;
	
	//QStringList nodes_, ensured_nodes_;
};

#endif
