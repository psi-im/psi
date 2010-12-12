/*
 * wbnewimage.cpp - a class used for representing an image on the whiteboard 
 *			  while it's being added.
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

#include "wbnewimage.h"
#include "../sxe/sxesession.h"

#include <QGraphicsScene>
#include <QFile>

WbNewImage::WbNewImage(QGraphicsScene* s, QPointF startPos, const QString &filename) : WbNewItem(s),
					   graphicsitem_(QPixmap(filename)) {
	filename_ = filename;
	graphicsitem_.setZValue(std::numeric_limits<double>::max());
	graphicsitem_.setPos(startPos);

	scene->addItem(&graphicsitem_);
}


QDomNode WbNewImage::serializeToSvg() {
	// TODO: Should we perhaps scale large images?

	if(graphicsitem_.pixmap().isNull()) {
		return QDomNode();
	}

	QFile file(filename_);
	if (file.open(QIODevice::ReadOnly)) {
		QDomDocument d = QDomDocument();
		QDomElement image = QDomDocument().createElement("image");
		image.setAttribute("id", "e" + SxeSession::generateUUID());
		image.setAttribute("x", graphicsitem_.x());
		image.setAttribute("y", graphicsitem_.y());
		image.setAttribute("xmlns:xlink", "http://www.w3.org/1999/xlink");
		image.setAttribute("xlink:href", QString("data:image/%1;base64,%2")
											.arg(filename_.mid(filename_.lastIndexOf(".") + 1).toLower())
											.arg(file.readAll().toBase64().constData()));

		// QDomElement g = QDomDocument().createElement("g");
		// g.setAttribute("id", "e" + SxeSession::generateUUID());
		// g.appendChild(image);

		return image;
	}

	return QDomNode();
}

void WbNewImage::parseCursorMove(QPointF newPos) {
	Q_UNUSED(newPos);
}

QGraphicsItem* WbNewImage::graphicsItem() {
	return &graphicsitem_;
}
