/*
 * optionstreemodel.h
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

#ifndef OPTIONSTREEMODEL_H
#define OPTIONSTREEMODEL_H

#include <QAbstractItemModel>
#include <QVariant>

class OptionsTree;

class OptionsTreeModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	OptionsTreeModel(OptionsTree* tree, QObject* parent = 0);

	// Reimplemented from QAbstractItemModel
	int rowCount(const QModelIndex& parent = QModelIndex()) const;
	int columnCount (const QModelIndex& parent = QModelIndex()) const;
	QVariant data(const QModelIndex& index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
	QModelIndex index(int row, int column, const QModelIndex & parent = QModelIndex() ) const;
	QModelIndex parent(const QModelIndex& index) const;
	Qt::ItemFlags flags(const QModelIndex& index) const;

public slots:
	void setFlat(bool);

protected:
	enum Section { Name = 0, Type = 1, Value = 2, Comment = 3};

private:
	OptionsTree* tree_;
	bool flat_;
};

#endif
