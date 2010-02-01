/*
 * psicontactlistmodel.h - a ContactListModel subclass that does Psi-specific things
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

#ifndef PSICONTACTLISTMODEL_H
#define PSICONTACTLISTMODEL_H

#include "contactlistdragmodel.h"

class PsiContactList;

class PsiContactListModel : public ContactListDragModel
{
	Q_OBJECT
public:
	PsiContactListModel(PsiContactList* contactList);

	// reimplemented
	QVariant data(const QModelIndex &index, int role) const;
	bool setData(const QModelIndex& index, const QVariant& data, int role);
	virtual QVariant contactData(const PsiContact* contact, int role) const;
	virtual QVariant contactGroupData(const ContactListGroup* group, int role) const;
	virtual QVariant accountData(const ContactListAccountGroup* account, int role) const;
};

#endif
