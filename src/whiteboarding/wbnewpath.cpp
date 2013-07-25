/*
 * wbnewpath.cpp - a class used for representing a path on the whiteboard
 *			  while it's being drawn.
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

#include "wbnewpath.h"
#include "../sxe/sxesession.h"

#include <QGraphicsScene>

WbNewPath::WbNewPath(QGraphicsScene* s, QPointF startPos, int strokeWidth, const QColor &strokeColor, const QColor &fillColor) : WbNewItem(s) {
	controlPoint_ = 0;

	graphicsitem_.setZValue(std::numeric_limits<double>::max());

	graphicsitem_.setBrush(QBrush(fillColor));
	graphicsitem_.setPen(QPen(QBrush(strokeColor), strokeWidth));

	scene->addItem(&graphicsitem_);

	QPainterPath painterpath(startPos);
	painterpath.setFillRule(Qt::WindingFill);
	graphicsitem_.setPath(painterpath);
}

WbNewPath::~WbNewPath() {
	if(controlPoint_) {
		delete controlPoint_;
	}
}

void WbNewPath::parseCursorMove(QPointF newPos) {
	if(controlPoint_) {
		QPainterPath painterpath = graphicsitem_.path();
		// FIXME: the path should actually go through the "controlPoint_".
		painterpath.quadTo(*controlPoint_, newPos);
		graphicsitem_.setPath(painterpath);

		delete controlPoint_;
		controlPoint_ = 0;
	}
	else {
		controlPoint_ = new QPointF(newPos);
	}
}

QDomNode WbNewPath::serializeToSvg(QDomDocument *doc) {
	if(controlPoint_) {
		QPainterPath painterpath = graphicsitem_.path();
		painterpath.lineTo(*controlPoint_);
		graphicsitem_.setPath(painterpath);

		delete controlPoint_;
		controlPoint_ = 0;
	}

	// trim the generated SVG to remove unnecessary nested <g/>'s

	// first find the <path/> element
	QDomNode out = WbNewItem::serializeToSvg(doc);
	QDomElement trimmed;
	for(QDomNode n = out.firstChild(); !n.isNull(); n = n.nextSibling()) {
		if(n.isElement()) {
			if(n.nodeName() == "path") {
				trimmed = n.toElement();
				break;
			} else {
				trimmed = n.toElement().elementsByTagName("path").at(0).toElement();
				if(!trimmed.isNull())
					break;
			}
		}
	}

	if (!trimmed.isNull()) {
		// copy relevant attributes from the parent <g/>
		QDomNamedNodeMap parentAttr = trimmed.parentNode().toElement().attributes();
		for(int i = parentAttr.length() - 1; i >= 0; i--) {
			QString name = parentAttr.item(i).nodeName();
			if((name == "stroke"
				|| name == "stroke-width"
				|| name == "stroke-linecap"
				|| name == "fill"
				|| name == "fill-opacity")
				&& !trimmed.hasAttribute(name))
				trimmed.setAttributeNode(parentAttr.item(i).toAttr());
		}

		// add a unique 'id' attribute in anticipation of WbWidget's requirements
		trimmed.setAttribute("id", "e" + SxeSession::generateUUID());
	}

	return trimmed;
}

QGraphicsItem* WbNewPath::graphicsItem() {
	return &graphicsitem_;
}
