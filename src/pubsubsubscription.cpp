/*
 * pubsubsubscription.cpp
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
#include <QString>

#include "pubsubsubscription.h"


PubSubSubscription::PubSubSubscription() 
{ 
}

PubSubSubscription::PubSubSubscription(const QDomElement& e) 
{
	fromXml(e);
}

const QString& PubSubSubscription::jid() const 
{ 
	return jid_; 
}

const QString& PubSubSubscription::node() const 
{ 
	return node_;
}

PubSubSubscription::State PubSubSubscription::state() const 
{ 
	return state_; 
}

bool PubSubSubscription::isNull() const 
{
	return jid_.isEmpty() && node_.isEmpty();
}

void PubSubSubscription::fromXml(const QDomElement& e) 
{
	if (e.tagName() != "subscription")
		return;

	node_ = e.attribute("node");
	jid_ = e.attribute("jid");

	QString sub = e.attribute("subscription");
	if (sub == "none")
		state_ = None;
	else if (sub == "pending")
		state_ = Pending;
	else if (sub == "unconfigured")
		state_ = Unconfigured;
	else if (sub == "subscribed")
		state_ = Subscribed;
}

QDomElement PubSubSubscription::toXml(QDomDocument& doc) const 
{
	QDomElement s = doc.createElement("subscription");
	s.setAttribute("node",node_);
	if (state_ == None)
		s.setAttribute("subscription","none");
	else if (state_ == Pending)
		s.setAttribute("subscription","pending");
	else if (state_ == Unconfigured)
		s.setAttribute("subscription","unconfigured");
	else if (state_ == Subscribed)
		s.setAttribute("subscription","subscribed");

	return s;
}

bool PubSubSubscription::operator==(const PubSubSubscription& s) const
{
	return jid() == s.jid() && node() == s.node() && state() == s.state();
}

bool PubSubSubscription::operator!=(const PubSubSubscription& s) const
{
	return !((*this) == s);
}
