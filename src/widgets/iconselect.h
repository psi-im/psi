/*
 * iconselect.h - class that allows user to select an PsiIcon from an Iconset
 * Copyright (C) 2003  Michail Pishchagin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef ICONSELECT_H
#define ICONSELECT_H

#include <QMenu>

class PsiIcon;
class Iconset;

class IconSelectPopup : public QMenu
{
	Q_OBJECT

public:
	IconSelectPopup(QWidget *parent = 0);
	~IconSelectPopup();

	void setIconset(const Iconset &);
	const Iconset &iconset() const;

	// reimplemented
	void mousePressEvent(QMouseEvent *e);

signals:
	void iconSelected(const PsiIcon *);
	void textSelected(QString);

private:
	class Private;
	Private *d;
};

#endif
