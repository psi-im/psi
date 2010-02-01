/*
 * psicontactlistmodel.cpp - a ContactListModel subclass that does Psi-specific things
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

#include "psicontactlistmodel.h"

#include "psicontact.h"
#include "psiaccount.h"
#include "userlist.h"
#include "contactlistgroup.h"
#include "contactlistaccountgroup.h"

PsiContactListModel::PsiContactListModel(PsiContactList* contactList)
	: ContactListDragModel(contactList)
{
}

QVariant PsiContactListModel::data(const QModelIndex &index, int role) const
{
	return ContactListDragModel::data(index, role);
}

bool PsiContactListModel::setData(const QModelIndex& index, const QVariant& data, int role)
{
	return ContactListDragModel::setData(index, data, role);
}

QVariant PsiContactListModel::contactData(const PsiContact* contact, int role) const
{
	if (role == Qt::ToolTipRole) {
		return QVariant(contact->userListItem().makeTip(true, false));
	}

	return ContactListDragModel::contactData(contact, role);
}

QVariant PsiContactListModel::contactGroupData(const ContactListGroup* group, int role) const
{
	if (role == Qt::ToolTipRole) {
		QString text = itemData(group, Qt::DisplayRole).toString();
		text += QString(" (%1/%2)")
		        .arg(itemData(group, ContactListModel::OnlineContactsRole).toInt())
		        .arg(itemData(group, ContactListModel::TotalContactsRole).toInt());
		return QVariant(text);
	}

	return ContactListDragModel::contactGroupData(group, role);
}

QVariant PsiContactListModel::accountData(const ContactListAccountGroup* account, int role) const
{
	if (role == Qt::ToolTipRole) {
		return itemData(account->account()->selfContact(), role);
	}

	return ContactListDragModel::accountData(account, role);
}
