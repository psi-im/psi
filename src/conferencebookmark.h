/*
 * conferencebookmark.h
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

#ifndef CONFERENCEBOOKMARK_H
#define CONFERENCEBOOKMARK_H

#include <QString>

#include "xmpp_jid.h"

class QDomElement;
class QDomDocument;

class ConferenceBookmark
{
public:
	ConferenceBookmark(const QString& name, const XMPP::Jid& jid, bool auto_join, const QString& nick = QString(), const QString& password = QString());
	ConferenceBookmark(const QDomElement&);

	const QString& name() const;
	const XMPP::Jid& jid() const;
	bool autoJoin() const;
	const QString& nick() const;
	const QString& password() const;
	
	bool isNull() const;

	void fromXml(const QDomElement&);
	QDomElement toXml(QDomDocument&) const;

	bool operator==(const ConferenceBookmark& other) const;

private:
	QString name_;
	XMPP::Jid jid_;
	bool auto_join_;
	QString nick_;
	QString password_;
};

#endif
