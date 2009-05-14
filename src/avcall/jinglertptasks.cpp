/*
 * Copyright (C) 2009  Barracuda Networks, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "jinglertptasks.h"

#include "xmpp_xmlcommon.h"

static QDomElement candidateToElement(QDomDocument *doc, const XMPP::Ice176::Candidate &c)
{
	QDomElement e = doc->createElement("candidate");
	e.setAttribute("component", QString::number(c.component));
	e.setAttribute("foundation", c.foundation);
	e.setAttribute("generation", QString::number(c.generation));
	if(!c.id.isEmpty())
		e.setAttribute("id", c.id);
	e.setAttribute("ip", c.ip.toString());
	if(c.network != -1)
		e.setAttribute("network", QString::number(c.network));
	else // weird?
		e.setAttribute("network", QString::number(0));
	e.setAttribute("port", QString::number(c.port));
	e.setAttribute("priority", QString::number(c.priority));
	e.setAttribute("protocol", c.protocol);
	if(!c.rel_addr.isNull())
		e.setAttribute("rel-addr", c.rel_addr.toString());
	if(c.rel_port != -1)
		e.setAttribute("rel-port", QString::number(c.rel_port));
	// TODO: remove this?
	//if(!c.rem_addr.isNull())
	//	e.setAttribute("rem-addr", c.rem_addr.toString());
	//if(c.rem_port != -1)
	//	e.setAttribute("rem-port", QString::number(c.rem_port));
	e.setAttribute("type", c.type);
	return e;
}

static XMPP::Ice176::Candidate elementToCandidate(const QDomElement &e)
{
	if(e.tagName() != "candidate")
		return XMPP::Ice176::Candidate();

	XMPP::Ice176::Candidate c;
	c.component = e.attribute("component").toInt();
	c.foundation = e.attribute("foundation");
	c.generation = e.attribute("generation").toInt();
	c.id = e.attribute("id");
	c.ip = QHostAddress(e.attribute("ip"));
	c.network = e.attribute("network").toInt();
	c.port = e.attribute("port").toInt();
	c.priority = e.attribute("priority").toInt();
	c.protocol = e.attribute("protocol");
	c.rel_addr = QHostAddress(e.attribute("rel-addr"));
	c.rel_port = e.attribute("rel-port").toInt();
	// TODO: remove this?
	//c.rem_addr = QHostAddress(e.attribute("rem-addr"));
	//c.rem_port = e.attribute("rem-port").toInt();
	c.type = e.attribute("type");
	return c;
}

static QDomElement payloadTypeToElement(QDomDocument *doc, const JingleRtpPayloadType &type)
{
	QDomElement e = doc->createElement("payload-type");
	e.setAttribute("id", QString::number(type.id));
	if(!type.name.isEmpty())
		e.setAttribute("name", type.name);
	e.setAttribute("clockrate", QString::number(type.clockrate));
	if(type.channels > 1)
		e.setAttribute("channels", QString::number(type.channels));
	if(type.ptime != -1)
		e.setAttribute("ptime", QString::number(type.ptime));
	if(type.maxptime != -1)
		e.setAttribute("maxptime", QString::number(type.maxptime));
	foreach(const JingleRtpPayloadType::Parameter &p, type.parameters)
	{
		QDomElement pe = doc->createElement("parameter");
		pe.setAttribute("name", p.name);
		pe.setAttribute("value", p.value);
		e.appendChild(pe);
	}
	return e;
}

static JingleRtpPayloadType elementToPayloadType(const QDomElement &e)
{
	if(e.tagName() != "payload-type")
		return JingleRtpPayloadType();

	JingleRtpPayloadType out;
	bool ok;
	int x;

	x = e.attribute("id").toInt(&ok);
	if(!ok)
		return JingleRtpPayloadType();
	out.id = x;

	out.name = e.attribute("name");

	x = e.attribute("clockrate").toInt(&ok);
	if(!ok)
		return JingleRtpPayloadType();
	out.clockrate = x;

	x = e.attribute("channels").toInt(&ok);
	if(ok)
		out.channels = x;

	x = e.attribute("ptime").toInt(&ok);
	if(ok)
		out.ptime = x;

	x = e.attribute("maxptime").toInt(&ok);
	if(ok)
		out.maxptime = x;

	QList<JingleRtpPayloadType::Parameter> plist;
	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling())
	{
		if(!n.isElement())
			continue;

		QDomElement pe = n.toElement();
		if(pe.tagName() == "parameter")
		{
			JingleRtpPayloadType::Parameter p;
			p.name = pe.attribute("name");
			p.value = pe.attribute("value");
			plist += p;
		}
	}
	out.parameters = plist;

	return out;
}

struct JingleCondEntry
{
	const char *str;
	int cond;
};

static JingleCondEntry jingleCondTable[] =
{
	{ "alternative-session",      JingleRtpReason::AlternativeSession },
	{ "busy",                     JingleRtpReason::Busy },
	{ "cancel",                   JingleRtpReason::Cancel },
	{ "connectivity-error",       JingleRtpReason::ConnectivityError },
	{ "decline",                  JingleRtpReason::Decline },
	{ "expired",                  JingleRtpReason::Expired },
	{ "failed-application",       JingleRtpReason::FailedApplication },
	{ "failed-transport",         JingleRtpReason::FailedTransport },
	{ "general-error",            JingleRtpReason::GeneralError },
	{ "gone",                     JingleRtpReason::Gone },
	{ "incompatible-parameters",  JingleRtpReason::IncompatibleParameters },
	{ "media-error",              JingleRtpReason::MediaError },
	{ "security-error",           JingleRtpReason::SecurityError },
	{ "success",                  JingleRtpReason::Success },
	{ "timeout",                  JingleRtpReason::Timeout },
	{ "unsupported-applications", JingleRtpReason::UnsupportedApplications },
	{ "unsupported-transports",   JingleRtpReason::UnsupportedTransports },
	{ 0, 0 }
};

static QString conditionToElementName(JingleRtpReason::Condition cond)
{
	for(int n = 0; jingleCondTable[n].str; ++n)
	{
		if(jingleCondTable[n].cond == cond)
			return QString::fromLatin1(jingleCondTable[n].str);
	}
	return QString();
}

static int elementNameToCondition(const QString &in)
{
	for(int n = 0; jingleCondTable[n].str; ++n)
	{
		if(QLatin1String(jingleCondTable[n].str) == in)
			return jingleCondTable[n].cond;
	}
	return -1;
}

static QDomElement reasonToElement(QDomDocument *doc, const JingleRtpReason &reason)
{
	QDomElement e = doc->createElement("reason");
	e.appendChild(doc->createElement(conditionToElementName(reason.condition)));
	if(!reason.text.isEmpty())
	{
		QDomElement text = doc->createElement("text");
		text.appendChild(doc->createTextNode(reason.text));
		e.appendChild(text);
	}
	return e;
}

static QDomElement firstChildElement(const QDomElement &in)
{
	for(QDomNode n = in.firstChild(); !n.isNull(); n = n.nextSibling())
	{
		if(!n.isElement())
			continue;

		return n.toElement();
	}
	return QDomElement();
}

static JingleRtpReason elementToReason(const QDomElement &e)
{
	if(e.tagName() != "reason")
		return JingleRtpReason();

	QDomElement condElement = firstChildElement(e);
	if(condElement.isNull())
		return JingleRtpReason();
	int x = elementNameToCondition(condElement.tagName());
	if(x == -1)
		return JingleRtpReason();

	JingleRtpReason out;
	out.condition = (JingleRtpReason::Condition)x;
	bool found;
	QDomElement text = findSubTag(e, "text", &found);
	if(found)
		out.text = tagContent(text);

	return out;
}

//----------------------------------------------------------------------------
// JT_JingleRtp
//----------------------------------------------------------------------------
JT_JingleRtp::JT_JingleRtp(XMPP::Task *parent) :
	XMPP::Task(parent)
{
}

JT_JingleRtp::~JT_JingleRtp()
{
}

void JT_JingleRtp::request(const XMPP::Jid &to, const JingleRtpEnvelope &envelope)
{
	to_ = to;
	iq_ = createIQ(doc(), "set", to.full(), id());
	QDomElement query = doc()->createElement("jingle");
	query.setAttribute("xmlns", "urn:xmpp:jingle:1");
	query.setAttribute("action", envelope.action);
	if(!envelope.initiator.isEmpty())
		query.setAttribute("initiator", envelope.initiator);
	if(!envelope.responder.isEmpty())
		query.setAttribute("responder", envelope.responder);
	query.setAttribute("sid", envelope.sid);

	if(envelope.action == "session-terminate")
	{
		// for session terminate, there is no content list, just
		//   a reason for termination
		query.appendChild(reasonToElement(doc(), envelope.reason));
	}
	else
	{
		foreach(const JingleRtpContent &c, envelope.contentList)
		{
			QDomElement content = doc()->createElement("content");
			content.setAttribute("creator", c.creator);
			if(!c.disposition.isEmpty())
				content.setAttribute("disposition", c.disposition);
			content.setAttribute("name", c.name);
			if(!c.senders.isEmpty())
				content.setAttribute("senders", c.senders);

			if(!c.desc.media.isEmpty())
			{
				// TODO: ssrc, bitrate, crypto
				QDomElement description = doc()->createElement("description");
				description.setAttribute("xmlns", "urn:xmpp:jingle:apps:rtp:1");
				description.setAttribute("media", c.desc.media);
				foreach(const JingleRtpPayloadType &pt, c.desc.payloadTypes)
				{
					QDomElement p = payloadTypeToElement(doc(), pt);
					if(!p.isNull())
						description.appendChild(p);
				}
				content.appendChild(description);
			}

			if(!c.trans.user.isEmpty())
			{
				QDomElement transport = doc()->createElement("transport");
				transport.setAttribute("xmlns", "urn:xmpp:jingle:transports:ice-udp:1");
				transport.setAttribute("ufrag", c.trans.user);
				transport.setAttribute("pwd", c.trans.pass);
				foreach(const XMPP::Ice176::Candidate &ic, c.trans.candidates)
				{
					QDomElement e = candidateToElement(doc(), ic);
					if(!e.isNull())
						transport.appendChild(e);
				}
				content.appendChild(transport);
			}

			query.appendChild(content);
		}
	}

	iq_.appendChild(query);
}

void JT_JingleRtp::onGo()
{
	send(iq_);
}

bool JT_JingleRtp::take(const QDomElement &x)
{
	if(!iqVerify(x, to_, id()))
		return false;

	if(x.attribute("type") == "result")
		setSuccess();
	else
		setError(x);

	return true;
}

//----------------------------------------------------------------------------
// JT_PushJingleRtp
//----------------------------------------------------------------------------
JT_PushJingleRtp::JT_PushJingleRtp(XMPP::Task *parent) :
	XMPP::Task(parent)
{
}

JT_PushJingleRtp::~JT_PushJingleRtp()
{
}

void JT_PushJingleRtp::respondSuccess(const XMPP::Jid &to, const QString &id)
{
	QDomElement iq = createIQ(doc(), "result", to.full(), id);
	send(iq);
}

void JT_PushJingleRtp::respondError(const XMPP::Jid &to, const QString &id, int code, const QString &str)
{
	QDomElement iq = createIQ(doc(), "error", to.full(), id);
	QDomElement err = textTag(doc(), "error", str);
	err.setAttribute("code", QString::number(code));
	iq.appendChild(err);
	send(iq);
}

bool JT_PushJingleRtp::take(const QDomElement &e)
{
	// must be an iq-set tag
	if(e.tagName() != "iq")
		return false;
	if(e.attribute("type") != "set")
		return false;

	QDomElement je;
	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling())
	{
		if(!n.isElement())
			continue;

		QDomElement e = n.toElement();
		if(e.tagName() == "jingle" && e.attribute("xmlns") == "urn:xmpp:jingle:1")
		{
			je = e;
			break;
		}
	}

	if(je.isNull())
		return false;

	XMPP::Jid from = e.attribute("from");
	QString iq_id = e.attribute("id");

	JingleRtpEnvelope envelope;
	envelope.action = je.attribute("action");
	envelope.initiator = je.attribute("initiator");
	envelope.responder = je.attribute("responder");
	envelope.sid = je.attribute("sid");

	if(envelope.action == "session-terminate")
	{
		bool found;
		QDomElement re = findSubTag(je, "reason", &found);
		if(!found)
		{
			respondError(from, iq_id, 400, QString());
			return true;
		}

		envelope.reason = elementToReason(re);
		if((int)envelope.reason.condition == -1)
		{
			respondError(from, iq_id, 400, QString());
			return true;
		}
	}
	else
	{
		for(QDomNode n = je.firstChild(); !n.isNull(); n = n.nextSibling())
		{
			if(!n.isElement())
				continue;

			QDomElement e = n.toElement();
			if(e.tagName() != "content")
				continue;

			JingleRtpContent c;
			c.creator = e.attribute("creator");
			c.disposition = e.attribute("disposition");
			c.name = e.attribute("name");
			c.senders = e.attribute("senders");

			for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling())
			{
				if(!n.isElement())
					continue;

				QDomElement e = n.toElement();
				if(e.tagName() == "description" && e.attribute("xmlns") == "urn:xmpp:jingle:apps:rtp:1")
				{
					c.desc.media = e.attribute("media");

					for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling())
					{
						if(!n.isElement())
							continue;

						QDomElement e = n.toElement();
						JingleRtpPayloadType pt = elementToPayloadType(e);
						if(pt.id == -1)
						{
							respondError(from, iq_id, 400, QString());
							return true;
						}

						c.desc.payloadTypes += pt;
					}
				}
				else if(e.tagName() == "transport" && e.attribute("xmlns") == "urn:xmpp:jingle:transports:ice-udp:1")
				{
					c.trans.user = e.attribute("ufrag");
					c.trans.pass = e.attribute("pwd");

					for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling())
					{
						if(!n.isElement())
							continue;

						QDomElement e = n.toElement();
						XMPP::Ice176::Candidate ic = elementToCandidate(e);
						if(ic.type.isEmpty())
						{
							respondError(from, iq_id, 400, QString());
							return true;
						}

						c.trans.candidates += ic;
					}
				}
			}

			envelope.contentList += c;
		}
	}

	emit incomingRequest(from, iq_id, envelope);
	return true;
}
