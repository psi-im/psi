/*
 * svgstreamwidget.h - a widget for showing changing SVG documents
 * Copyright (C) 2006  Joonas Govenius
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

#ifndef SVGSTREAMWIDGET_H
#define SVGSTREAMWIDGET_H
#include "svgstreamrenderer.h"
#include <QWidget>

class QPaintEvent;
class QWheelEvent;

class SvgStreamWidget : public QWidget
{
	Q_OBJECT
public:
	SvgStreamWidget(QWidget *parent=0);
	virtual QSize sizeHint() const;

public slots:
	bool load(const QString &filename);
	bool load(const QByteArray &contents);
	bool setElement(const QDomElement &element, bool forcenew=false );
	bool setElement(const QString &element, bool forcenew=false );
	bool moveElement(const QString &id, const int &n);
	bool removeElement(const QString &id);

protected:
	virtual void paintEvent(QPaintEvent *event);

private:
	SvgStreamRenderer *renderer;
};

#endif
