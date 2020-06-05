/*
 * svgiconengine.cpp - icon engine designed to be used with PsiIcon
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

#include "svgiconengine.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPixmapCache>

QSize SvgIconEngine::actualSize(const QSize &size, QIcon::Mode mode, QIcon::State state)
{
    Q_UNUSED(mode);
    Q_UNUSED(state);
    return size.isEmpty() ? renderer->defaultSize() : renderer->defaultSize().scaled(size, Qt::KeepAspectRatio);
}

QIconEngine *SvgIconEngine::clone() const { return new SvgIconEngine(name, renderer); }

void SvgIconEngine::paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state)
{
    Q_UNUSED(mode);
    Q_UNUSED(state);
    auto r = rect.isEmpty() ? QRect(0, 0, painter->device()->width(), painter->device()->height()) : rect;
    renderer->render(painter, r);
}

void SvgIconEngine::virtual_hook(int id, void *data)
{
    switch (id) {
    case QIconEngine::AvailableSizesHook:
        reinterpret_cast<AvailableSizesArgument *>(data)->sizes.clear();
        break;
    case QIconEngine::IconNameHook:
        *reinterpret_cast<QString *>(data) = name;
        break;
#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
    case QIconEngine::IsNullHook:
        *reinterpret_cast<bool *>(data) = !(renderer && renderer->isValid());
        break;
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    case QIconEngine::ScaledPixmapHook: {
        auto arg    = reinterpret_cast<ScaledPixmapArgument *>(data);
        arg->pixmap = pixmap(arg->size * arg->scale, arg->mode, arg->state);
        break;
    }
#endif
    }
}

QPixmap SvgIconEngine::pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state)
{
    QPixmap pm;
    if (mode == QIcon::Disabled && QPixmapCache::find(disabledCache, &pm))
        return pm;

    if (QPixmapCache::find(normalCache, &pm)) {
        if (mode == QIcon::Active || mode == QIcon::Normal)
            return pm;
    }

    if (!pm)
        pm = renderPixmap(size, mode, state);
    normalCache = QPixmapCache::insert(pm);

    if (mode == QIcon::Selected) {
        auto hlColor = qApp->palette().color(QPalette::Normal, QPalette::Highlight);
        hlColor.setAlpha(128);
        QPainter p(&pm);
        p.setPen(Qt::NoPen);
        p.fillRect(pm.rect(), hlColor);
    } else if (mode == QIcon::Disabled) {
        auto img = pm.toImage();
        for (int x = 0; x < img.width(); x++) {
            for (int y = 0; y < img.height(); y++) {
#if QT_VERSION < QT_VERSION_CHECK(5, 6, 0)
                QColor c = QColor(img.pixel(x, y));
#else
                QColor c = img.pixelColor(x, y);
#endif
                auto t = c.alpha();
                auto h = c.hue();
                auto v = c.value();
                c.setHsv(h, 0, v, t);
#if QT_VERSION < QT_VERSION_CHECK(5, 6, 0)
                img.setPixel(x, y, c.rgba());
#else
                img.setPixelColor(x, y, c);
#endif
            }
        }
        pm            = QPixmap::fromImage(img);
        disabledCache = QPixmapCache::insert(pm);
    }

    return pm;
}

QPixmap SvgIconEngine::renderPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state)
{
    Q_UNUSED(mode)
    Q_UNUSED(state)
    auto    sz = size.isEmpty() ? renderer->defaultSize() : renderer->defaultSize().scaled(size, Qt::KeepAspectRatio);
    QPixmap pix(sz);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    renderer->render(&p);
    return pix;
}
