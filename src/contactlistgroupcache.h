/*
 * contactlistgroupcache.h - contact list group cache
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

#ifndef CONTACTLISTGROUPCACHE_H
#define CONTACTLISTGROUPCACHE_H

#include <QHash>

class ContactListGroup;
class PsiContact;

class ContactListGroupCache : public QObject
{
	Q_OBJECT
public:
	ContactListGroupCache(QObject *parent);
	~ContactListGroupCache();

	QStringList groups() const;
	bool hasContacts(bool onlineOnly) const;

	QList<ContactListGroup*> groupsFor(PsiContact* contact) const;
	ContactListGroup* findGroup(const QString& fullName) const;

	void addContact(ContactListGroup* group, PsiContact* contact);
	void removeContact(ContactListGroup* group, PsiContact* contact);

	void addGroup(ContactListGroup* group);
	void removeGroup(ContactListGroup* group);

private:
	QHash<PsiContact*, QList<ContactListGroup*> > contacts_;
	QHash<QString, ContactListGroup*> groups_;
};

#endif
