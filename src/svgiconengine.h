/*
 * svgiconengine.h - icon engine designed to be used with PsiIcon
 * Copyright (C) 2020  Sergey Ilinykh
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef SVGICONENGINE_H
#define SVGICONENGINE_H

#include <QIconEngine>
#include <QSvgRenderer>
#include <memory>

class SvgIconEngine : public QIconEngine {

    std::shared_ptr<QSvgRenderer> renderer;

public:
    SvgIconEngine(std::shared_ptr<QSvgRenderer> renderer) : renderer(renderer) { }

    QIconEngine *clone() const override;
    void         paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override;
};

#endif // SVGICONENGINE_H
