/*
 * xmpp_agentitem.h
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

#ifndef XMPP_AGENTITEM
#define XMPP_AGENTITEM

#include <QString>

#include "xmpp/jid/jid.h"
#include "xmpp_features.h"

namespace XMPP {
	class AgentItem
	{
	public:
		AgentItem() { }

		const Jid & jid() const { return v_jid; }
		const QString & name() const { return v_name; }
		const QString & category() const { return v_category; }
		const QString & type() const { return v_type; }
		const Features & features() const { return v_features; }

		void setJid(const Jid &j) { v_jid = j; }
		void setName(const QString &n) { v_name = n; }
		void setCategory(const QString &c) { v_category = c; }
		void setType(const QString &t) { v_type = t; }
		void setFeatures(const Features &f) { v_features = f; }

	private:
		Jid v_jid;
		QString v_name, v_category, v_type;
		Features v_features;
	};
}

#endif

