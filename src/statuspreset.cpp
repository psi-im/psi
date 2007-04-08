/*
 * statuspreset.cpp
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

#include <QString>

#include "maybe.h"
#include "statuspreset.h"

// From common.h
#define STATUS_OFFLINE   0
#define STATUS_ONLINE    1
#define STATUS_AWAY      2
#define STATUS_XA        3
#define STATUS_DND       4
#define STATUS_INVISIBLE 5
#define STATUS_CHAT      6


//-----------------------------------------------------------------------------
// StatusPreset
//-----------------------------------------------------------------------------

StatusPreset::StatusPreset() :  name_(""), message_(""), status_(STATUS_AWAY)
{
}

StatusPreset::StatusPreset(QString name, QString message, int status) 
:  name_(name), message_(message), status_(status)
{
}

StatusPreset::StatusPreset(QString name, int priority, QString message, int status)
:  name_(name), message_(message), status_(status)
{
	setPriority(priority);
}

StatusPreset::StatusPreset(const QDomElement& el)
:  name_(""), message_(""), status_(STATUS_AWAY)
{
	fromXml(el);
}

QString StatusPreset::name() const
{
	return name_;
}

void StatusPreset::setName(const QString& name)
{
	name_ = name;
}

QString StatusPreset::message() const
{
	return message_;
}

void StatusPreset::setMessage(const QString& message)
{
	message_ = message;
}

int StatusPreset::status() const
{
	return status_;
}

void StatusPreset::setStatus(int status)
{
	status_ = status;
}

Maybe<int> StatusPreset::priority() const
{
	return priority_;
}

void StatusPreset::setPriority(int priority)
{
	priority_ = Maybe<int>(priority);
}

void StatusPreset::clearPriority() 
{
	priority_ = Maybe<int>();
}

QDomElement StatusPreset::toXml(QDomDocument& doc) const
{
	QDomElement preset = doc.createElement("preset");
	QDomText text = doc.createTextNode(message());
	preset.appendChild(text);
							
	preset.setAttribute("name",name());
	if (priority_.hasValue()) 
		preset.setAttribute("priority", priority_.value());
	QString stat;
	switch(status()) {
		case STATUS_OFFLINE: stat = "offline"; break;
		case STATUS_ONLINE: stat = "online"; break;
		case STATUS_AWAY: stat = "away"; break;
		case STATUS_XA: stat = "xa"; break;
		case STATUS_DND: stat = "dnd"; break;
		case STATUS_INVISIBLE: stat = "invisible"; break;
		case STATUS_CHAT: stat = "chat"; break;
		default: stat = "away";
	}
	preset.setAttribute("status", stat);
	return preset;
}

void StatusPreset::fromXml(const QDomElement &el)
{
	// FIXME: This is the old format. Should be removed in the future
	if (el.tagName() == "item") {
		setName(el.attribute("name"));
		setMessage(el.text());
		return;
	}

	if (el.isNull() || el.tagName() != "preset")
		return;

	setName(el.attribute("name"));
	setMessage(el.text());
	if (el.hasAttribute("priority")) 
		setPriority(el.attribute("priority").toInt());
	
	QString stat = el.attribute("status","away");
	if (stat == "offline")
		setStatus(STATUS_OFFLINE);
	else if (stat == "online")
		setStatus(STATUS_ONLINE);
	else if (stat == "away")
		setStatus(STATUS_AWAY);
	else if (stat == "xa")
		setStatus(STATUS_XA);
	else if (stat == "dnd")
		setStatus(STATUS_DND);
	else if (stat == "invisible")
		setStatus(STATUS_INVISIBLE);
	else if (stat == "chat")
		setStatus(STATUS_CHAT);
	else 
		setStatus(STATUS_AWAY);
 }
