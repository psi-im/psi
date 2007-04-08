/*
 * iconsetselect.h - contact list widget
 * Copyright (C) 2001-2005  Justin Karneges, Michail Pishchagin 
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

#ifndef ICONSETSELECT_H
#define ICONSETSELECT_H

#include <QListWidget>

class Iconset;
class IconsetSelectItem;

class IconsetSelect : public QListWidget
{
	Q_OBJECT
public:
	IconsetSelect(QWidget *parent = 0);
	~IconsetSelect();

	void insert(const Iconset &); // iconsets must be inserted in following order: most prioritent first

	const Iconset *iconset() const;
	
	QListWidgetItem *lastItem() const;

	QStyleOptionViewItem viewOptions() const;
	
public slots:
	void moveItemUp();
	void moveItemDown();

private:
	friend class IconsetSelectItem;
};

#endif
