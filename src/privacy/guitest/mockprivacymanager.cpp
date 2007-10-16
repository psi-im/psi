/*
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

#include <QDebug>
 
#include "mockprivacymanager.h"
#include "privacylist.h"
#include "privacylistitem.h"

MockPrivacyManager::MockPrivacyManager()
{
}

void MockPrivacyManager::requestListNames()
{
	qDebug() << "requestListNames()";
	QStringList lists;
	lists << "a" << "b" << "c";
	emit listsReceived("a","c",lists);
}

void MockPrivacyManager::changeDefaultList(const QString& name)
{
	qDebug() << "changeDefaultList(" << name << ")";
}

void MockPrivacyManager::changeActiveList(const QString& name)
{
	qDebug() << "changeActiveList(" << name << ")";
}

void MockPrivacyManager::changeList(const PrivacyList& list)
{
	qDebug() << "changeList(" << list.name() << ")";
}

void MockPrivacyManager::getDefaultList()
{
	qDebug() << "getDefaultList()";
}

void MockPrivacyManager::requestList(const QString& name)
{
	qDebug() << "requestList(" << name << ")";
	QList<PrivacyListItem> items;
	if (name == "a") {
		items += createItem(PrivacyListItem::JidType, "me@example.com", PrivacyListItem::Deny, true, true, false, false);
		items += createItem(PrivacyListItem::FallthroughType, "", PrivacyListItem::Allow, true, true, true, true);
	}
	else if (name == "b") {
		items += createItem(PrivacyListItem::GroupType, "mygroup", PrivacyListItem::Deny, false, false, true, true);
	}
	else if (name == "c") {
		items += createItem(PrivacyListItem::SubscriptionType, "to", PrivacyListItem::Allow, false, false, false, false);
		items += createItem(PrivacyListItem::FallthroughType, "", PrivacyListItem::Deny, true, true, true, true);
	}
	PrivacyList list(name, items);
	emit listReceived(list);
}

PrivacyListItem MockPrivacyManager::createItem(PrivacyListItem::Type type, const QString& value, PrivacyListItem::Action action, bool message, bool presence_in, bool presence_out, bool iq)
{
	PrivacyListItem item;
	item.setType(type);
	item.setValue(value);
	item.setAction(action);
	item.setMessage(message);
	item.setPresenceIn(presence_in);
	item.setPresenceOut(presence_out);
	item.setIQ(iq);
	return item;
}
