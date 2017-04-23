/*
 * ahcommand.cpp - Ad-Hoc Command
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

#include <QDomDocument>
#include <QDomElement>
#include <QSharedData>

#include "ahcommand.h"
#include "xmpp_xdata.h"

#define AHC_NS "http://jabber.org/protocol/commands"
#define XMPPSTANZA_NS "urn:ietf:params:xml:ns:xmpp-stanzas"

using namespace XMPP;

// --------------------------------------------------------------------------
// AHCommand: The class representing an Ad-Hoc command request or reply.
// --------------------------------------------------------------------------

class AHCommandPrivate : public QSharedData
{
public:
	QString node;
	bool hasData = false;
	XMPP::XData data;
	AHCommand::Status status = AHCommand::NoStatus;
	AHCommand::Action defaultAction = AHCommand::NoAction;
	AHCommand::ActionList actions;
	AHCommand::Action action = AHCommand::NoAction;
	QString sessionId;
	AHCError error;
	AHCommand::Note note;

	AHCommandPrivate() {}
	AHCommandPrivate(const AHCommandPrivate &other) :
	    QSharedData(other),
	    node(other.node),
		hasData(other.hasData),
		data(other.data),
		status(other.status),
		defaultAction(other.defaultAction),
		actions(other.actions),
		action(other.action),
		sessionId(other.sessionId),
		error(other.error),
		note(other.note)
	{

	}
	~AHCommandPrivate() { }
};

AHCommand::AHCommand() :
    d(new AHCommandPrivate)
{

}

AHCommand::AHCommand(const QString& node, const QString& sessionId, Action action) :
    d(new AHCommandPrivate)
{
	d->node = node;
	d->action = action;
	d->sessionId = sessionId;
}

AHCommand::AHCommand(const QString& node, XData data, const QString& sessionId, Action action) :
    d(new AHCommandPrivate)
{
	d->node = node;
	d->action = action;
	d->sessionId = sessionId;

	d->hasData = true;
	d->data = data;

}

AHCommand::AHCommand(const QDomElement& q) :
    d(new AHCommandPrivate)
{
	// Parse attributes
	QString status = q.attribute("status");
	setStatus(string2status(status));
	d->node = q.attribute("node");
	d->action = string2action(q.attribute("action"));
	d->sessionId = q.attribute("sessionid");

	// Parse the body
	for (QDomNode n = q.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement e = n.toElement();
		if (e.isNull())
			continue;

		QString tag = e.tagName();

		// A form
		if (tag == "x" && e.attribute("xmlns") =="jabber:x:data") {
			d->data.fromXml(e);
			d->hasData = true;
		}

		// Actions
		else if (tag == "actions") {
			QString execute = e.attribute("execute");
			if (!execute.isEmpty())
				setDefaultAction(string2action(execute));

			for (QDomNode m = e.firstChild(); !m.isNull(); m = m.nextSibling()) {
				Action a = string2action(m.toElement().tagName());
				if (a == Prev || a == Next || a == Complete)
					d->actions += a;
			}
		}

		// Note
		else if (tag == "note") {
			QString stype = e.attribute("type");
			if (stype == "warn")
				d->note.type = Warn;
			else if (stype == "error")
				d->note.type = Error;
			else
				d->note.type = Info;
			d->note.text = e.text();
		}
	}
}

AHCommand::AHCommand(const AHCommand &other) :
    d(other.d)
{
}

AHCommand::~AHCommand()
{

}

AHCommand &AHCommand::operator=(const AHCommand &other)
{
	d = other.d;
	return *this;
}

const QString &AHCommand::node() const { return d->node; }

bool AHCommand::hasData() const { return d->hasData; }

const XData &AHCommand::data() const { return d->data; }

const AHCommand::ActionList &AHCommand::actions() const { return d->actions; }

AHCommand::Action AHCommand::defaultAction() const { return d->defaultAction; }

AHCommand::Status AHCommand::status() const { return d->status; }

AHCommand::Action AHCommand::action() const { return d->action; }

const QString &AHCommand::sessionId() const { return d->sessionId; }

const AHCError &AHCommand::error() const { return d->error; }

bool AHCommand::hasNote() const { return !d->note.text.isEmpty(); }

const AHCommand::Note &AHCommand::note() const { return d->note; }

QDomElement AHCommand::toXml(QDomDocument* doc, bool submit) const
{
	QDomElement command = doc->createElement("command");
	command.setAttribute("xmlns", AHC_NS);
	if (d->status != NoStatus)
		command.setAttribute("status",status2string(status()));
	if (hasData())
		command.appendChild(data().toXml(doc, submit));
	if (d->action != Execute)
		command.setAttribute("action",action2string(d->action));
	command.setAttribute("node", d->node);
	if (!d->sessionId.isEmpty())
		command.setAttribute("sessionid", d->sessionId);

	return command;
}


AHCommand AHCommand::formReply(const AHCommand& c, const XData& data)
{
	AHCommand r(c.node(), data, c.sessionId());
	r.setStatus(AHCommand::Executing);
	return r;
}

AHCommand AHCommand::formReply(const AHCommand& c, const XData& data, const QString& sessionId)
{
	AHCommand r(c.node(), data, sessionId);
	r.setStatus(AHCommand::Executing);
	return r;
}

AHCommand AHCommand::canceledReply(const AHCommand& c)
{
	AHCommand r(c.node(), c.sessionId());
	r.setStatus(Canceled);
	return r;
}

AHCommand AHCommand::completedReply(const AHCommand& c)
{
	AHCommand r(c.node(), c.sessionId());
	r.setStatus(Completed);
	return r;
}

AHCommand AHCommand::completedReply(const AHCommand& c, const XData& d)
{
	AHCommand r(c.node(), d, c.sessionId());
	r.setStatus(Completed);
	return r;
}

//AHCommand AHCommand::errorReply(const AHCommand& c, const AHCError& error)
//{
//	AHCommand r(c.node(), c.sessionId());
//	r.setError(error);
//	return r;
//}

void AHCommand::setStatus(Status s)
{
	d->status = s;
}

void AHCommand::setError(const AHCError& e)
{
	d->error = e;
}

void AHCommand::setDefaultAction(Action a)
{
	d->defaultAction = a;
}

QString AHCommand::status2string(Status status)
{
	QString s;
	switch (status) {
		case Executing : s = "executing"; break;
		case Completed : s = "completed"; break;
		case Canceled : s = "canceled"; break;
		case NoStatus : s = ""; break;
	}
	return s;
}

QString AHCommand::action2string(Action action)
{
	QString s;
	switch (action) {
		case Prev : s = "prev"; break;
		case Next : s = "next"; break;
		case Cancel : s = "cancel"; break;
		case Complete : s = "complete"; break;
		default: break;
	}
	return s;
}

AHCommand::Action AHCommand::string2action(const QString& s)
{
	if (s == "prev")
		return Prev;
	else if (s == "next")
		return Next;
	else if (s == "complete")
		return Complete;
	else if (s == "cancel")
		return Cancel;
	else
		return Execute;
}

AHCommand::Status AHCommand::string2status(const QString& s)
{
	if (s == "canceled")
		return Canceled;
	else if (s == "completed")
		return Completed;
	else if (s == "executing")
		return Executing;
	else
		return NoStatus;
}


// --------------------------------------------------------------------------
// AHCError: The class representing an Ad-Hoc command error
// --------------------------------------------------------------------------

AHCError::AHCError(ErrorType t) : type_(t)
{
}

AHCError::AHCError(const QDomElement& e) : type_(None)
{
	QString errorGeneral = "", errorSpecific = "";

	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull())
			continue;

		QString tag = i.tagName();

		if ((tag == "bad-request" || tag == "not-allowed" || tag == "forbidden" || tag == "forbidden" || tag == "item-not-found" || tag == "feature-not-implemented") && e.attribute("xmlns") == XMPPSTANZA_NS) {
			errorGeneral = tag;
		}
		else if ((tag == "malformed-action" || tag == "bad-action" || tag == "bad-locale" || tag == "bad-payload" || tag == "bad-sessionid" || tag == "session-expired") && e.attribute("xmlns") == AHC_NS) {
			errorSpecific = tag;
		}
	}

	type_ = strings2error(errorGeneral, errorSpecific);
}

QDomElement AHCError::toXml(QDomDocument* doc) const
{
	QDomElement err = doc->createElement("error");

	// Error handling
	if (type_ != None) {
		QString desc, specificCondition = "";
		switch (type_) {
			case MalformedAction:
				desc = "bad-request";
				specificCondition = "malformed-action";
				break;
			case BadAction:
				desc = "bad-request";
				specificCondition = "bad-action";
				break;
			case BadLocale:
				desc = "bad-request";
				specificCondition = "bad-locale";
				break;
			case BadPayload:
				desc = "bad-request";
				specificCondition = "bad-payload";
				break;
			case BadSessionID:
				desc = "bad-request";
				specificCondition = "bad-sessionid";
				break;
			case SessionExpired:
				desc = "not-allowed";
				specificCondition = "session-expired";
				break;
			case Forbidden:
				desc = "forbidden";
				break;
			case ItemNotFound:
				desc = "item-not-found";
				break;
			case FeatureNotImplemented:
				desc = "feature-not-implemented";
				break;
			case None:
				break;
		}

		// General error condition
		QDomElement generalElement = doc->createElement(desc);
		generalElement.setAttribute("xmlns", XMPPSTANZA_NS);
		err.appendChild(generalElement);

		// Specific error condition
		if (!specificCondition.isEmpty()) {
			QDomElement generalElement = doc->createElement(specificCondition);
			generalElement.setAttribute("xmlns", AHC_NS);
			err.appendChild(generalElement);
		}
	}

	return err;
}

AHCError::ErrorType AHCError::strings2error(const QString& g, const QString& s)
{
	if (s == "malformed-action")
		return MalformedAction;
	if (s == "bad-action")
		return BadAction;
	if (s == "bad-locale")
		return BadLocale;
	if (s == "bad-payload")
		return BadPayload;
	if (s == "bad-sessionid")
		return BadSessionID;
	if (s == "session-expired")
		return SessionExpired;
	if (g == "forbidden")
		return Forbidden;
	if (g == "item-not-found")
		return ItemNotFound;
	if (g == "feature-not-implemented")
		return FeatureNotImplemented;
	return None;
}

QString AHCError::error2description(const AHCError& e)
{
	QString desc;
	switch (e.type()) {
		case MalformedAction:
			desc = QString("The responding JID does not understand the specified action");
			break;
		case BadAction:
			desc = QString("The responding JID cannot accept the specified action");
			break;
		case BadLocale:
			desc = QString("The responding JID cannot accept the specified language/locale");
			break;
		case BadPayload:
			desc = QString("The responding JID cannot accept the specified payload (eg the data form did not provide one or more required fields)");
			break;
		case BadSessionID:
			desc = QString("The responding JID cannot accept the specified sessionid");
			break;
		case SessionExpired:
			desc = QString("The requesting JID specified a sessionid that is no longer active (either because it was completed, canceled, or timed out)");
			break;
		case Forbidden:
			desc = QString("The requesting JID is not allowed to execute the command");
			break;
		case ItemNotFound:
			desc = QString("The responding JID cannot find the requested command node");
			break;
		case FeatureNotImplemented:
			desc = QString("The responding JID does not support Ad-hoc commands");
			break;
		case None:
			break;
	}
	return desc;
}
