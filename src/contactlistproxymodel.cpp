/*
 * contactlistproxymodel.cpp - contact list model sorting and filtering
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

#include "contactlistproxymodel.h"

#include "psicontactlist.h"
#include "psicontact.h"
#include "contactlistitem.h"
#include "contactlistgroup.h"
#include "contactlistitemproxy.h"
#include "contactlistspecialgroup.h"

ContactListProxyModel::ContactListProxyModel(QObject* parent)
	: QSortFilterProxyModel(parent)
{
	sort(0, Qt::AscendingOrder);
	setDynamicSortFilter(true);
}

void ContactListProxyModel::setSourceModel(QAbstractItemModel* model)
{
	Q_ASSERT(dynamic_cast<ContactListModel*>(model));
	QSortFilterProxyModel::setSourceModel(model);
	connect(model, SIGNAL(showOfflineChanged()), SLOT(filterParametersChanged()));
	connect(model, SIGNAL(showSelfChanged()), SLOT(filterParametersChanged()));
	connect(model, SIGNAL(showTransportsChanged()), SLOT(filterParametersChanged()));
	connect(model, SIGNAL(showHiddenChanged()), SLOT(filterParametersChanged()));
	connect(model, SIGNAL(contactSortStyleChanged()), SLOT(updateSorting()));
}

bool ContactListProxyModel::showOffline() const
{
	return static_cast<ContactListModel*>(sourceModel())->showOffline();
}

bool ContactListProxyModel::showSelf() const
{
	return static_cast<ContactListModel*>(sourceModel())->showSelf();
}

bool ContactListProxyModel::showTransports() const
{
	return static_cast<ContactListModel*>(sourceModel())->showTransports();
}

bool ContactListProxyModel::showHidden() const
{
	return static_cast<ContactListModel*>(sourceModel())->showHidden();
}

bool ContactListProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
	QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
	if (!index.isValid())
		return false;

	ContactListItemProxy* itemProxy = static_cast<ContactListItemProxy*>(index.internalPointer());
	ContactListItem* item = itemProxy ? itemProxy->item() : 0;
	if (!item) {
		Q_ASSERT(false);
		return false;
	}

	if (item->editing()) {
		return true;
	}

	switch (ContactListModel::indexType(index)) {
	case ContactListModel::ContactType: {
		PsiContact* psiContact = static_cast<PsiContact*>(item);

		if (psiContact->isSelf()) {
			return showSelf();
		}
		else if (psiContact->isAgent()) {
			return showTransports();
		}

		bool show = true;
		if (psiContact->isHidden()) {
			show = showHidden();
		}

		if (!showOffline()) {
			return show && psiContact->isOnline();
		}
		else {
			return show;
		}
	}
	case ContactListModel::GroupType:
		{
			ContactListGroup::SpecialType specialGroupType = static_cast<ContactListGroup::SpecialType>(index.data(ContactListModel::SpecialGroupTypeRole).toInt());
			if (specialGroupType != ContactListGroup::SpecialType_None) {
				if (specialGroupType == ContactListGroup::SpecialType_Transports)
					return showTransports();
			}

			bool show = true;
			if (index.data(Qt::DisplayRole) == PsiContact::hiddenGroupName()) {
				show = showHidden();
			}

			if (!showOffline()) {
				ContactListGroup* group = static_cast<ContactListGroup*>(item);
				return show && group->haveOnlineContacts();
			}
			else {
				return show;
			}
		}
	case ContactListModel::AccountType:
		return true;
	case ContactListModel::InvalidType:
		return true;
	default:
		Q_ASSERT(false);
	}

	return true;
}

bool ContactListProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
	ContactListItemProxy* item1 = static_cast<ContactListItemProxy*>(left.internalPointer());
	ContactListItemProxy* item2 = static_cast<ContactListItemProxy*>(right.internalPointer());
	if (!item1 || !item2)
		return false;

	ContactListModel *model = static_cast<ContactListModel*>(sourceModel());
	if((model->contactSortStyle() == "status") ||
	   !dynamic_cast<const PsiContact*>(item1->item()) ||
	   !dynamic_cast<const PsiContact*>(item2->item()) ) {
		return item1->item()->compare(item2->item());
	}
	else {
		return item1->item()->name().toLower() < item2->item()->name().toLower();
	}
}

void ContactListProxyModel::filterParametersChanged()
{
	invalidateFilter();
	emit recalculateSize();
}

void ContactListProxyModel::updateSorting()
{
	invalidate();
}
