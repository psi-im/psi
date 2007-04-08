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

//-----------------------------------------------------------------------------
// StatusPreset
//-----------------------------------------------------------------------------

StatusPreset::StatusPreset() :  name_(""), message_(""), status_(XMPP::Status::Away)
{
}

StatusPreset::StatusPreset(QString name, QString message, XMPP::Status::Type status)
:  name_(name), message_(message), status_(status)
{
}

StatusPreset::StatusPreset(QString name, int priority, QString message, XMPP::Status::Type status)
:  name_(name), message_(message), status_(status)
{
	setPriority(priority);
}

StatusPreset::StatusPreset(const QDomElement& el)
:  name_(""), message_(""), status_(XMPP::Status::Away)
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

XMPP::Status::Type StatusPreset::status() const
{
	return status_;
}

void StatusPreset::setStatus(XMPP::Status::Type status)
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
	preset.setAttribute("status", XMPP::Status(status()).typeString());
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
	
	XMPP::Status status;
	status.setType(el.attribute("status", "away"));
	setStatus(status.type());
 }
