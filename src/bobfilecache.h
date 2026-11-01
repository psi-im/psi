/*
 * bob.h - Bits of Binary server and manager
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

#ifndef BOBFILECACHE_H
#define BOBFILECACHE_H

#include "iris/xmpp_bitsofbinary.h"

class FileCache;

using namespace XMPP;

// One account's view of the cache. All views keep the bytes in one place, but a view
// answers only for data that came through its own account: BoBManager hands whatever
// the cache returns to any remote requester.
class BoBFileCache : public BoBCache {
    Q_OBJECT
public:
    explicit BoBFileCache(const QString &accountId, QObject *parent = nullptr);

    virtual void    put(const BoBData &) override;
    virtual BoBData get(const Hash &) override;

private:
    QString _accountId;
};

#endif // BOBFILECACHE_H
