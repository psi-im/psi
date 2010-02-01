/*
 * contactlistgroupcache.cpp - contact list group cache
 * Copyright (C) 2008-2010  Yandex LLC (Michail Pishchagin)
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

#include "contactlistgroupcache.h"

#include <QStringList>

#include "contactlistgroup.h"
#include "psicontact.h"

ContactListGroupCache::ContactListGroupCache(QObject *parent)
	: QObject(parent)
{
}

ContactListGroupCache::~ContactListGroupCache()
{
}

QStringList ContactListGroupCache::groups() const
{
	return groups_.keys();
}

QList<ContactListGroup*> ContactListGroupCache::groupsFor(PsiContact* contact) const
{
	return contacts_[contact];
}

ContactListGroup* ContactListGroupCache::findGroup(const QString& fullName) const
{
	return groups_[fullName];
}

void ContactListGroupCache::addContact(ContactListGroup* group, PsiContact* contact)
{
	Q_ASSERT(group);
	Q_ASSERT(contact);
	if (!contacts_.contains(contact))
		contacts_[contact] = QList<ContactListGroup*>();

	if (!contacts_[contact].contains(group))
		contacts_[contact] << group;
}

void ContactListGroupCache::removeContact(ContactListGroup* group, PsiContact* contact)
{
	Q_ASSERT(group);
	Q_ASSERT(contact);
	if (contacts_[contact].contains(group))
		contacts_[contact].removeAll(group);

	if (contacts_[contact].isEmpty())
		contacts_.remove(contact);
}

void ContactListGroupCache::addGroup(ContactListGroup* group)
{
	Q_ASSERT(group);
	groups_[group->fullName()] = group;
}

void ContactListGroupCache::removeGroup(ContactListGroup* group)
{
	groups_.remove(group->fullName());
}

bool ContactListGroupCache::hasContacts(bool onlineOnly) const
{
	bool result = false;
	QHashIterator<PsiContact*, QList<ContactListGroup*> > it(contacts_);
	while (it.hasNext()) {
		it.next();
		if (!it.key()->isHidden()) {
			if (onlineOnly && !it.key()->isOnline())
				continue;

			result = true;
			break;
		}
	}
	return result;
}
