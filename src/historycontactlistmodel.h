/*
 * historycontactlistmodel.h
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

#ifndef HISTORYCONTACTLISTMODEL_H
#define HISTORYCONTACTLISTMODEL_H

#include <QAbstractItemModel>
#include <QSortFilterProxyModel>

#include "psicon.h"

class HistoryContactListModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	enum ItemType { Root, Group, RosterContact, NotInRosterContact, Other };
	enum {
		ItemIdRole   = Qt::UserRole,
		ItemPosRole  = Qt::UserRole + 1,
		ItemTypeRole = Qt::UserRole + 2
	};

	HistoryContactListModel(QObject *parent = 0);
	~HistoryContactListModel();
	void clear();
	void updateContacts(PsiCon *psi, const QString &id);
	void displayPrivateContacts(bool enable) { dispPrivateContacts = enable; }
	void displayAllContacts(bool enable) { dispAllContacts = enable; }

	int rowCount(const QModelIndex &parent) const;
	int columnCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;
	QModelIndex index(int row, int column, const QModelIndex &parent) const;
	QModelIndex parent(const QModelIndex &child) const;
	bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());

private:
	class TreeItem
	{
	public:
		TreeItem(ItemType type, const QString &text, const QString &id = QString(), int pos = 0);
		TreeItem(ItemType type, const QString &text, const QString &tooltip, const QString &id, int pos = 0);
		~TreeItem();
		void appendChild(TreeItem *item);
		void removeChild(int row);
		int row() const;
		TreeItem *parent() { return _parent; }
		TreeItem *child(int row) { return child_items.value(row); }
		int childCount() const { return child_items.count(); }
		ItemType type() const { return _type; }
		QString id() const { return _id; }
		int position() const { return _position; }
		QString text(bool tooltip = false) const { return tooltip ? _tooltip : _text; }

	private:
		TreeItem *_parent;
		ItemType _type;
		QString  _text;
		QString  _tooltip;
		QString  _id;
		int      _position;
		QList<TreeItem *> child_items;
	};

private:
	void loadContacts(PsiCon *psi, const QString &acc_id);
	QString makeContactToolTip(PsiCon *psi, const QString &accId, const Jid &jid, bool bare) const;
	TreeItem *rootItem;
	TreeItem *generalGroup;
	TreeItem *notInList;
	TreeItem *confPrivate;
	bool dispPrivateContacts;
	bool dispAllContacts;

};

class HistoryContactListProxyModel : public QSortFilterProxyModel
{
	Q_OBJECT

public:
	HistoryContactListProxyModel(QObject *parent = 0);

public slots:
	void setFilterFixedString(const QString & pattern);

protected:
	bool lessThan(const QModelIndex &left, const QModelIndex &right) const;
	bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;

private:
	QString _pattern;

};

#endif // HISTORYCONTACTLISTMODEL_H
