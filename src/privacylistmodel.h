/*
 * privacylistmodel.h
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
 
#ifndef PRIVACYLISTMODEL_H
#define PRIVACYLISTMODEL_H

#include <QAbstractListModel>

#include "privacylist.h"

class QObject;

class PrivacyListModel : public QAbstractListModel
{
public:
	enum { TextColumn = 0, ValueColumn };
	enum { 
		BlockedRole = Qt::UserRole + 0 
	};

	PrivacyListModel(const PrivacyList& list = PrivacyList(""), QObject* parent = NULL);

	// Overridden from QAbstractListModel
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	int columnCount(const QModelIndex &parent = QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role) const;
	bool removeRows(int row, int count, const QModelIndex & parent = QModelIndex());
	void reset() { QAbstractListModel::reset(); } // Not really clean

	// Own functions
	PrivacyList& list() { return list_; }
	void setList(const PrivacyList& list);
	bool moveUp(const QModelIndex& index);
	bool moveDown(const QModelIndex& index);
	bool edit(const QModelIndex& index);
	bool add();

private:
	PrivacyList list_;
};

#endif
