/*
 * Copyright (C) 2001, 2002  Justin Karneges
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

#ifndef XMPP_DISCOINFOTASK_H
#define XMPP_DISCOINFOTASK_H

#include "xmpp_task.h"
#include "xmpp_discoitem.h"

namespace XMPP {
	class Jid;
}
class QString;
class QDomElement;

namespace XMPP
{
	class DiscoInfoTask : public Task
	{
		Q_OBJECT
	public:
		DiscoInfoTask(Task *);
		~DiscoInfoTask();
	
		void get(const Jid &, const QString &node = QString::null, const DiscoItem::Identity = DiscoItem::Identity());
		void get(const DiscoItem &);
	
		const DiscoItem &item() const;
		const Jid& jid() const;
		const QString& node() const;
	
		void onGo();
		bool take(const QDomElement &);
	
	private:
		class Private;
		Private *d;
	};

	// Deprecated name
	typedef DiscoInfoTask JT_DiscoInfo;
}
#endif
