/*
 * bob.cpp - Bits of Binary server and manager
 * Copyright (C) 2010 Rion
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
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "bobfilecache.h"
#include "applicationinfo.h"
#include "filecache.h"
#include <QApplication>

using namespace XMPP;


BoBFileCache::BoBFileCache()
	: BoBCache(0)
{
	setParent(QApplication::instance());
	_fileCache = new FileCache(ApplicationInfo::bobDir(), this);
}

BoBFileCache* BoBFileCache::instance()
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
	_fileCache->append(data.cid(), data.data(), md, data.maxAge());
}

BoBData BoBFileCache::get(const QString &cid)
{
	FileCacheItem *item = _fileCache->get(cid);
	BoBData bd;
	if (item) {
		bd.setCid(item->id());
		bd.setData(item->data());
		bd.setMaxAge(item->maxAge());
		QVariantMap md = item->metadata();
		bd.setType(md[QLatin1String("type")].toString());
	}
	return bd;
}

BoBFileCache* BoBFileCache::_instance = 0;
