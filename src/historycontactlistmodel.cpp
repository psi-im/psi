/*
 * historycontactlistmodel.cpp
 * Copyright (C) 2017  Aleksey Andreev
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


#include "historycontactlistmodel.h"
#include "eventdb.h"
#include "psiaccount.h"
#include "psicontact.h"
#include "psiiconset.h"
#include "jidutil.h"


HistoryContactListModel::HistoryContactListModel(QObject *parent)
	: QAbstractItemModel(parent)
	, rootItem(NULL)
	, generalGroup(NULL)
	, notInList(NULL)
	, confPrivate(NULL)
	, dispPrivateContacts(false)
	, dispAllContacts(false)
{
}

HistoryContactListModel::~HistoryContactListModel()
{
	delete rootItem;
}

void HistoryContactListModel::clear()
{
	delete rootItem;
	rootItem     = NULL;
	generalGroup = NULL;
	notInList    = NULL;
	confPrivate  = NULL;
}

void HistoryContactListModel::updateContacts(PsiCon *psi, const QString &id)
{
	beginResetModel();
	clear();
	loadContacts(psi, id);
	endResetModel();
}

int HistoryContactListModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid())
	{
		TreeItem *pItem = static_cast<TreeItem*>(parent.internalPointer());
		return pItem->childCount();
	}
	return rootItem ? rootItem->childCount() : 0;
}

int HistoryContactListModel::columnCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	return 1;
}

QVariant HistoryContactListModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

	TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
	switch (role)
	{
	case Qt::DisplayRole:
	case Qt::ToolTipRole:
		if (item->type() == Group)
			return QString("%1 (%2)").arg(item->text(false)).arg(item->childCount());
		return item->text(role == Qt::ToolTipRole);
	case Qt::DecorationRole:
		if (item->type() == RosterContact)
			return PsiIconset::instance()->statusPtr(item->id().section('|', 1, 1), Status::Online)->icon();
		if (item->type() == NotInRosterContact)
			return PsiIconset::instance()->statusPtr(item->id().section('|', 1, 1), Status::Offline)->icon();
		break;
	case ItemIdRole:
		return item->id();
	case ItemPosRole:
		return item->position();
	case ItemTypeRole:
		return item->type();
	default:
		break;
	}

	return QVariant();
}

Qt::ItemFlags HistoryContactListModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return 0;
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex HistoryContactListModel::index(int row, int column, const QModelIndex &parent) const
{
	if (!hasIndex(row, column, parent) || column > 0)
		return QModelIndex();

	TreeItem *parentItem;
	if (parent.isValid())
		parentItem = static_cast<TreeItem*>(parent.internalPointer());
	else
		parentItem = rootItem;

	TreeItem *item = parentItem->child(row);
	if (!item)
		return QModelIndex();

	return createIndex(row, 0, item);
}

QModelIndex HistoryContactListModel::parent(const QModelIndex &child) const
{
	if (!child.isValid())
		return QModelIndex();

	TreeItem *childItem = static_cast<TreeItem*>(child.internalPointer());
	TreeItem *parentItem = childItem->parent();
	if (parentItem->type() == Root)
		return QModelIndex();

	return createIndex(parentItem->row(), 0, parentItem);
}

void HistoryContactListModel::loadContacts(PsiCon *psi, const QString &acc_id)
{
	rootItem = new TreeItem(Root, QString());
	generalGroup = new TreeItem(Group, tr("General"), "general", -1);
	rootItem->appendChild(generalGroup);

	QList<PsiContact*> contactList;
	if (acc_id.isEmpty())
		contactList = psi->contactList()->contacts();
	else
		contactList = psi->contactList()->getAccount(acc_id)->contactList();
	QHash<QString, bool> c_list;
	QHash<QString, TreeItem*> groups;
	// Roster contacts
	foreach (PsiContact* contact, contactList)
	{
		if (contact->isConference() || contact->isPrivate())
			continue;
		QString cId = contact->account()->id() + "|" + contact->jid().bare();
		if (c_list.value(cId))
			continue;

		TreeItem *groupItem = NULL;
		if (contact->groups().count() > 0)
		{
			QString g = contact->groups().at(0);
			if (!g.isEmpty())
			{
				groupItem = groups.value(g);
				if (!groupItem)
				{
					groupItem = new TreeItem(Group, g);
					rootItem->appendChild(groupItem);
					groups[g] = groupItem;
				}
			}
			if (!groupItem)
				groupItem = generalGroup;
		}
		QString tooltipStr = makeContactToolTip(psi, acc_id, contact->jid(), true);
		groupItem->appendChild(new TreeItem(RosterContact, contact->name(), tooltipStr, cId));
		c_list[cId] = true;
	}
	// Self contact
	foreach (PsiAccount *pa, psi->contactList()->accounts())
	{
		if (acc_id.isEmpty() || pa->id() == acc_id)
		{
			PsiContact *self = pa->selfContact();
			QString cId = pa->id() + "|" + self->jid().bare();
			if (c_list.value(cId))
				continue;

			QString tooltipStr = makeContactToolTip(psi, acc_id, self->jid(), true);
			generalGroup->appendChild(new TreeItem(RosterContact, self->name(), tooltipStr, cId));
			c_list[cId] = true;
			if (!acc_id.isEmpty())
				break;
		}
	}
	// Not in roster list
	foreach (const EDB::ContactItem &ci, psi->edb()->contacts(acc_id, EDB::Contact))
	{
		QString cId = ci.accId + "|" + ci.jid.bare();
		if (c_list.value(cId))
			continue;

		if (!notInList)
		{
			notInList = new TreeItem(Group, tr("Not in list"), "not-in-list", 10);
			rootItem->appendChild(notInList);
		}
		QString tooltipStr = makeContactToolTip(psi, acc_id, ci.jid, true);
		notInList->appendChild(new TreeItem(NotInRosterContact, ci.jid.bare(), tooltipStr, cId));
		c_list[cId] = true;
	}
	// Private messages
	if (dispPrivateContacts)
	{
		foreach (const EDB::ContactItem &ci, psi->edb()->contacts(acc_id, EDB::GroupChatContact))
		{
			if (!confPrivate)
			{
				confPrivate = new TreeItem(Group, tr("Private messages"), "conf-private", 11);
				rootItem->appendChild(confPrivate);
			}
			QString cId = ci.accId + "|" + ci.jid.full();
			QString tooltipStr = makeContactToolTip(psi, acc_id, ci.jid, false);
			confPrivate->appendChild(new TreeItem(NotInRosterContact, ci.jid.resource(), tooltipStr, cId));
		}
	}
	if (dispAllContacts)
	{
		QString s = tr("All contacts");
		rootItem->appendChild(new TreeItem(Other, s, s, "*all", 12));
	}
}

bool HistoryContactListModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if (row < 0 || count <= 0)
		return false;

	TreeItem *parentItem;
	if (parent.isValid())
		parentItem = static_cast<TreeItem*>(parent.internalPointer());
	else
		parentItem = rootItem;
	if (!parentItem || row >= parentItem->childCount())
		return false;

	int childCount = parentItem->childCount();
	if (row + count > childCount)
		count = childCount - row;

	beginRemoveRows(parent, row, row + count - 1);
	for ( ; count > 0; --count)
		parentItem->removeChild(row);
	endRemoveRows();
	return true;
}

QString HistoryContactListModel::makeContactToolTip(PsiCon *psi, const QString &accId, const XMPP::Jid &jid, bool bare) const
{
	PsiAccount *pa = psi->contactList()->getAccount(accId);
	return QString("%1 [%2]").arg(JIDUtil::toString(jid, !bare)).arg((pa) ? pa->name() : tr("deleted"));
}

HistoryContactListModel::TreeItem::TreeItem(ItemType type, const QString &text, const QString &id, int pos)
	: _parent(NULL)
	, _type(type)
	, _text(text)
	, _id(id)
	, _position(pos)
{
}

HistoryContactListModel::TreeItem::TreeItem(ItemType type, const QString &text, const QString &tooltip, const QString &id, int pos)
	: _parent(NULL)
	, _type(type)
	, _text(text)
	, _tooltip(tooltip)
	, _id(id)
	, _position(pos)
{
}

HistoryContactListModel::TreeItem::~TreeItem()
{
	qDeleteAll(child_items);
}

void HistoryContactListModel::TreeItem::appendChild(TreeItem *item)
{
	child_items.append(item);
	item->_parent = static_cast<HistoryContactListModel::TreeItem*>(this);
}

void HistoryContactListModel::TreeItem::removeChild(int row)
{
	if (row >= 0 && row < child_items.count())
		delete child_items.takeAt(row);
}

int HistoryContactListModel::TreeItem::row() const
{
	if (!_parent)
		return 0;
	return _parent->child_items.indexOf(const_cast<HistoryContactListModel::TreeItem*>(this));
}

HistoryContactListProxyModel::HistoryContactListProxyModel(QObject *parent)
	: QSortFilterProxyModel(parent)
{
	setDynamicSortFilter(true);
}

void HistoryContactListProxyModel::setFilterFixedString(const QString &pattern)
{
	if (_pattern != pattern)
	{
		_pattern = pattern;
		invalidate();
	}
}

bool HistoryContactListProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
	int lp = 0;
	int rp = 0;
	if (left.isValid() && right.isValid())
	{
		QAbstractItemModel *model = sourceModel();
		lp = model->data(left, HistoryContactListModel::ItemPosRole).toInt();
		rp = model->data(right, HistoryContactListModel::ItemPosRole).toInt();
		if (lp == rp)
		{
			QString ls = model->data(left, Qt::DisplayRole).toString().toLower();
			QString rs = model->data(right, Qt::DisplayRole).toString().toLower();
			return QString::localeAwareCompare(ls, rs) > 0;
		}
	}
	return lp > rp;
}

bool HistoryContactListProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
	if (_pattern.isEmpty())
		return true;

	QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
	if (!index.isValid())
		return false;

	if (!source_parent.isValid())
	{
		QModelIndex child = index.child(0, 0);
		while (child.isValid())
		{
			if (filterAcceptsRow(child.row(), child.parent()))
				return true;
			child = child.sibling(child.row() + 1, 0);
		}
		return false;
	}

	if (sourceModel()->data(index, Qt::DisplayRole).toString().contains(_pattern, Qt::CaseInsensitive))
		return true;
	QString jid = sourceModel()->data(index, HistoryContactListModel::ItemIdRole).toString().section('|', 1, 1);
	if (jid.contains(_pattern, Qt::CaseInsensitive))
		return true;

	return false;
}
