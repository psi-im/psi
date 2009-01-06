/*
 * xmpp_discoitem.h
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

#ifndef XMPP_DISCOITEM
#define XMPP_DISCOITEM

#include <QString>

#include "xmpp/jid/jid.h"
#include "xmpp_features.h"
#include "xmpp_agentitem.h"

namespace XMPP {
	class DiscoItem
	{
	public:
		DiscoItem();
		~DiscoItem();

		const Jid &jid() const;
		const QString &node() const;
		const QString &name() const;

		void setJid(const Jid &);
		void setName(const QString &);
		void setNode(const QString &);

		enum Action {
			None = 0,
			Remove,
			Update
		};

		Action action() const;
		void setAction(Action);

		const Features &features() const;
		void setFeatures(const Features &);

		struct Identity
		{
			QString category;
			QString name;
			QString type;
		};

		typedef QList<Identity> Identities;

		const Identities &identities() const;
		void setIdentities(const Identities &);

		// some useful helper functions
		static Action string2action(QString s);
		static QString action2string(Action a);

		DiscoItem & operator= (const DiscoItem &);
		DiscoItem(const DiscoItem &);

		operator AgentItem() const { return toAgentItem(); }
		AgentItem toAgentItem() const;
		void fromAgentItem(const AgentItem &);

	private:
		class Private;
		Private *d;
	};
}

#endif
