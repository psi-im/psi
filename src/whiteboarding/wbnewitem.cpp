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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
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

QDomNode WbNewItem::serializeToSvg(QDomDocument *doc) {
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
	doc->setContent(buffer.buffer());
	QDomDocumentFragment fragment = doc->createDocumentFragment();

	for(QDomNode n = doc->documentElement().lastChild(); !n.isNull(); n = n.previousSibling()) {
		// skip <title/>, <desc/>, and <defs/>
		if(n.isElement() &&
			!(n.nodeName() == "title"
				|| n.nodeName() == "desc"
				|| n.nodeName() == "defs")) {
			n.toElement().setAttribute("id", "e" + SxeSession::generateUUID());
			fragment.insertBefore(n, QDomNode());
		}
	}

	return fragment;
}
