/*
 * wbnewpath.cpp - a class used for representing a path on the whiteboard 
 *              while it's being drawn.
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

#include <QGraphicsScene>

WbNewPath::WbNewPath(QGraphicsScene* s, QPointF startPos, int strokeWidth, const QColor &strokeColor, const QColor &fillColor) : WbNewItem(s) {
    controlPoint_ = 0;

    graphicsitem_.setZValue(std::numeric_limits<double>::max());

    graphicsitem_.setBrush(QBrush(fillColor));
    graphicsitem_.setPen(QPen(QBrush(strokeColor), strokeWidth));

    scene->addItem(&graphicsitem_);

    QPainterPath painterpath(startPos);
    graphicsitem_.setPath(painterpath);
}

WbNewPath::~WbNewPath() {
    if(controlPoint_)
        delete controlPoint_;
}

void WbNewPath::parseCursorMove(QPointF newPos) {
    if(controlPoint_) {
        QPainterPath painterpath = graphicsitem_.path();
        // FIXME: the path should actually go through the "controlPoint_".
        painterpath.quadTo(*controlPoint_, newPos);
        graphicsitem_.setPath(painterpath);

        delete controlPoint_;
        controlPoint_ = 0;
    } else {
        controlPoint_ = new QPointF(newPos);
    }
}

QDomNode WbNewPath::serializeToSvg() {
    if(controlPoint_) {
        QPainterPath painterpath = graphicsitem_.path();
        // FIXME: the path should actually go through the "controlPoint_".
        painterpath.lineTo(*controlPoint_);
        graphicsitem_.setPath(painterpath);

        delete controlPoint_;
        controlPoint_ = 0;
    }

    return WbNewItem::serializeToSvg();
}

QGraphicsItem* WbNewPath::graphicsItem() {
    return &graphicsitem_;
}
