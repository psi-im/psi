/*
 * contactlistnestedgroup.cpp - class to handle nested contact list groups
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "contactlistnestedgroup.h"

#include <QStringList>

#include "psicontact.h"
#include "contactlistitemproxy.h"
#include "contactlistspecialgroup.h"

ContactListNestedGroup::ContactListNestedGroup(ContactListModel* model, ContactListGroup* parent, QString name)
	: ContactListGroup(model, parent)
{
	quietSetName(name);
}

ContactListNestedGroup::~ContactListNestedGroup()
{
	clearGroup();
}

void ContactListNestedGroup::clearGroup()
{
	quietSetName(QString());
	foreach(ContactListGroup* group, groups_) {
CL_DEBUG("~ContactListNextedGroup(%p): %s", this, qPrintable(group->fullName()));
		removeItem(ContactListGroup::findGroup(group));
	}
	qDeleteAll(groups_);
	groups_.clear();

	QHashIterator<ContactListGroup::SpecialType, QPointer<ContactListGroup> > it(specialGroups_);
	while (it.hasNext())
	{
		it.next();
		ContactListGroup* group = it.value().data();
		if (group)
		{
CL_DEBUG("~ContactListNextedGroup(%p): %s", this, qPrintable(group->fullName()));
			removeItem(ContactListGroup::findGroup(group));
			delete group;
		}
	}
	specialGroups_.clear();

	ContactListGroup::clearGroup();
}

void ContactListNestedGroup::addContact(PsiContact* contact, QStringList contactGroups)
{
	if (canContainSpecialGroups()) {
		ContactListGroup* group = specialGroupFor(contact);
		if (group) {
			if (!contact->isAgent())
				group->addContact(contact, contactGroups);
			else
				group->addContact(contact, QStringList());
			return;
		}
	}

	foreach(QString groupName, contactGroups) {
		if (groupName == name()) {
			ContactListGroup::addContact(contact, contactGroups);
		}
		else {
			QStringList nestedGroups;
#ifdef CONTACTLIST_NESTED_GROUPS
			nestedGroups = groupName.split(groupDelimiter());
#else
			nestedGroups += groupName;
#endif
			if (!name().isEmpty()) {
				QString firstPart = nestedGroups.takeFirst();
				Q_ASSERT(firstPart == name());
			}

			Q_ASSERT(!nestedGroups.isEmpty());

			ContactListGroup* group = findGroup(nestedGroups.first());
			if (!group) {
				group = new ContactListNestedGroup(model(), this, nestedGroups.first());
				addGroup(group);
CL_DEBUG("ContactListNextedGroup(%p)::addContact: %s", this, qPrintable(group->fullName()));
			}

			QStringList moreGroups;
			moreGroups << nestedGroups.join(groupDelimiter());
			group->addContact(contact, moreGroups);
		}
	}
}

void ContactListNestedGroup::addGroup(ContactListGroup* group)
{
	Q_ASSERT(group);
	Q_ASSERT(!groups_.contains(group));
	groups_.append(group);
	addItem(new ContactListItemProxy(this, group));
}

void ContactListNestedGroup::contactUpdated(PsiContact* contact)
{
	// since now we're using groupCache() passing contact to
	// all nested groups is not necessary anymore
	// foreach(ContactListGroup* group, groups_)
	// 	group->contactUpdated(contact);
	ContactListGroup::contactUpdated(contact);
}

#define CONTACTLISTNESTEDGROUP_OLD_CONTACTGROUPSCHANGED

void ContactListNestedGroup::contactGroupsChanged(PsiContact* contact, QStringList contactGroups)
{
	bool restrictContactAdd = false;
	if (canContainSpecialGroups()) {
		ContactListGroup* specialGroup = specialGroupFor(contact);
		QHashIterator<ContactListGroup::SpecialType, QPointer<ContactListGroup> > it(specialGroups_);
		while (it.hasNext()) {
			it.next();
			if (!it.value().isNull()) {
				it.value()->contactGroupsChanged(contact, it.value() == specialGroup ?
				                                 contactGroups : QStringList());
			}
		}

		restrictContactAdd = specialGroup != 0;
	}
	else if (!isSpecial() && contact->isAgent() && model()->groupsEnabled())
	{
		ContactListNestedGroup *rootGroup = this;
		while (rootGroup->parent())
		{
			rootGroup = static_cast<ContactListNestedGroup *>(rootGroup->parent());
			if (rootGroup->canContainSpecialGroups())
			{
				ContactListGroup* specialGroup = rootGroup->specialGroupFor(contact);
				if (specialGroup && specialGroup != this)
				{
					specialGroup->contactGroupsChanged(contact, QStringList());
					return;
				}
			}
		}
	}

#ifdef CONTACTLISTNESTEDGROUP_OLD_CONTACTGROUPSCHANGED
	bool addToSelf = false;
	QList<QStringList> splitGroupNames;
	foreach(QString group, contactGroups) {
		if (group == name()) {
			addToSelf = true;
			continue;
		}

		QStringList split;
#ifdef CONTACTLIST_NESTED_GROUPS
		split = group.split(groupDelimiter());
#else
		split += group;
#endif
		if (!name().isEmpty()) {
			QString firstPart = split.takeFirst();
			// hmm, probably should continue as the data should be invalid
		}
		splitGroupNames << split;
	}

	// here we process splitGroupNames and update nested groups which are already present
	QList<ContactListGroup*> emptyGroups;
	QVector<ContactListGroup*> unnotifiedGroups = groups_;
	QList<QStringList> newGroups;
	while (!splitGroupNames.isEmpty()) {
		const QStringList& split = splitGroupNames.first();
		ContactListGroup* group = 0;
		if (!split.isEmpty())
			group = findGroup(split.first());

		if (!group) {
			newGroups << splitGroupNames.takeFirst();
			continue;
		}

		QStringList mergedGroupNames;
		foreach(QStringList i, splitGroupNames)
			if (!i.isEmpty() && i.first() == split.first())
				mergedGroupNames += i.join(groupDelimiter());

		foreach(QString i, mergedGroupNames) {
#ifdef CONTACTLIST_NESTED_GROUPS
			splitGroupNames.removeAll(i.split(groupDelimiter()));
#else
			splitGroupNames.removeAll(QStringList() << i);
#endif
		}

		group->contactGroupsChanged(contact, mergedGroupNames);
		if (!group->itemsCount()) {
			emptyGroups << group;
		}
		unnotifiedGroups.remove(unnotifiedGroups.indexOf(group));
	}

	// remove the contact from the unnotified groups
	foreach(ContactListGroup* group, unnotifiedGroups) {
		if (group->isSpecial() && group->itemsCount())
			continue;

		group->contactGroupsChanged(contact, QStringList());
		if (!group->itemsCount()) {
			emptyGroups << group;
		}
	}

	// remove empty groups afterwards
	foreach(ContactListGroup* group, emptyGroups) {
CL_DEBUG("ContactListNextedGroup(%p)::contactGroupsChanged: removing empty group: %s", this, qPrintable(group->fullName()));
		removeItem(ContactListGroup::findGroup(group));
		groups_.remove(groups_.indexOf(group));
		delete group;
	}

	if (restrictContactAdd) {
		return;
	}

	// create new groups, if required
	foreach(QStringList split, newGroups) {
		QStringList fullGroupName;
		if (!name().isEmpty())
			fullGroupName << name();
		fullGroupName += split;
		QStringList tmp;
		tmp << fullGroupName.join(groupDelimiter());
		addContact(contact, tmp);
	}

	if (addToSelf) {
		// contact should be located here
		if (!findContact(contact)) {
			Q_ASSERT(!contactGroups.isEmpty());
			ContactListGroup::contactGroupsChanged(contact, contactGroups);
		}
	}
	else {
		// remove self
		if (findContact(contact)) {
			ContactListGroup::contactGroupsChanged(contact, QStringList());
		}
	}
#else
	QStringList newNestedGroups = fullName().isEmpty()
	                              ? contactGroups
	                              : contactGroups.filter(QRegExp(QString("^%1($|%2)").arg(fullName(), groupDelimiter())));

	QStringList directChildren;
	foreach(QString nnGroup, newNestedGroups) {
		QString unqualifiedName = nnGroup.mid(QString(fullName().isEmpty() ? "" : fullName() + groupDelimiter()).length());
		if (!unqualifiedName.isEmpty()) {
			// direct children!
			directChildren << QString(name().isEmpty() ? "" : name() + groupDelimiter()) + unqualifiedName;
		}
	}

	// let's add contacts to the children
	addContact(contact, directChildren);

	// we're running into recursion
	foreach(ContactListGroup* child, groups_) {
		child->contactGroupsChanged(contact, newNestedGroups);
		if (!child->itemsCount()) {
CL_DEBUG("ContactListNextedGroup(%x)::contactGroupsChanged: removing empty group2: %s", this, qPrintable(child->fullName()));
			removeItem(ContactListGroup::findGroup(child));
			groups_.removeAll(groups_.indexOf(child));
			delete group;
		}
	}

	// let's add/remove the contact from itself, if necessary
	ContactListGroup::contactGroupsChanged(contact, newNestedGroups.filter(QRegExp(QString("^%1$").arg(fullName()))));
#endif
}

ContactListGroup* ContactListNestedGroup::findGroup(const QString& groupName) const
{
	foreach(ContactListGroup* group, groups_) {
		if (group->name() == groupName) {
			if (!dynamic_cast<ContactListSpecialGroup*>(group))
				return group;
		}
	}

	return 0;
}

bool ContactListNestedGroup::canContainSpecialGroups() const
{
	return !parent() && model()->groupsEnabled();
}

ContactListGroup* ContactListNestedGroup::specialGroupFor(PsiContact* contact)
{
	if (!contact->isValid()) {
		return 0;
	}

	ContactListGroup::SpecialType type = ContactListGroup::SpecialType_None;
	if (contact->isPrivate()) {
		type = ContactListGroup::SpecialType_MUCPrivateChats;
	}
	else if (!contact->inList()) {
		type = ContactListGroup::SpecialType_NotInList;
	}
	else if (contact->isAgent()) {
		type = ContactListGroup::SpecialType_Transports;
	}
	else if (contact->isConference()) {
		type = ContactListGroup::SpecialType_Conference;
	}
	else if (contact->noGroups()) {
		type = ContactListGroup::SpecialType_General;
	}
	else {
		return 0;
	}

	if (!specialGroups_.contains(type) || specialGroups_[type].isNull()) {
		specialGroups_[type] = new ContactListSpecialGroup(model(), this, type);
		addGroup(specialGroups_[type]);
	}
	return specialGroups_[type];
}
