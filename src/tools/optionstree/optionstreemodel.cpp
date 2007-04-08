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

#include "optionstreemodel.h"
#include "optionstree.h"

OptionsTreeModel::OptionsTreeModel(OptionsTree* tree, QObject* parent) : QAbstractItemModel(parent), tree_(tree), flat_(false)
{
}

void OptionsTreeModel::setFlat(bool b)
{
	if (flat_ != b) {
		flat_ = b;
		reset();
	}
}

Qt::ItemFlags OptionsTreeModel::flags(const QModelIndex& index) const
{
	Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
	if (index.column() == Value)
		f |= Qt::ItemIsEditable;
	return f;
}

int OptionsTreeModel::rowCount(const QModelIndex& parent) const
{
	if (flat_) {
		return (parent.isValid() ? 0 : tree_->getChildOptionNames("",!flat_).count());
	}
	else {
		QString option;
		if (parent.isValid())
			option = tree_->getChildOptionNames("",false,true).at(parent.internalId());
		return tree_->getChildOptionNames(option,true,true).count();
	}
}

int OptionsTreeModel::columnCount(const QModelIndex&) const 
{
	return 4;
}

QVariant OptionsTreeModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
		return QVariant();

	QString option = tree_->getChildOptionNames("",false,true).at(index.internalId());
	Section section = (Section) index.column();
	if (role == Qt::DisplayRole) {
		if (section == Name) {
			if (flat_) {
				return option;
			}
			else {
				int dot_index = option.lastIndexOf('.');
				return option.right(option.length() - dot_index - 1);
			}
		}
		else if (section == Type)
			return tree_->getOption(option).typeName();
		else if (section == Value)
			return tree_->getOption(option).toString();
		else if (section == Comment)
			return tree_->getComment(option);
	}
	return QVariant();
}

QVariant OptionsTreeModel::headerData(int s, Qt::Orientation, int role) const
{
	if (role != Qt::DisplayRole)
		return QVariant();
	
	Section section = (Section) s;
	switch (section) {
		case Name: return tr("Name");
		case Type: return QString(tr("Type"));
		case Value: return QString(tr("Value"));
		case Comment: return QString(tr("Comment"));
	}

	return QVariant();
}

QModelIndex OptionsTreeModel::index(int row, int column, const QModelIndex & parent) const
{
	// FIXME: Horribly inefficient
	QStringList all_options = tree_->getChildOptionNames("",false,true);
	int id = 0;
	if (flat_) {
		QStringList options = tree_->getChildOptionNames("",false,false);
		id = all_options.indexOf(options.at(row));
	}
	else {
		QString parent_option;
		if (parent.isValid()) 
			parent_option = all_options.at(parent.internalId());
		QStringList children = tree_->getChildOptionNames(parent_option,true,true);
		if (row >= children.count())
			return QModelIndex();
		id = all_options.indexOf(children.at(row));
	}
	return createIndex(row,column,id);
}

QModelIndex OptionsTreeModel::parent(const QModelIndex& index) const
{
	if (!index.isValid() || flat_) 
		return QModelIndex();
	
	QStringList all_options = tree_->getChildOptionNames("",false,true);
	QString option = all_options.at(index.internalId());
	
	// Determine the parent option
	int dot_index = option.lastIndexOf('.');
	if (dot_index == -1)
		return QModelIndex();
	QString parent_option = option.left(dot_index);
	
	// Determine the parent's parent
	QString parent_parent_option;
	dot_index = parent_option.lastIndexOf('.');
	if (dot_index != -1)
		parent_parent_option = parent_option.left(dot_index);
	int row = tree_->getChildOptionNames(parent_parent_option,true,true).indexOf(parent_option);
	
	return createIndex(row,0,all_options.indexOf(parent_option));
}
