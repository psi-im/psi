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

#include <QCoreApplication>
#include <QStringList>

using namespace XMPP;

static const QString accountsKey = QStringLiteral("accounts");

// Lives as long as the process, because accounts may use it until they are gone. It must
// finish its work before the application goes away: the database is not usable after that.
static FileCache *sharedCache()
{
    static FileCache *cache = [] {
        auto *created = new FileCache(QStringLiteral("bob"), ApplicationInfo::bobDir());
        QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, created, [created] {
            created->gc();
            created->sync(true);
        });
        return created;
    }();
    return cache;
}

BoBFileCache::BoBFileCache(const QString &accountId, QObject *parent) : BoBCache(parent), _accountId(accountId) { }

void BoBFileCache::put(const BoBData &data)
{
    // A cid names its content, and the bytes are shared between accounts: a peer
    // of one account must not be able to replace them for another.
    if (Hash::from(data.hash().type(), data.data()).data() != data.hash().data()) {
        qWarning("Ignoring Bits of Binary data that does not match its cid");
        return;
    }
    auto       *cache = sharedCache();
    QStringList accounts;
    if (auto *existing = cache->get(data.hash()))
        accounts = existing->metadata().value(accountsKey).toStringList();
    if (!accounts.contains(_accountId))
        accounts.append(_accountId);
    QVariantMap md;
    md.insert(QLatin1String("type"), data.type());
    md.insert(accountsKey, accounts);
    cache->append(data.hash(), data.data(), md, data.maxAge());
}

BoBData BoBFileCache::get(const Hash &h)
{
    FileCacheItem *item = sharedCache()->get(h);
    BoBData        bd;
    if (item && item->metadata().value(accountsKey).toStringList().contains(_accountId)) {
        bd.setHash(h);
        bd.setData(item->data());
        bd.setMaxAge(item->maxAge());
        QVariantMap md = item->metadata();
        bd.setType(md[QLatin1String("type")].toString());
    }
    return bd;
}
