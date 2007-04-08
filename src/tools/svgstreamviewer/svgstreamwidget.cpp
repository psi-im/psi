/*
 * svgstreamwidget.cpp - a widget for showing changing SVG documents
 * Copyright (C) 2006  Joonas Govenius
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

#include "svgstreamwidget.h"
#include <QPainter>

SvgStreamWidget::SvgStreamWidget(QWidget *parent) : QWidget(parent)
{
    renderer = new SvgStreamRenderer(this);
    connect(renderer, SIGNAL(repaintNeeded()), this, SLOT(update()));
}

QSize SvgStreamWidget::sizeHint() const
{
    if (renderer)
        return renderer->defaultSize();
    return QWidget::sizeHint();
}

bool  SvgStreamWidget::load(const QString &filename) {
	return renderer->load(filename);
}

bool  SvgStreamWidget::load(const QByteArray &contents) {
	return renderer->load(contents);
}

bool SvgStreamWidget::setElement(const QDomElement &element, bool forcenew ) {
	return renderer->setElement(element, forcenew);
}

bool SvgStreamWidget::setElement(const QString &element, bool forcenew ) {
	return renderer->setElement(element, forcenew);
}

bool SvgStreamWidget::moveElement(const QString &id, const int &n) {
	return renderer->moveElement(id, n);
}

bool SvgStreamWidget::removeElement(const QString &id) {
	return renderer->removeElement(id);
}

void SvgStreamWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setViewport(0, 0, width(), height());
    renderer->render(&p);
}
