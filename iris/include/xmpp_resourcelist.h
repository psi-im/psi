/*
 * Copyright (C) 2003  Justin Karneges
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef XMPP_RESOURCELIST_H
#define XMPP_RESOURCELIST_H

#include <QList>

#include "xmpp_resource.h"

class QString;

namespace XMPP
{
	class ResourceList : public QList<Resource>
	{
	public:
		ResourceList();
		~ResourceList();

		ResourceList::Iterator find(const QString &);
		ResourceList::Iterator priority();

		ResourceList::ConstIterator find(const QString &) const;
		ResourceList::ConstIterator priority() const;
	};
}

#endif
