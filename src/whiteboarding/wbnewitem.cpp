/*
 * wbnewitem.cpp - a class used for representing items on the whiteboard 
 *			  while they're being drawn.
 * Copyright (C) 2008  Joonas Govenius
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

#include "wbnewitem.h"
#include "../sxe/sxesession.h"

#include <QSvgGenerator>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QBuffer>

WbNewItem::WbNewItem(QGraphicsScene* s) {
	scene = s;
}

WbNewItem::~WbNewItem() {
}

QDomNode WbNewItem::serializeToSvg() {
	if(!graphicsItem()) {
		return QDomDocumentFragment();
	}

	// Generate the SVG using QSvgGenerator
	QBuffer buffer;

	QSvgGenerator generator;
	generator.setOutputDevice(&buffer);

	QPainter painter;
	QStyleOptionGraphicsItem options;
	painter.begin(&generator);
	graphicsItem()->paint(&painter, &options);
	painter.end();

	// qDebug("Serialized SVG doc:");
	// qDebug(buffer.buffer());

	// Parse the children of the new root <svg/> from the buffer to a document fragment
	// also add an 'id' attribute to each of the children
	QDomDocument tempDoc;
	tempDoc.setContent(buffer.buffer());
	QDomDocumentFragment fragment = tempDoc.createDocumentFragment();

	QDomNodeList children = tempDoc.documentElement().childNodes();
	for(int i = children.length() - 1; i >= 0; i--) {
		// skip <title/>, <desc/>, and <defs/>
		if(children.at(i).isElement() &&
			!(children.at(i).nodeName() == "title"
				|| children.at(i).nodeName() == "desc"
				|| children.at(i).nodeName() == "defs")) {
			children.at(i).toElement().setAttribute("id", "e" + SxeSession::generateUUID());
			fragment.insertBefore(children.at(i), QDomNode());
		}
	}

	return fragment;
}
