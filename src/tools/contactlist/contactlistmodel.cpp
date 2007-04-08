#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>
#include <QColor>

#include "contactlist.h"
#include "contactlistitem.h"
#include "contactlistgroup.h"
#include "contactlistmodel.h"
#include "contactlistrootitem.h"
#include "contactlistcontact.h"
#include "contactlistgroupitem.h"

#define COLUMNS 3

// TEMPORARY
#define GROUPBG_COLOR QColor(0x96,0x96,0x96)
#define GROUPFG_COLOR QColor(0xFF,0xFF,0xFF)
#define AWAY_COLOR QColor(0x00,0x4b,0xb4)
#define OFFLINE_COLOR QColor(0x64,0x64,0x64)
#define DND_COLOR QColor(0x7e,0x00,0x00)
#define ONLINE_COLOR QColor(0x00,0x00,0x00)

ContactListModel::ContactListModel(ContactList* contactList) : contactList_(contactList), showStatus_(true)
{
	connect(contactList_,SIGNAL(dataChanged()),this,SLOT(contactList_changed()));
}


QVariant ContactListModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

	ContactListItem* item = static_cast<ContactListItem*>(index.internalPointer());
	ContactListGroup*     group     = 0;
	ContactListGroupItem* groupItem = 0;
	ContactListContact*   contact   = 0;

	if (role == Qt::DisplayRole && index.column() == NameColumn) {
		if ((contact = dynamic_cast<ContactListContact*>(item))) {
			QString txt;
			if (showStatus_ && !contact->status().message().isEmpty()) {
				txt = QString("%1 (%2)").arg(contact->name()).arg(contact->status().message());
			}
			else
				txt = contact->name();
			return QVariant(txt);
		}
		else if ((group = dynamic_cast<ContactListGroup*>(item))) {
			return QVariant(QString("%1 (%2/%3)").arg(group->name()).arg(group->countOnline()).arg(group->count()));
		}
	}
	else if (role == Qt::EditRole && index.column() == NameColumn) {
		if ((contact = dynamic_cast<ContactListContact*>(item))) {
			return QVariant(contact->name());
		}
		else if ((group = dynamic_cast<ContactListGroup*>(item))) {
			return QVariant(group->name());
		}
	}
	else if (role == Qt::DecorationRole && (index.column() == StatusIconColumn || index.column() == PictureColumn)) {
		if ((contact = dynamic_cast<ContactListContact*>(item))) {
			if (index.column() == PictureColumn)
				return QVariant(contact->picture());
			else if (index.column() == StatusIconColumn)
				return QVariant(contact->statusIcon());
		}
	}
	else if (role == Qt::BackgroundColorRole) {
		if ((group = dynamic_cast<ContactListGroup*>(item))) {
			return qVariantFromValue(GROUPBG_COLOR);
		}
	}
	else if (role == Qt::TextColorRole) {
		if ((contact = dynamic_cast<ContactListContact*>(item))) {
			if(contact->status().type() == Status::Away || contact->status().type() == Status::XA) {
				return qVariantFromValue(AWAY_COLOR);
			}
			else if (contact->status().type() == Status::Offline) {
				return qVariantFromValue(OFFLINE_COLOR);
			}
			else if (contact->status().type() == Status::DND) {
				return qVariantFromValue(DND_COLOR);
			}
			else {
				return qVariantFromValue(ONLINE_COLOR);
			}
		}
		else if ((group = dynamic_cast<ContactListGroup*>(item))) {
			return qVariantFromValue(GROUPFG_COLOR);
		}
	}
	else if (role == Qt::ToolTipRole) {
		return QVariant(item->toolTip());
	}
	else if (role == ExpandedRole) {
		if ((groupItem = dynamic_cast<ContactListGroupItem*>(item))) {
			if (contactList_->search().isEmpty()) {
				return QVariant(groupItem->expanded());
			}
			else {
				return QVariant(true);
			}
		}
		else {
			return QVariant(false);
		}
	}
	return QVariant();
}

bool ContactListModel::setData(const QModelIndex& index, const QVariant& data, int role)
{
	if (!index.isValid())
		return false;

	ContactListItem* item = static_cast<ContactListItem*>(index.internalPointer());
	if (role == ContextMenuRole) {
		item->showContextMenu(data.toPoint());
	}

	return true;
}

QVariant ContactListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	return QVariant();
}

QModelIndex ContactListModel::index(int row, int column, const QModelIndex &parent) const
{
	ContactListGroupItem* parentItem;
	if (parent.isValid())
		parentItem = static_cast<ContactListGroupItem*>(parent.internalPointer());
	else
		parentItem = contactList_->rootItem();

	ContactListItem* item = parentItem->atIndex(row);
	return (item ? createIndex(row, column, item) : QModelIndex());
}


QModelIndex ContactListModel::parent(const QModelIndex &index) const
{
	if (!index.isValid())
		return QModelIndex();

	ContactListItem* item = static_cast<ContactListItem*>(index.internalPointer());
	ContactListGroupItem* parent = item->parent();

	return (parent == contactList_->rootItem() ? QModelIndex() : createIndex(parent->index(),0,parent));
}


int ContactListModel::rowCount(const QModelIndex &parent) const
{
	ContactListGroupItem* parentItem;
	if (parent.isValid()) {
		ContactListItem* item = static_cast<ContactListItem*>(parent.internalPointer());
		parentItem = dynamic_cast<ContactListGroupItem*>(item);
	}
	else {
		parentItem = contactList_->rootItem();
	}

	return (parentItem ? parentItem->items() : 0);
}


int ContactListModel::columnCount(const QModelIndex&) const
{
	return COLUMNS;
}

Qt::ItemFlags ContactListModel::flags(const QModelIndex& index) const
{
	Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
	if (index.column() == NameColumn)
		f = f | Qt::ItemIsEditable;
	return f;
}


void ContactListModel::contactList_changed()
{
	reset();
}
