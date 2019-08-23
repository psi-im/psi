/*
 * wbnewpath.h - a class used for representing a path on the whiteboard
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef WBNEWPATH_H
#define WBNEWPATH_H

#include "wbnewitem.h"

class WbNewPath : public WbNewItem {
    public:
        WbNewPath(QGraphicsScene* s, QPointF startPos, int strokeWidth, const QColor &strokeColor, const QColor &fillColor);
        ~WbNewPath();
        void parseCursorMove(QPointF newPos);
        QDomNode serializeToSvg(QDomDocument *doc);

    protected:
        QGraphicsItem* graphicsItem();

    private:
        QGraphicsPathItem graphicsitem_;
        QPointF* controlPoint_;
};

#endif // WBNEWPATH_H
