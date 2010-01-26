/*
 * pepmanager.cpp - Classes for PEP
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
 
#include "pepmanager.h"

#include <QtDebug>
#include "xmpp_xmlcommon.h"
#include "xmpp_tasks.h"
#include "serverinfomanager.h"

using namespace XMPP;

// TODO: Get affiliations upon startup, and only create nodes based on that.
// (subscriptions is not accurate, since one doesn't subscribe to the
// avatar data node)

// -----------------------------------------------------------------------------

class PEPGetTask : public Task
{
public:
	PEPGetTask(Task* parent, const QString& jid, const QString& node, const QString& itemID) : Task(parent), jid_(jid), node_(node) {
		iq_ = createIQ(doc(), "get", jid_, id());
		
		QDomElement pubsub = doc()->createElement("pubsub");
		pubsub.setAttribute("xmlns", "http://jabber.org/protocol/pubsub");
		iq_.appendChild(pubsub);
		
		QDomElement items = doc()->createElement("items");
		items.setAttribute("node", node);
		pubsub.appendChild(items);
		
		QDomElement item = doc()->createElement("item");
		item.setAttribute("id", itemID);
		items.appendChild(item);
	}
	
	void onGo() {
		send(iq_);
	}
	
	bool take(const QDomElement &x) {
		if(!iqVerify(x, jid_, id()))
			return false;

		if(x.attribute("type") == "result") {
			bool found;
			// FIXME Check namespace...
			QDomElement e = findSubTag(x, "pubsub", &found);
			if (found) {
				QDomElement i = findSubTag(e, "items", &found);
				if (found) {
					for(QDomNode n1 = i.firstChild(); !n1.isNull(); n1 = n1.nextSibling()) {
						QDomElement e1 = n1.toElement();
						if (!e1.isNull() && e1.tagName() == "item") {
							for(QDomNode n2 = e1.firstChild(); !n2.isNull(); n2 = n2.nextSibling()) {
								QDomElement e2 = n2.toElement();
								if (!e2.isNull()) {
									items_ += PubSubItem(e1.attribute("id"),e2);
								}
							}
						}
					}
				}
			}
			setSuccess();
			return true;
		}
		else {
			setError(x);
			return true;
		}
	}
		
	const QList<PubSubItem>& items() const {
		return items_;
	}

	const QString& jid() const {
		return jid_;
	}
	
	const QString& node() const {
		return node_;
	}

private:
	QDomElement iq_;
	QString jid_;
	QString node_;
	QList<PubSubItem> items_;
};


// -----------------------------------------------------------------------------

/*
class PEPUnsubscribeTask : public Task
{
public:
	PEPUnsubscribeTask(Task* parent, const QString& jid, const QString& node) : Task(parent), jid_(jid) {
		iq_ = createIQ(doc(), "set", jid_, id());
		
		QDomElement pubsub = doc()->createElement("pubsub");
		pubsub.setAttribute("xmlns", "http://jabber.org/protocol/pubsub");
		iq_.appendChild(pubsub);
		
		QDomElement unsubscribe = doc()->createElement("unsubscribe");
		unsubscribe.setAttribute("node", node);
		unsubscribe.setAttribute("jid", client()->jid().bare());
		pubsub.appendChild(unsubscribe);
	}

	void onGo() {
		send(iq_);
	}

	bool take(const QDomElement& x) {
		if(!iqVerify(x, jid_, id()))
			return false;

		if(x.attribute("type") == "result") {
			setSuccess();
		}
		else {
			setError(x);
		}
		return true;
	}

private:
	QDomElement iq_;
	QString jid_;
};

// -----------------------------------------------------------------------------

class PEPSubscribeTask : public Task
{
public:
	PEPSubscribeTask(Task* parent, const QString& jid, const QString& node) : Task(parent), jid_(jid) {
		iq_ = createIQ(doc(), "set", jid_, id());
		
		QDomElement pubsub = doc()->createElement("pubsub");
		pubsub.setAttribute("xmlns", "http://jabber.org/protocol/pubsub");
		iq_.appendChild(pubsub);
		
		QDomElement subscribe = doc()->createElement("subscribe");
		subscribe.setAttribute("node", node);
		subscribe.setAttribute("jid", client()->jid().bare());
		pubsub.appendChild(subscribe);
	}

	void onGo() {
		send(iq_);
	}

	bool take(const QDomElement& x) {
		if(!iqVerify(x, jid_, id()))
			return false;

		if(x.attribute("type") == "result") {
			setSuccess();
		}
		else {
			setError(x);
		}
		return true;
	}

private:
	QDomElement iq_;
	QString jid_;
};*/

// -----------------------------------------------------------------------------

/*
class PEPCreateNodeTask : public Task
{
public:
	PEPCreateNodeTask(Task* parent, const QString& node) : Task(parent) {
		node_ = node;
		iq_ = createIQ(doc(), "set", "", id());
		
		QDomElement pubsub = doc()->createElement("pubsub");
		pubsub.setAttribute("xmlns", "http://jabber.org/protocol/pubsub");
		iq_.appendChild(pubsub);
		
		QDomElement subscribe = doc()->createElement("create");
		subscribe.setAttribute("node", node);
		pubsub.appendChild(subscribe);

		QDomElement configure = doc()->createElement("configure");
		pubsub.appendChild(configure);
	}

	const QString& node() const {
		return node_;
	}
	
	void onGo() {
		send(iq_);
	}

	bool take(const QDomElement& x) {
		if(!iqVerify(x, "", id()))
			return false;

		if(x.attribute("type") == "result") {
			setSuccess();
		}
		else {
			setError(x);
		}
		return true;
	}

private:
	QDomElement iq_;
	QString node_;
};*/

// -----------------------------------------------------------------------------

class PEPPublishTask : public Task
{
public:
	PEPPublishTask(Task* parent, const QString& node, const PubSubItem& it, PEPManager::Access access) : Task(parent), node_(node), item_(it) {
		iq_ = createIQ(doc(), "set", "", id());
		
		QDomElement pubsub = doc()->createElement("pubsub");
		pubsub.setAttribute("xmlns", "http://jabber.org/protocol/pubsub");
		iq_.appendChild(pubsub);
		
		QDomElement publish = doc()->createElement("publish");
		publish.setAttribute("node", node);
		pubsub.appendChild(publish);
		
		QDomElement item = doc()->createElement("item");
		item.setAttribute("id", it.id());
		publish.appendChild(item);

		if (access != PEPManager::DefaultAccess) {
			QDomElement conf = doc()->createElement("configure");
			QDomElement conf_x = doc()->createElementNS("jabber:x:data","x");

			// Form type
			QDomElement conf_x_field_type = doc()->createElement("field");
			conf_x_field_type.setAttribute("var","FORM_TYPE");
			conf_x_field_type.setAttribute("type","hidden");
			QDomElement conf_x_field_type_value = doc()->createElement("value");
			conf_x_field_type_value.appendChild(doc()->createTextNode("http://jabber.org/protocol/pubsub#node_config"));
			conf_x_field_type.appendChild(conf_x_field_type_value);
			conf_x.appendChild(conf_x_field_type);
			
			// Access model
			QDomElement access_model = doc()->createElement("field");
			access_model.setAttribute("var","pubsub#access_model");
			QDomElement access_model_value = doc()->createElement("value");
			access_model.appendChild(access_model_value);
			if (access == PEPManager::PublicAccess) {
				access_model_value.appendChild(doc()->createTextNode("open"));
			}
			else if (access == PEPManager::PresenceAccess) {
				access_model_value.appendChild(doc()->createTextNode("presence"));
			}
			conf_x.appendChild(access_model);
			
			
			conf.appendChild(conf_x);
			pubsub.appendChild(conf);
		}
		
		item.appendChild(it.payload());
	}
	
	bool take(const QDomElement& x) {
		if(!iqVerify(x, "", id()))
			return false;

		if(x.attribute("type") == "result") {
			setSuccess();
		}
		else {
			setError(x);
		}
		return true;
	}
	
	void onGo() {
		send(iq_);
	}

	const PubSubItem& item() const {
		return item_;
	}

	const QString& node() const {
		return node_;
	}

private:
	QDomElement iq_;
	QString node_;
	PubSubItem item_;
};


// -----------------------------------------------------------------------------

class PEPRetractTask : public Task
{
public:
	PEPRetractTask(Task* parent, const QString& node, const QString& itemId) : Task(parent), node_(node), itemId_(itemId) {
		iq_ = createIQ(doc(), "set", "", id());
		
		QDomElement pubsub = doc()->createElement("pubsub");
		pubsub.setAttribute("xmlns", "http://jabber.org/protocol/pubsub");
		iq_.appendChild(pubsub);
		
		QDomElement retract = doc()->createElement("retract");
		retract.setAttribute("node", node);
		retract.setAttribute("notify", "1");
		pubsub.appendChild(retract);
		
		QDomElement item = doc()->createElement("item");
		item.setAttribute("id", itemId);
		retract.appendChild(item);
	}
	
	bool take(const QDomElement& x) {
		if(!iqVerify(x, "", id()))
			return false;

		if(x.attribute("type") == "result") {
			setSuccess();
		}
		else {
			setError(x);
		}
		return true;
	}
	
	void onGo() {
		send(iq_);
	}

	const QString& node() const {
		return node_;
	}

private:
	QDomElement iq_;
	QString node_;
	QString itemId_;
};


// -----------------------------------------------------------------------------

/*
class GetPubSubSubscriptionsTask : public Task
{
public:
	GetPubSubSubscriptionsTask(Task* parent, const Jid& jid) : Task(parent), jid_(jid) {
		iq_ = createIQ(doc(), "get", jid_.bare(), id());
		
		QDomElement pubsub = doc()->createElement("pubsub");
		pubsub.setAttribute("xmlns", "http://jabber.org/protocol/pubsub");
		iq_.appendChild(pubsub);
		
		QDomElement subscriptions = doc()->createElement("subscriptions");
		pubsub.appendChild(subscriptions);
	}
	
	bool take(const QDomElement &x) {
		if(!iqVerify(x, jid_.bare(), id()))
			return false;

		if(x.attribute("type") == "result") {
			subscriptions_.clear();
			for(QDomNode m = x.firstChild(); !m.isNull(); m = m.nextSibling()) {
				QDomElement me = m.toElement();
				if (me.tagName() != "pubsub")
					continue;

				for(QDomNode n = me.firstChild(); !n.isNull(); n = n.nextSibling()) {
					QDomElement s = n.toElement();
					if (s.tagName() != "subscriptions") 
						continue;

					for(QDomNode sub_node = s.firstChild(); !sub_node.isNull(); sub_node = sub_node.nextSibling()) {
						// TODO: Check if the jid is the right one (ours) ?
						PubSubSubscription sub(sub_node.toElement());
						subscriptions_ += sub;
					}
				}
			}

			setSuccess();
			return true;
		}
		else {
			setError(x);
			return true;
		}
	}
		
	const Jid& jid() const {
		return jid_;
	}

	const QList<PubSubSubscription> subscriptions() const {
		return subscriptions_;
	}

	void onGo() {
		send(iq_);
	}

private:
	QDomElement iq_;
	Jid jid_;
	QList<PubSubSubscription> subscriptions_;
};*/

// -----------------------------------------------------------------------------

PEPManager::PEPManager(Client* client, ServerInfoManager* serverInfo) : client_(client), serverInfo_(serverInfo)
{
	connect(client_, SIGNAL(messageReceived(const Message &)), SLOT(messageReceived(const Message &)));
}

/*void PEPManager::setAvailable(bool a)
{
	if (available_ != a) {
		available_ = a;
		emit available(available_);
	}
}

void PEPManager::registerNode(const QString& node)
{
	if (nodes_.contains(node))
		return;
	
	if (available_) {
		createNode(node);
	}

	nodes_ += node;
}

void PEPManager::registerNodes(const QStringList& nodes)
{
	foreach(QString node, nodes) {
		registerNode(node);
	}
}

bool PEPManager::canPublish(const QString& node) const
{
	return ensured_nodes_.contains(node);
}

void PEPManager::saveSubscriptions()
{
}

void PEPManager::createNode(const QString& node) 
{
	PEPCreateNodeTask* t = new PEPCreateNodeTask(client_->rootTask(),node);
	connect(t,SIGNAL(finished()),SLOT(createFinished()));
	t->go(true);
}


void PEPManager::subscribe(const QString& jid, const QString& ns)
{
	PEPSubscribeTask* t = new PEPSubscribeTask(client_->rootTask(),jid,ns);
	connect(t,SIGNAL(finished()),SLOT(subscribeFinished()));
	t->go(true);
}
		
void PEPManager::createFinished()
{
	PEPCreateNodeTask* task = (PEPCreateNodeTask*) sender();
	if (task->success() || task->statusCode() == 409) {
		if (task->statusCode() == 409) 
			qWarning(QString("[%1] PEP Node already exists. Ignoring.").arg(client_->jid().full()));

		// Subscribe to our own nodes
		if (task->node() != "urn:xmpp:avatar:data")
			subscribe(client_->jid().bare(),task->node());
		
		// Notify
		ensured_nodes_ += task->node();
		emit ready(task->node());
	}
	else {
		qWarning(QString("[%3] PEP Create failed: '%1' (%2)").arg(task->statusString()).arg(QString::number(task->statusCode())).arg(client_->jid().full()));
	}
}
		
void PEPManager::subscribeFinished()
{
	PEPSubscribeTask* task = (PEPSubscribeTask*) sender();
	if (task->success()) {
		//subscriptions_ += task->subscription();
		saveSubscriptions();
	}
	else {
		qWarning(QString("[%3] PEP Subscribe failed: '%1' (%2)").arg(task->statusString()).arg(QString::number(task->statusCode())).arg(client_->jid().full()));
	}
}

void PEPManager::unsubscribe(const QString& jid, const QString& node)
{
	PEPUnsubscribeTask* t = new PEPUnsubscribeTask(client_->rootTask(),jid,node);
	connect(t,SIGNAL(finished()),SLOT(unsubscribeFinished()));
	t->go(true);
}

void PEPManager::unsubscribeFinished()
{
	PEPUnsubscribeTask* task = (PEPUnsubscribeTask*) sender();
	if (!task->success()) {
		qWarning(QString("[%3] PEP Unsubscribe failed: '%1' (%2)").arg(task->statusString()).arg(QString::number(task->statusCode())).arg(client_->jid().full()));
	}

	// We're being conservative about unsubscribing. Remove subscription,
	// even if there was an error.
	//subscriptions_.remove(task->subscription());
	saveSubscriptions();
}*/

void PEPManager::publish(const QString& node, const PubSubItem& it, Access access)
{
	//if (!canPublish(node))
	//	return;
	if (!serverInfo_->hasPEP())
		return;
	
	PEPPublishTask* tp = new PEPPublishTask(client_->rootTask(),node,it,access);
	connect(tp, SIGNAL(finished()), SLOT(publishFinished()));
	tp->go(true);
}


void PEPManager::retract(const QString& node, const QString& id)
{
	if (!serverInfo_->hasPEP())
		return;
	
	PEPRetractTask* tp = new PEPRetractTask(client_->rootTask(),node,id);
	// FIXME: add notification of success/failure
	tp->go(true);
}


void PEPManager::publishFinished()
{
	PEPPublishTask* task = (PEPPublishTask*) sender();
	if (task->success()) {
		emit publish_success(task->node(),task->item());
	}
	else {
		qWarning() << QString("[%3] PEP Publish failed: '%1' (%2)").arg(task->statusString()).arg(QString::number(task->statusCode())).arg(client_->jid().full());
		emit publish_error(task->node(),task->item());
	}
}

void PEPManager::get(const Jid& jid, const QString& node, const QString& id) 
{
	PEPGetTask* g = new PEPGetTask(client_->rootTask(),jid.bare(),node,id);
	connect(g, SIGNAL(finished()), SLOT(getFinished()));
	g->go(true);
}

void PEPManager::messageReceived(const Message& m)
{
	if (m.type() != "error") {
		foreach(PubSubRetraction i, m.pubsubRetractions()) {
			emit itemRetracted(m.from(),m.pubsubNode(), i);
		}
		foreach(PubSubItem i, m.pubsubItems()) {
			emit itemPublished(m.from(),m.pubsubNode(),i);
		}
	}
}

/*void PEPManager::serverFeaturesChanged()
{
	if (!available_ && serverInfo_->hasPEP()) {
		GetPubSubSubscriptionsTask* task = new GetPubSubSubscriptionsTask(client_->rootTask(),client_->jid());
		connect(task,SIGNAL(finished()),SLOT(getSelfSubscriptionsTaskFinished()));
		task->go(true);
	}
	else if (available_ && !serverInfo_->hasPEP()) {
		ensured_nodes_.clear();
		setAvailable(false);
	}
}*/

void PEPManager::getFinished()
{
	PEPGetTask* task = (PEPGetTask*) sender();
	if (task->success()) {
		// Act as if the item was published. This is a convenience 
		// implementation, probably should be changed later.
		if (!task->items().isEmpty()) {
			emit itemPublished(task->jid(),task->node(),task->items().first());
		}
	}
	else {
		qWarning() << QString("[%3] PEP Get failed: '%1' (%2)").arg(task->statusString()).arg(QString::number(task->statusCode())).arg(client_->jid().full());
	}
}

/*
void PEPManager::getSubscriptions(const Jid& jid)
{
	GetPubSubSubscriptionsTask* task = new GetPubSubSubscriptionsTask(client_->rootTask(),jid);
	connect(task,SIGNAL(finished()),SLOT(getSubscriptionsTaskFinished()));
	task->go(true);
}

void PEPManager::getSelfSubscriptionsTaskFinished()
{
	GetPubSubSubscriptionsTask* task = (GetPubSubSubscriptionsTask*) sender();
	if (task->success() || task->statusCode() == 404) {
		QStringList nodes = nodes_;
		foreach(PubSubSubscription s, task->subscriptions()) {
			if (nodes.contains(s.node())) {
				nodes.remove(s.node());
				ensured_nodes_ += s.node();
				emit ready(s.node());

				// Subscribe to our own nodes
				if (s.state() == PubSubSubscription::None && s.node() != "urn:xmpp:avatar:data") {
					subscribe(client_->jid().bare(),s.node());
				}
			}
		}

		// Create remaining nodes
		foreach(QString node, nodes) {
			createNode(node);
		}
	}
	else {
		qWarning(QString("[%3] Error getting own subscriptions: '%1' (%2)").arg(task->statusString()).arg(QString::number(task->statusCode())).arg(client_->jid().full()));
	}
}

void PEPManager::getSubscriptionsTaskFinished()
{
	GetPubSubSubscriptionsTask* task = (GetPubSubSubscriptionsTask*) sender();
	if (task->success()) {
		emit getSubscriptions_success(task->jid(), task->subscriptions());
	}
	else {
		emit getSubscriptions_error(task->jid(),task->statusCode(), task->statusString());
	}
}*/
