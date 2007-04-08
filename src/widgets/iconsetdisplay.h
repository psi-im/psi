/*
 * iconsetdisplay.h - contact list widget
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

#ifndef ICONSETDISPLAY_H
#define ICONSETDISPLAY_H

#include <QListWidget>

class IconsetDisplayItem;
class Iconset;

class IconsetDisplay : public QListWidget
{
	Q_OBJECT
public:
	IconsetDisplay(QWidget *parent = 0);
	~IconsetDisplay();

	void setIconset(const Iconset &);
private:
	friend class IconsetDisplayItem;
};

#endif
