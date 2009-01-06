/*
 * im.h - XMPP "IM" library API
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

#ifndef XMPP_IM_H
#define XMPP_IM_H

#include <qdatetime.h>
//Added by qt3to4:
#include <QList>

#include "xmpp.h"
#include "xmpp/jid/jid.h"
#include "xmpp_muc.h"
#include "xmpp_message.h"
#include "xmpp_chatstate.h"
#include "xmpp_status.h"
#include "xmpp_htmlelement.h"
#include "xmpp_features.h"
#include "xmpp_httpauthrequest.h"
#include "xmpp_url.h"
#include "xmpp_task.h"
#include "xmpp_resource.h"
#include "xmpp_resourcelist.h"
#include "xmpp_roster.h"
#include "xmpp_rosteritem.h"
#include "xmpp_liverosteritem.h"
#include "xmpp_liveroster.h"
#include "xmpp_rosterx.h"
#include "xmpp_xdata.h"
#include "xmpp_discoitem.h"
#include "xmpp_agentitem.h"
#include "xmpp_client.h"
#include "xmpp_address.h"
#include "xmpp_pubsubitem.h"
#include "xmpp_pubsubretraction.h"

namespace XMPP
{
	typedef QMap<QString, QString> StringMap;
	
	typedef QList<AgentItem> AgentList;
	typedef QList<DiscoItem> DiscoList;

	class FormField
	{
	public:
		enum { username, nick, password, name, first, last, email, address, city, state, zip, phone, url, date, misc };
		FormField(const QString &type="", const QString &value="");
		~FormField();

		int type() const;
		QString fieldName() const;
		QString realName() const;
		bool isSecret() const;
		const QString & value() const;
		void setType(int);
		bool setType(const QString &);
		void setValue(const QString &);

	private:
		int tagNameToType(const QString &) const;
		QString typeToTagName(int) const;

		int v_type;
		QString v_value;

		class Private;
		Private *d;
	};

	class Form : public QList<FormField>
	{
	public:
		Form(const Jid &j="");
		~Form();

		Jid jid() const;
		QString instructions() const;
		QString key() const;
		void setJid(const Jid &);
		void setInstructions(const QString &);
		void setKey(const QString &);

	private:
		Jid v_jid;
		QString v_instructions, v_key;

		class Private;
		Private *d;
	};

	class SearchResult
	{
	public:
		SearchResult(const Jid &jid="");
		~SearchResult();

		const Jid & jid() const;
		const QString & nick() const;
		const QString & first() const;
		const QString & last() const;
		const QString & email() const;

		void setJid(const Jid &);
		void setNick(const QString &);
		void setFirst(const QString &);
		void setLast(const QString &);
		void setEmail(const QString &);

	private:
		Jid v_jid;
		QString v_nick, v_first, v_last, v_email;
	};
}

#endif
