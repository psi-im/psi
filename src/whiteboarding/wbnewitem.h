/*
 * wbnewitem.h - a class used for representing items on the whiteboard
 *              while they're being drawn.
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef WBNEWITEM_H
#define WBNEWITEM_H

#include <QDomNode>
#include <QGraphicsItem>

class WbNewItem {
public:
    WbNewItem(QGraphicsScene *s);
    virtual ~WbNewItem();

    virtual void     parseCursorMove(QPointF newPos) = 0;
    virtual QDomNode serializeToSvg(QDomDocument *doc);

protected:
    virtual QGraphicsItem *graphicsItem() = 0;

    QGraphicsScene *scene;
};

#endif // WBNEWITEM_H
