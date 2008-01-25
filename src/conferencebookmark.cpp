/*
 * conferencebookmark.cpp
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

#include <QDomDocument>
#include <QDomElement>

#include "conferencebookmark.h"
#include "xmpp_xmlcommon.h"

ConferenceBookmark::ConferenceBookmark(const QString& name, const XMPP::Jid& jid, bool auto_join, const QString& nick, const QString& password) : name_(name), jid_(jid), auto_join_(auto_join), nick_(nick), password_(password)
{ 
}

ConferenceBookmark::ConferenceBookmark(const QDomElement& el) : auto_join_(false)
{
	fromXml(el);
}

const QString& ConferenceBookmark::name() const 
{ 
	return name_; 
}

const XMPP::Jid& ConferenceBookmark::jid() const 
{ 
	return jid_; 
}

bool ConferenceBookmark::autoJoin() const 
{ 
	return auto_join_; 
}

const QString& ConferenceBookmark::nick() const 
{ 
	return nick_; 
}

const QString& ConferenceBookmark::password() const 
{ 
	return password_; 
}

bool ConferenceBookmark::isNull() const
{
	return name_.isEmpty() && jid_.isEmpty();
}

void ConferenceBookmark::fromXml(const QDomElement& e)
{
	jid_ = e.attribute("jid");
	name_ = e.attribute("name");
	if (e.attribute("autojoin") == "true" || e.attribute("autojoin") == "1")
		auto_join_ = true;
	
	for (QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if (i.isNull())
			continue;
		else if (i.tagName() == "nick") {
			nick_ = i.text();
		}
		else if (i.tagName() == "password") {
			password_ = i.text();
		}
	}
}

QDomElement ConferenceBookmark::toXml(QDomDocument& doc) const
{
	QDomElement e = doc.createElement("conference");
	e.setAttribute("jid",jid_.full());
	e.setAttribute("name",name_);
	if (auto_join_) 
		e.setAttribute("autojoin","true");
	if (!nick_.isEmpty())
		e.appendChild(textTag(&doc,"nick",nick_));
	if (!password_.isEmpty())
		e.appendChild(textTag(&doc,"password",password_));

	return e;
}

bool ConferenceBookmark::operator==(const ConferenceBookmark & other) const
{
	return
	    name_       == other.name_       &&
	    jid_.full() == other.jid_.full() &&
	    auto_join_  == other.auto_join_  &&
	    nick_       == other.nick_       &&
	    password_   == other.password_;
}
