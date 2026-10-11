/*
 * wbnewshape.cpp - a basic shape on the whiteboard while it is being drawn
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

#include "wbnewshape.h"

#include <iris/xmpp-im/jingle-sxe-document.h>

#include <QGraphicsScene>
#include <QPainterPath>
#include <QPen>

#include <cmath>
#include <limits>

namespace {
QString number(qreal value) { return QString::number(std::round(value * 100) / 100); }

QPolygonF rhombus(const QRectF &r)
{
    return QPolygonF() << QPointF(r.center().x(), r.top()) << QPointF(r.right(), r.center().y()) << QPointF(r.center().x(), r.bottom()) << QPointF(r.left(), r.center().y());
}
} // namespace

WbNewShape::WbNewShape(QGraphicsScene *s, QPointF startPos, Shape shape, int strokeWidth, const QColor &strokeColor,
                       const QColor &fillColor) :
    WbNewItem(s), shape_(shape), start_(startPos), end_(startPos), strokeWidth_(strokeWidth),
    strokeColor_(strokeColor), fillColor_(fillColor)
{
    graphicsitem_.setZValue(std::numeric_limits<double>::max());
    graphicsitem_.setBrush(QBrush(fillColor));
    graphicsitem_.setPen(QPen(QBrush(strokeColor), strokeWidth));
    scene->addItem(&graphicsitem_);
}

void WbNewShape::parseCursorMove(QPointF newPos)
{
    end_ = newPos;
    updatePreview();
}

void WbNewShape::setConstrained(bool constrained)
{
    if (constrained_ == constrained)
        return;
    constrained_ = constrained;
    updatePreview();
}

QRectF WbNewShape::bounds() const
{
    QPointF end = end_;
    if (constrained_) {
        const qreal side = qMax(qAbs(end_.x() - start_.x()), qAbs(end_.y() - start_.y()));
        end              = QPointF(start_.x() + (end_.x() < start_.x() ? -side : side),
                                   start_.y() + (end_.y() < start_.y() ? -side : side));
    }
    return QRectF(start_, end).normalized();
}

void WbNewShape::updatePreview()
{
    const QRectF r = bounds();
    QPainterPath path;
    switch (shape_) {
    case Shape::Rectangle:
        path.addRect(r);
        break;
    case Shape::Ellipse:
        path.addEllipse(r);
        break;
    case Shape::Rhombus:
        path.addPolygon(rhombus(r));
        path.closeSubpath();
        break;
    }
    graphicsitem_.setPath(path);
}

void WbNewShape::setPresentation(QDomElement &element, int strokeWidth, const QColor &strokeColor,
                                 const QColor &fillColor)
{
    element.setAttribute(QStringLiteral("stroke"), strokeColor.name());
    if (strokeColor.alpha() < 255)
        element.setAttribute(QStringLiteral("stroke-opacity"), number(strokeColor.alphaF()));
    element.setAttribute(QStringLiteral("stroke-width"), strokeWidth);
    if (fillColor.alpha() == 0) {
        element.setAttribute(QStringLiteral("fill"), QStringLiteral("none"));
    } else {
        element.setAttribute(QStringLiteral("fill"), fillColor.name());
        if (fillColor.alpha() < 255)
            element.setAttribute(QStringLiteral("fill-opacity"), number(fillColor.alphaF()));
    }
}

QDomNode WbNewShape::serializeToSvg(QDomDocument *doc)
{
    const QRectF r = bounds();
    if (r.width() < 1 && r.height() < 1)
        return {}; // a click, not a shape

    QDomElement element;
    switch (shape_) {
    case Shape::Rectangle:
        element = doc->createElement(QStringLiteral("rect"));
        element.setAttribute(QStringLiteral("x"), number(r.x()));
        element.setAttribute(QStringLiteral("y"), number(r.y()));
        element.setAttribute(QStringLiteral("width"), number(r.width()));
        element.setAttribute(QStringLiteral("height"), number(r.height()));
        break;
    case Shape::Ellipse:
        element = doc->createElement(QStringLiteral("ellipse"));
        element.setAttribute(QStringLiteral("cx"), number(r.center().x()));
        element.setAttribute(QStringLiteral("cy"), number(r.center().y()));
        element.setAttribute(QStringLiteral("rx"), number(r.width() / 2));
        element.setAttribute(QStringLiteral("ry"), number(r.height() / 2));
        break;
    case Shape::Rhombus: {
        element = doc->createElement(QStringLiteral("polygon"));
        QStringList points;
        for (const auto &p : rhombus(r))
            points << number(p.x()) + QLatin1Char(',') + number(p.y());
        element.setAttribute(QStringLiteral("points"), points.join(QLatin1Char(' ')));
        break;
    }
    }
    element.setAttribute(QStringLiteral("id"), QLatin1String("e") + XMPP::Jingle::SXE::Document::generateUUID());
    setPresentation(element, strokeWidth_, strokeColor_, fillColor_);
    return element;
}

QGraphicsItem *WbNewShape::graphicsItem() { return &graphicsitem_; }
