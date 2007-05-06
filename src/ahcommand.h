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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef AHCOMMAND_H
#define AHCOMMAND_H

#include <QString>

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


class AHCommand 
{
public:
	// Types
	enum Action { NoAction, Execute, Prev, Next, Complete, Cancel };
	enum Status { NoStatus, Completed, Executing, Canceled };
	typedef QList<Action> ActionList;

	// Constructors
	AHCommand(const QString& node, const QString& sessionId = "", Action action = Execute);
	AHCommand(const QString& node, XMPP::XData data, const QString& sessionId = "", Action action = Execute);
	AHCommand(const QDomElement &e);

	// Inspectors
	const QString& node() const { return node_; }
	bool hasData() const { return hasData_; }
	const XMPP::XData& data() const { return data_; }
	const ActionList& actions() const { return actions_; }
	Action defaultAction() const { return defaultAction_; }
	Status status() const { return status_; }
	Action action() const { return action_; }
	const QString& sessionId() const { return sessionId_; }
	const AHCError& error() const { return error_; }

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
	QString node_;
	bool hasData_;
	XMPP::XData data_;
	Status status_;
	Action defaultAction_;
	ActionList actions_;
	Action action_;
	QString sessionId_;
	AHCError error_;
};

#endif
