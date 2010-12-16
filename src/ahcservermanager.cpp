/*
 * ahcservermanager.cpp - Server implementation of JEP-50 (Ad-Hoc Commands)
 * Copyright (C) 2005  Remko Troncon
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

#include <QComboBox>
#include <QMessageBox>
#include <QPushButton>
#include <QLayout>

#include "ahcservermanager.h"
#include "ahcommandserver.h"
#include "psiaccount.h"

#include "xmpp_xmlcommon.h"
#include "xmpp_tasks.h"
#include "xmpp_xdata.h"
#include "xdata_widget.h"
#include "ahcommand.h"

using namespace XMPP;

#define AHC_NS "http://jabber.org/protocol/commands"

// -------------------------------------------------------------------------- 
// JT_AHCServer: Task to handle ad-hoc command requests
// -------------------------------------------------------------------------- 

class JT_AHCServer : public Task
{
	Q_OBJECT

public:
	JT_AHCServer(Task*, AHCServerManager*);
	bool take(const QDomElement& e);
	void sendReply(const AHCommand&, const Jid& to, const QString& id);

protected:
	bool commandListQuery(const QDomElement& e);
	bool commandExecuteQuery(const QDomElement& e);
	void sendCommandList(const QString& to, const QString& from, const QString& id);

private:
	AHCServerManager* manager_;
};


JT_AHCServer::JT_AHCServer(Task* t, AHCServerManager* manager) : Task(t), manager_(manager)
{
}

bool JT_AHCServer::take(const QDomElement& e)
{
	// Check if it's a query
	if (e.tagName() != "iq")
		return false;

	return (commandListQuery(e) || commandExecuteQuery(e));
}

bool JT_AHCServer::commandListQuery(const QDomElement& e)
{
	if (e.attribute("type") == "get") {
		bool found;
		QDomElement q = findSubTag(e, "query", &found);
		if (!found)
			return false;
			
		// Disco replies to the AdHoc node
		if (q.attribute("xmlns") == "http://jabber.org/protocol/disco#items" && q.attribute("node") == AHC_NS) {
			sendCommandList(e.attribute("from"),e.attribute("to"),e.attribute("id"));
			return true;
		}
		else if (q.attribute("xmlns") == "http://jabber.org/protocol/disco#info" && q.attribute("node") == AHC_NS) {
			QDomElement iq = createIQ(doc(), "result", e.attribute("from"), e.attribute("id"));
			QDomElement query = doc()->createElement("query");
			query.setAttribute("xmlns", "http://jabber.org/protocol/disco#info");
			iq.appendChild(query);
			send(iq);
			return true;
		}
		// Disco replies to specific adhoc nodes
		else if (q.attribute("xmlns") == "http://jabber.org/protocol/disco#items" && manager_->hasServer(q.attribute("node"), Jid(e.attribute("from")))) {
			QDomElement iq = createIQ(doc(), "result", e.attribute("from"), e.attribute("id"));
			QDomElement query = doc()->createElement("query");			  query.setAttribute("xmlns", "http://jabber.org/protocol/disco#items");			query.setAttribute("node",q.attribute("node"));
			iq.appendChild(query);

			send(iq);
			return true;
		}
		else if (q.attribute("xmlns") == "http://jabber.org/protocol/disco#info" && manager_->hasServer(q.attribute("node"), Jid(e.attribute("from")))) {
			QDomElement iq = createIQ(doc(), "result", e.attribute("from"), e.attribute("id"));
			QDomElement query = doc()->createElement("query");
			query.setAttribute("xmlns", "http://jabber.org/protocol/disco#info");
			query.setAttribute("node",q.attribute("node"));
			iq.appendChild(query);

			QDomElement identity;
			identity = doc()->createElement("identity");
			identity.setAttribute("category", "automation");
			identity.setAttribute("type", "command-node");
			query.appendChild(identity);

			QDomElement feature;
			feature = doc()->createElement("feature");
			feature.setAttribute("var", AHC_NS);
			query.appendChild(feature);

			feature = doc()->createElement("feature");
			feature.setAttribute("var", "jabber:x:data");
			query.appendChild(feature);

			send(iq);
			return true;
		}

	}
	
	return false;
}

bool JT_AHCServer::commandExecuteQuery(const QDomElement& e)
{
	if (e.attribute("type") == "set") {
		bool found;
		QDomElement q = findSubTag(e, "command", &found);
		if (found && q.attribute("xmlns") == AHC_NS && manager_->hasServer(q.attribute("node"), Jid(e.attribute("from")))) {
			AHCommand command(q);
			manager_->execute(command, Jid(e.attribute("from")), e.attribute("id"));
			return true;
		} 
		else 
			return false;
	}
	return false;
}

void JT_AHCServer::sendCommandList(const QString& to, const QString& from, const QString& id) 
{
	// Create query element 
	QDomElement iq = createIQ(doc(), "result", to, id);
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "http://jabber.org/protocol/disco#items");
	query.setAttribute("node", AHC_NS);
	iq.appendChild(query);

	// Add all commands
	foreach(AHCommandServer* c, manager_->commands(Jid(to))) {
		QDomElement command = doc()->createElement("item");
		command.setAttribute("jid",from);
		command.setAttribute("name",c->name());
		command.setAttribute("node",c->node());
		query.appendChild(command);
	}

	// Send the message
	send(iq);
}
	
void JT_AHCServer::sendReply(const AHCommand& c, const Jid& to, const QString& id)
{
	// if (c.error().type() != AHCError::None) {
		QDomElement iq = createIQ(doc(), "result", to.full(), id);
		QDomElement command = c.toXml(doc(), false);
		iq.appendChild(command);
		send(iq);
	// }
	// else {
	// }
}

// -------------------------------------------------------------------------- 

AHCServerManager::AHCServerManager(PsiAccount *pa) : pa_(pa)
{
	server_task_ = new JT_AHCServer(pa_->client()->rootTask(), this);
}

void AHCServerManager::addServer(AHCommandServer* server)
{
	servers_.append(server);
}

void AHCServerManager::removeServer(AHCommandServer* server)
{
	servers_.removeAll(server);
}

AHCServerManager::ServerList AHCServerManager::commands(const Jid& j) const 
{
	ServerList list;
	for (ServerList::ConstIterator it = servers_.begin(); it != servers_.end(); ++it) {
		if ((*it)->isAllowed(j)) 
			list.append(*it);
	}
	return list;
}


void AHCServerManager::execute(const AHCommand& command, const Jid& requester, QString id)
{
	AHCommandServer* c = findServer(command.node());

	// Check if the command is provided
	if (!c) {
		//server_task_->sendReply(AHCommand::errorReply(command,AHCError(AHCError::ItemNotFound)), requester, id);
		return;
	}

	// Check if the requester is allowed to execute the command
	if (c->isAllowed(requester)) {
		if (command.action() == AHCommand::Cancel) {
			c->cancel(command);
			server_task_->sendReply(AHCommand::canceledReply(command), requester, id);
		}
		else
			// Execute the command & send back the response
			server_task_->sendReply(c->execute(command, requester), requester, id);
	}
	else {
		//server_task_->sendReply(AHCommand::errorReply(command,AHCError(AHCError::Forbidden)), requester, id);
		return;
	}
}


bool AHCServerManager::hasServer(const QString& node, const Jid& requester) const
{
	AHCommandServer* c = findServer(node);
	return c && c->isAllowed(requester);
}

AHCommandServer* AHCServerManager::findServer(const QString& node) const
{
	for (ServerList::ConstIterator it = servers_.begin(); it != servers_.end(); ++it) {
		if ((*it)->node() == node) 
			return (*it);
	}
	return 0;
}


#include "ahcservermanager.moc"
