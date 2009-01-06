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

#ifndef XMPP_LIVEROSTERITEM_H
#define XMPP_LIVEROSTERITEM_H

#include "xmpp_status.h"
#include "xmpp_resourcelist.h"
#include "xmpp_rosteritem.h"

namespace XMPP
{
	class LiveRosterItem : public RosterItem
	{
	public:
		LiveRosterItem(const Jid &j="");
		LiveRosterItem(const RosterItem &);
		~LiveRosterItem();

		void setRosterItem(const RosterItem &);

		ResourceList & resourceList();
		ResourceList::Iterator priority();

		const ResourceList & resourceList() const;
		ResourceList::ConstIterator priority() const;

		bool isAvailable() const;
		const Status & lastUnavailableStatus() const;
		bool flagForDelete() const;

		void setLastUnavailableStatus(const Status &);
		void setFlagForDelete(bool);

	private:
		ResourceList v_resourceList;
		Status v_lastUnavailableStatus;
		bool v_flagForDelete;
	};
}

#endif
