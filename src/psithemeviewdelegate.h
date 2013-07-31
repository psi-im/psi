/*
 * psithemeviewdelegate.h - renders theme items
 * Copyright (C) 2010 Rion (Sergey Ilinyh)
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

#ifndef PSITHEMEVIEWDELEGATE_H
#define PSITHEMEVIEWDELEGATE_H

#include <QAbstractItemDelegate>

class PsiThemeViewDelegate : public QAbstractItemDelegate
{
	Q_OBJECT
public:
	PsiThemeViewDelegate ( QObject * parent = 0 )
		: QAbstractItemDelegate ( parent ) { }

	// painting
	virtual void paint(QPainter *painter,
					   const QStyleOptionViewItem &option,
					   const QModelIndex &index) const;

	virtual QSize sizeHint(const QStyleOptionViewItem &option,
						   const QModelIndex &index) const;

	// editing
	virtual QWidget *createEditor(QWidget *parent,
								  const QStyleOptionViewItem &option,
								  const QModelIndex &index) const;

	static const int TextPadding = 3;
};

#endif
