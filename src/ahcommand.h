/*
 * ahcommand.h - Ad-Hoc Command
 * Copyright (C) 2005  Remko Troncon
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

#ifndef AHCOMMAND_H
#define AHCOMMAND_H

#include <QString>
#include <QSharedDataPointer>

#include "xmpp_xdata.h"

class QDomElement;
class QDomDocument;

class AHCError
{
public:
	enum ErrorType {
		None, MalformedAction, BadAction, BadLocale,
		BadPayload, BadSessionID, SessionExpired, Forbidden, ItemNotFound,
		FeatureNotImplemented
	};

	AHCError(ErrorType = None);
	AHCError(const QDomElement& e);

	ErrorType type() const { return type_; }
	static QString error2description(const AHCError&);
	QDomElement toXml(QDomDocument* doc) const;

protected:
	static ErrorType strings2error(const QString&, const QString&);

private:
	ErrorType type_;
};

class AHCommandPrivate;
class AHCommand
{
public:
	// Types
	enum Action { NoAction, Execute, Prev, Next, Complete, Cancel };
	enum Status { NoStatus, Completed, Executing, Canceled };
	typedef QList<Action> ActionList;
	enum NoteType { Info, Warn, Error };
	struct Note {
		QString  text;
		NoteType type;
		Note() : type(Info) {}
	};

	// Constructors
	AHCommand();
	AHCommand(const QString& node, const QString& sessionId = "", Action action = Execute);
	AHCommand(const QString& node, XMPP::XData data, const QString& sessionId = "", Action action = Execute);
	AHCommand(const QDomElement &e);
	AHCommand(const AHCommand &other);
	~AHCommand();

	AHCommand& operator=(const AHCommand &other);

	// Inspectors
	const QString& node() const;
	bool hasData() const;
	const XMPP::XData& data() const;
	const ActionList& actions() const;
	Action defaultAction() const;
	Status status() const;
	Action action() const;
	const QString& sessionId() const;
	const AHCError& error() const;
	bool hasNote() const;
	const Note& note() const;

	// XML conversion
	QDomElement toXml(QDomDocument* doc, bool submit) const;

	// Helper constructors
	static AHCommand formReply(const AHCommand&, const XMPP::XData&);
	static AHCommand formReply(const AHCommand&, const XMPP::XData&, const QString& sessionId);
	static AHCommand canceledReply(const AHCommand&);
	static AHCommand completedReply(const AHCommand&);
	static AHCommand completedReply(const AHCommand&, const XMPP::XData&);
	//static AHCommand errorReply(const AHCommand&, const AHCError&);

protected:
	void setStatus(Status s);
	void setError(const AHCError& e);
	void setDefaultAction(Action a);

	static QString action2string(Action);
	static QString status2string(Status);
	static Action string2action(const QString&);
	static Status string2status(const QString&);

private:
	QSharedDataPointer<AHCommandPrivate> d;
};

#endif
