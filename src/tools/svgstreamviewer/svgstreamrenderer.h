/*
 * svgstreamrenderer.h - a renderer for changing SVG documents
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

#ifndef SVGSTREAMRENDERER_H
#define SVGSTREAMRENDERER_H
#include <QSvgRenderer>
#include <QtXml>

class SvgStreamRenderer : public QSvgRenderer
{
	Q_OBJECT

public:
	SvgStreamRenderer( QObject * parent = 0 );
	SvgStreamRenderer( const QString & filename, QObject * parent = 0 );
	SvgStreamRenderer( const QByteArray & contents, QObject * parent = 0 );
	~SvgStreamRenderer() {};
	QDomElement element(QString id) const;

public slots:
	bool load(const QString &filename);
	bool load(const QByteArray &contents);
	bool setElement(const QDomElement &element, bool forcenew=false );
	bool setElement(const QString &element, bool forcenew=false );
	int moveElement(const QString &id, const int &n);
	bool removeElement(const QString &id);
	// TODO: method to modify SVG root element attributes

private:
	bool store(const QString &filename);
 	QDomDocument doc;
};
#endif
