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
#include <QPixmapCache>
#include <QSvgRenderer>
#include <memory>

class SvgIconEngine : public QIconEngine {

    QString                       name;
    std::shared_ptr<QSvgRenderer> renderer;

    struct CacheEntry {
        QPixmapCache::Key key;
        QSize             size;
    };
    std::list<CacheEntry> normalCache, disabledCache;

public:
    SvgIconEngine(const QString &name, std::shared_ptr<QSvgRenderer> renderer) : name(name), renderer(renderer) { }

    QSize        actualSize(const QSize &size, QIcon::Mode mode, QIcon::State state) override;
    QIconEngine *clone() const override;
    void         paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override;
    void         virtual_hook(int id, void *data) override;

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override;

private:
    QPixmap renderPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state);
};

#endif // SVGICONENGINE_H
