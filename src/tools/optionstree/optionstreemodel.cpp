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

#include <QStringList>

#include "optionstree.h"


// Enable this if you have Trolltech Labs' ModelTest and are not going
// to distribute the source or binary. You need to include modeltest.pri
// somewhere too.
//#define HAVE_MODELTEST

#ifdef HAVE_MODELTEST
#include <modeltest.h>
#endif

OptionsTreeModel::OptionsTreeModel(OptionsTree* tree, QObject* parent)
		: QAbstractItemModel(parent),
		tree_(tree), 
		flat_(false), 
		nextIdx(0)
{
	connect(tree_, SIGNAL(optionChanged(const QString&)), SLOT(optionChanged(const QString&)));
	connect(tree_, SIGNAL(optionAboutToBeInserted(const QString&)), SLOT(optionAboutToBeInserted(const QString&)));
	connect(tree_, SIGNAL(optionInserted(const QString&)), SLOT(optionInserted(const QString&)));
	connect(tree_, SIGNAL(optionAboutToBeRemoved(const QString&)), SLOT(optionAboutToBeRemoved(const QString&)));
	connect(tree_, SIGNAL(optionRemoved(const QString&)), SLOT(optionRemoved(const QString&)));

#ifdef HAVE_MODELTEST
	new ModelTest(this, this);
#endif
}

void OptionsTreeModel::setFlat(bool b)
{
	if (flat_ != b) {
		flat_ = b;
		reset();
	}
}


/**
 * Get the parent option of @a option
 * @param option the option name to be splitted
 * @return the part of option until the last dot (or empty)
 */
QString OptionsTreeModel::getParentName(const QString &option) const
{
	QString parentname;
	int dot_index = option.lastIndexOf('.');
	if (dot_index != -1) {
		parentname = option.left(dot_index);
	}
	return parentname;
}



/**
 * Get index of given @a option
 * @param option the option to retrieve the index for
 * @param sec Section the new index should point to
 * @return a QModelIndex to @a option
 */
QModelIndex OptionsTreeModel::index(const QString &option, Section sec) const
{
	if (option.isEmpty()) {
		return QModelIndex();
	}
	
	if (flat_) {
		QStringList options = tree_->getChildOptionNames("",false,false);
		options.sort();
		int row = options.indexOf(option);
		return createIndex(row, sec, nameToIndex(options.at(row)));
	} else {
		QString parentname(getParentName(option));
		
		QStringList children = tree_->getChildOptionNames(parentname,true,true);
		children.sort();
		int row = children.indexOf(option);
		
		return createIndex(row, sec, nameToIndex(option));
	}
}


Qt::ItemFlags OptionsTreeModel::flags(const QModelIndex& index) const
{
	if (!index.isValid()) {
		// Root item
		return 0;
	}
	Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
	if ((index.column() == Value) && !internalNode(indexToOptionName(index)))
		f |= Qt::ItemIsEditable;
	return f;
}

int OptionsTreeModel::rowCount(const QModelIndex& parent) const
{
	if ((Section)parent.column() == Name || !parent.isValid()) {
		if (flat_) {
			return (parent.isValid() ? 0 : tree_->getChildOptionNames("",false,false).count());
		} else {
			QString option;
			if (parent.isValid())
				option = indexToOptionName(parent);
			return tree_->getChildOptionNames(option,true,true).count();
		}
	}
	return 0;
}

int OptionsTreeModel::columnCount(const QModelIndex&) const 
{
	return 4;
}

QVariant OptionsTreeModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
		return QVariant();

	QString option = indexToOptionName(index);
	Section section = (Section) index.column();
	if ((role == Qt::DisplayRole) || (role == Qt::EditRole)) {
		if (section == Name) {
			if (flat_) {
				return option;
			} else {
				int dot_index = option.lastIndexOf('.');
				return option.mid(dot_index + 1);
			}
		}
		else if (section == Comment) {
			return tree_->getComment(option);
		} else if (!tree_->isInternalNode(option)) {
			if (section == Type)
				return tree_->getOption(option).typeName();
			else if (section == Value)
				return tree_->getOption(option);//.toString();
		}
	} else if (role == Qt::ToolTipRole) {
		if (!tree_->isInternalNode(option)) {
			return tree_->getComment(option);
		}
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
		default:
			return QVariant();
	}
}

QModelIndex OptionsTreeModel::index(int row, int column, const QModelIndex & parent) const
{
	if  (column < 0  || column >= SectionBound || row < 0) {
		return QModelIndex();
	}
	int id = 0;
	QStringList options;
	if (flat_) {
		options = tree_->getChildOptionNames("",false,false);
	} else {
		QString parent_option;
		if (parent.isValid()) {
			parent_option = indexToOptionName(parent);
		}
		options = tree_->getChildOptionNames(parent_option,true,true);
	}
	if (row >= options.size()) {
		return QModelIndex();
	}
	options.sort();
	id = nameToIndex(options.at(row));
	return createIndex(row,column,id);
}

QModelIndex OptionsTreeModel::parent(const QModelIndex& modelindex) const
{
	if (!modelindex.isValid() || flat_) 
		return QModelIndex();
	
	QString option = indexToOptionName(modelindex);
	
	QString parent_option = getParentName(option);
	
	return index(parent_option);
}

bool OptionsTreeModel::setData ( const QModelIndex & index, const QVariant & value, int role)
{
	QString option = indexToOptionName(index);
	if ((role != Qt::EditRole) || ((Section) index.column() != Value) || internalNode(option)) {
		return false;
	}
	QVariant current = tree_->getOption(option);
	QVariant newval = value;
	if (!newval.canConvert(current.type())) {
		qWarning("Sorry don't know how to do that!");
		return false;
	}
	newval.convert(current.type());
	tree_->setOption(option, newval);
	return true;
}




void OptionsTreeModel::optionAboutToBeInserted(const QString& option)
{
	QString parentname(getParentName(option));
	
	// FIXME? handle cases when parent doesn't exist either.
	
	QModelIndex parent(index(parentname));
	
	QStringList children = tree_->getChildOptionNames(parentname,true,true);
	children << option;
	children.sort();
	int row = children.indexOf(option);
	
	emit beginInsertRows(parent, row, row);
	
}

void OptionsTreeModel::optionInserted(const QString& option)
{
	Q_UNUSED(option)
	endInsertRows ();
}

void OptionsTreeModel::optionAboutToBeRemoved(const QString& option)
{
	QString parentname(getParentName(option));
	
	QModelIndex parent(index(parentname));
	
	QStringList children = tree_->getChildOptionNames(parentname,true,true);
	children.sort();
	int row = children.indexOf(option);
	
	if (row != -1) {
		realRemove.push(true);
		emit beginRemoveRows(parent, row, row);
	} else {
		realRemove.push(false);
	}
}
void OptionsTreeModel::optionRemoved(const QString& option)
{
	Q_UNUSED(option)
	if (realRemove.pop()) endRemoveRows ();
}


void OptionsTreeModel::optionChanged(const QString& option)
{
	// only need to notify about options the view can possibly know anything about.
	if (nameMap.contains(option)) {
		QModelIndex modelindex(index(option, Value));
		emit dataChanged(modelindex, modelindex);
	}
}
bool OptionsTreeModel::internalNode(QString option) const
{
	return tree_->isInternalNode(option);
}

int OptionsTreeModel::nameToIndex(QString name) const
{
	if (!nameMap.contains(name)) {
		int idx = nextIdx++;
		//qDebug() << "adding " << name << " as " << idx;
		nameMap[name] = idx;
		indexMap[idx] = name;
		return idx;
	} else {
		return nameMap[name];
	}
}

QString OptionsTreeModel::indexToOptionName(QModelIndex idx) const
{
	return indexMap[idx.internalId()];
}
