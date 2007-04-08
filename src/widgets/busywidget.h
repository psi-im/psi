/*
 * busywidget.h - cool animating widget
 * Copyright (C) 2001, 2002  Justin Karneges
 *                           Hideaki Omuro
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

#ifndef BUSYWIDGET_H
#define BUSYWIDGET_H

#include <QWidget>

class CColor;
class CPanel;

class BusyWidget : public QWidget
{
	Q_OBJECT
	Q_PROPERTY( bool active READ isActive WRITE setActive )

	Q_OVERRIDE( QSize minimumSize DESIGNABLE false SCRIPTABLE false )
	Q_OVERRIDE( QSize maximumSize DESIGNABLE false SCRIPTABLE false )

public:
	BusyWidget(QWidget *parent=0);
	~BusyWidget();

	bool isActive() const;

	QSize minimumSizeHint() const;
	QSize sizeHint() const;

public slots:
	void start();
	void stop();
	void setActive(bool);

protected:
	void paintEvent(QPaintEvent *);
	void resizeEvent(QResizeEvent *);

private slots:
	void animate();

public:
	class Private;
private:
	Private *d;
	friend class Private;
};

#endif
