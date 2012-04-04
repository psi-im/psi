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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <QTimer>

#include "psicontactlistmodel.h"

#include "psicontact.h"
#include "psiaccount.h"
#include "contactlistgroup.h"
#include "contactlistaccountgroup.h"

PsiContactListModel::PsiContactListModel(PsiContactList* contactList)
	: ContactListDragModel(contactList)
	, secondPhase_(false)
{
	animTimer_ = new QTimer(this);
	animTimer_->setInterval(300);
	connect(animTimer_, SIGNAL(timeout()), SLOT(updateAnim()));
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
	if (role == ContactListModel::PhaseRole) {
		return QVariant(secondPhase_);
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

void PsiContactListModel::contactAnim(PsiContact* contact)
{
	QModelIndexList indexes = ContactListModel::indexesFor(contact);
	if(!indexes.isEmpty()) {
		foreach(const QModelIndex& i, indexes) {
			bool anim = data(i, ContactListModel::IsAnimRole).toBool();
			if (anim) {
				animIndexes_.insert(i, true);
			}
			else {
				animIndexes_.remove(i);
			}
		}
	}

	if (animIndexes_.isEmpty()) {
		animTimer_->stop();
	}
	else if(!animTimer_->isActive()) {
		animTimer_->start();
	}
}

void PsiContactListModel::updateAnim()
{
	secondPhase_ = !secondPhase_;
	foreach(const QModelIndex& index, animIndexes_.keys()) {
		dataChanged(index, index);
	}
}
