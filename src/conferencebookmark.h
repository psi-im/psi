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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
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
	enum JoinType
	{
		Never,
		Always,
		OnlyThisComputer,
		ExceptThisComputer,
		LastJoinType
	};

	ConferenceBookmark(const QString& name, const XMPP::Jid& jid, JoinType auto_join, const QString& nick = QString(), const QString& password = QString());
	ConferenceBookmark(const QDomElement&);

	static QStringList joinTypeNames();

	const QString& name() const;
	const XMPP::Jid& jid() const;
	JoinType autoJoin() const;
	void setAutoJoin(JoinType type);
	const QString& nick() const;
	const QString& password() const;

	bool isNull() const;

	void fromXml(const QDomElement&);
	QDomElement toXml(QDomDocument&) const;

	bool operator==(const ConferenceBookmark& other) const;

private:
	QString name_;
	XMPP::Jid jid_;
	JoinType auto_join_;
	QString nick_;
	QString password_;
};

#endif
