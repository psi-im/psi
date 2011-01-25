/*
 * bob.h - Bits of Binary server and manager
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef BOB_H
#define BOB_H

#include "xmpp/xmpp-im/xmpp_bitsofbinary.h"

using namespace XMPP;

class FileCache;

class BoBFileCache : public BoBCache
{
	Q_OBJECT
public:
	static BoBFileCache *instance();

	virtual void put(const BoBData &);
	virtual BoBData get(const QString &);

private:
	BoBFileCache();

	FileCache *_fileCache;
	static BoBFileCache *_instance;
};

#endif
