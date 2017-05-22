/*
 * contactmanagerview.h
 * Copyright (C) 2010 Rion
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

#ifndef CONTACTMANAGERVIEW_H
#define CONTACTMANAGERVIEW_H

#include <QTableView>

class ContactManagerView : public QTableView
{
	Q_OBJECT
public:
	ContactManagerView( QWidget * parent = 0 );
	void init();

protected:
	void contextMenuEvent( QContextMenuEvent * e );
	void keyPressEvent( QKeyEvent * e );
};

#endif // CONTACTMANAGERVIEW_H
