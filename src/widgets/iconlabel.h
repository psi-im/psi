/*
 * iconlabel.h - the FancyLabel widget
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

#ifndef ICONLABEL_H
#define ICONLABEL_H

#include <qwidget.h>
#include <qlabel.h>

class Icon;

class IconLabel : public QLabel
{
	Q_OBJECT
	Q_PROPERTY( QString iconName READ iconName WRITE setIcon )

public:
	IconLabel(QWidget *parent = 0);
	~IconLabel();

	const Icon *icon () const;
	QString iconName () const;
	void setIcon (const Icon *, bool copyIcon = true);
	void setIcon (const QString &);

	void setScaledContents(int width, int height);

	class Private;
private:
	Private *d;
};

#endif
