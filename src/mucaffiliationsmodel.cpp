/*
 * mucaffiliationsmodel.cpp
 * Copyright (C) 2006  Remko Troncon
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

#include <QMimeData>
#include <QFont>
#include <QVariant>

#include "mucaffiliationsmodel.h"

using namespace XMPP;


MUCAffiliationsModel::MUCAffiliationsModel() : QStandardItemModel(Unknown,1) 
{ 
	QFont font;
	font.setBold(true);
	QVariant font_variant = qVariantFromValue(font);
	for (int i = 0; i < Unknown; i++) {
		QModelIndex ind = index(i, 0, QModelIndex());
		setData(ind,QVariant(affiliationlistindexToString((AffiliationListIndex) i)));
		setData(ind,font_variant,Qt::FontRole);
		insertColumns(0,1,ind);
		enabled_[(AffiliationListIndex) i] = false;
	}
}

Qt::ItemFlags MUCAffiliationsModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags a;
	if (!index.parent().isValid()) {
		// List headers
		if (enabled_[(AffiliationListIndex) index.row()]) {
			a |= Qt::ItemIsDropEnabled | Qt::ItemIsSelectable | Qt::ItemIsEnabled;
		}
	}
	else {
		a |= Qt::ItemIsDropEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
	}
	return a;
}

Qt::DropActions MUCAffiliationsModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

bool MUCAffiliationsModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int, const QModelIndex &parent)
{
	if (!data || action != Qt::MoveAction || !(data->hasFormat("application/vnd.text.list") || data->hasFormat("text/plain")))
		return false;
	
	// Decode the data
	QStringList newItems;
	int nb_rows = 0;
	if (data->hasFormat("application/vnd.text.list")) {
		QByteArray encodedData = data->data("application/vnd.text.list");
		QDataStream stream(&encodedData, QIODevice::ReadOnly);
		while (!stream.atEnd()) {
			QString text;
			stream >> text;
			newItems << text;
			nb_rows++;
		}
	}
	else if (data->hasFormat("text/plain")) {
		QString item(data->data("text/plain"));
		if (Jid(item).isValid()) {
			newItems += item;
			nb_rows++;
		}
	}

	if (nb_rows == 0)
		return false;

	// Determine the correct index
	QModelIndex real_index;
	if (parent.isValid())  {
		real_index = (parent.parent().isValid() ? parent.parent() : parent);
	}
	else {
		if (row > 0)
			real_index = index(row-1,0,parent);
		else if (row == 0)
			real_index = index(0,0,parent);
		else
			real_index = index(Outcast,0,parent);
	}
	int real_row = rowCount(real_index);

	// Insert the data
	insertRows(real_row, nb_rows, real_index);
	foreach (QString text, newItems) {
		QModelIndex idx = index(real_row, 0, real_index);
		setData(idx, text);
		real_row++;
	}

	return true;
}

QStringList MUCAffiliationsModel::mimeTypes() const
{
	QStringList types;
	types << "application/vnd.text.list";
	types << "text/plain";
	return types;
}

QMimeData* MUCAffiliationsModel::mimeData(const QModelIndexList &indexes) const
{
 	QMimeData *mimeData = new QMimeData();
	QByteArray encodedData;
	QDataStream stream(&encodedData, QIODevice::WriteOnly);
	foreach (QModelIndex index, indexes) {
		if (index.isValid()) {
			QString text = data(index, Qt::DisplayRole).toString();
			stream << text;
		}
	}
	mimeData->setData("application/vnd.text.list", encodedData);
	return mimeData;
}

void MUCAffiliationsModel::resetAffiliationLists()
{
	items_.clear();
	resetAffiliationList(MUCItem::Outcast);
	resetAffiliationList(MUCItem::Member);
	resetAffiliationList(MUCItem::Admin);
	resetAffiliationList(MUCItem::Owner);
}

void MUCAffiliationsModel::resetAffiliationList(MUCItem::Affiliation a)
{
	emit layoutAboutToBeChanged();
	enabled_[(AffiliationListIndex) affiliationToIndex(a)] = false;
	QModelIndex index = affiliationListIndex(a);
	if (hasChildren(index)) {
		removeRows(0,rowCount(index),index);
	}
	emit layoutChanged();
}

void MUCAffiliationsModel::setAffiliationListEnabled(MUCItem::Affiliation a, bool b)
{
	emit layoutAboutToBeChanged();
	QModelIndex index = affiliationListIndex(a);
	enabled_[(AffiliationListIndex) index.row()] = b;
	emit layoutChanged();
}

QString MUCAffiliationsModel::affiliationlistindexToString(AffiliationListIndex list)
{
	if (list == Members)
		return tr("Members");
	else if (list == Admins)
		return tr("Administrators");
	else if (list == Owners)
		return tr("Owners");
	else if (list == Outcast)
		return tr("Banned");
	return QString();
}

QModelIndex MUCAffiliationsModel::affiliationListIndex(MUCItem::Affiliation a) 
{
	AffiliationListIndex i = affiliationToIndex(a);
	if (i == Unknown) {
		qWarning("mucconfigdlg.cpp: Unexpected affiliation");
		return QModelIndex();
	}
	return index(i, 0, QModelIndex());
}

MUCAffiliationsModel::AffiliationListIndex MUCAffiliationsModel::affiliationToIndex(MUCItem::Affiliation a)
{
	if (a == MUCItem::Member) 
		return Members;
	else if (a == MUCItem::Admin) 
		return Admins;
	else if (a == MUCItem::Owner) 
		return Owners;
	else if (a == MUCItem::Outcast) 
		return Outcast;
	else
		return Unknown;
}


void MUCAffiliationsModel::addItems(const QList<MUCItem>& items)
{
	bool dirty = false;
	foreach(MUCItem item, items) {
		QModelIndex list = affiliationListIndex(item.affiliation());
		if (list.isValid() && !item.jid().isEmpty()) {
			if (!dirty) {
				emit layoutAboutToBeChanged();
			}
			int row = rowCount(list);
			if (row == 0) {
				enabled_[(AffiliationListIndex) list.row()] = true;
			}
			insertRows(row,1,list);
			setData(index(row,0,list),QVariant(item.jid().full()));
			MUCItem i(MUCItem::UnknownRole,item.affiliation());
			i.setJid(item.jid());
			items_ += i;
			dirty = true;
		}
		else {
			qDebug("Unexpected item");
		}
	}
	if (dirty)
		emit layoutChanged();
}

QList<MUCItem> MUCAffiliationsModel::changes() const
{
	QList<MUCItem> items_old = items_;
	QList<MUCItem> items_delta;

	// Add all new items
	for (int i = 0; i < rowCount(QModelIndex()); i++) {
		QModelIndex list = index(i,0,QModelIndex());
		for(int j = 0; j < rowCount(list); j++) {
			Jid jid(data(index(j,0,list)).toString());
			MUCItem item(MUCItem::UnknownRole,indexToAffiliation(i));
			item.setJid(jid);
			if (!items_.contains(item)) {
				items_delta += item;
			}
			else 
				items_old.removeAll(item);
		}
	}

	// Remove all old items not present in the delta
	foreach(MUCItem item_old, items_old) {
		bool found = false;
		foreach(MUCItem item_new, items_delta) {
			if (item_new.jid().compare(item_old.jid(),false)) {
				found = true;
				break;
			}
		}
		if (!found) {
			MUCItem item(MUCItem::UnknownRole,MUCItem::NoAffiliation);
			item.setJid(item_old.jid());
			items_delta += item;
		}
	}
	
	return items_delta;
}

MUCItem::Affiliation MUCAffiliationsModel::indexToAffiliation(int li)
{
	if (li == Members)
		return MUCItem::Member;
	else if (li == Admins)
		return MUCItem::Admin;
	else if (li == Owners)
		return MUCItem::Owner;
	else if (li == Outcast)
		return MUCItem::Outcast;
	else
		return MUCItem::UnknownAffiliation;
}

