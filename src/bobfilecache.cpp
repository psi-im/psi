/*
 * bob.cpp - Bits of Binary server and manager
 * Copyright (C) 2010  Sergey Ilinykh
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "bobfilecache.h"

#include "applicationinfo.h"
#include "filecache.h"

#include <QApplication>

using namespace XMPP;

BoBFileCache::BoBFileCache() : BoBCache(nullptr)
{
    setParent(QApplication::instance());
    _fileCache = new FileCache(ApplicationInfo::bobDir(), this);
}

BoBFileCache *BoBFileCache::instance()
{
    if (!_instance) {
        _instance = new BoBFileCache;
    }
    return _instance;
}

void BoBFileCache::put(const BoBData &data)
{
    QVariantMap md;
    md.insert(QLatin1String("type"), data.type());
    _fileCache->append(data.hash(), data.data(), md, data.maxAge());
}

BoBData BoBFileCache::get(const Hash &h)
{
    FileCacheItem *item = _fileCache->get(h);
    BoBData        bd;
    if (item) {
        bd.setHash(h);
        bd.setData(item->data());
        bd.setMaxAge(item->maxAge());
        QVariantMap md = item->metadata();
        bd.setType(md[QLatin1String("type")].toString());
    }
    return bd;
}

BoBFileCache *BoBFileCache::_instance = nullptr;
