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

#include "contactlistmodel.h"
#include "contactlistitem.h"
#include "psiaccount.h"
#include "psicontact.h"
#include "psicontactlist.h"
#include "userlist.h"
#include "debug.h"

ContactListProxyModel::ContactListProxyModel(QObject* parent)
	: QSortFilterProxyModel(parent)
{
	sort(0, Qt::AscendingOrder);

	// False by default on Qt4
	setDynamicSortFilter(true);
}

void ContactListProxyModel::setSourceModel(QAbstractItemModel* model)
{
	Q_ASSERT(qobject_cast<ContactListModel*>(model));
	QSortFilterProxyModel::setSourceModel(model);
	connect(model, SIGNAL(showOfflineChanged()), SLOT(filterParametersChanged()));
	connect(model, SIGNAL(showSelfChanged()), SLOT(filterParametersChanged()));
	connect(model, SIGNAL(showTransportsChanged()), SLOT(filterParametersChanged()));
	connect(model, SIGNAL(showHiddenChanged()), SLOT(filterParametersChanged()));
	connect(model, SIGNAL(contactSortStyleChanged()), SLOT(updateSorting()));
}

bool ContactListProxyModel::showOffline() const
{
	return qobject_cast<ContactListModel*>(sourceModel())->showOffline();
}

bool ContactListProxyModel::showSelf() const
{
	return qobject_cast<ContactListModel*>(sourceModel())->showSelf();
}

bool ContactListProxyModel::showTransports() const
{
	return qobject_cast<ContactListModel*>(sourceModel())->showTransports();
}

bool ContactListProxyModel::showHidden() const
{
	return qobject_cast<ContactListModel*>(sourceModel())->showHidden();
}

bool ContactListProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
	QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
	if (!index.isValid())
		return false;

	ContactListItem *item = static_cast<ContactListItem*>(index.internalPointer());
	if (!item) {
		Q_ASSERT(false);
		return false;
	}

	if (item->editing()) {
		return true;
	}

	switch (item->type()) {
	case ContactListItem::Type::ContactType: {
		PsiContact* psiContact = item->contact();

		if (psiContact->alerting()) {
			return true;
		}

		if (psiContact->isSelf()) {
			return showSelf() && (psiContact->userListItem().userResourceList().count() > 0);
		}
		else if (psiContact->isAgent()) {
			return showTransports();
		}
		else if (psiContact->isAlwaysVisible()) {
			return true;
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
	case ContactListItem::Type::GroupType: {
		ContactListItem::SpecialGroupType type = item->specialGroupType();

		if (type == ContactListItem::SpecialGroupType::TransportsSpecialGroupType) {
			return showTransports();
		}

		if (item->shouldBeVisible())
			return true;

		bool show = true;
		if (item->name() == PsiContact::hiddenGroupName() || item->isHidden()) {
			show = showHidden();
		}

		if (!showOffline()) {
			return show && ((item->value(ContactListModel::OnlineContactsRole).toInt() > 0) ||
			                item->shouldBeVisible());
			// shouldBeVisible is updated during OnlineContactsRole.
			// So it may have different value here (see above for dup).
			// OnlineContactsRole counts visible contacts ans also finds always-visible ones.
			// Kind of bug? maybe..
		}
		else {
			return show;
		}
	}
		break;

	case ContactListItem::Type::AccountType: {
		PsiAccount *account = item->account();
		Q_ASSERT(account);
		return account->enabled();
	}

	case ContactListItem::Type::InvalidType:
		return true;

	default:
		Q_ASSERT(false);
	}

	return true;
}

bool ContactListProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
	ContactListItem* item1 = qvariant_cast<ContactListItem*>(left.data(ContactListModel::ContactListItemRole));
	ContactListItem* item2 = qvariant_cast<ContactListItem*>(right.data(ContactListModel::ContactListItemRole));
	if (!item1 || !item2)
		return false;

	ContactListModel *model = qobject_cast<ContactListModel*>(sourceModel());
	if (model->contactSortStyle() == "status" || !item1->isContact() || !item2->isContact()) {
		return item1->lessThan(item2);
	}
	else {
		return item1->name().toLower() < item2->name().toLower();
	}
}

void ContactListProxyModel::filterParametersChanged()
{
	invalidate();
	emit recalculateSize();
}

void ContactListProxyModel::updateSorting()
{
	invalidate();
}
