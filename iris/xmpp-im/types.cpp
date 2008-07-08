/*
 * types.cpp - IM data types
 * Copyright (C) 2003  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "im.h"
#include "xmpp_features.h"

#include <qmap.h>
#include <qapplication.h>
//Added by qt3to4:
#include <QList>

#include "xmpp_xmlcommon.h"
#define NS_XML     "http://www.w3.org/XML/1998/namespace"

namespace XMPP
{

//----------------------------------------------------------------------------
// Url
//----------------------------------------------------------------------------
class Url::Private
{
public:
	QString url;
	QString desc;
};

//! \brief Construct Url object with a given URL and Description.
//!
//! This function will construct a Url object.
//! \param QString - url (default: empty string)
//! \param QString - description of url (default: empty string)
//! \sa setUrl() setDesc()
Url::Url(const QString &url, const QString &desc)
{
	d = new Private;
	d->url = url;
	d->desc = desc;
}

//! \brief Construct Url object.
//!
//! Overloaded constructor which will constructs a exact copy of the Url object that was passed to the constructor.
//! \param Url - Url Object
Url::Url(const Url &from)
{
	d = new Private;
	*this = from;
}

//! \brief operator overloader needed for d pointer (Internel).
Url & Url::operator=(const Url &from)
{
	*d = *from.d;
	return *this;
}

//! \brief destroy Url object.
Url::~Url()
{
	delete d;
}

//! \brief Get url information.
//!
//! Returns url information.
QString Url::url() const
{
	return d->url;
}

//! \brief Get Description information.
//!
//! Returns desction of the URL.
QString Url::desc() const
{
	return d->desc;
}

//! \brief Set Url information.
//!
//! Set url information.
//! \param url - url string (eg: http://psi.affinix.com/)
void Url::setUrl(const QString &url)
{
	d->url = url;
}

//! \brief Set Description information.
//!
//! Set description of the url.
//! \param desc - description of url
void Url::setDesc(const QString &desc)
{
	d->desc = desc;
}

 //----------------------------------------------------------------------------
// Address
//----------------------------------------------------------------------------

//! \brief Construct Address object with a given Type and Jid.
//!
//! This function will construct a Address object.
//! \param Type - type (default: Unknown)
//! \param Jid - specify address (default: empty string)
//! \sa setType() setJid()
Address::Address(Type type, const Jid & jid)
: v_delivered(false)
{
	v_type = type;
	v_jid = jid;
	v_delivered = false;
}

Address::Address(const QDomElement& e)
: v_delivered(false)
{
	fromXml(e);
}

void Address::fromXml(const QDomElement& t)
{
	setJid(t.attribute("jid"));
	setUri(t.attribute("uri"));
	setNode(t.attribute("node"));
	setDesc(t.attribute("desc"));
	setDelivered(t.attribute("delivered") == "true");
	QString type = t.attribute("type");
	if (type == "to")
		setType(To);
	else if (type == "cc")
		setType(Cc);
	else if (type == "bcc")
		setType(Bcc);
	else if (type == "replyto")
		setType(ReplyTo);
	else if (type == "replyroom")
		setType(ReplyRoom);
	else if (type == "noreply")
		setType(NoReply);
	else if (type == "ofrom")
		setType(OriginalFrom);
	else if (type == "oto")
		setType(OriginalTo);
}

QDomElement Address::toXml(Stanza& s) const
{
	QDomElement e = s.createElement("http://jabber.org/protocol/address", "address");
	if(!jid().isEmpty())
		e.setAttribute("jid", jid().full());
	if(!uri().isEmpty())
		e.setAttribute("uri", uri());
	if(!node().isEmpty())
		e.setAttribute("node", node());
	if(!desc().isEmpty())
		e.setAttribute("desc", desc());
	if(delivered())
		e.setAttribute("delivered", "true");
	switch (type()) {
		case To:
			e.setAttribute("type", "to");
			break;
		case Cc:
			e.setAttribute("type", "cc");
			break;
		case Bcc:
			e.setAttribute("type", "bcc");
			break;
		case ReplyTo: 
			e.setAttribute("type", "replyto");
			break;
		case ReplyRoom: 
			e.setAttribute("type", "replyroom");
			break;
		case NoReply: 
			e.setAttribute("type", "noreply");
			break;
		case OriginalFrom:
			e.setAttribute("type", "ofrom");
			break;
		case OriginalTo: 
			e.setAttribute("type", "oto");
			break;
		case Unknown:
			// Add nothing
			break;
	}
	return e;
}

//! \brief Get Jid information.
//!
//! Returns jid information.
const Jid& Address::jid() const
{
	return v_jid;
}

//! \brief Get Uri information.
//!
//! Returns desction of the Address.
const QString& Address::uri() const
{
	return v_uri;
}

//! \brief Get Node information.
//!
//! Returns node of the Address.
const QString& Address::node() const
{
	return v_node;
}

//! \brief Get Description information.
//!
//! Returns desction of the Address.
const QString& Address::desc() const
{
	return v_desc;
}

//! \brief Get Delivered information.
//!
//! Returns delivered of the Address.
bool Address::delivered() const
{
	return v_delivered;
}

//! \brief Get Type information.
//!
//! Returns type of the Address.
Address::Type Address::type() const
{
	return v_type;
}

//! \brief Set Address information.
//!
//! Set jid information.
//! \param jid - jid
void Address::setJid(const Jid &jid)
{
	v_jid = jid;
}

//! \brief Set Address information.
//!
//! Set uri information.
//! \param uri - url string (eg: http://psi.affinix.com/)
void Address::setUri(const QString &uri)
{
	v_uri = uri;
}

//! \brief Set Node information.
//!
//! Set node information.
//! \param node - node string
void Address::setNode(const QString &node)
{
	v_node = node;
}

//! \brief Set Description information.
//!
//! Set description of the url.
//! \param desc - description of url
void Address::setDesc(const QString &desc)
{
	v_desc = desc;
}

//! \brief Set delivered information.
//!
//! Set delivered information.
//! \param delivered - delivered flag
void Address::setDelivered(bool delivered)
{
	v_delivered = delivered;
}

//! \brief Set Type information.
//!
//! Set type information.
//! \param type - type
void Address::setType(Type type)
{
	v_type = type;
}

//----------------------------------------------------------------------------
// RosterExchangeItem
//----------------------------------------------------------------------------

RosterExchangeItem::RosterExchangeItem(const Jid& jid, const QString& name, const QStringList& groups, Action action) : jid_(jid), name_(name), groups_(groups), action_(action)
{
}

RosterExchangeItem::RosterExchangeItem(const QDomElement& el) : action_(Add)
{
	fromXml(el);
}

const Jid& RosterExchangeItem::jid() const
{
	return jid_;
}

RosterExchangeItem::Action RosterExchangeItem::action() const
{
	return action_;
}

const QString& RosterExchangeItem::name() const
{
	return name_;
}

const QStringList& RosterExchangeItem::groups() const
{
	return groups_;
}

bool RosterExchangeItem::isNull() const
{
	return jid_.isEmpty();
}

void RosterExchangeItem::setJid(const Jid& jid)
{
	jid_ = jid;
}

void RosterExchangeItem::setAction(Action action)
{
	action_ = action;
}

void RosterExchangeItem::setName(const QString& name)
{
	name_ = name;
}

void RosterExchangeItem::setGroups(const QStringList& groups)
{
	groups_ = groups;
}

QDomElement RosterExchangeItem::toXml(Stanza& s) const
{
	QDomElement e = s.createElement("http://jabber.org/protocol/rosterx", "item");
	e.setAttribute("jid", jid().full());
	if (!name().isEmpty())
		e.setAttribute("name", name());
	switch(action()) {
		case Add:
			e.setAttribute("action","add");
			break;
		case Delete:
			e.setAttribute("action","delete");
			break;
		case Modify:
			e.setAttribute("action","modify");
			break;
	}
	foreach(QString group, groups_) {
		e.appendChild(s.createTextElement("http://jabber.org/protocol/rosterx", "group",group));
	}
	return e;
}

void RosterExchangeItem::fromXml(const QDomElement& e)
{
	setJid(e.attribute("jid"));
	setName(e.attribute("name"));
	if (e.attribute("action") == "delete") {
		setAction(Delete);
	}
	else if (e.attribute("action") == "modify") {
		setAction(Modify);
	}
	else {
		setAction(Add);
	}
	QDomNodeList nl = e.childNodes();
	for(int n = 0; n < nl.count(); ++n) {
		QDomElement g = nl.item(n).toElement();
		if (!g.isNull() && g.tagName() == "group") {
			groups_ += g.text();
		}
	}
}


//----------------------------------------------------------------------------
// MUCItem
//----------------------------------------------------------------------------
MUCItem::MUCItem(Role r, Affiliation a) : affiliation_(a), role_(r)
{
}

MUCItem::MUCItem(const QDomElement& el) : affiliation_(UnknownAffiliation), role_(UnknownRole)
{
	fromXml(el);
}

void MUCItem::setNick(const QString& n)
{
	nick_ = n;
}

void MUCItem::setJid(const Jid& j)
{
	jid_ = j;
}

void MUCItem::setAffiliation(Affiliation a)
{
	affiliation_ = a;
}

void MUCItem::setRole(Role r)
{
	role_ = r;
}

void MUCItem::setActor(const Jid& a)
{
	actor_ = a;
}

void MUCItem::setReason(const QString& r)
{
	reason_ = r;
}

const QString& MUCItem::nick() const
{
	return nick_;
}

const Jid& MUCItem::jid() const
{
	return jid_;
}

MUCItem::Affiliation MUCItem::affiliation() const
{
	return affiliation_;
}

MUCItem::Role MUCItem::role() const
{
	return role_;
}

const Jid& MUCItem::actor() const
{
	return actor_;
}

const QString& MUCItem::reason() const
{
	return reason_;
}

void MUCItem::fromXml(const QDomElement& e)
{
	if (e.tagName() != "item")
		return;

	jid_ = Jid(e.attribute("jid"));
	nick_ = e.attribute("nick");
	
	// Affiliation
	if (e.attribute("affiliation") == "owner") {
		affiliation_ = Owner;
	}
	else if (e.attribute("affiliation") == "admin") {
		affiliation_ = Admin;
	}
	else if (e.attribute("affiliation") == "member") {
		affiliation_ = Member;
	}
	else if (e.attribute("affiliation") == "outcast") {
		affiliation_ = Outcast;
	}
	else if (e.attribute("affiliation") == "none") {
		affiliation_ = NoAffiliation;
	}
	
	// Role
	if (e.attribute("role") == "moderator") {
		role_ = Moderator;
	}
	else if (e.attribute("role") == "participant") {
		role_ = Participant;
	}
	else if (e.attribute("role") == "visitor") {
		role_ = Visitor;
	}
	else if (e.attribute("role") == "none") {
		role_ = NoRole;
	}
	
	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull())
			continue;

		if (i.tagName() == "actor") 
			actor_ = Jid(i.attribute("jid"));
		else if (i.tagName() == "reason") 
			reason_ = i.text();
	}
}

QDomElement MUCItem::toXml(QDomDocument& d)
{
	QDomElement e = d.createElement("item");
	
	if (!nick_.isEmpty())
		e.setAttribute("nick",nick_);

	if (!jid_.isEmpty())
		e.setAttribute("jid",jid_.full());
	
	if (!reason_.isEmpty())
		e.appendChild(textTag(&d,"reason",reason_));
	
	switch (affiliation_) {
		case NoAffiliation:
			e.setAttribute("affiliation","none");
			break;
		case Owner:
			e.setAttribute("affiliation","owner");
			break;
		case Admin:
			e.setAttribute("affiliation","admin");
			break;
		case Member:
			e.setAttribute("affiliation","member");
			break;
		case Outcast:
			e.setAttribute("affiliation","outcast");
			break;
		default:
			break;
	}
	switch (role_) {
		case NoRole:
			e.setAttribute("role","none");
			break;
		case Moderator:
			e.setAttribute("role","moderator");
			break;
		case Participant:
			e.setAttribute("role","participant");
			break;
		case Visitor:
			e.setAttribute("role","visitor");
			break;
		default:
			break;
	}
	
	return e;
}

bool MUCItem::operator==(const MUCItem& o)
{
	return !nick_.compare(o.nick_) && ((!jid_.isValid() && !o.jid_.isValid()) || jid_.compare(o.jid_,true)) && ((!actor_.isValid() && !o.actor_.isValid()) || actor_.compare(o.actor_,true)) && affiliation_ == o.affiliation_ && role_ == o.role_ && !reason_.compare(o.reason_);
}

//----------------------------------------------------------------------------
// MUCInvite
//----------------------------------------------------------------------------

MUCInvite::MUCInvite() : cont_(false)
{
}

MUCInvite::MUCInvite(const Jid& to, const QString& reason) : to_(to), reason_(reason), cont_(false)
{
}

MUCInvite::MUCInvite(const QDomElement& e) : cont_(false)
{
	fromXml(e);
}

const Jid& MUCInvite::from() const
{
	return from_;
}

void MUCInvite::setFrom(const Jid& j)
{
	from_ = j;
}

const Jid& MUCInvite::to() const
{
	return to_;
}

void MUCInvite::setTo(const Jid& j)
{
	to_ = j;
}

const QString& MUCInvite::reason() const
{
	return reason_;
}

void MUCInvite::setReason(const QString& r)
{
	reason_ = r;
}

bool MUCInvite::cont() const
{
	return cont_;
}

void MUCInvite::setCont(bool b)
{
	cont_ = b;
}

void MUCInvite::fromXml(const QDomElement& e)
{
	if (e.tagName() != "invite")
		return;
	
	from_ = e.attribute("from");
	to_ = e.attribute("to");
	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull())
			continue;

		if (i.tagName() == "continue")
		   cont_ = true;	
		else if (i.tagName() == "reason") 
			reason_ = i.text();
	}
}

QDomElement MUCInvite::toXml(QDomDocument& d) const
{
	QDomElement invite = d.createElement("invite");
	if (!to_.isEmpty()) {
		invite.setAttribute("to",to_.full());
	}
	if (!from_.isEmpty()) {
		invite.setAttribute("from",from_.full());
	}
	if (!reason_.isEmpty()) {
		invite.appendChild(textTag(&d, "reason", reason_));
	}
	if (cont_) {
		invite.appendChild(d.createElement("continue"));
	}
	return invite;
}

bool MUCInvite::isNull() const
{
	return to_.isEmpty() && from_.isEmpty();
}

//----------------------------------------------------------------------------
// MUCDecline
//----------------------------------------------------------------------------

MUCDecline::MUCDecline()
{
}

MUCDecline::MUCDecline(const QDomElement& e) 
{
	fromXml(e);
}

const Jid& MUCDecline::from() const
{
	return from_;
}

void MUCDecline::setFrom(const Jid& j)
{
	from_ = j;
}

const Jid& MUCDecline::to() const
{
	return to_;
}

void MUCDecline::setTo(const Jid& j)
{
	to_ = j;
}

const QString& MUCDecline::reason() const
{
	return reason_;
}

void MUCDecline::setReason(const QString& r)
{
	reason_ = r;
}

void MUCDecline::fromXml(const QDomElement& e)
{
	if (e.tagName() != "decline")
		return;
	
	from_ = e.attribute("from");
	to_ = e.attribute("to");
	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull())
			continue;

		if (i.tagName() == "reason") 
			reason_ = i.text();
	}
}

QDomElement MUCDecline::toXml(QDomDocument& d) const
{
	QDomElement decline = d.createElement("decline");
	if (!to_.isEmpty()) {
		decline.setAttribute("to",to_.full());
	}
	if (!from_.isEmpty()) {
		decline.setAttribute("from",from_.full());
	}
	if (!reason_.isEmpty()) {
		decline.appendChild(textTag(&d, "reason", reason_));
	}
	return decline;
}

bool MUCDecline::isNull() const
{
	return to_.isEmpty() && from_.isEmpty();
}

//----------------------------------------------------------------------------
// MUCDestroy
//----------------------------------------------------------------------------

MUCDestroy::MUCDestroy()
{
}

MUCDestroy::MUCDestroy(const QDomElement& e) 
{
	fromXml(e);
}

const Jid& MUCDestroy::jid() const
{
	return jid_;
}

void MUCDestroy::setJid(const Jid& j)
{
	jid_ = j;
}

const QString& MUCDestroy::reason() const
{
	return reason_;
}

void MUCDestroy::setReason(const QString& r)
{
	reason_ = r;
}

void MUCDestroy::fromXml(const QDomElement& e)
{
	if (e.tagName() != "destroy")
		return;
	
	jid_ = e.attribute("jid");
	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull())
			continue;

		if (i.tagName() == "reason") 
			reason_ = i.text();
	}
}

QDomElement MUCDestroy::toXml(QDomDocument& d) const
{
	QDomElement destroy = d.createElement("destroy");
	if (!jid_.isEmpty()) {
		destroy.setAttribute("jid",jid_.full());
	}
	if (!reason_.isEmpty()) {
		destroy.appendChild(textTag(&d, "reason", reason_));
	}
	return destroy;
}

//----------------------------------------------------------------------------
// HTMLElement
//----------------------------------------------------------------------------
HTMLElement::HTMLElement() 
{
}

HTMLElement::HTMLElement(const QDomElement &body) { setBody(body); }

void HTMLElement::setBody(const QDomElement &body)
{
	body_ = doc_.importNode(body, true).toElement();
}

const QDomElement& HTMLElement::body() const
{
	return body_;
}

/**
 * Returns the string reperesentation of the HTML element.
 * By default, this is of the form <body ...>...</body>, but the
 * root tag can be modified using a parameter.
 *
 * \param rootTagName the tagname of the root element to use.
 */
QString HTMLElement::toString(const QString &rootTagName) const
{
	// create a copy of the body_ node,
	// get rid of the xmlns attribute and
	// change the root node name
	QDomElement e = body_.cloneNode().toElement();
	e.setTagName(rootTagName);

	// instead of using:
	//	QDomDocument msg;
	//	msg.appendChild(e);
	//	return msg.toString();
	// call Stream::xmlToString, to remove unwanted namespace attributes
	return(Stream::xmlToString(e));
}

QString HTMLElement::text() const
{
	return body_.text();
}


//----------------------------------------------------------------------------
// Message
//----------------------------------------------------------------------------
class Message::Private
{
public:
	Jid to, from;
	QString id, type, lang;

	StringMap subject, body;
	QString thread;
	bool threadSend;
	Stanza::Error error;

	// extensions
	QDateTime timeStamp;
	bool timeStampSend;
	UrlList urlList;
	AddressList addressList;
	RosterExchangeItems rosterExchangeItems;
	QList<MsgEvent> eventList;
	QString pubsubNode;
	QList<PubSubItem> pubsubItems;
	QList<PubSubRetraction> pubsubRetractions;
	QString eventId;
	QString xencrypted, invite;
	ChatState chatState;
	MessageReceipt messageReceipt;
	QString nick;
	HttpAuthRequest httpAuthRequest;
	XData xdata;
	QMap<QString,HTMLElement> htmlElements;
 	QDomElement sxe;
	
	int mucStatus;
	QList<MUCInvite> mucInvites;
	MUCDecline mucDecline;
	QString mucPassword;

	bool spooled, wasEncrypted;
};

//! \brief Constructs Message with given Jid information.
//!
//! This function will construct a Message container.
//! \param to - specify reciver (default: empty string)
Message::Message(const Jid &to)
{
	d = new Private;
	d->to = to;
	d->spooled = false;
	d->threadSend = false;
	d->timeStampSend = false;
	d->wasEncrypted = false;
	/*d->flag = false;
	d->spooled = false;
	d->wasEncrypted = false;
	d->errorCode = -1;*/
	d->chatState = StateNone;
	d->mucStatus = -1;
	d->messageReceipt = ReceiptNone;
}

//! \brief Constructs a copy of Message object
//!
//! Overloaded constructor which will constructs a exact copy of the Message
//! object that was passed to the constructor.
//! \param from - Message object you want to copy
Message::Message(const Message &from)
{
	d = new Private;
	*this = from;
}

//! \brief Required for internel use.
Message & Message::operator=(const Message &from)
{
	*d = *from.d;
	return *this;
}

//! \brief Destroy Message object.
Message::~Message()
{
	delete d;
}

//! \brief Return receiver's Jid information.
Jid Message::to() const
{
	return d->to;
}

//! \brief Return sender's Jid information.
Jid Message::from() const
{
	return d->from;
}

QString Message::id() const
{
	return d->id;
}

//! \brief Return type information
QString Message::type() const
{
	return d->type;
}

QString Message::lang() const
{
	return d->lang;
}

//! \brief Return subject information.
QString Message::subject(const QString &lang) const
{
	return d->subject[lang];
}

//! \brief Return body information.
//!
//! This function will return a plain text or the Richtext version if it
//! it exists.
//! \param rich - Returns richtext if true and plain text if false. (default: false)
//! \note Richtext is in Qt's richtext format and not in xhtml.
QString Message::body(const QString &lang) const
{
	if (d->body.empty())
		return "";
	else if (d->body.contains(lang))
		return d->body[lang];
	else
		return d->body.begin().value();
}

//! \brief Return xhtml body.
//!
//! This function will return the richtext version of the body, if
//! available.
//! \param lang - body language
//! \note The return string is in xhtml
HTMLElement Message::html(const QString &lang) const
{
	if(containsHTML()) {
		if (d->htmlElements.contains(lang))
			return d->htmlElements[lang];
		else
			return d->htmlElements.begin().value();
	}
	else
		return HTMLElement();
}

//! \brief Tells if message has xhtml-im items.
//!
//! Returns true if there is at least one xhtml-im body
//! in the message.
bool Message::containsHTML() const
{
	return !(d->htmlElements.isEmpty());
}

QString Message::thread() const
{
	return d->thread;
}

Stanza::Error Message::error() const
{
	return d->error;
}

//! \brief Set receivers information
//!
//! \param to - Receivers Jabber id
void Message::setTo(const Jid &j)
{
	d->to = j;
	//d->flag = false;
}

void Message::setFrom(const Jid &j)
{
	d->from = j;
	//d->flag = false;
}

void Message::setId(const QString &s)
{
	d->id = s;
}

//! \brief Set Type of message
//!
//! \param type - type of message your going to send
void Message::setType(const QString &s)
{
	d->type = s;
	//d->flag = false;
}

void Message::setLang(const QString &s)
{
	d->lang = s;
}

//! \brief Set subject
//!
//! \param subject - Subject information
void Message::setSubject(const QString &s, const QString &lang)
{
	d->subject[lang] = s;
	//d->flag = false;
}

//! \brief Set body
//!
//! \param body - body information
//! \param rich - set richtext if true and set plaintext if false.
//! \note Richtext support will be implemented in the future... Sorry.
void Message::setBody(const QString &s, const QString &lang)
{
	d->body[lang] = s;
	//d->flag = false;
}

//! \brief Set xhtml body
//!
//! \param s - body node
//! \param lang - body language
//! \note The body should be in xhtml.
void Message::setHTML(const HTMLElement &e, const QString &lang)
{
	d->htmlElements[lang] = e;
}

void Message::setThread(const QString &s, bool send)
{
	d->threadSend = send;
	d->thread = s;
}

void Message::setError(const Stanza::Error &err)
{
	d->error = err;
}

const QString& Message::pubsubNode() const
{
	return d->pubsubNode;
}

const QList<PubSubItem>& Message::pubsubItems() const
{
	return d->pubsubItems;
}

const QList<PubSubRetraction>& Message::pubsubRetractions() const
{
	return d->pubsubRetractions;
}

QDateTime Message::timeStamp() const
{
	return d->timeStamp;
}

void Message::setTimeStamp(const QDateTime &ts, bool send)
{
	d->timeStampSend = send;
	d->timeStamp = ts;
}

//! \brief Return list of urls attached to message.
UrlList Message::urlList() const
{
	return d->urlList;
}

//! \brief Add Url to the url list.
//!
//! \param url - url to append
void Message::urlAdd(const Url &u)
{
	d->urlList += u;
}

//! \brief clear out the url list.
void Message::urlsClear()
{
	d->urlList.clear();
}

//! \brief Set urls to send
//!
//! \param urlList - list of urls to send
void Message::setUrlList(const UrlList &list)
{
	d->urlList = list;
}

//! \brief Return list of addresses attached to message.
AddressList Message::addresses() const
{
	return d->addressList;
}

//! \brief Add Address to the address list.
//!
//! \param address - address to append
void Message::addAddress(const Address &a)
{
	d->addressList += a;
}

//! \brief clear out the address list.
void Message::clearAddresses()
{
	d->addressList.clear();
}

AddressList Message::findAddresses(Address::Type t) const
{
	AddressList matches;
	foreach(Address a, d->addressList) {
		if (a.type() == t)
			matches.append(a);
	}
	return matches;
}

//! \brief Set addresses to send
//!
//! \param list - list of addresses to send
void Message::setAddresses(const AddressList &list)
{
	d->addressList = list;
}

const RosterExchangeItems& Message::rosterExchangeItems() const
{
	return d->rosterExchangeItems;
}

void Message::setRosterExchangeItems(const RosterExchangeItems& items)
{
	d->rosterExchangeItems = items;
}

QString Message::eventId() const
{
	return d->eventId;
}

void Message::setEventId(const QString& id)
{
	d->eventId = id;
}

bool Message::containsEvents() const
{
	return !d->eventList.isEmpty();
}

bool Message::containsEvent(MsgEvent e) const
{
	return d->eventList.contains(e);
}

void Message::addEvent(MsgEvent e)
{
	if (!d->eventList.contains(e)) {
		if (e == CancelEvent || containsEvent(CancelEvent)) 
			d->eventList.clear(); // Reset list
		d->eventList += e;
	}
}

ChatState Message::chatState() const
{
	return d->chatState;
}

void Message::setChatState(ChatState state)
{
	d->chatState = state;
}

MessageReceipt Message::messageReceipt() const
{
	return d->messageReceipt;
}

void Message::setMessageReceipt(MessageReceipt messageReceipt)
{
	d->messageReceipt = messageReceipt;
}

QString Message::xencrypted() const
{
	return d->xencrypted;
}

void Message::setXEncrypted(const QString &s)
{
	d->xencrypted = s;
}

bool Message::hasMUCStatus() const
{
	return d->mucStatus != -1;
}

int Message::mucStatus() const
{
	return d->mucStatus;
}

void Message::setMUCStatus(int i)
{
	d->mucStatus = i;
}

void Message::addMUCInvite(const MUCInvite& i)
{
	d->mucInvites += i;
}

const QList<MUCInvite>& Message::mucInvites() const
{
	return d->mucInvites;
}

void Message::setMUCDecline(const MUCDecline& de)
{
	d->mucDecline = de;
}

const MUCDecline& Message::mucDecline() const
{
	return d->mucDecline;
}

const QString& Message::mucPassword() const
{
	return d->mucPassword;
}

void Message::setMUCPassword(const QString& p) 
{
	d->mucPassword = p;
}

QString Message::invite() const
{
	return d->invite;
}

void Message::setInvite(const QString &s)
{
	d->invite = s;
}

const QString& Message::nick() const
{
	return d->nick;
}

void Message::setNick(const QString& n)
{
	d->nick = n;
}

void Message::setHttpAuthRequest(const HttpAuthRequest &req)
{
	d->httpAuthRequest = req;
}

HttpAuthRequest Message::httpAuthRequest() const
{
	return d->httpAuthRequest;
}

void Message::setForm(const XData &form)
{
	d->xdata = form;
}

const XData& Message::getForm() const
{
	return d->xdata;
}

const QDomElement& Message::sxe() const
{
	return d->sxe;
}

void Message::setSxe(const QDomElement& e)
{
	d->sxe = e;
}

bool Message::spooled() const
{
	return d->spooled;
}

void Message::setSpooled(bool b)
{
	d->spooled = b;
}

bool Message::wasEncrypted() const
{
	return d->wasEncrypted;
}

void Message::setWasEncrypted(bool b)
{
	d->wasEncrypted = b;
}

Stanza Message::toStanza(Stream *stream) const
{
	Stanza s = stream->createStanza(Stanza::Message, d->to, d->type);
	if(!d->from.isEmpty())
		s.setFrom(d->from);
	if(!d->id.isEmpty())
		s.setId(d->id);
	if(!d->lang.isEmpty())
		s.setLang(d->lang);

	StringMap::ConstIterator it;
	for(it = d->subject.begin(); it != d->subject.end(); ++it) {
		const QString &str = it.data();
		if(!str.isEmpty()) {
			QDomElement e = s.createTextElement(s.baseNS(), "subject", str);
			if(!it.key().isEmpty())
				e.setAttributeNS(NS_XML, "xml:lang", it.key());
			s.appendChild(e);
		}
	}
	for(it = d->body.begin(); it != d->body.end(); ++it) {
		const QString &str = it.data();
		if(!str.isEmpty()) {
			QDomElement e = s.createTextElement(s.baseNS(), "body", str);
			if(!it.key().isEmpty())
				e.setAttributeNS(NS_XML, "xml:lang", it.key());
			s.appendChild(e);
		}
	}

	if(d->type == "error")
		s.setError(d->error);

	// thread
	if(d->threadSend && !d->thread.isEmpty()) {
		QDomElement e = s.createTextElement(s.baseNS(), "thread", d->thread);
		s.appendChild(e);
	}

	// timestamp
	if(d->timeStampSend && !d->timeStamp.isNull()) {
		QDomElement e = s.createElement("jabber:x:delay", "x");
		e.setAttribute("stamp", TS2stamp(d->timeStamp.toUTC()));
		s.appendChild(e);
	}

	// urls
	for(QList<Url>::ConstIterator uit = d->urlList.begin(); uit != d->urlList.end(); ++uit) {
		QDomElement x = s.createElement("jabber:x:oob", "x");
		x.appendChild(s.createTextElement("jabber:x:oob", "url", (*uit).url()));
		if(!(*uit).desc().isEmpty())
			x.appendChild(s.createTextElement("jabber:x:oob", "desc", (*uit).desc()));
		s.appendChild(x);
	}

	// events
	if (!d->eventList.isEmpty()) {
		QDomElement x = s.createElement("jabber:x:event", "x");

		if (d->body.isEmpty()) {
			if (d->eventId.isEmpty())
				x.appendChild(s.createElement("jabber:x:event","id"));
			else
				x.appendChild(s.createTextElement("jabber:x:event","id",d->eventId));
		}

		for(QList<MsgEvent>::ConstIterator ev = d->eventList.begin(); ev != d->eventList.end(); ++ev) {
			switch (*ev) {
				case OfflineEvent:
					x.appendChild(s.createElement("jabber:x:event", "offline"));
					break;
				case DeliveredEvent:
					x.appendChild(s.createElement("jabber:x:event", "delivered"));
					break;
				case DisplayedEvent:
					x.appendChild(s.createElement("jabber:x:event", "displayed"));
					break;
				case ComposingEvent: 
					x.appendChild(s.createElement("jabber:x:event", "composing"));
					break;
				case CancelEvent:
					// Add nothing
					break;
			}
		}
		s.appendChild(x);
	} 

	// chat state
	QString chatStateNS = "http://jabber.org/protocol/chatstates";
	if (d->chatState != StateNone) {
		switch(d->chatState) {
			case StateActive:
				s.appendChild(s.createElement(chatStateNS, "active"));
				break;
			case StateComposing:
				s.appendChild(s.createElement(chatStateNS, "composing"));
				break;
			case StatePaused:
				s.appendChild(s.createElement(chatStateNS, "paused"));
				break;
			case StateInactive:
				s.appendChild(s.createElement(chatStateNS, "inactive"));
				break;
			case StateGone:
				s.appendChild(s.createElement(chatStateNS, "gone"));
				break;
			default: 
				break;
		}
	}

	// message receipt
	QString messageReceiptNS = "urn:xmpp:receipts";
	if (d->messageReceipt != ReceiptNone) {
		switch(d->messageReceipt) {
			case ReceiptRequest:
				s.appendChild(s.createElement(messageReceiptNS, "request"));
				break;
			case ReceiptReceived:
				s.appendChild(s.createElement(messageReceiptNS, "received"));
				break;
			default: 
				break;
		}
	}

	// xencrypted
	if(!d->xencrypted.isEmpty())
		s.appendChild(s.createTextElement("jabber:x:encrypted", "x", d->xencrypted));

	// addresses
	if (!d->addressList.isEmpty()) {
		QDomElement as = s.createElement("http://jabber.org/protocol/address","addresses");
		foreach(Address a, d->addressList) {
			as.appendChild(a.toXml(s));
		}
		s.appendChild(as);
	}
	
	// roster item exchange
	if (!d->rosterExchangeItems.isEmpty()) {
		QDomElement rx = s.createElement("http://jabber.org/protocol/rosterx","x");
		foreach(RosterExchangeItem r, d->rosterExchangeItems) {
			rx.appendChild(r.toXml(s));
		}
		s.appendChild(rx);
	}

	// invite
	if(!d->invite.isEmpty()) {
		QDomElement e = s.createElement("jabber:x:conference", "x");
		e.setAttribute("jid", d->invite);
		s.appendChild(e);
	}

	// nick
	if(!d->nick.isEmpty()) {
		s.appendChild(s.createTextElement("http://jabber.org/protocol/nick", "nick", d->nick));
	}

	// sxe
	if(!d->sxe.isNull()) {
		s.appendChild(d->sxe);
	}

	// muc
	if(!d->mucInvites.isEmpty()) {
		QDomElement e = s.createElement("http://jabber.org/protocol/muc#user","x");
		foreach(MUCInvite i, d->mucInvites) {
			e.appendChild(i.toXml(s.doc()));
		}
		if (!d->mucPassword.isEmpty()) {
			e.appendChild(s.createTextElement("http://jabber.org/protocol/muc#user","password",d->mucPassword));
		}
		s.appendChild(e);
	}
	else if(!d->mucDecline.isNull()) {
		QDomElement e = s.createElement("http://jabber.org/protocol/muc#user","x");
		e.appendChild(d->mucDecline.toXml(s.doc()));
		s.appendChild(e);
	}

	// http auth
	if(!d->httpAuthRequest.isEmpty()) {
		s.appendChild(d->httpAuthRequest.toXml(s.doc()));
	}

	// data form
	if(!d->xdata.fields().empty() || (d->xdata.type() == XData::Data_Cancel)) {
		bool submit = (d->xdata.type() == XData::Data_Submit) || (d->xdata.type() == XData::Data_Cancel);
		s.appendChild(d->xdata.toXml(&s.doc(), submit));
	}

	return s;
}

bool Message::fromStanza(const Stanza &s, int timeZoneOffset)
{
	if(s.kind() != Stanza::Message)
		return false;

	setTo(s.to());
	setFrom(s.from());
	setId(s.id());
	setType(s.type());
	setLang(s.lang());

	d->subject.clear();
	d->body.clear();
	d->htmlElements.clear();
	d->thread = QString();

	QDomElement root = s.element();

	XDomNodeList nl = root.childNodes();
	int n;
	for(n = 0; n < nl.count(); ++n) {
		QDomNode i = nl.item(n);
		if(i.isElement()) {
			QDomElement e = i.toElement();
			if(e.namespaceURI() == s.baseNS()) {
				if(e.tagName() == "subject") {
					QString lang = e.attributeNS(NS_XML, "lang", "");
					d->subject[lang] = e.text();
				}
				else if(e.tagName() == "body") {
					QString lang = e.attributeNS(NS_XML, "lang", "");
					d->body[lang] = e.text();
				}
				else if(e.tagName() == "thread")
					d->thread = e.text();
			}
			else if(e.tagName() == "event" && e.namespaceURI() == "http://jabber.org/protocol/pubsub#event") {
				for(QDomNode enode = e.firstChild(); !enode.isNull(); enode = enode.nextSibling()) {
					QDomElement eel = enode.toElement();
					if (eel.tagName() == "items") {
						d->pubsubNode = eel.attribute("node");
						for(QDomNode inode = eel.firstChild(); !inode.isNull(); inode = inode.nextSibling()) {
							QDomElement o = inode.toElement();
							if (o.tagName() == "item") {
								for(QDomNode j = o.firstChild(); !j.isNull(); j = j.nextSibling()) {
									QDomElement item = j.toElement();
									if (!item.isNull()) {
										d->pubsubItems += PubSubItem(o.attribute("id"),item);
									}
								}
							}
							if (o.tagName() == "retract") {
								d->pubsubRetractions += PubSubRetraction(o.attribute("id"));
							}
						}
					}
				}
			}
			else {
				//printf("extension element: [%s]\n", e.tagName().latin1());
			}
		}
	}

	if(s.type() == "error")
		d->error = s.error();

	// xhtml-im
	nl = childElementsByTagNameNS(root, "http://jabber.org/protocol/xhtml-im", "html");
	if (nl.count()) {
		nl = nl.item(0).childNodes();
		for(n = 0; n < nl.count(); ++n) {
			QDomElement e = nl.item(n).toElement();
			if (e.tagName() == "body" && e.namespaceURI() == "http://www.w3.org/1999/xhtml") {
				QString lang = e.attributeNS(NS_XML, "lang", "");
				d->htmlElements[lang] = e;
			}
		}
	}

	// timestamp
	QDomElement t = childElementsByTagNameNS(root, "jabber:x:delay", "x").item(0).toElement();
	if(!t.isNull()) {
		d->timeStamp = stamp2TS(t.attribute("stamp"));
		d->timeStamp = d->timeStamp.addSecs(timeZoneOffset * 3600);
		d->spooled = true;
	}
	else {
		d->timeStamp = QDateTime::currentDateTime();
		d->spooled = false;
	}

	// urls
	d->urlList.clear();
	nl = childElementsByTagNameNS(root, "jabber:x:oob", "x");
	for(n = 0; n < nl.count(); ++n) {
		QDomElement t = nl.item(n).toElement();
		Url u;
		u.setUrl(t.elementsByTagName("url").item(0).toElement().text());
		u.setDesc(t.elementsByTagName("desc").item(0).toElement().text());
		d->urlList += u;
	}
	
    // events
	d->eventList.clear();
	nl = childElementsByTagNameNS(root, "jabber:x:event", "x");
	if (nl.count()) {
		nl = nl.item(0).childNodes();
		for(n = 0; n < nl.count(); ++n) {
			QString evtag = nl.item(n).toElement().tagName();
			if (evtag == "id") {
				d->eventId =  nl.item(n).toElement().text();
			}
			else if (evtag == "displayed")
				d->eventList += DisplayedEvent;
			else if (evtag == "composing")
				d->eventList += ComposingEvent;
			else if (evtag == "delivered")
				d->eventList += DeliveredEvent;
		}
		if (d->eventList.isEmpty())
			d->eventList += CancelEvent;
	}

	// Chat states
	QString chatStateNS = "http://jabber.org/protocol/chatstates";
	t = childElementsByTagNameNS(root, chatStateNS, "active").item(0).toElement();
	if(!t.isNull())
		d->chatState = StateActive;
	t = childElementsByTagNameNS(root, chatStateNS, "composing").item(0).toElement();
	if(!t.isNull())
		d->chatState = StateComposing;
	t = childElementsByTagNameNS(root, chatStateNS, "paused").item(0).toElement();
	if(!t.isNull())
		d->chatState = StatePaused;
	t = childElementsByTagNameNS(root, chatStateNS, "inactive").item(0).toElement();
	if(!t.isNull())
		d->chatState = StateInactive;
	t = childElementsByTagNameNS(root, chatStateNS, "gone").item(0).toElement();
	if(!t.isNull())
		d->chatState = StateGone;

	// message receipts
	QString messageReceiptNS = "urn:xmpp:receipts";
	t = childElementsByTagNameNS(root, messageReceiptNS, "request").item(0).toElement();
	if(!t.isNull())
		d->messageReceipt = ReceiptRequest;
	t = childElementsByTagNameNS(root, messageReceiptNS, "received").item(0).toElement();
	if(!t.isNull())
		d->messageReceipt = ReceiptReceived;

	// xencrypted
	t = childElementsByTagNameNS(root, "jabber:x:encrypted", "x").item(0).toElement();
	if(!t.isNull())
		d->xencrypted = t.text();
	else
		d->xencrypted = QString();
		
	// addresses
	d->addressList.clear();
	nl = childElementsByTagNameNS(root, "http://jabber.org/protocol/address", "addresses");
	if (nl.count()) {
		QDomElement t = nl.item(0).toElement();
		nl = t.elementsByTagName("address");
		for(n = 0; n < nl.count(); ++n) {
			d->addressList += Address(nl.item(n).toElement());
		}
	}
	
	// roster item exchange
	d->rosterExchangeItems.clear();
	nl = childElementsByTagNameNS(root, "http://jabber.org/protocol/rosterx", "x");
	if (nl.count()) {
		QDomElement t = nl.item(0).toElement();
		nl = t.elementsByTagName("item");
		for(n = 0; n < nl.count(); ++n) {
			RosterExchangeItem it = RosterExchangeItem(nl.item(n).toElement());
			if (!it.isNull())
				d->rosterExchangeItems += it;
		}
	}

	// invite
	t = childElementsByTagNameNS(root, "jabber:x:conference", "x").item(0).toElement();
	if(!t.isNull())
		d->invite = t.attribute("jid");
	else
		d->invite = QString();
	
	// nick
	t = childElementsByTagNameNS(root, "http://jabber.org/protocol/nick", "nick").item(0).toElement();
	if(!t.isNull())
		d->nick = t.text();
	else
		d->nick = QString();

	// sxe
	t = childElementsByTagNameNS(root, "http://jabber.org/protocol/sxe", "sxe").item(0).toElement();
	if(!t.isNull())
		d->sxe = t;
	else
		d->sxe = QDomElement();

	t = childElementsByTagNameNS(root, "http://jabber.org/protocol/muc#user", "x").item(0).toElement();
	if(!t.isNull()) {
		for(QDomNode muc_n = t.firstChild(); !muc_n.isNull(); muc_n = muc_n.nextSibling()) {
			QDomElement muc_e = muc_n.toElement();
			if(muc_e.isNull())
				continue;
			if (muc_e.tagName() == "status") {
				setMUCStatus(muc_e.attribute("code").toInt());
			}
			else if (muc_e.tagName() == "invite") {
				MUCInvite inv(muc_e);
				if (!inv.isNull())
					addMUCInvite(inv);
			}
			else if (muc_e.tagName() == "decline") {
				setMUCDecline(MUCDecline(muc_e));
			}
			else if (muc_e.tagName() == "password") {
				setMUCPassword(muc_e.text());
			}
		}
	}

	// http auth
	t = childElementsByTagNameNS(root, "http://jabber.org/protocol/http-auth", "confirm").item(0).toElement();
	if(!t.isNull()){
		d->httpAuthRequest = HttpAuthRequest(t);
	}
	else {
		d->httpAuthRequest = HttpAuthRequest();
	}

	// data form
	t = childElementsByTagNameNS(root, "jabber:x:data", "x").item(0).toElement();
	if(!t.isNull()){
		d->xdata.fromXml(t);
	}

	return true;
}

/*!
	Error object used to deny a request.
*/
Stanza::Error HttpAuthRequest::denyError(Stanza::Error::Auth, Stanza::Error::NotAuthorized);

/*!
	Constructs request of resource URL \a u, made by method \a m, with transaction id \a i.
*/
HttpAuthRequest::HttpAuthRequest(const QString &m, const QString &u, const QString &i) : method_(m), url_(u), id_(i), hasId_(true)
{
}

/*!
        Constructs request of resource URL \a u, made by method \a m, without transaction id.
*/
HttpAuthRequest::HttpAuthRequest(const QString &m, const QString &u) : method_(m), url_(u), hasId_(false)
{
}

/*!
	Constructs request object by reading XML <confirm/> element \a e.
*/
HttpAuthRequest::HttpAuthRequest(const QDomElement &e)
{
	fromXml(e);
}

/*!
	Returns true is object is empty (not valid).
*/
bool HttpAuthRequest::isEmpty() const
{
	return method_.isEmpty() && url_.isEmpty();
}

/*!
	Sets request method.
*/
void HttpAuthRequest::setMethod(const QString& m)
{
	method_ = m;
}

/*!
	Sets requested URL.
*/
void HttpAuthRequest::setUrl(const QString& u)
{
	url_ = u;
}

/*!
	Sets transaction identifier.
*/
void HttpAuthRequest::setId(const QString& i)
{
	id_ = i;
	hasId_ = true;
}

/*!
	Returns request method.
*/
QString HttpAuthRequest::method() const
{
	return method_;
}

/*!
	Returns requested URL.
*/
QString HttpAuthRequest::url() const
{
	return url_;
}

/*!
	Returns transaction identifier.
	Empty QString may mean both empty id or no id. Use hasId() to tell the difference.
*/
QString HttpAuthRequest::id() const
{
	return id_;
}

/*!
	Returns true if the request contains transaction id.
*/
bool HttpAuthRequest::hasId() const
{
	return hasId_;
}

/*!
	Returns XML element representing the request.
	If object is empty, this function returns empty element.
*/
QDomElement HttpAuthRequest::toXml(QDomDocument &doc) const
{
	QDomElement e;
	if(isEmpty())
		return e;

	e = doc.createElementNS("http://jabber.org/protocol/http-auth", "confirm");
	e.setAttribute("xmlns", "http://jabber.org/protocol/http-auth");

	if(hasId_) 
		e.setAttribute("id", id_);
	e.setAttribute("method", method_);
	e.setAttribute("url", url_);

	return e;
}

/*!
	Reads request data from XML element \a e.
*/
bool HttpAuthRequest::fromXml(const QDomElement &e)
{
	if(e.tagName() != "confirm")
		return false;

	hasId_ = e.hasAttribute("id");
	if(hasId_)
		id_ = e.attribute("id");

	method_ = e.attribute("method");
	url_ = e.attribute("url");

	return true;
}

//---------------------------------------------------------------------------
// Subscription
//---------------------------------------------------------------------------
Subscription::Subscription(SubType type)
{
	value = type;
}

int Subscription::type() const
{
	return value;
}

QString Subscription::toString() const
{
	switch(value) {
		case Remove:
			return "remove";
		case Both:
			return "both";
		case From:
			return "from";
		case To:
			return "to";
		case None:
		default:
			return "none";
	}
}

bool Subscription::fromString(const QString &s)
{
	if(s == "remove")
		value = Remove;
	else if(s == "both")
		value = Both;
	else if(s == "from")
		value = From;
	else if(s == "to")
		value = To;
	else if(s == "none")
		value = None;
	else
		return false;

	return true;
}


//---------------------------------------------------------------------------
// Status
//---------------------------------------------------------------------------

Status::Status(const QString &show, const QString &status, int priority, bool available)
{
	v_isAvailable = available;
	v_show = show;
	v_status = status;
	v_priority = priority;
	v_timeStamp = QDateTime::currentDateTime();
	v_isInvisible = false;
	v_hasPhotoHash = false;
	v_isMUC = false;
	v_hasMUCItem = false;
	v_hasMUCDestroy = false;
	v_mucStatus = -1;
	v_mucHistoryMaxChars = -1;
	v_mucHistoryMaxStanzas = -1;
	v_mucHistorySeconds = -1;
	ecode = -1;
}

Status::Status(Type type, const QString& status, int priority)
{
	v_status = status;
	v_priority = priority;
	v_timeStamp = QDateTime::currentDateTime();
	v_hasPhotoHash = false;
	v_isMUC = false;
	v_hasMUCItem = false;
	v_hasMUCDestroy = false;
	v_mucStatus = -1;
	v_mucHistoryMaxChars = -1;
	v_mucHistoryMaxStanzas = -1;
	v_mucHistorySeconds = -1;
	ecode = -1;
	setType(type);
}

Status::~Status()
{
}

bool Status::hasError() const
{
	return (ecode != -1);
}

void Status::setError(int code, const QString &str)
{
	ecode = code;
	estr = str;
}

void Status::setIsAvailable(bool available)
{
	v_isAvailable = available;
}

void Status::setIsInvisible(bool invisible)
{
	v_isInvisible = invisible;
}

void Status::setPriority(int x)
{
	v_priority = x;
}

void Status::setType(Status::Type _type)
{
	bool available = true;
	bool invisible = false;
	QString show;
	switch(_type) {
		case Away:    show = "away"; break;
		case FFC:     show = "chat"; break;
		case XA:      show = "xa"; break;
		case DND:     show = "dnd"; break;
		case Offline: available = false; break;
		case Invisible: invisible = true; break;
		default: break;
	}
	setShow(show);
	setIsAvailable(available);
	setIsInvisible(invisible);
}

void Status::setType(QString stat)
{
	if (stat == "offline")
		setType(XMPP::Status::Offline);
	else if (stat == "online")
		setType(XMPP::Status::Online);
	else if (stat == "away")
		setType(XMPP::Status::Away);
	else if (stat == "xa")
		setType(XMPP::Status::XA);
	else if (stat == "dnd")
		setType(XMPP::Status::DND);
	else if (stat == "invisible")
		setType(XMPP::Status::Invisible);
	else if (stat == "chat")
		setType(XMPP::Status::FFC);
	else
		setType(XMPP::Status::Away);
}

void Status::setShow(const QString & _show)
{
	v_show = _show;
}

void Status::setStatus(const QString & _status)
{
	v_status = _status;
}

void Status::setTimeStamp(const QDateTime & _timestamp)
{
	v_timeStamp = _timestamp;
}

void Status::setKeyID(const QString &key)
{
	v_key = key;
}

void Status::setXSigned(const QString &s)
{
	v_xsigned = s;
}

void Status::setSongTitle(const QString & _songtitle)
{
	v_songTitle = _songtitle;
}

void Status::setCapsNode(const QString & _capsNode)
{
	v_capsNode = _capsNode;
}

void Status::setCapsVersion(const QString & _capsVersion)
{
	v_capsVersion = _capsVersion;
}

void Status::setCapsExt(const QString & _capsExt)
{
	v_capsExt = _capsExt;
}

void Status::setMUC() 
{
	v_isMUC = true;
}

void Status::setMUCItem(const MUCItem& i)
{
	v_hasMUCItem = true;
	v_mucItem = i;
}

void Status::setMUCDestroy(const MUCDestroy& i)
{
	v_hasMUCDestroy = true;
	v_mucDestroy = i;
}

void Status::setMUCHistory(int maxchars, int maxstanzas, int seconds)
{
	v_mucHistoryMaxChars = maxchars;
	v_mucHistoryMaxStanzas = maxstanzas;
	v_mucHistorySeconds = seconds;
}


const QString& Status::photoHash() const
{
	return v_photoHash;
}

void Status::setPhotoHash(const QString& h)
{
	v_photoHash = h;
	v_hasPhotoHash = true;
}

bool Status::hasPhotoHash() const
{
	return v_hasPhotoHash;
}

bool Status::isAvailable() const
{
	return v_isAvailable;
}

bool Status::isAway() const
{
	return (v_show == "away" || v_show == "xa" || v_show == "dnd");
}

bool Status::isInvisible() const
{
	return v_isInvisible;
}

int Status::priority() const
{
	return v_priority;
}

Status::Type Status::type() const
{
	Status::Type type = Status::Online;
	if (!isAvailable()) {
		type = Status::Offline;
	}
	else if (isInvisible()) {
		type = Status::Invisible;
	}
	else {
		QString s = show();
		if (s == "away")
			type = Status::Away;
		else if (s == "xa")
		  	type = Status::XA;
		else if (s == "dnd")
		 	type = Status::DND;
		else if (s == "chat")
			type = Status::FFC;
	}
	return type;
}

QString Status::typeString() const
{
	QString stat;
	switch(type()) {
		case XMPP::Status::Offline: stat = "offline"; break;
		case XMPP::Status::Online: stat = "online"; break;
		case XMPP::Status::Away: stat = "away"; break;
		case XMPP::Status::XA: stat = "xa"; break;
		case XMPP::Status::DND: stat = "dnd"; break;
		case XMPP::Status::Invisible: stat = "invisible"; break;
		case XMPP::Status::FFC: stat = "chat"; break;
		default: stat = "away";
	}
	return stat;
}

const QString & Status::show() const
{
	return v_show;
}
const QString & Status::status() const
{
	return v_status;
}

QDateTime Status::timeStamp() const
{
	return v_timeStamp;
}

const QString & Status::keyID() const
{
	return v_key;
}

const QString & Status::xsigned() const
{
	return v_xsigned;
}

const QString & Status::songTitle() const
{
	return v_songTitle;
}

const QString & Status::capsNode() const
{
	return v_capsNode;
}

const QString & Status::capsVersion() const
{
	return v_capsVersion;
}

const QString & Status::capsExt() const
{
	return v_capsExt;
}

bool Status::isMUC() const
{
	return v_isMUC || !v_mucPassword.isEmpty() || hasMUCHistory();
}

bool Status::hasMUCItem() const
{
	return v_hasMUCItem;
}

const MUCItem& Status::mucItem() const
{
	return v_mucItem;
}

bool Status::hasMUCDestroy() const
{
	return v_hasMUCDestroy;
}

const MUCDestroy& Status::mucDestroy() const
{
	return v_mucDestroy;
}

bool Status::hasMUCStatus() const
{
	return v_mucStatus != -1;
}

int Status::mucStatus() const
{
	return v_mucStatus;
}

void Status::setMUCStatus(int i)
{
	v_mucStatus = i;
}

const QString& Status::mucPassword() const
{
	return v_mucPassword;
}

bool Status::hasMUCHistory() const
{
	return v_mucHistoryMaxChars >= 0 || v_mucHistoryMaxStanzas >= 0 || v_mucHistorySeconds >= 0;
}

int Status::mucHistoryMaxChars() const
{
	return v_mucHistoryMaxChars;
}

int Status::mucHistoryMaxStanzas() const
{
	return v_mucHistoryMaxStanzas;
}

int Status::mucHistorySeconds() const
{
	return v_mucHistorySeconds;
}

void Status::setMUCPassword(const QString& i)
{
	v_mucPassword = i;
}

int Status::errorCode() const
{
	return ecode;
}

const QString & Status::errorString() const
{
	return estr;
}


//---------------------------------------------------------------------------
// Resource
//---------------------------------------------------------------------------
Resource::Resource(const QString &name, const Status &stat)
{
	v_name = name;
	v_status = stat;
}

Resource::~Resource()
{
}

const QString & Resource::name() const
{
	return v_name;
}

int Resource::priority() const
{
	return v_status.priority();
}

const Status & Resource::status() const
{
	return v_status;
}

void Resource::setName(const QString & _name)
{
	v_name = _name;
}

void Resource::setStatus(const Status & _status)
{
	v_status = _status;
}


//---------------------------------------------------------------------------
// ResourceList
//---------------------------------------------------------------------------
ResourceList::ResourceList()
:QList<Resource>()
{
}

ResourceList::~ResourceList()
{
}

ResourceList::Iterator ResourceList::find(const QString & _find)
{
	for(ResourceList::Iterator it = begin(); it != end(); ++it) {
		if((*it).name() == _find)
			return it;
	}

	return end();
}

ResourceList::Iterator ResourceList::priority()
{
	ResourceList::Iterator highest = end();

	for(ResourceList::Iterator it = begin(); it != end(); ++it) {
		if(highest == end() || (*it).priority() > (*highest).priority())
			highest = it;
	}

	return highest;
}

ResourceList::ConstIterator ResourceList::find(const QString & _find) const
{
	for(ResourceList::ConstIterator it = begin(); it != end(); ++it) {
		if((*it).name() == _find)
			return it;
	}

	return end();
}

ResourceList::ConstIterator ResourceList::priority() const
{
	ResourceList::ConstIterator highest = end();

	for(ResourceList::ConstIterator it = begin(); it != end(); ++it) {
		if(highest == end() || (*it).priority() > (*highest).priority())
			highest = it;
	}

	return highest;
}


//---------------------------------------------------------------------------
// RosterItem
//---------------------------------------------------------------------------
RosterItem::RosterItem(const Jid &_jid)
{
	v_jid = _jid;
	v_push = false;
}

RosterItem::~RosterItem()
{
}

const Jid & RosterItem::jid() const
{
	return v_jid;
}

const QString & RosterItem::name() const
{
	return v_name;
}

const QStringList & RosterItem::groups() const
{
	return v_groups;
}

const Subscription & RosterItem::subscription() const
{
	return v_subscription;
}

const QString & RosterItem::ask() const
{
	return v_ask;
}

bool RosterItem::isPush() const
{
	return v_push;
}

bool RosterItem::inGroup(const QString &g) const
{
	for(QStringList::ConstIterator it = v_groups.begin(); it != v_groups.end(); ++it) {
		if(*it == g)
			return true;
	}
	return false;
}

void RosterItem::setJid(const Jid &_jid)
{
	v_jid = _jid;
}

void RosterItem::setName(const QString &_name)
{
	v_name = _name;
}

void RosterItem::setGroups(const QStringList &_groups)
{
	v_groups = _groups;
}

void RosterItem::setSubscription(const Subscription &type)
{
	v_subscription = type;
}

void RosterItem::setAsk(const QString &_ask)
{
	v_ask = _ask;
}

void RosterItem::setIsPush(bool b)
{
	v_push = b;
}

bool RosterItem::addGroup(const QString &g)
{
	if(inGroup(g))
		return false;

	v_groups += g;
	return true;
}

bool RosterItem::removeGroup(const QString &g)
{
	for(QStringList::Iterator it = v_groups.begin(); it != v_groups.end(); ++it) {
		if(*it == g) {
			v_groups.remove(it);
			return true;
		}
	}

	return false;
}

QDomElement RosterItem::toXml(QDomDocument *doc) const
{
	QDomElement item = doc->createElement("item");
	item.setAttribute("jid", v_jid.full());
	item.setAttribute("name", v_name);
	item.setAttribute("subscription", v_subscription.toString());
	if(!v_ask.isEmpty())
		item.setAttribute("ask", v_ask);
	for(QStringList::ConstIterator it = v_groups.begin(); it != v_groups.end(); ++it)
		item.appendChild(textTag(doc, "group", *it));

	return item;
}

bool RosterItem::fromXml(const QDomElement &item)
{
	if(item.tagName() != "item")
		return false;
	Jid j(item.attribute("jid"));
	if(!j.isValid())
		return false;
	QString na = item.attribute("name");
	Subscription s;
	if(!s.fromString(item.attribute("subscription")) )
		return false;
	QStringList g;
	for(QDomNode n = item.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull())
			continue;
		if(i.tagName() == "group")
			g += tagContent(i);
	}
	QString a = item.attribute("ask");

	v_jid = j;
	v_name = na;
	v_subscription = s;
	v_groups = g;
	v_ask = a;

	return true;
}


//---------------------------------------------------------------------------
// Roster
//---------------------------------------------------------------------------
Roster::Roster()
:QList<RosterItem>()
{
}

Roster::~Roster()
{
}

Roster::Iterator Roster::find(const Jid &j)
{
	for(Roster::Iterator it = begin(); it != end(); ++it) {
		if((*it).jid().compare(j))
			return it;
	}

	return end();
}

Roster::ConstIterator Roster::find(const Jid &j) const
{
	for(Roster::ConstIterator it = begin(); it != end(); ++it) {
		if((*it).jid().compare(j))
			return it;
	}

	return end();
}


//---------------------------------------------------------------------------
// FormField
//---------------------------------------------------------------------------
FormField::FormField(const QString &type, const QString &value)
{
	v_type = misc;
	if(!type.isEmpty()) {
		int x = tagNameToType(type);
		if(x != -1)
			v_type = x;
	}
	v_value = value;
}

FormField::~FormField()
{
}

int FormField::type() const
{
	return v_type;
}

QString FormField::realName() const
{
	return typeToTagName(v_type);
}

QString FormField::fieldName() const
{
	switch(v_type) {
		case username:  return QObject::tr("Username");
		case nick:      return QObject::tr("Nickname");
		case password:  return QObject::tr("Password");
		case name:      return QObject::tr("Name");
		case first:     return QObject::tr("First Name");
		case last:      return QObject::tr("Last Name");
		case email:     return QObject::tr("E-mail");
		case address:   return QObject::tr("Address");
		case city:      return QObject::tr("City");
		case state:     return QObject::tr("State");
		case zip:       return QObject::tr("Zipcode");
		case phone:     return QObject::tr("Phone");
		case url:       return QObject::tr("URL");
		case date:      return QObject::tr("Date");
		case misc:      return QObject::tr("Misc");
		default:        return "";
	};
}

bool FormField::isSecret() const
{
	return (type() == password);
}

const QString & FormField::value() const
{
	return v_value;
}

void FormField::setType(int x)
{
	v_type = x;
}

bool FormField::setType(const QString &in)
{
	int x = tagNameToType(in);
	if(x == -1)
		return false;

	v_type = x;
	return true;
}

void FormField::setValue(const QString &in)
{
	v_value = in;
}

int FormField::tagNameToType(const QString &in) const
{
	if(!in.compare("username")) return username;
	if(!in.compare("nick"))     return nick;
	if(!in.compare("password")) return password;
	if(!in.compare("name"))     return name;
	if(!in.compare("first"))    return first;
	if(!in.compare("last"))     return last;
	if(!in.compare("email"))    return email;
	if(!in.compare("address"))  return address;
	if(!in.compare("city"))     return city;
	if(!in.compare("state"))    return state;
	if(!in.compare("zip"))      return zip;
	if(!in.compare("phone"))    return phone;
	if(!in.compare("url"))      return url;
	if(!in.compare("date"))     return date;
	if(!in.compare("misc"))     return misc;

	return -1;
}

QString FormField::typeToTagName(int type) const
{
	switch(type) {
		case username:  return "username";
		case nick:      return "nick";
		case password:  return "password";
		case name:      return "name";
		case first:     return "first";
		case last:      return "last";
		case email:     return "email";
		case address:   return "address";
		case city:      return "city";
		case state:     return "state";
		case zip:       return "zipcode";
		case phone:     return "phone";
		case url:       return "url";
		case date:      return "date";
		case misc:      return "misc";
		default:        return "";
	};
}


//---------------------------------------------------------------------------
// Form
//---------------------------------------------------------------------------
Form::Form(const Jid &j)
:QList<FormField>()
{
	setJid(j);
}

Form::~Form()
{
}

Jid Form::jid() const
{
	return v_jid;
}

QString Form::instructions() const
{
	return v_instructions;
}

QString Form::key() const
{
	return v_key;
}

void Form::setJid(const Jid &j)
{
	v_jid = j;
}

void Form::setInstructions(const QString &s)
{
	v_instructions = s;
}

void Form::setKey(const QString &s)
{
	v_key = s;
}


//---------------------------------------------------------------------------
// SearchResult
//---------------------------------------------------------------------------
SearchResult::SearchResult(const Jid &jid)
{
	setJid(jid);
}

SearchResult::~SearchResult()
{
}

const Jid & SearchResult::jid() const
{
	return v_jid;
}

const QString & SearchResult::nick() const
{
	return v_nick;
}

const QString & SearchResult::first() const
{
	return v_first;
}

const QString & SearchResult::last() const
{
	return v_last;
}

const QString & SearchResult::email() const
{
	return v_email;
}

void SearchResult::setJid(const Jid &jid)
{
	v_jid = jid;
}

void SearchResult::setNick(const QString &nick)
{
	v_nick = nick;
}

void SearchResult::setFirst(const QString &first)
{
	v_first = first;
}

void SearchResult::setLast(const QString &last)
{
	v_last = last;
}

void SearchResult::setEmail(const QString &email)
{
	v_email = email;
}

PubSubItem::PubSubItem() 
{
}

PubSubItem::PubSubItem(const QString& id, const QDomElement& payload) : id_(id), payload_(payload) 
{ 
}

const QString& PubSubItem::id() const 
{ 
	return id_; 
}

const QDomElement& PubSubItem::payload() const 
{ 
	return payload_; 
}


PubSubRetraction::PubSubRetraction() 
{
}

PubSubRetraction::PubSubRetraction(const QString& id) : id_(id) 
{ 
}

const QString& PubSubRetraction::id() const 
{ 
	return id_; 
}


}
