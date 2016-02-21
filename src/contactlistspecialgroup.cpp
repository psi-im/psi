/*
 * contactlistspecialgroup.cpp
 * Copyright (C) 2009-2010  Michail Pishchagin
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

#include "contactlistspecialgroup.h"
#include "psicontact.h"

#include <QStringList>

ContactListSpecialGroup::ContactListSpecialGroup(ContactListModel* model, ContactListGroup* parent, ContactListGroup::SpecialType type)
	: ContactListNestedGroup(model, parent, QString())
	, specialType_(type)
{
	name_ = QString::fromUtf8("☣special_group_");
	switch (specialType_) {
	case SpecialType_General:
		name_ += "general";
		displayName_ = tr("General");
		break;
	case SpecialType_NotInList:
		name_ += "notinlist";
		displayName_ = tr("Not in List");
		break;
	case SpecialType_Transports:
		name_ += "transports";
		displayName_ = tr("Agents/Transports");
		break;
	case SpecialType_MUCPrivateChats:
		name_ += "mucprivatechats";
		displayName_ = tr("Private messages");
		break;
	case SpecialType_Conference:
		name_ += "conferences";
		displayName_ = tr("Conferences");
		break;
	default:
		Q_ASSERT(false);
	}
	quietSetName(name_);
}

QString ContactListSpecialGroup::sourceOperationsForSpecialGroupContact(PsiContact* contact) const
{
	Q_UNUSED(contact)
	return QString();
}

QString ContactListSpecialGroup::destinationOperationsForSpecialGroupContact(PsiContact* contact) const
{
	Q_UNUSED(contact)
	return QString();
}

QStringList ContactListSpecialGroup::moveOperationsForSpecialGroupContact(PsiContact* contact) const
{
	Q_UNUSED(contact)
	Q_ASSERT(isEditable());
	QStringList result;
	if (specialType_ == SpecialType_General) {
		result << QString();
	}
	return result;
}

bool ContactListSpecialGroup::isSpecial() const
{
	return true;
}

ContactListGroup::SpecialType ContactListSpecialGroup::specialGroupType() const
{
	return specialType_;
}

QString ContactListSpecialGroup::internalGroupName() const
{
	return name_;
}

const QString& ContactListSpecialGroup::name() const
{
	return displayName_;
}

bool ContactListSpecialGroup::compare(const ContactListItem* other) const
{
	const ContactListGroup* group = dynamic_cast<const ContactListGroup*>(other);
	if (group) {
		QMap<ContactListGroup::SpecialType, int> rank;
		rank[SpecialType_General]         = 0;
		rank[SpecialType_None]            = 1;
		rank[SpecialType_MUCPrivateChats] = 2;
		rank[SpecialType_Conference]      = 3;
		rank[SpecialType_NotInList]       = 4;
		rank[SpecialType_Transports]      = 5;
		Q_ASSERT(rank.contains(specialGroupType()));
		Q_ASSERT(rank.contains(group->specialGroupType()));
		return rank[specialGroupType()] < rank[group->specialGroupType()];
	}

	return ContactListGroup::compare(other);
}

bool ContactListSpecialGroup::isDragEnabled() const
{
	return false;
}

bool ContactListSpecialGroup::isEditable() const
{
	return false;	//specialType_ == SpecialType_General;
}

bool ContactListSpecialGroup::isRemovable() const
{
	return specialType_ == SpecialType_General ||
	       specialType_ == SpecialType_NotInList ||
	       specialType_ == SpecialType_MUCPrivateChats;
}

void ContactListSpecialGroup::addContact(PsiContact* contact, QStringList contactGroups)
{
	ContactListGroup::addContact(contact, contactGroups);
}

void ContactListSpecialGroup::contactGroupsChanged(PsiContact* contact, QStringList contactGroups)
{
	ContactListGroup::contactGroupsChanged(contact, contactGroups);
}
