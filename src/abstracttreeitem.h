/*
 * Copyright (C) 2015  Ivan Romanov <drizt@land.ru>
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

#pragma once

#include <QList>
#include <QString>

class AbstractTreeItem;

typedef QList<AbstractTreeItem*> AbstractTreeItemList;

class AbstractTreeItem
{
public:
	explicit AbstractTreeItem(AbstractTreeItem *parent = nullptr);
	virtual ~AbstractTreeItem();

	void setRow(int row);
	int row() const;

	void setParent(AbstractTreeItem *newParent);
	AbstractTreeItem *parent() const;

	void insertChild(int row, AbstractTreeItem *child);
	void appendChild(AbstractTreeItem *child);
	void removeChild(AbstractTreeItem *child);

	AbstractTreeItem *child(int row) const;
	int childCount() const;
	AbstractTreeItemList children() const;

	virtual AbstractTreeItem *clone() const { return nullptr; /* no default implementation */ }
	void dump(int indent = 0) const;
	virtual QString toString() const { return QString(); /* no default implementation */ }

private:
	AbstractTreeItem *_parent;
	AbstractTreeItemList _children;
};
