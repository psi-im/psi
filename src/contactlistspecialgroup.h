/*
 * contactlistspecialgroup.h
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef CONTACTLISTSPECIALGROUP_H
#define CONTACTLISTSPECIALGROUP_H

#include "contactlistnestedgroup.h"

class ContactListSpecialGroup : public ContactListNestedGroup
{
	Q_OBJECT
public:
	ContactListSpecialGroup(ContactListModel* model, ContactListGroup* parent, ContactListGroup::SpecialType type);

	QString sourceOperationsForSpecialGroupContact(PsiContact* contact) const;
	QString destinationOperationsForSpecialGroupContact(PsiContact* contact) const;
	QStringList moveOperationsForSpecialGroupContact(PsiContact* contact) const;

	// reimplemented
	virtual bool isDragEnabled() const;
	virtual bool isEditable() const;
	virtual bool isRemovable() const;
	virtual bool isSpecial() const;
	virtual SpecialType specialGroupType() const;
	virtual QString internalGroupName() const;
	virtual const QString& name() const;
	virtual bool compare(const ContactListItem* other) const;

	// reimplemented
	virtual void addContact(PsiContact* contact, QStringList contactGroups);
	virtual void contactGroupsChanged(PsiContact* contact, QStringList contactGroups);

private:
	SpecialType specialType_;
	QString name_;
	QString displayName_;
};


#endif
