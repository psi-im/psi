/*
 * privacylistitem.h
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

#ifndef PRIVACYLISTITEM_H
#define PRIVACYLISTITEM_H

#include <QString>

class QDomElement;
class QDomDocument;

class PrivacyListItem
{
public:
	typedef enum { FallthroughType, JidType, GroupType, SubscriptionType } Type;
	typedef enum { Allow, Deny } Action;

	PrivacyListItem();
	PrivacyListItem(const QDomElement& e);

	Type type() const { return type_; }
	Action action() const { return action_; }
	bool message() const { return message_; }
	bool presenceIn() const { return presenceIn_; }
	bool presenceOut() const { return presenceOut_; }
	bool iq() const { return iq_; }
	bool all() const { return iq_ && presenceIn_ && presenceOut_ && message_; }
	const QString& value() const { return value_; }
	unsigned int order() const { return order_; }

	void setType(Type type) { type_ = type; }
	void setAction(Action action) { action_ = action; }
	void setMessage(bool b) { message_ = b; }
	void setPresenceIn(bool b) { presenceIn_ = b; }
	void setPresenceOut(bool b) { presenceOut_ = b; }
	void setIQ(bool b) { iq_ = b; }
	void setAll() { iq_ = presenceIn_ = presenceOut_ = message_ = true; }
	void setValue(const QString& value) { value_ = value; }
	void setOrder(unsigned int order) { order_ = order; }
	
	bool isBlock() const;
	QString toString() const;
	QDomElement toXml(QDomDocument&) const;
	void fromXml(const QDomElement& e);

	bool operator<(const PrivacyListItem& it) const { return order() < it.order(); }
	bool operator==(const PrivacyListItem& o) const { return type_ == o.type_ && action_ == o.action_ && message_ == o.message_ && presenceIn_ == o.presenceIn_ && presenceOut_ == o.presenceOut_ && iq_ == o.iq_ && order_ == o.order_ && value_ == o.value_; }

	static PrivacyListItem blockItem(const QString& jid);
	
private:
	Type type_;
	Action action_;
	bool message_, presenceIn_, presenceOut_, iq_;
	unsigned int order_;
	QString value_;
};

#endif
