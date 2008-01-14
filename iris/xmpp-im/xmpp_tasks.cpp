/*
 * tasks.cpp - basic tasks
 * Copyright (C) 2001, 2002  Justin Karneges
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

#include "xmpp_tasks.h"

//#include <QtCrypto>
//#include "sha1.h"
#include "xmpp_xmlcommon.h"
//#include "xmpp_stream.h"
//#include "xmpp_types.h"
#include "xmpp_vcard.h"

#include <qregexp.h>
#include <QList>

using namespace XMPP;


static QString lineEncode(QString str)
{
	str.replace(QRegExp("\\\\"), "\\\\");   // backslash to double-backslash
	str.replace(QRegExp("\\|"), "\\p");     // pipe to \p
	str.replace(QRegExp("\n"), "\\n");      // newline to \n
	return str;
}

static QString lineDecode(const QString &str)
{
	QString ret;

	for(int n = 0; n < str.length(); ++n) {
		if(str.at(n) == '\\') {
			++n;
			if(n >= str.length())
				break;

			if(str.at(n) == 'n')
				ret.append('\n');
			if(str.at(n) == 'p')
				ret.append('|');
			if(str.at(n) == '\\')
				ret.append('\\');
		}
		else {
			ret.append(str.at(n));
		}
	}

	return ret;
}

static Roster xmlReadRoster(const QDomElement &q, bool push)
{
	Roster r;

	for(QDomNode n = q.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull())
			continue;

		if(i.tagName() == "item") {
			RosterItem item;
			item.fromXml(i);

			if(push)
				item.setIsPush(true);

			r += item;
		}
	}

	return r;
}

//----------------------------------------------------------------------------
// JT_Session
//----------------------------------------------------------------------------
//
#include "protocol.h"

JT_Session::JT_Session(Task *parent) : Task(parent)
{
}

void JT_Session::onGo() 
{
	QDomElement iq = createIQ(doc(), "set", "", id());
	QDomElement session = doc()->createElement("session");
	session.setAttribute("xmlns",NS_SESSION);
	iq.appendChild(session);
	send(iq);
}

bool JT_Session::take(const QDomElement& x) 
{
	if(!iqVerify(x, "", id()))
		return false;

	if(x.attribute("type") == "result") {
		setSuccess();
	}
	else {
		setError(x);
	}
	return true;
}

//----------------------------------------------------------------------------
// JT_Register
//----------------------------------------------------------------------------
class JT_Register::Private
{
public:
	Private() {}

	Form form;
	XData xdata;
	bool hasXData;
	Jid jid;
	int type;
};

JT_Register::JT_Register(Task *parent)
:Task(parent)
{
	d = new Private;
	d->type = -1;
	d->hasXData = false;
}

JT_Register::~JT_Register()
{
	delete d;
}

void JT_Register::reg(const QString &user, const QString &pass)
{
	d->type = 0;
	to = client()->host();
	iq = createIQ(doc(), "set", to.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "jabber:iq:register");
	iq.appendChild(query);
	query.appendChild(textTag(doc(), "username", user));
	query.appendChild(textTag(doc(), "password", pass));
}

void JT_Register::changepw(const QString &pass)
{
	d->type = 1;
	to = client()->host();
	iq = createIQ(doc(), "set", to.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "jabber:iq:register");
	iq.appendChild(query);
	query.appendChild(textTag(doc(), "username", client()->user()));
	query.appendChild(textTag(doc(), "password", pass));
}

void JT_Register::unreg(const Jid &j)
{
	d->type = 2;
	to = j.isEmpty() ? client()->host() : j.full();
	iq = createIQ(doc(), "set", to.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "jabber:iq:register");
	iq.appendChild(query);

	// this may be useful
	if(!d->form.key().isEmpty())
		query.appendChild(textTag(doc(), "key", d->form.key()));

	query.appendChild(doc()->createElement("remove"));
}

void JT_Register::getForm(const Jid &j)
{
	d->type = 3;
	to = j;
	iq = createIQ(doc(), "get", to.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "jabber:iq:register");
	iq.appendChild(query);
}

void JT_Register::setForm(const Form &form)
{
	d->type = 4;
	to = form.jid();
	iq = createIQ(doc(), "set", to.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "jabber:iq:register");
	iq.appendChild(query);

	// key?
	if(!form.key().isEmpty())
		query.appendChild(textTag(doc(), "key", form.key()));

	// fields
	for(Form::ConstIterator it = form.begin(); it != form.end(); ++it) {
		const FormField &f = *it;
		query.appendChild(textTag(doc(), f.realName(), f.value()));
	}
}

void JT_Register::setForm(const Jid& to, const XData& xdata)
{
	d->type = 4;
	iq = createIQ(doc(), "set", to.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "jabber:iq:register");
	iq.appendChild(query);
	query.appendChild(xdata.toXml(doc(), true));
}

const Form & JT_Register::form() const
{
	return d->form;
}

bool JT_Register::hasXData() const
{
	return d->hasXData;
}

const XData& JT_Register::xdata() const
{
	return d->xdata;
}

void JT_Register::onGo()
{
	send(iq);
}

bool JT_Register::take(const QDomElement &x)
{
	if(!iqVerify(x, to, id()))
		return false;

	Jid from(x.attribute("from"));
	if(x.attribute("type") == "result") {
		if(d->type == 3) {
			d->form.clear();
			d->form.setJid(from);

			QDomElement q = queryTag(x);
			for(QDomNode n = q.firstChild(); !n.isNull(); n = n.nextSibling()) {
				QDomElement i = n.toElement();
				if(i.isNull())
					continue;

				if(i.tagName() == "instructions")
					d->form.setInstructions(tagContent(i));
				else if(i.tagName() == "key")
					d->form.setKey(tagContent(i));
				else if(i.tagName() == "x" && i.attribute("xmlns") == "jabber:x:data") {
					d->xdata.fromXml(i);
					d->hasXData = true;
				}
				else {
					FormField f;
					if(f.setType(i.tagName())) {
						f.setValue(tagContent(i));
						d->form += f;
					}
				}
			}
		}

		setSuccess();
	}
	else
		setError(x);

	return true;
}

//----------------------------------------------------------------------------
// JT_UnRegister
//----------------------------------------------------------------------------
class JT_UnRegister::Private
{
public:
	Private() { }

	Jid j;
	JT_Register *jt_reg;
};

JT_UnRegister::JT_UnRegister(Task *parent)
: Task(parent)
{
	d = new Private;
	d->jt_reg = 0;
}

JT_UnRegister::~JT_UnRegister()
{
	delete d->jt_reg;
	delete d;
}

void JT_UnRegister::unreg(const Jid &j)
{
	d->j = j;
}

void JT_UnRegister::onGo()
{
	delete d->jt_reg;

	d->jt_reg = new JT_Register(this);
	d->jt_reg->getForm(d->j);
	connect(d->jt_reg, SIGNAL(finished()), SLOT(getFormFinished()));
	d->jt_reg->go(false);
}

void JT_UnRegister::getFormFinished()
{
	disconnect(d->jt_reg, 0, this, 0);

	d->jt_reg->unreg(d->j);
	connect(d->jt_reg, SIGNAL(finished()), SLOT(unregFinished()));
	d->jt_reg->go(false);
}

void JT_UnRegister::unregFinished()
{
	if ( d->jt_reg->success() )
		setSuccess();
	else
		setError(d->jt_reg->statusCode(), d->jt_reg->statusString());

	delete d->jt_reg;
	d->jt_reg = 0;
}

//----------------------------------------------------------------------------
// JT_Roster
//----------------------------------------------------------------------------
class JT_Roster::Private
{
public:
	Private() {}

	Roster roster;
	QList<QDomElement> itemList;
};

JT_Roster::JT_Roster(Task *parent)
:Task(parent)
{
	type = -1;
	d = new Private;
}

JT_Roster::~JT_Roster()
{
	delete d;
}

void JT_Roster::get()
{
	type = 0;
	//to = client()->host();
	iq = createIQ(doc(), "get", to.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "jabber:iq:roster");
	iq.appendChild(query);
}

void JT_Roster::set(const Jid &jid, const QString &name, const QStringList &groups)
{
	type = 1;
	//to = client()->host();
	QDomElement item = doc()->createElement("item");
	item.setAttribute("jid", jid.full());
	if(!name.isEmpty())
		item.setAttribute("name", name);
	for(QStringList::ConstIterator it = groups.begin(); it != groups.end(); ++it)
		item.appendChild(textTag(doc(), "group", *it));
	d->itemList += item;
}

void JT_Roster::remove(const Jid &jid)
{
	type = 1;
	//to = client()->host();
	QDomElement item = doc()->createElement("item");
	item.setAttribute("jid", jid.full());
	item.setAttribute("subscription", "remove");
	d->itemList += item;
}

void JT_Roster::onGo()
{
	if(type == 0)
		send(iq);
	else if(type == 1) {
		//to = client()->host();
		iq = createIQ(doc(), "set", to.full(), id());
		QDomElement query = doc()->createElement("query");
		query.setAttribute("xmlns", "jabber:iq:roster");
		iq.appendChild(query);
		for(QList<QDomElement>::ConstIterator it = d->itemList.begin(); it != d->itemList.end(); ++it)
			query.appendChild(*it);
		send(iq);
	}
}

const Roster & JT_Roster::roster() const
{
	return d->roster;
}

QString JT_Roster::toString() const
{
	if(type != 1)
		return "";

	QDomElement i = doc()->createElement("request");
	i.setAttribute("type", "JT_Roster");
	for(QList<QDomElement>::ConstIterator it = d->itemList.begin(); it != d->itemList.end(); ++it)
		i.appendChild(*it);
	return lineEncode(Stream::xmlToString(i));
	return "";
}

bool JT_Roster::fromString(const QString &str)
{
	QDomDocument *dd = new QDomDocument;
	if(!dd->setContent(lineDecode(str).utf8()))
		return false;
	QDomElement e = doc()->importNode(dd->documentElement(), true).toElement();
	delete dd;

	if(e.tagName() != "request" || e.attribute("type") != "JT_Roster")
		return false;

	type = 1;
	d->itemList.clear();
	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull())
			continue;
		d->itemList += i;
	}

	return true;
}

bool JT_Roster::take(const QDomElement &x)
{
	if(!iqVerify(x, client()->host(), id()))
		return false;

	// get
	if(type == 0) {
		if(x.attribute("type") == "result") {
			QDomElement q = queryTag(x);
			d->roster = xmlReadRoster(q, false);
			setSuccess();
		}
		else {
			setError(x);
		}

		return true;
	}
	// set
	else if(type == 1) {
		if(x.attribute("type") == "result")
			setSuccess();
		else
			setError(x);

		return true;
	}
	// remove
	else if(type == 2) {
		setSuccess();
		return true;
	}

	return false;
}


//----------------------------------------------------------------------------
// JT_PushRoster
//----------------------------------------------------------------------------
JT_PushRoster::JT_PushRoster(Task *parent)
:Task(parent)
{
}

JT_PushRoster::~JT_PushRoster()
{
}

bool JT_PushRoster::take(const QDomElement &e)
{
	// must be an iq-set tag
	if(e.tagName() != "iq" || e.attribute("type") != "set")
		return false;

	if(!iqVerify(e, client()->host(), "", "jabber:iq:roster"))
		return false;

	roster(xmlReadRoster(queryTag(e), true));
	send(createIQ(doc(), "result", e.attribute("from"), e.attribute("id")));

	return true;
}


//----------------------------------------------------------------------------
// JT_Presence
//----------------------------------------------------------------------------
JT_Presence::JT_Presence(Task *parent)
:Task(parent)
{
	type = -1;
}

JT_Presence::~JT_Presence()
{
}

void JT_Presence::pres(const Status &s)
{
	type = 0;

	tag = doc()->createElement("presence");
	if(!s.isAvailable()) {
		tag.setAttribute("type", "unavailable");
		if(!s.status().isEmpty())
			tag.appendChild(textTag(doc(), "status", s.status()));
	}
	else {
		if(s.isInvisible())
			tag.setAttribute("type", "invisible");

		if(!s.show().isEmpty())
			tag.appendChild(textTag(doc(), "show", s.show()));
		if(!s.status().isEmpty())
			tag.appendChild(textTag(doc(), "status", s.status()));

		tag.appendChild( textTag(doc(), "priority", QString("%1").arg(s.priority()) ) );

		if(!s.keyID().isEmpty()) {
			QDomElement x = textTag(doc(), "x", s.keyID());
			x.setAttribute("xmlns", "http://jabber.org/protocol/e2e");
			tag.appendChild(x);
		}
		if(!s.xsigned().isEmpty()) {
			QDomElement x = textTag(doc(), "x", s.xsigned());
			x.setAttribute("xmlns", "jabber:x:signed");
			tag.appendChild(x);
		}

		if(!s.capsNode().isEmpty() && !s.capsVersion().isEmpty()) {
			QDomElement c = doc()->createElement("c");
			c.setAttribute("xmlns","http://jabber.org/protocol/caps");
			c.setAttribute("node",s.capsNode());
			c.setAttribute("ver",s.capsVersion());
			if (!s.capsExt().isEmpty()) 
				c.setAttribute("ext",s.capsExt());
			tag.appendChild(c);
		}

		if(s.isMUC()) {
			QDomElement m = doc()->createElement("x");
			m.setAttribute("xmlns","http://jabber.org/protocol/muc");
			if (!s.mucPassword().isEmpty()) {
				m.appendChild(textTag(doc(),"password",s.mucPassword()));
			}
			if (s.hasMUCHistory()) {
				QDomElement h = doc()->createElement("history");
				if (s.mucHistoryMaxChars() >= 0)
					h.setAttribute("maxchars",s.mucHistoryMaxChars());
				if (s.mucHistoryMaxStanzas() >= 0)
					h.setAttribute("maxstanzas",s.mucHistoryMaxStanzas());
				if (s.mucHistorySeconds() >= 0)
					h.setAttribute("seconds",s.mucHistorySeconds());
				m.appendChild(h);
			}
			tag.appendChild(m);
		}

		if(s.hasPhotoHash()) {
			QDomElement m = doc()->createElement("x");
			m.setAttribute("xmlns", "vcard-temp:x:update");
			m.appendChild(textTag(doc(), "photo", s.photoHash()));
			tag.appendChild(m);
		}
	}
}

void JT_Presence::pres(const Jid &to, const Status &s)
{
	pres(s);
	tag.setAttribute("to", to.full());
}

void JT_Presence::sub(const Jid &to, const QString &subType, const QString& nick)
{
	type = 1;

	tag = doc()->createElement("presence");
	tag.setAttribute("to", to.full());
	tag.setAttribute("type", subType);
	if (!nick.isEmpty()) {
		QDomElement nick_tag = textTag(doc(),"nick",nick);
		nick_tag.setAttribute("xmlns","http://jabber.org/protocol/nick");
		tag.appendChild(nick_tag);
	}
}

void JT_Presence::onGo()
{
	send(tag);
	setSuccess();
}


//----------------------------------------------------------------------------
// JT_PushPresence
//----------------------------------------------------------------------------
JT_PushPresence::JT_PushPresence(Task *parent)
:Task(parent)
{
}

JT_PushPresence::~JT_PushPresence()
{
}

bool JT_PushPresence::take(const QDomElement &e)
{
	if(e.tagName() != "presence")
		return false;

	Jid j(e.attribute("from"));
	Status p;

	if(e.hasAttribute("type")) {
		QString type = e.attribute("type");
		if(type == "unavailable") {
			p.setIsAvailable(false);
		}
		else if(type == "error") {
			QString str = "";
			int code = 0;
			getErrorFromElement(e, client()->stream().baseNS(), &code, &str);
			p.setError(code, str);
		}
		else if(type == "subscribe" || type == "subscribed" || type == "unsubscribe" || type == "unsubscribed") {
			QString nick;
			bool found;
			QDomElement tag = findSubTag(e, "nick", &found);
			if (found && tag.attribute("xmlns") == "http://jabber.org/protocol/nick") {
				nick = tagContent(tag);
			}
			subscription(j, type, nick);
			return true;
		}
	}

	QDomElement tag;
	bool found;

	tag = findSubTag(e, "status", &found);
	if(found)
		p.setStatus(tagContent(tag));
	tag = findSubTag(e, "show", &found);
	if(found)
		p.setShow(tagContent(tag));
	tag = findSubTag(e, "priority", &found);
	if(found)
		p.setPriority(tagContent(tag).toInt());

	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull())
			continue;

		if(i.tagName() == "x" && i.attribute("xmlns") == "jabber:x:delay") {
			if(i.hasAttribute("stamp")) {
				QDateTime dt;
				if(stamp2TS(i.attribute("stamp"), &dt))
					dt = dt.addSecs(client()->timeZoneOffset() * 3600);
				p.setTimeStamp(dt);
			}
		}
		else if(i.tagName() == "x" && i.attribute("xmlns") == "gabber:x:music:info") {
			QDomElement t;
			bool found;
			QString title, state;

			t = findSubTag(i, "title", &found);
			if(found)
				title = tagContent(t);
			t = findSubTag(i, "state", &found);
			if(found)
				state = tagContent(t);

			if(!title.isEmpty() && state == "playing")
				p.setSongTitle(title);
		}
		else if(i.tagName() == "x" && i.attribute("xmlns") == "jabber:x:signed") {
			p.setXSigned(tagContent(i));
		}
		else if(i.tagName() == "x" && i.attribute("xmlns") == "http://jabber.org/protocol/e2e") {
			p.setKeyID(tagContent(i));
		}
 		else if(i.tagName() == "c" && i.attribute("xmlns") == "http://jabber.org/protocol/caps") {
 			p.setCapsNode(i.attribute("node"));
 			p.setCapsVersion(i.attribute("ver"));
 			p.setCapsExt(i.attribute("ext"));
  		}
		else if(i.tagName() == "x" && i.attribute("xmlns") == "vcard-temp:x:update") {
			QDomElement t;
			bool found;
			t = findSubTag(i, "photo", &found);
			if (found)
				p.setPhotoHash(tagContent(t));
			else
				p.setPhotoHash("");
		}
		else if(i.tagName() == "x" && i.attribute("xmlns") == "http://jabber.org/protocol/muc#user") {
			for(QDomNode muc_n = i.firstChild(); !muc_n.isNull(); muc_n = muc_n.nextSibling()) {
				QDomElement muc_e = muc_n.toElement();
				if(muc_e.isNull())
					continue;

				if (muc_e.tagName() == "item") 
					p.setMUCItem(MUCItem(muc_e));
				else if (muc_e.tagName() == "status") 
					p.setMUCStatus(muc_e.attribute("code").toInt());
				else if (muc_e.tagName() == "destroy")
					p.setMUCDestroy(MUCDestroy(muc_e));
			}
		}
	}

	presence(j, p);

	return true;
}


//----------------------------------------------------------------------------
// JT_Message
//----------------------------------------------------------------------------
static QDomElement oldStyleNS(const QDomElement &e)
{
	// find closest parent with a namespace
	QDomNode par = e.parentNode();
	while(!par.isNull() && par.namespaceURI().isNull())
		par = par.parentNode();
	bool noShowNS = false;
	if(!par.isNull() && par.namespaceURI() == e.namespaceURI())
		noShowNS = true;

	QDomElement i;
	int x;
	//if(noShowNS)
		i = e.ownerDocument().createElement(e.tagName());
	//else
	//	i = e.ownerDocument().createElementNS(e.namespaceURI(), e.tagName());

	// copy attributes
	QDomNamedNodeMap al = e.attributes();
	for(x = 0; x < al.count(); ++x)
		i.setAttributeNode(al.item(x).cloneNode().toAttr());

	if(!noShowNS)
		i.setAttribute("xmlns", e.namespaceURI());

	// copy children
	QDomNodeList nl = e.childNodes();
	for(x = 0; x < nl.count(); ++x) {
		QDomNode n = nl.item(x);
		if(n.isElement())
			i.appendChild(oldStyleNS(n.toElement()));
		else
			i.appendChild(n.cloneNode());
	}
	return i;
}

JT_Message::JT_Message(Task *parent, const Message &msg)
:Task(parent)
{
	m = msg;
	if (m.id().isEmpty())
		m.setId(id());
}

JT_Message::~JT_Message()
{
}

void JT_Message::onGo()
{
	Stanza s = m.toStanza(&(client()->stream()));
	QDomElement e = oldStyleNS(s.element());
	send(e);
	setSuccess();
}


//----------------------------------------------------------------------------
// JT_PushMessage
//----------------------------------------------------------------------------
JT_PushMessage::JT_PushMessage(Task *parent)
:Task(parent)
{
}

JT_PushMessage::~JT_PushMessage()
{
}

bool JT_PushMessage::take(const QDomElement &e)
{
	if(e.tagName() != "message")
		return false;

	Stanza s = client()->stream().createStanza(addCorrectNS(e));
	if(s.isNull()) {
		//printf("take: bad stanza??\n");
		return false;
	}

	Message m;
	if(!m.fromStanza(s, client()->timeZoneOffset())) {
		//printf("bad message\n");
		return false;
	}

	message(m);
	return true;
}


//----------------------------------------------------------------------------
// JT_GetServices
//----------------------------------------------------------------------------
JT_GetServices::JT_GetServices(Task *parent)
:Task(parent)
{
}

void JT_GetServices::get(const Jid &j)
{
	agentList.clear();

	jid = j;
	iq = createIQ(doc(), "get", jid.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "jabber:iq:agents");
	iq.appendChild(query);
}

const AgentList & JT_GetServices::agents() const
{
	return agentList;
}

void JT_GetServices::onGo()
{
	send(iq);
}

bool JT_GetServices::take(const QDomElement &x)
{
	if(!iqVerify(x, jid, id()))
		return false;

	if(x.attribute("type") == "result") {
		QDomElement q = queryTag(x);

		// agents
		for(QDomNode n = q.firstChild(); !n.isNull(); n = n.nextSibling()) {
			QDomElement i = n.toElement();
			if(i.isNull())
				continue;

			if(i.tagName() == "agent") {
				AgentItem a;

				a.setJid(Jid(i.attribute("jid")));

				QDomElement tag;
				bool found;

				tag = findSubTag(i, "name", &found);
				if(found)
					a.setName(tagContent(tag));

				// determine which namespaces does item support
				QStringList ns;

				tag = findSubTag(i, "register", &found);
				if(found)
					ns << "jabber:iq:register";
				tag = findSubTag(i, "search", &found);
				if(found)
					ns << "jabber:iq:search";
				tag = findSubTag(i, "groupchat", &found);
				if(found)
					ns << "jabber:iq:conference";
				tag = findSubTag(i, "transport", &found);
				if(found)
					ns << "jabber:iq:gateway";

				a.setFeatures(ns);

				agentList += a;
			}
		}

		setSuccess(true);
	}
	else {
		setError(x);
	}

	return true;
}


//----------------------------------------------------------------------------
// JT_VCard
//----------------------------------------------------------------------------
class JT_VCard::Private
{
public:
	Private() {}

	QDomElement iq;
	Jid jid;
	VCard vcard;
};

JT_VCard::JT_VCard(Task *parent)
:Task(parent)
{
	type = -1;
	d = new Private;
}

JT_VCard::~JT_VCard()
{
	delete d;
}

void JT_VCard::get(const Jid &_jid)
{
	type = 0;
	d->jid = _jid;
	d->iq = createIQ(doc(), "get", type == 1 ? Jid().full() : d->jid.full(), id());
	QDomElement v = doc()->createElement("vCard");
	v.setAttribute("xmlns", "vcard-temp");
	v.setAttribute("version", "2.0");
	v.setAttribute("prodid", "-//HandGen//NONSGML vGen v1.0//EN");
	d->iq.appendChild(v);
}

const Jid & JT_VCard::jid() const
{
	return d->jid;
}

const VCard & JT_VCard::vcard() const
{
	return d->vcard;
}

void JT_VCard::set(const Jid &j, const VCard &card)
{
	type = 1;
	d->vcard = card;
	d->jid = j;
	d->iq = createIQ(doc(), "set", "", id());
	d->iq.appendChild(card.toXml(doc()) );
}

void JT_VCard::onGo()
{
	send(d->iq);
}

bool JT_VCard::take(const QDomElement &x)
{
	Jid to = d->jid;
	if (to.userHost() == client()->jid().userHost())
		to = client()->host();
	if(!iqVerify(x, to, id()))
		return false;

	if(x.attribute("type") == "result") {
		if(type == 0) {
			for(QDomNode n = x.firstChild(); !n.isNull(); n = n.nextSibling()) {
				QDomElement q = n.toElement();
				if(q.isNull())
					continue;

				if(q.tagName().upper() == "VCARD") {
					if(d->vcard.fromXml(q)) {
						setSuccess();
						return true;
					}
				}
			}

			setError(ErrDisc + 1, tr("No VCard available"));
			return true;
		}
		else {
			setSuccess();
			return true;
		}
	}
	else {
		setError(x);
	}

	return true;
}


//----------------------------------------------------------------------------
// JT_Search
//----------------------------------------------------------------------------
class JT_Search::Private
{
public:
	Private() {}

	Jid jid;
	Form form;
	QList<SearchResult> resultList;
};

JT_Search::JT_Search(Task *parent)
:Task(parent)
{
	d = new Private;
	type = -1;
}

JT_Search::~JT_Search()
{
	delete d;
}

void JT_Search::get(const Jid &jid)
{
	type = 0;
	d->jid = jid;
	iq = createIQ(doc(), "get", d->jid.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "jabber:iq:search");
	iq.appendChild(query);
}

void JT_Search::set(const Form &form)
{
	type = 1;
	d->jid = form.jid();
	iq = createIQ(doc(), "set", d->jid.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "jabber:iq:search");
	iq.appendChild(query);

	// key?
	if(!form.key().isEmpty())
		query.appendChild(textTag(doc(), "key", form.key()));

	// fields
	for(Form::ConstIterator it = form.begin(); it != form.end(); ++it) {
		const FormField &f = *it;
		query.appendChild(textTag(doc(), f.realName(), f.value()));
	}
}

const Form & JT_Search::form() const
{
	return d->form;
}

const QList<SearchResult> & JT_Search::results() const
{
	return d->resultList;
}

void JT_Search::onGo()
{
	send(iq);
}

bool JT_Search::take(const QDomElement &x)
{
	if(!iqVerify(x, d->jid, id()))
		return false;

	Jid from(x.attribute("from"));
	if(x.attribute("type") == "result") {
		if(type == 0) {
			d->form.clear();
			d->form.setJid(from);

			QDomElement q = queryTag(x);
			for(QDomNode n = q.firstChild(); !n.isNull(); n = n.nextSibling()) {
				QDomElement i = n.toElement();
				if(i.isNull())
					continue;

				if(i.tagName() == "instructions")
					d->form.setInstructions(tagContent(i));
				else if(i.tagName() == "key")
					d->form.setKey(tagContent(i));
				else {
					FormField f;
					if(f.setType(i.tagName())) {
						f.setValue(tagContent(i));
						d->form += f;
					}
				}
			}
		}
		else {
			d->resultList.clear();

			QDomElement q = queryTag(x);
			for(QDomNode n = q.firstChild(); !n.isNull(); n = n.nextSibling()) {
				QDomElement i = n.toElement();
				if(i.isNull())
					continue;

				if(i.tagName() == "item") {
					SearchResult r(Jid(i.attribute("jid")));

					QDomElement tag;
					bool found;

					tag = findSubTag(i, "nick", &found);
					if(found)
						r.setNick(tagContent(tag));
					tag = findSubTag(i, "first", &found);
					if(found)
						r.setFirst(tagContent(tag));
					tag = findSubTag(i, "last", &found);
					if(found)
						r.setLast(tagContent(tag));
					tag = findSubTag(i, "email", &found);
					if(found)
						r.setEmail(tagContent(tag));

					d->resultList += r;
				}
			}
		}
		setSuccess();
	}
	else {
		setError(x);
	}

	return true;
}


//----------------------------------------------------------------------------
// JT_ClientVersion
//----------------------------------------------------------------------------
JT_ClientVersion::JT_ClientVersion(Task *parent)
:Task(parent)
{
}

void JT_ClientVersion::get(const Jid &jid)
{
	j = jid;
	iq = createIQ(doc(), "get", j.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "jabber:iq:version");
	iq.appendChild(query);
}

void JT_ClientVersion::onGo()
{
	send(iq);
}

bool JT_ClientVersion::take(const QDomElement &x)
{
	if(!iqVerify(x, j, id()))
		return false;

	if(x.attribute("type") == "result") {
		bool found;
		QDomElement q = queryTag(x);
		QDomElement tag;
		tag = findSubTag(q, "name", &found);
		if(found)
			v_name = tagContent(tag);
		tag = findSubTag(q, "version", &found);
		if(found)
			v_ver = tagContent(tag);
		tag = findSubTag(q, "os", &found);
		if(found)
			v_os = tagContent(tag);

		setSuccess();
	}
	else {
		setError(x);
	}

	return true;
}

const Jid & JT_ClientVersion::jid() const
{
	return j;
}

const QString & JT_ClientVersion::name() const
{
	return v_name;
}

const QString & JT_ClientVersion::version() const
{
	return v_ver;
}

const QString & JT_ClientVersion::os() const
{
	return v_os;
}


//----------------------------------------------------------------------------
// JT_ClientTime
//----------------------------------------------------------------------------
/*JT_ClientTime::JT_ClientTime(Task *parent, const Jid &_j)
:Task(parent)
{
	j = _j;
	iq = createIQ("get", j.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "jabber:iq:time");
	iq.appendChild(query);
}

void JT_ClientTime::go()
{
	send(iq);
}

bool JT_ClientTime::take(const QDomElement &x)
{
	if(x.attribute("id") != id())
		return FALSE;

	if(x.attribute("type") == "result") {
		bool found;
		QDomElement q = queryTag(x);
		QDomElement tag;
		tag = findSubTag(q, "utc", &found);
		if(found)
			stamp2TS(tagContent(tag), &utc);
		tag = findSubTag(q, "tz", &found);
		if(found)
			timezone = tagContent(tag);
		tag = findSubTag(q, "display", &found);
		if(found)
			display = tagContent(tag);

		setSuccess(TRUE);
	}
	else {
		setError(getErrorString(x));
		setSuccess(FALSE);
	}

	return TRUE;
}
*/


//----------------------------------------------------------------------------
// JT_ServInfo
//----------------------------------------------------------------------------
JT_ServInfo::JT_ServInfo(Task *parent)
:Task(parent)
{
}

JT_ServInfo::~JT_ServInfo()
{
}

bool JT_ServInfo::take(const QDomElement &e)
{
	if(e.tagName() != "iq" || e.attribute("type") != "get")
		return false;

	QString ns = queryNS(e);
	if(ns == "jabber:iq:version") {
		QDomElement iq = createIQ(doc(), "result", e.attribute("from"), e.attribute("id"));
		QDomElement query = doc()->createElement("query");
		query.setAttribute("xmlns", "jabber:iq:version");
		iq.appendChild(query);
		query.appendChild(textTag(doc(), "name", client()->clientName()));
		query.appendChild(textTag(doc(), "version", client()->clientVersion()));
		query.appendChild(textTag(doc(), "os", client()->OSName()));
		send(iq);
		return true;
	}
	//else if(ns == "jabber:iq:time") {
	//	QDomElement iq = createIQ("result", e.attribute("from"), e.attribute("id"));
	//	QDomElement query = doc()->createElement("query");
	//	query.setAttribute("xmlns", "jabber:iq:time");
	//	iq.appendChild(query);
	//	QDateTime local = QDateTime::currentDateTime();
	//	QDateTime utc = local.addSecs(-getTZOffset() * 3600);
	//	QString str = getTZString();
	//	query.appendChild(textTag("utc", TS2stamp(utc)));
	//	query.appendChild(textTag("tz", str));
	//	query.appendChild(textTag("display", QString("%1 %2").arg(local.toString()).arg(str)));
	//	send(iq);
	//	return TRUE;
	//}
	else if(ns == "http://jabber.org/protocol/disco#info") {
		// Find out the node
		bool invalid_node = false;
		QString node;
		bool found;
		QDomElement q = findSubTag(e, "query", &found);
		if(found) // NOTE: Should always be true, since a NS was found above
			node = q.attribute("node");
		
		QDomElement iq = createIQ(doc(), "result", e.attribute("from"), e.attribute("id"));
		QDomElement query = doc()->createElement("query");
		query.setAttribute("xmlns", "http://jabber.org/protocol/disco#info");
		if (!node.isEmpty())
			query.setAttribute("node", node);
		iq.appendChild(query);

		// Identity
		DiscoItem::Identity identity = client()->identity();
		QDomElement id = doc()->createElement("identity");
		if (!identity.category.isEmpty() && !identity.type.isEmpty()) {
			id.setAttribute("category",identity.category);
			id.setAttribute("type",identity.type);
			if (!identity.name.isEmpty()) {
				id.setAttribute("name",identity.name);
			}
		}
		else {
			// Default values
			id.setAttribute("category","client");
			id.setAttribute("type","pc");
		}
		query.appendChild(id);

		QDomElement feature;
		if (node.isEmpty() || node == client()->capsNode() + "#" + client()->capsVersion()) {
			// Standard features
			feature = doc()->createElement("feature");
			feature.setAttribute("var", "http://jabber.org/protocol/bytestreams");
			query.appendChild(feature);

			feature = doc()->createElement("feature");
			feature.setAttribute("var", "http://jabber.org/protocol/si");
			query.appendChild(feature);

			feature = doc()->createElement("feature");
			feature.setAttribute("var", "http://jabber.org/protocol/si/profile/file-transfer");
			query.appendChild(feature);
			
			feature = doc()->createElement("feature");
			feature.setAttribute("var", "http://jabber.org/protocol/disco#info");
			query.appendChild(feature);

			// Client-specific features
			QStringList clientFeatures = client()->features().list();
			for (QStringList::ConstIterator i = clientFeatures.begin(); i != clientFeatures.end(); ++i) {
				feature = doc()->createElement("feature");
				feature.setAttribute("var", *i);
				query.appendChild(feature);
			}

			if (node.isEmpty()) {
				// Extended features
				QStringList exts = client()->extensions();
				for (QStringList::ConstIterator i = exts.begin(); i != exts.end(); ++i) {
					const QStringList& l = client()->extension(*i).list();
					for ( QStringList::ConstIterator j = l.begin(); j != l.end(); ++j ) {
						feature = doc()->createElement("feature");
						feature.setAttribute("var", *j);
						query.appendChild(feature);
					}
				}
			}
		}
		else if (node.startsWith(client()->capsNode() + "#")) {
			QString ext = node.right(node.length()-client()->capsNode().length()-1);
			if (client()->extensions().contains(ext)) {
				const QStringList& l = client()->extension(ext).list();
				for ( QStringList::ConstIterator it = l.begin(); it != l.end(); ++it ) {
					feature = doc()->createElement("feature");
					feature.setAttribute("var", *it);
					query.appendChild(feature);
				}
			}
			else {
				invalid_node = true;
			}
		}
		else {
			invalid_node = true;
		}
		
		if (!invalid_node) {
			send(iq);
		}
		else {
			// Create error reply
			QDomElement error_reply = createIQ(doc(), "result", e.attribute("from"), e.attribute("id"));
			
			// Copy children
			for (QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
				error_reply.appendChild(n.cloneNode());
			}
			
			// Add error
			QDomElement error = doc()->createElement("error");
			error.setAttribute("type","cancel");
			error_reply.appendChild(error);
			QDomElement error_type = doc()->createElement("item-not-found");
			error_type.setAttribute("xmlns","urn:ietf:params:xml:ns:xmpp-stanzas");
			error.appendChild(error_type);
			send(error_reply);
		}
		return true;
	}

	return false;
}


//----------------------------------------------------------------------------
// JT_Gateway
//----------------------------------------------------------------------------
JT_Gateway::JT_Gateway(Task *parent)
:Task(parent)
{
	type = -1;
}

void JT_Gateway::get(const Jid &jid)
{
	type = 0;
	v_jid = jid;
	iq = createIQ(doc(), "get", v_jid.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "jabber:iq:gateway");
	iq.appendChild(query);
}

void JT_Gateway::set(const Jid &jid, const QString &prompt)
{
	type = 1;
	v_jid = jid;
	v_prompt = prompt;
	iq = createIQ(doc(), "set", v_jid.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "jabber:iq:gateway");
	iq.appendChild(query);
	query.appendChild(textTag(doc(), "prompt", v_prompt));
}

void JT_Gateway::onGo()
{
	send(iq);
}

Jid JT_Gateway::jid() const
{
	return v_jid;
}

QString JT_Gateway::desc() const
{
	return v_desc;
}

QString JT_Gateway::prompt() const
{
	return v_prompt;
}

bool JT_Gateway::take(const QDomElement &x)
{
	if(!iqVerify(x, v_jid, id()))
		return false;

	if(x.attribute("type") == "result") {
		if(type == 0) {
			QDomElement query = queryTag(x);
			bool found;
			QDomElement tag;
			tag = findSubTag(query, "desc", &found);
			if(found)
				v_desc = tagContent(tag);
			tag = findSubTag(query, "prompt", &found);
			if(found)
				v_prompt = tagContent(tag);
		}
		else {
			QDomElement query = queryTag(x);
			bool found;
			QDomElement tag;
			tag = findSubTag(query, "prompt", &found);
			if(found)
				v_prompt = tagContent(tag);
		}

		setSuccess();
	}
	else {
		setError(x);
	}

	return true;
}

//----------------------------------------------------------------------------
// JT_Browse
//----------------------------------------------------------------------------
class JT_Browse::Private
{
public:
	QDomElement iq;
	Jid jid;
	AgentList agentList;
	AgentItem root;
};

JT_Browse::JT_Browse (Task *parent)
:Task (parent)
{
	d = new Private;
}

JT_Browse::~JT_Browse ()
{
	delete d;
}

void JT_Browse::get (const Jid &j)
{
	d->agentList.clear();

	d->jid = j;
	d->iq = createIQ(doc(), "get", d->jid.full(), id());
	QDomElement query = doc()->createElement("item");
	query.setAttribute("xmlns", "jabber:iq:browse");
	d->iq.appendChild(query);
}

const AgentList & JT_Browse::agents() const
{
	return d->agentList;
}

const AgentItem & JT_Browse::root() const
{
	return d->root;
}

void JT_Browse::onGo ()
{
	send(d->iq);
}

AgentItem JT_Browse::browseHelper (const QDomElement &i)
{
	AgentItem a;

	if ( i.tagName() == "ns" )
		return a;

	a.setName ( i.attribute("name") );
	a.setJid  ( i.attribute("jid") );

	// there are two types of category/type specification:
	//
	//   1. <item category="category_name" type="type_name" />
	//   2. <category_name type="type_name" />

	if ( i.tagName() == "item" || i.tagName() == "query" )
		a.setCategory ( i.attribute("category") );
	else
		a.setCategory ( i.tagName() );

	a.setType ( i.attribute("type") );

	QStringList ns;
	for(QDomNode n = i.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull())
			continue;

		if ( i.tagName() == "ns" )
			ns << i.text();
	}

	// For now, conference.jabber.org returns proper namespace only
	// when browsing individual rooms. So it's a quick client-side fix.
	if ( !a.features().canGroupchat() && a.category() == "conference" )
		ns << "jabber:iq:conference";

	a.setFeatures (ns);

	return a;
}

bool JT_Browse::take(const QDomElement &x)
{
	if(!iqVerify(x, d->jid, id()))
		return false;

	if(x.attribute("type") == "result") {
		for(QDomNode n = x.firstChild(); !n.isNull(); n = n.nextSibling()) {
			QDomElement i = n.toElement();
			if(i.isNull())
				continue;

			d->root = browseHelper (i);

			for(QDomNode nn = i.firstChild(); !nn.isNull(); nn = nn.nextSibling()) {
				QDomElement e = nn.toElement();
				if ( e.isNull() )
					continue;
				if ( e.tagName() == "ns" )
					continue;

				d->agentList += browseHelper (e);
			}
		}

		setSuccess(true);
	}
	else {
		setError(x);
	}

	return true;
}

//----------------------------------------------------------------------------
// JT_DiscoItems
//----------------------------------------------------------------------------
class JT_DiscoItems::Private
{
public:
	Private() { }

	QDomElement iq;
	Jid jid;
	DiscoList items;
};

JT_DiscoItems::JT_DiscoItems(Task *parent)
: Task(parent)
{
	d = new Private;
}

JT_DiscoItems::~JT_DiscoItems()
{
	delete d;
}

void JT_DiscoItems::get(const DiscoItem &item)
{
	get(item.jid(), item.node());
}

void JT_DiscoItems::get (const Jid &j, const QString &node)
{
	d->items.clear();

	d->jid = j;
	d->iq = createIQ(doc(), "get", d->jid.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "http://jabber.org/protocol/disco#items");

	if ( !node.isEmpty() )
		query.setAttribute("node", node);

	d->iq.appendChild(query);
}

const DiscoList &JT_DiscoItems::items() const
{
	return d->items;
}

void JT_DiscoItems::onGo ()
{
	send(d->iq);
}

bool JT_DiscoItems::take(const QDomElement &x)
{
	if(!iqVerify(x, d->jid, id()))
		return false;

	if(x.attribute("type") == "result") {
		QDomElement q = queryTag(x);

		for(QDomNode n = q.firstChild(); !n.isNull(); n = n.nextSibling()) {
			QDomElement e = n.toElement();
			if( e.isNull() )
				continue;

			if ( e.tagName() == "item" ) {
				DiscoItem item;

				item.setJid ( e.attribute("jid")  );
				item.setName( e.attribute("name") );
				item.setNode( e.attribute("node") );
				item.setAction( DiscoItem::string2action(e.attribute("action")) );

				d->items.append( item );
			}
		}

		setSuccess(true);
	}
	else {
		setError(x);
	}

	return true;
}

//----------------------------------------------------------------------------
// JT_DiscoPublish
//----------------------------------------------------------------------------
class JT_DiscoPublish::Private
{
public:
	Private() { }

	QDomElement iq;
	Jid jid;
	DiscoList list;
};

JT_DiscoPublish::JT_DiscoPublish(Task *parent)
: Task(parent)
{
	d = new Private;
}

JT_DiscoPublish::~JT_DiscoPublish()
{
	delete d;
}

void JT_DiscoPublish::set(const Jid &j, const DiscoList &list)
{
	d->list = list;
	d->jid = j;

	d->iq = createIQ(doc(), "set", d->jid.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "http://jabber.org/protocol/disco#items");

	// FIXME: unsure about this
	//if ( !node.isEmpty() )
	//	query.setAttribute("node", node);

	DiscoList::ConstIterator it = list.begin();
	for ( ; it != list.end(); ++it) {
		QDomElement w = doc()->createElement("item");

		w.setAttribute("jid", (*it).jid().full());
		if ( !(*it).name().isEmpty() )
			w.setAttribute("name", (*it).name());
		if ( !(*it).node().isEmpty() )
		w.setAttribute("node", (*it).node());
		w.setAttribute("action", DiscoItem::action2string((*it).action()));

		query.appendChild( w );
	}

	d->iq.appendChild(query);
}

void JT_DiscoPublish::onGo ()
{
	send(d->iq);
}

bool JT_DiscoPublish::take(const QDomElement &x)
{
	if(!iqVerify(x, d->jid, id()))
		return false;

	if(x.attribute("type") == "result") {
		setSuccess(true);
	}
	else {
		setError(x);
	}

	return true;
}

