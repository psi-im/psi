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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef PEPMANAGER_H
#define PEPMANAGER_H

#include "iris/xmpp_task.h"

class PubSubSubscription;
class QString;

namespace XMPP {
class Client;
class Jid;
class Message;
class PubSubItem;
class PubSubRetraction;
class ServerInfoManager;
}
using namespace XMPP;

class PEPGetTask : public Task {
public:
    PEPGetTask(Task *parent, const QString &jid, const QString &node, const QString &itemID);

    void onGo();

    bool take(const QDomElement &x);

    const QList<PubSubItem> &items() const;

    const QString &jid() const;

    const QString &node() const;

private:
    QDomElement       iq_;
    QString           jid_;
    QString           node_;
    QList<PubSubItem> items_;
};

class PEPManager : public QObject {
    Q_OBJECT

public:
    enum Access { DefaultAccess, PresenceAccess, PublicAccess };

    /* Implement all
     *    <option><value>authorize</value></option>
          <option><value>open</value></option>
          <option><value>presence</value></option>
          <option><value>roster</value></option>
          <option><value>whitelist</value></option>
    */

    PEPManager(XMPP::Client *client, ServerInfoManager *serverInfo);

    // void registerNode(const QString&);
    // void registerNodes(const QStringList&);
    // bool canPublish(const QString&) const;

    // void subscribe(const QString&, const QString&);
    // void unsubscribe(const QString&, const QString&);

    XMPP::Task *publish(const QString &node, const PubSubItem &, Access = DefaultAccess, bool persisteItems = false);
    void        retract(const QString &node, const QString &id);
    void        disable(const QString &tagName, const QString &node, const QString &id);
    PEPGetTask *get(const Jid &jid, const QString &node, const QString &id);

    // void getSubscriptions(const Jid& jid);

signals:
    void publish_success(const QString &, const PubSubItem &);
    void publish_error(const QString &, const PubSubItem &);
    void itemPublished(const Jid &jid, const QString &node, const PubSubItem &);
    void itemRetracted(const Jid &jid, const QString &node, const PubSubRetraction &);
    // void ready(const QString& node);
    // void getSubscriptions_success(const Jid& jid, const QList<PubSubSubscription>& subscriptions);
    // void getSubscriptions_error(const Jid&, int, const QString&);
    // void available(bool);

protected slots:
    void messageReceived(const Message &);
    void getFinished();
    // void serverFeaturesChanged();
    // void getSelfSubscriptionsTaskFinished();
    // void getSubscriptionsTaskFinished();
    void publishFinished();
    // void subscribeFinished();
    // void unsubscribeFinished();
    // void createFinished();

protected:
    // void createNode(const QString& node);
    // void saveSubscriptions();

private:
    XMPP::Client      *client_;
    ServerInfoManager *serverInfo_;

    // QStringList nodes_, ensured_nodes_;
};

#endif // PEPMANAGER_H
