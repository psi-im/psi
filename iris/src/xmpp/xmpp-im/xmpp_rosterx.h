/*
 * rosterexchangeitem.h
 * Copyright (C) 2003  Remko Troncon
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

#ifndef XMPP_ROSTERX_H
#define XMPP_ROSTERX_H

#include <QString>
#include <QStringList>

#include "xmpp/jid/jid.h"

class QDomElement;

namespace XMPP
{
	class Stanza;

	class RosterExchangeItem
	{
	public:
		enum Action { Add, Delete, Modify };

		RosterExchangeItem(const Jid& jid, const QString& name = "", const QStringList& groups = QStringList(), Action = Add);
		RosterExchangeItem(const QDomElement&);
		
		const Jid& jid() const;
		Action action() const;
		const QString& name() const;
		const QStringList& groups() const;
		bool isNull() const;

		void setJid(const Jid&);
		void setAction(Action);
		void setName(const QString&);
		void setGroups(const QStringList&);

		QDomElement toXml(Stanza&) const;
		void fromXml(const QDomElement&);
		
	private:
		Jid jid_;
		QString name_;
		QStringList groups_;
		Action action_;
	};
	typedef QList<RosterExchangeItem> RosterExchangeItems;
}

#endif
