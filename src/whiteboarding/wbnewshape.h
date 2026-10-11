/*
 * wbnewshape.h - a basic shape on the whiteboard while it is being drawn
 * Copyright (C) 2026  Sergey Ilinykh
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

#ifndef WBNEWSHAPE_H
#define WBNEWSHAPE_H

#include "wbnewitem.h"

#include <QColor>
#include <QGraphicsPathItem>

/*! \brief A rectangle, ellipse or rhombus spanned between the press point and the cursor. */
class WbNewShape : public WbNewItem {
public:
    enum class Shape { Rectangle, Ellipse, Rhombus };

    WbNewShape(QGraphicsScene *s, QPointF startPos, Shape shape, int strokeWidth, const QColor &strokeColor,
               const QColor &fillColor);
    void     parseCursorMove(QPointF newPos) override;
    QDomNode serializeToSvg(QDomDocument *doc) override;
    /*! \brief With \a constrained the shape keeps equal width and height (square, circle). */
    void setConstrained(bool constrained);

    /*! \brief Common SVG presentation attributes of a new drawing. */
    static void setPresentation(QDomElement &element, int strokeWidth, const QColor &strokeColor,
                                const QColor &fillColor);

protected:
    QGraphicsItem *graphicsItem() override;

private:
    QRectF bounds() const;
    void   updatePreview();

    QGraphicsPathItem graphicsitem_;
    Shape             shape_;
    QPointF           start_;
    QPointF           end_;
    bool              constrained_ = false;
    int               strokeWidth_;
    QColor            strokeColor_;
    QColor            fillColor_;
};

#endif // WBNEWSHAPE_H
