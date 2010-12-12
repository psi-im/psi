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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

QDomNode WbNewPath::serializeToSvg() {
	if(controlPoint_) {
		QPainterPath painterpath = graphicsitem_.path();
		painterpath.lineTo(*controlPoint_);
		graphicsitem_.setPath(painterpath);

		delete controlPoint_;
		controlPoint_ = 0;
	}

	// trim the generated SVG to remove unnecessary nested <g/>'s

	// first find the <path/> element
	QDomNode out = WbNewItem::serializeToSvg();
	QDomNodeList children = out.childNodes();
	QDomElement trimmed;
	for(uint i = 0; i < children.length(); i++) {
		if(children.at(i).isElement()) {
			if(children.at(i).nodeName() == "path") {
				trimmed = children.at(i).toElement();
				break;
			} else {
				trimmed = children.at(i).toElement().elementsByTagName("path").at(0).toElement();
				if(!trimmed.isNull())
					break;
			}
		}
	}

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

	return trimmed;
}

QGraphicsItem* WbNewPath::graphicsItem() {
	return &graphicsitem_;
}
