/*
 * rosteritemexchangetask.cpp
 * Copyright (C) 2006  Remko Troncon
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "xmpp_xmlcommon.h"
#include "xmpp_client.h"
#include "xmpp_liveroster.h"
#include "rosteritemexchangetask.h"

RosterItemExchangeTask::RosterItemExchangeTask(Task* parent) : Task(parent), ignoreNonRoster_(false)
{
}

bool RosterItemExchangeTask::take(const QDomElement& e) 
{
	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull())
			continue;
		if(i.tagName() == "x" && i.attribute("xmlns") == "http://jabber.org/protocol/rosterx") {
			Jid from(e.attribute("from"));
			if (client()->roster().find(from,false) == client()->roster().end() && ignoreNonRoster_) {
				// Send a not-authorized error
				QDomElement iq = createIQ(doc(), "error", e.attribute("from"), e.attribute("id"));
				QDomElement error = doc()->createElement("error");
				error.setAttribute("type","cancel");
				QDomElement notauthorized = doc()->createElement("not-authorized");
				notauthorized.setAttribute("xmlns","urn:ietf:params:xml:ns:xmpp-stanzas");
				error.appendChild(notauthorized);
				iq.appendChild(error);
				send(iq);
				setError(e);
				return true;
			}

			// Parse all items
			RosterExchangeItems items;
			for(QDomNode m = i.firstChild(); !m.isNull(); m = m.nextSibling()) {
				QDomElement j = m.toElement();
				if(j.isNull())
					continue;
				RosterExchangeItem it(j);
				if (!it.isNull())
					items += it;
			}

			// Return success
			QDomElement iq = createIQ(doc(), "result", e.attribute("from"), e.attribute("id"));
			send(iq);

			emit rosterItemExchange(from,items);
			setSuccess(true);
			return true;
		}
	}
	return false;
}


void RosterItemExchangeTask::setIgnoreNonRoster(bool b)
{
	ignoreNonRoster_ = b;
}
