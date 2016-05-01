/*
 * mucmanager.cpp
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

// TODO:
// The MUCManager should listen to Client::messageReceived, and update the
// role and affiliation of the user, maybe even keep its own list of
// participants. This should make all the 'can*' functions a lot simpler,
// and take more weight of the groupchat dialog.
// The MUCManager should also signal things such as 'room destroyed' etc.
// In the most extreme case, the MUC manager also broadcasts status changes,
// taking all the protocol responsibilities from the groupchat.

#include <QObject>

#include "mucmanager.h"
#include "xmpp_xdata.h"
#include "xmpp_xmlcommon.h"
#include "xmpp_task.h"
#include "xmpp_client.h"
#include "psiaccount.h"

using namespace XMPP;

// -----------------------------------------------------------------------------

class MUCItemsTask : public Task
{
public:
	MUCItemsTask(const Jid& room, Task* parent) : Task(parent), room_(room)
	{
	}

	void set(const QList<MUCItem>& items, MUCManager::Action action = MUCManager::Unknown) {
		action_ = action;
		iq_ = createIQ(doc(), "set", room_.full(), id());

		QDomElement muc = doc()->createElement("query");
		muc.setAttribute("xmlns", "http://jabber.org/protocol/muc#admin");
		iq_.appendChild(muc);

		foreach(MUCItem item, items) {
			muc.appendChild(item.toXml(*doc()));
		}
	}

	void getByAffiliation(MUCItem::Affiliation affiliation) {
		affiliation_ = affiliation;
		iq_ = createIQ(doc(), "get", room_.full(), id());

		QDomElement muc = doc()->createElement("query");
		muc.setAttribute("xmlns", "http://jabber.org/protocol/muc#admin");
		muc.appendChild(MUCItem(MUCItem::UnknownRole, affiliation).toXml(*doc()));
		iq_.appendChild(muc);
	}

	void onGo() {
		send(iq_);
	}

	bool take(const QDomElement& x) {
		if(!iqVerify(x, room_, id()))
			return false;

		if(x.attribute("type") == "result") {
			QDomElement q = queryTag(x);
			for (QDomNode n = q.firstChild(); !n.isNull(); n = n.nextSibling()) {
				QDomElement e = n.toElement();
				if (!e.isNull() && e.tagName() == "item") {
					items_ += MUCItem(e);
				}
			}
			setSuccess();
		}
		else {
			setError(x);
		}
		return true;
	}

	const QList<MUCItem>& items() const {
		return items_;
	}

	MUCManager::Action action() const {
		return action_;
	}

	MUCItem::Affiliation affiliation() const {
		return affiliation_;
	}

private:
	QDomElement iq_;
	Jid room_;
	MUCManager::Action action_;
	MUCItem::Affiliation affiliation_;
	QList<MUCItem> items_;
};

// -----------------------------------------------------------------------------

class MUCConfigurationTask : public Task
{
public:
	MUCConfigurationTask(const Jid& room, Task* parent) : Task(parent), room_(room)
	{
	}

	void set(const XData& data) {
		iq_ = createIQ(doc(), "set", room_.full(), id());

		QDomElement muc = doc()->createElement("query");
		muc.setAttribute("xmlns", "http://jabber.org/protocol/muc#owner");
		iq_.appendChild(muc);

		muc.appendChild(data.toXml(doc()));
	}

	void get() {
		iq_ = createIQ(doc(), "get", room_.full(), id());

		QDomElement muc = doc()->createElement("query");
		muc.setAttribute("xmlns", "http://jabber.org/protocol/muc#owner");
		iq_.appendChild(muc);
	}

	void onGo() {
		send(iq_);
	}

	bool take(const QDomElement& x) {
		if(!iqVerify(x, room_, id()))
			return false;

		if(x.attribute("type") == "result") {
			QDomElement q = queryTag(x);
			for (QDomNode n = q.firstChild(); !n.isNull(); n = n.nextSibling()) {
				QDomElement e = n.toElement();
				if (!e.isNull() && e.tagName() == "x" && e.attribute("xmlns") == "jabber:x:data") {
					data_.fromXml(e);
				}
			}
			setSuccess();
		}
		else {
			setError(x);
		}
		return true;
	}

	const XData& data() const {
		return data_;
	}

private:
	QDomElement iq_;
	Jid room_;
	XData data_;
};

// -----------------------------------------------------------------------------

class MUCDestroyTask : public Task
{
public:
	MUCDestroyTask(const Jid& room, const QString& reason, const Jid& venue, Task* parent) : Task(parent), room_(room)
	{
		iq_ = createIQ(doc(), "set", room.full(), id());

		QDomElement muc = doc()->createElement("query");
		muc.setAttribute("xmlns", "http://jabber.org/protocol/muc#owner");
		iq_.appendChild(muc);

		MUCDestroy d;
		d.setJid(venue);
		d.setReason(reason);
		muc.appendChild(d.toXml(*doc()));
	}

	void onGo() {
		send(iq_);
	}

	bool take(const QDomElement& x) {
		if(!iqVerify(x, room_, id()))
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
	Jid room_;
	QDomElement iq_;
};


// -----------------------------------------------------------------------------

MUCManager::MUCManager(PsiAccount *account, const Jid& room) : account_(account), room_(room)
{
}

const Jid& MUCManager::room() const
{
	return room_;
}

void MUCManager::getConfiguration()
{
	MUCConfigurationTask* t = new MUCConfigurationTask(room_, client()->rootTask());
	connect(t,SIGNAL(finished()),SLOT(getConfiguration_finished()));
	t->get();
	t->go(true);
}

void MUCManager::setConfiguration(const XMPP::XData& c)
{
	MUCConfigurationTask* t = new MUCConfigurationTask(room_, client()->rootTask());
	XData config = c;
	config.setType(XData::Data_Submit);
	connect(t,SIGNAL(finished()),SLOT(setConfiguration_finished()));
	t->set(config);
	t->go(true);
}

void MUCManager::setDefaultConfiguration()
{
	setConfiguration(XData());
}

void MUCManager::destroy(const QString& reason, const Jid& venue)
{
	MUCDestroyTask* t = new MUCDestroyTask(room_, reason, venue, client()->rootTask());
	connect(t,SIGNAL(finished()),SLOT(destroy_finished()));
	t->go(true);
}

XMPP::Client* MUCManager::client() const
{
	return account_->client();
}

PsiAccount *MUCManager::account() const
{
	return account_;
}

void MUCManager::setItems(const QList<MUCItem>& items)
{
	MUCItemsTask* t = new MUCItemsTask(room_, client()->rootTask());
	connect(t,SIGNAL(finished()),SLOT(setItems_finished()));
	t->set(items);
	t->go(true);
}

void MUCManager::getItemsByAffiliation(MUCItem::Affiliation affiliation)
{
	MUCItemsTask* t = new MUCItemsTask(room_, client()->rootTask());
	connect(t,SIGNAL(finished()),SLOT(getItemsByAffiliation_finished()));
	t->getByAffiliation(affiliation);
	t->go(true);
}

void MUCManager::kick(const QString& nick, const QString& reason)
{
	setRole(nick, MUCItem::NoRole, reason, Kick);
}

void MUCManager::grantVoice(const QString& nick, const QString& reason)
{
	setRole(nick, MUCItem::Participant, reason, GrantVoice);
}

void MUCManager::revokeVoice(const QString& nick, const QString& reason)
{
	setRole(nick, MUCItem::Visitor, reason, RevokeVoice);
}

void MUCManager::grantModerator(const QString& nick, const QString& reason)
{
	setRole(nick, MUCItem::Moderator, reason, GrantModerator);
}

void MUCManager::revokeModerator(const QString& nick, const QString& reason)
{
	setRole(nick, MUCItem::Participant, reason, RevokeModerator);
}

void MUCManager::ban(const Jid& user, const QString& reason)
{
	setAffiliation(user, MUCItem::Outcast, reason, Ban);
}

void MUCManager::grantMember(const Jid& user, const QString& reason)
{
	setAffiliation(user, MUCItem::Member, reason, GrantMember);
}

void MUCManager::revokeMember(const Jid& user, const QString& reason)
{
	setAffiliation(user, MUCItem::NoAffiliation, reason, RevokeMember);
}

void MUCManager::grantOwner(const Jid& user, const QString& reason)
{
	setAffiliation(user, MUCItem::Owner, reason, GrantOwner);
}

void MUCManager::revokeOwner(const Jid& user, const QString& reason)
{
	setAffiliation(user, MUCItem::Member, reason, RevokeOwner);
}

void MUCManager::grantAdmin(const Jid& user, const QString& reason)
{
	setAffiliation(user, MUCItem::Admin, reason, GrantAdmin);
}

void MUCManager::revokeAdmin(const Jid& user, const QString& reason)
{
	setAffiliation(user, MUCItem::Member, reason, RevokeAdmin);
}

void MUCManager::setRole(const QString& nick, MUCItem::Role role, const QString& reason, Action action)
{
	QList<MUCItem> items;
	MUCItem item(role,MUCItem::UnknownAffiliation);
	item.setNick(nick);
	if (!reason.isEmpty())
		item.setReason(reason);
	items.push_back(item);

	MUCItemsTask* t = new MUCItemsTask(room_, client()->rootTask());
	connect(t,SIGNAL(finished()),SLOT(action_finished()));
	t->set(items,action);
	t->go(true);
}

void MUCManager::setAffiliation(const Jid& user, MUCItem::Affiliation affiliation, const QString& reason, Action action)
{
	QList<MUCItem> items;
	MUCItem item(MUCItem::UnknownRole,affiliation);
	item.setJid(user.bare());
	if (!reason.isEmpty())
		item.setReason(reason);
	items.push_back(item);

	MUCItemsTask* t = new MUCItemsTask(room_, client()->rootTask());
	connect(t,SIGNAL(finished()),SLOT(action_finished()));
	t->set(items,action);
	t->go(true);
}

QString MUCManager::roleToString(MUCItem::Role r, bool p)
{
	QString s;
	switch (r) {
		case MUCItem::Moderator:
			s = (p ? QObject::tr("a moderator") : QObject::tr("moderator"));
			break;
		case MUCItem::Participant:
			s = (p ? QObject::tr("a participant") : QObject::tr("participant"));
			break;
		case MUCItem::Visitor:
			s = (p ? QObject::tr("a visitor") : QObject::tr("visitor"));
			break;
		default:
			s = "";
	}
	return s;
}

QString MUCManager::affiliationToString(MUCItem::Affiliation a, bool p)
{
	QString s;
	switch (a) {
		case MUCItem::Owner:
			s = (p ? QObject::tr("an owner") : QObject::tr("owner"));
			break;
		case MUCItem::Admin:
			s = (p ? QObject::tr("an administrator") : QObject::tr("administrator"));
			break;
		case MUCItem::Member:
			s = (p ? QObject::tr("a member") : QObject::tr("member"));
			break;
		case MUCItem::Outcast:
			s = (p ? QObject::tr("an outcast") : QObject::tr("outcast"));
			break;
		case MUCItem::NoAffiliation:
			s = QObject::tr("unaffiliated");
		default:
			break;
	}
	return s;
}

bool MUCManager::canKick(const XMPP::MUCItem& i1, const XMPP::MUCItem& i2)
{
	return i1.role() == MUCItem::Moderator && i1.affiliation() >= i2.affiliation();
}

bool MUCManager::canGrantVoice(const XMPP::MUCItem& i1, const XMPP::MUCItem& i2)
{
	return i1.role() == MUCItem::Moderator && i2.role() < MUCItem::Participant;
}

bool MUCManager::canRevokeVoice(const XMPP::MUCItem& i1, const XMPP::MUCItem& i2)
{
	return i1.role() == MUCItem::Moderator && i1.affiliation() >= i2.affiliation() && i2.affiliation() < MUCItem::Owner && i2.role() >= MUCItem::Participant;
}

bool MUCManager::canGrantModerator(const XMPP::MUCItem& i1, const XMPP::MUCItem& i2)
{
	return i1.affiliation() >= MUCItem::Admin && i2.role() < MUCItem::Moderator;
}

bool MUCManager::canRevokeModerator(const XMPP::MUCItem& i1, const XMPP::MUCItem& i2)
{
	return i1.affiliation() >= MUCItem::Admin && i2.affiliation() < MUCItem::Admin && i2.role() >= MUCItem::Moderator;
}

bool MUCManager::canBan(const XMPP::MUCItem& i1, const XMPP::MUCItem& i2)
{
	return i1.affiliation() >= MUCItem::Admin && i1.affiliation() >= i2.affiliation() && !i2.jid().isEmpty();
}

bool MUCManager::canGrantMember(const XMPP::MUCItem& i1, const XMPP::MUCItem& i2)
{
	return i1.affiliation() >= MUCItem::Admin && i2.affiliation() < MUCItem::Member && !i2.jid().isEmpty();
}

bool MUCManager::canRevokeMember(const XMPP::MUCItem& i1, const XMPP::MUCItem& i2)
{
	return i1.affiliation() >= MUCItem::Admin && i2.affiliation() >= MUCItem::Member && !i2.jid().isEmpty();
}

bool MUCManager::canGrantOwner(const XMPP::MUCItem& i1, const XMPP::MUCItem& i2)
{
	return i1.affiliation() == MUCItem::Owner && i2.affiliation() < MUCItem::Owner && !i2.jid().isEmpty();
}

bool MUCManager::canRevokeOwner(const XMPP::MUCItem& i1, const XMPP::MUCItem& i2)
{
	return i1.affiliation() >= MUCItem::Owner && i2.affiliation() == MUCItem::Owner && !i2.jid().isEmpty();
}

bool MUCManager::canGrantAdmin(const XMPP::MUCItem& i1, const XMPP::MUCItem& i2)
{
	return i1.affiliation() == MUCItem::Owner && i2.affiliation() < MUCItem::Admin && !i2.jid().isEmpty();
}

bool MUCManager::canRevokeAdmin(const XMPP::MUCItem& i1, const XMPP::MUCItem& i2)
{
	return i1.affiliation() >= MUCItem::Owner && i2.affiliation() == MUCItem::Admin && !i2.jid().isEmpty();
}

void MUCManager::getConfiguration_finished()
{
	MUCConfigurationTask* t = (MUCConfigurationTask*) sender();
	if (t->success()) {
		emit getConfiguration_success(t->data());
	}
	else {
		emit getConfiguration_error(t->statusCode(), t->statusString());
	}
}

bool MUCManager::canSetRole(const XMPP::MUCItem& i1, const XMPP::MUCItem& i2, XMPP::MUCItem::Role r)
{
	if (i2.role() == r)
		return true;

	if (r == MUCItem::Visitor) {
		return (i2.role() == MUCItem::Moderator ? canRevokeModerator(i1,i2) : canRevokeVoice(i1,i2));
	}
	else if (r == MUCItem::Participant) {
		return (i2.role() < r ? canGrantVoice(i1,i2) : canRevokeModerator(i1,i2));
	}
	else if (r == MUCItem::Moderator) {
		return canGrantModerator(i1,i2);
	}

	return false;
}

bool MUCManager::canSetAffiliation(const XMPP::MUCItem& i1, const XMPP::MUCItem& i2, XMPP::MUCItem::Affiliation a)
{
	if (i2.affiliation() == a)
		return true;

	if (!i2.jid().isValid())
		return false;

	if (a == MUCItem::Outcast) {
		return canBan(i1,i2);
	}
	else if (a == MUCItem::NoAffiliation) {
		return (i2.affiliation() >= MUCItem::Admin ? canRevokeAdmin(i1,i2) : canRevokeMember(i1,i2));
	}
	else if (a == MUCItem::Member) {
		return (i2.affiliation() >= MUCItem::Admin ? canRevokeAdmin(i1,i2) : canGrantMember(i1,i2));
	}
	else if (a == MUCItem::Admin) {
		return (i2.affiliation() == MUCItem::Owner ? canRevokeOwner(i1,i2) : canGrantAdmin(i1,i2));
	}
	else if (a == MUCItem::Owner) {
		return canGrantOwner(i1,i2);
	}

	return false;
}

void MUCManager::setConfiguration_finished()
{
	MUCConfigurationTask* t = (MUCConfigurationTask*) sender();
	if (t->success()) {
		emit setConfiguration_success();
	}
	else {
		emit setConfiguration_error(t->statusCode(), t->statusString());
	}
}

void MUCManager::action_finished()
{
	MUCItemsTask* t = (MUCItemsTask*) sender();
	if (t->success()) {
		emit action_success(t->action());
	}
	else {
		QString text;
		Action action = t->action();
		if (t->statusCode() == 405) {
			if (action == Kick)
				text = tr("You are not allowed to kick this user.");
			else if (action == Ban)
				text = tr("You are not allowed to ban this user.");
			else if (action == GrantVoice)
				text = tr("You are not allowed to grant voice to this user.");
			else if (action == RevokeVoice)
				text = tr("You are not allowed to revoke voice from this user.");
			else if (action == GrantMember)
				text = tr("You are not allowed to grant membership to this user.");
			else if (action == RevokeMember)
				text = tr("You are not allowed to revoke membership from this user.");
			else if (action == GrantModerator)
				text = tr("You are not allowed to grant moderator privileges to this user.");
			else if (action == RevokeModerator)
				text = tr("You are not allowed to revoke moderator privileges from this user.");
			else if (action == GrantAdmin)
				text = tr("You are not allowed to grant administrative privileges to this user.");
			else if (action == RevokeAdmin)
				text = tr("You are not allowed to revoke administrative privileges from this user.");
			else if (action == GrantOwner)
				text = tr("You are not allowed to grant ownership privileges to this user.");
			else if (action == RevokeOwner)
				text = tr("You are not allowed to revoke ownership privileges from this user.");
			else
				text = tr("You are not allowed to perform this operation.");
		}
		else {
			text = tr("Failed to perform operation: ") + t->statusString();
		}
		emit action_error(t->action(), t->statusCode(), text);
	}
}

void MUCManager::getItemsByAffiliation_finished()
{
	MUCItemsTask* t = (MUCItemsTask*) sender();
	if (t->success()) {
		emit getItemsByAffiliation_success(t->affiliation(), t->items());
	}
	else {
		emit getItemsByAffiliation_error(t->affiliation(), t->statusCode(), t->statusString());
	}
}

void MUCManager::setItems_finished()
{
	MUCItemsTask* t = (MUCItemsTask*) sender();
	if (t->success()) {
		emit setItems_success();
	}
	else {
		emit setItems_error(t->statusCode(), t->statusString());
	}
}

void MUCManager::destroy_finished()
{
	MUCDestroyTask* t = (MUCDestroyTask*) sender();
	if (t->success()) {
		emit destroy_success();
	}
	else {
		emit destroy_error(t->statusCode(), t->statusString());
	}
}
