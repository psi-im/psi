/*
 * psievent.h - events
 * Copyright (C) 2001, 2002  Justin Karneges
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

#include "psievent.h"

#include <qdom.h>
#include <QTextStream>
#include <QList>

#include "psicon.h"
#include "psiaccount.h"
#include "xmpp_xmlcommon.h"
#include "filetransfer.h"
#include "applicationinfo.h"
#include "psicontactlist.h"
#include "atomicxmlfile.h"
#include "psioptions.h"
#include "avcall/avcall.h"

// FIXME renames those
const int eventPriorityHeadline = 0;
const int eventPriorityChat     = 1;
const int eventPriorityMessage  = 1;
const int eventPriorityAuth     = 2;
//const int eventPriorityFile     = 3;
const int eventPriorityFile     = 2;
const int eventPriorityRosterExchange = 0; // LEGOPTFIXME: was uninitialised


using namespace XMPP;
using namespace XMLHelper;

//----------------------------------------------------------------------------
// DummyStream
//----------------------------------------------------------------------------
class DummyStream : public Stream
{
public:
	QDomDocument & doc() const { return v_doc; }
	QString baseNS() const { return "jabber:client"; }
	bool old() const { return false; }

	void close() { }
	bool stanzaAvailable() const { return false; }
	Stanza read() { return Stanza(); }
	void write(const Stanza &) { }

	int errorCondition() const { return 0; }
	QString errorText() const { return QString::null; }
	QDomElement errorAppSpec() const { return v_doc.documentElement(); }

private:
	static QDomDocument v_doc;
};

QDomDocument DummyStream::v_doc;

//----------------------------------------------------------------------------
// PsiEvent
//----------------------------------------------------------------------------
PsiEvent::PsiEvent(PsiAccount *acc)
{
	v_originLocal = false;
	v_late = false;
	v_account = acc;
}

PsiEvent::PsiEvent(const PsiEvent &from)
: QObject()
{
	v_originLocal = from.v_originLocal;
	v_late = from.v_late;
	v_ts = from.v_ts;
	v_jid = from.v_jid;
	v_account = from.v_account;
}

PsiEvent::~PsiEvent()
{
}

XMPP::Jid PsiEvent::jid() const
{
	return v_jid;
}

void PsiEvent::setJid(const XMPP::Jid &j)
{
	v_jid = j;
}

PsiAccount *PsiEvent::account() const
{
	return v_account;
}

bool PsiEvent::originLocal() const
{
	return v_originLocal;
}

bool PsiEvent::late() const
{
	return v_late;
}

QDateTime PsiEvent::timeStamp() const
{
	return v_ts;
}

void PsiEvent::setOriginLocal(bool b)
{
	v_originLocal = b;
}

void PsiEvent::setLate(bool b)
{
	v_late = b;
}

void PsiEvent::setTimeStamp(const QDateTime &t)
{
	v_ts = t;
}

QDomElement PsiEvent::toXml(QDomDocument *doc) const
{
	QDomElement e = doc->createElement("event");
	e.setAttribute("type", metaObject()->className());

	e.appendChild( textTag(*doc, "originLocal",	v_originLocal) );
	e.appendChild( textTag(*doc, "late",		v_late) );
	e.appendChild( textTag(*doc, "ts",		v_ts.toString( Qt::ISODate )) );
	if ( !v_jid.full().isEmpty() )
		e.appendChild( textTag(*doc, "jid",		v_jid.full()) );

	if ( v_account )
		e.appendChild( textTag(*doc, "account",	v_account->name()) );

	return e;
}

bool PsiEvent::fromXml(PsiCon *psi, PsiAccount *account, const QDomElement *e)
{
	if ( e->tagName() != "event" )
		return false;
	if ( e->attribute("type") != metaObject()->className() )
		return false;

	readBoolEntry(*e, "originLocal", &v_originLocal);
	readBoolEntry(*e, "late", &v_late);
	v_ts  = QDateTime::fromString(subTagText(*e, "ts"), Qt::ISODate);
	v_jid = Jid( subTagText(*e, "jid") );

	if ( account ) {
		v_account = account;
	}
	else if ( hasSubTag(*e, "account") ) {
		QString accName = subTagText(*e, "account");
		foreach(PsiAccount* account, psi->contactList()->accounts()) {
			if ( account->name() == accName ) {
				v_account = account;
				break;
			}
		}
	}

	return true;
}

int PsiEvent::priority() const
{
	return EventPriorityDontCare;
}

QString PsiEvent::description() const
{
	return QString();
}

PsiEvent *PsiEvent::copy() const
{
	return 0;
}

//----------------------------------------------------------------------------
// MessageEvent
//----------------------------------------------------------------------------

MessageEvent::MessageEvent(PsiAccount *acc)
: PsiEvent(acc)
{
	v_sentToChatWindow = false;
}

MessageEvent::MessageEvent(const XMPP::Message &m, PsiAccount *acc)
: PsiEvent(acc)
{
	v_sentToChatWindow = false;
	setMessage(m);
}

MessageEvent::MessageEvent(const MessageEvent &from)
: PsiEvent(from), v_m(from.v_m), v_sentToChatWindow(from.v_sentToChatWindow)
{
}

MessageEvent::~MessageEvent()
{
}

int MessageEvent::type() const
{
	return Message;
}

Jid MessageEvent::from() const
{
	return v_m.from();
}

void MessageEvent::setFrom(const Jid &j)
{
	v_m.setFrom(j);
}

const QString& MessageEvent::nick() const
{
	return v_m.nick();
}

void MessageEvent::setNick(const QString &nick)
{
	v_m.setNick(nick);
}

bool MessageEvent::sentToChatWindow() const
{
	return v_sentToChatWindow;
}

const XMPP::Message & MessageEvent::message() const
{
	return v_m;
}

void MessageEvent::setSentToChatWindow(bool b)
{
	v_sentToChatWindow = b;
}

void MessageEvent::setMessage(const XMPP::Message &m)
{
	v_m = m;
	setTimeStamp ( v_m.timeStamp() );
	setLate ( v_m.spooled() );
}

QDomElement MessageEvent::toXml(QDomDocument *doc) const
{
	QDomElement e = PsiEvent::toXml(doc);

	DummyStream stream;
	Stanza s = v_m.toStanza(&stream);
	e.appendChild( s.element() );

	return e;
}

bool MessageEvent::fromXml(PsiCon *psi, PsiAccount *account, const QDomElement *e)
{
	if ( !PsiEvent::fromXml(psi, account, e) )
		return false;

	bool found = false;
	QDomElement msg = findSubTag(*e, "message", &found);
	if ( found ) {
		DummyStream stream;
		Stanza s = stream.createStanza(addCorrectNS(msg));
		v_m.fromStanza(s);

		// if message was not spooled, it will be initialized with the
		// current datetime. we want to reset it back to the original
		// receive time
		if (!v_m.timeStamp().secsTo(QDateTime::currentDateTime()))
			v_m.setTimeStamp(timeStamp());

		return true;
	}

	return false;
}

int MessageEvent::priority() const
{
	if ( v_m.type() == "headline" )
		return eventPriorityHeadline;
	else if ( v_m.type() == "chat" )
		return eventPriorityChat;

	return eventPriorityMessage;
}

QString MessageEvent::description() const
{
	QStringList result;
	if (!v_m.subject().isEmpty())
		result << v_m.subject();
	if (!v_m.body().isEmpty())
		result << v_m.body();
	foreach(Url url, v_m.urlList()) {
		QString text = url.url();
		if (!url.desc().isEmpty())
			text += QString("(%1)").arg(url.desc());
		result << text;
	}
	return result.join("\n");
}

PsiEvent *MessageEvent::copy() const
{
	return new MessageEvent( *this );
}

//----------------------------------------------------------------------------
// AuthEvent
//----------------------------------------------------------------------------

AuthEvent::AuthEvent(const Jid &j, const QString &authType, PsiAccount *acc)
: PsiEvent(acc)
{
	v_from = j;
	v_at = authType;
}

AuthEvent::AuthEvent(const AuthEvent &from)
: PsiEvent(from), v_from(from.v_from), v_at(from.v_at)
{
}

AuthEvent::~AuthEvent()
{
}

int AuthEvent::type() const
{
	return Auth;
}

Jid AuthEvent::from() const
{
	return v_from;
}

void AuthEvent::setFrom(const Jid &j)
{
	v_from = j;
}

const QString& AuthEvent::nick() const
{
	return v_nick;
}

void AuthEvent::setNick(const QString &nick)
{
	v_nick = nick;
}

QString AuthEvent::authType() const
{
	return v_at;
}

QDomElement AuthEvent::toXml(QDomDocument *doc) const
{
	QDomElement e = PsiEvent::toXml(doc);

	e.appendChild( textTag(*doc, "from",	 v_from.full()) );
	e.appendChild( textTag(*doc, "authType", v_at) );
	e.appendChild( textTag(*doc, "nick", v_nick) );

	return e;
}

bool AuthEvent::fromXml(PsiCon *psi, PsiAccount *account, const QDomElement *e)
{
	if ( !PsiEvent::fromXml(psi, account, e) )
		return false;

	v_from = Jid( subTagText(*e, "from") );
	v_at   = subTagText(*e, "authType");
	v_nick   = subTagText(*e, "nick");

	return false;
}

int AuthEvent::priority() const
{
	return eventPriorityAuth;
}

QString AuthEvent::description() const
{
	QString result;
	if (authType() == "subscribe")
		result = tr("This user wants to subscribe to your presence.");
	else if (authType() == "subscribed")
		result = tr("You are now authorized.");
	else if (authType() == "unsubscribed")
		result = tr("Your authorization has been removed!");
	else
		Q_ASSERT(false);

	return result;
}

PsiEvent *AuthEvent::copy() const
{
	return new AuthEvent( *this );
}

//----------------------------------------------------------------------------
// FileEvent
//----------------------------------------------------------------------------
FileEvent::FileEvent(const Jid &j, FileTransfer *_ft, PsiAccount *acc)
:PsiEvent(acc)
{
	v_from = j;
	ft = _ft;
}

FileEvent::~FileEvent()
{
	delete ft;
}

FileEvent::FileEvent(const FileEvent &from)
: PsiEvent(from.account())
{
	v_from = from.v_from;
	ft = from.ft->copy();
}

int FileEvent::priority() const
{
	return eventPriorityFile;
}

Jid FileEvent::from() const
{
	return v_from;
}

void FileEvent::setFrom(const Jid &j)
{
	v_from = j;
}

FileTransfer *FileEvent::takeFileTransfer()
{
	FileTransfer *_ft = ft;
	ft = 0;
	return _ft;
}

QString FileEvent::description() const
{
	return tr("This user wants to send you a file.");
}

PsiEvent *FileEvent::copy() const
{
	return new FileEvent( *this );
}

//----------------------------------------------------------------------------
// HttpAuthEvent
//----------------------------------------------------------------------------

HttpAuthEvent::HttpAuthEvent(const PsiHttpAuthRequest &req, PsiAccount *acc)
:MessageEvent(acc), v_req(req)
{
	const XMPP::Stanza &s = req.stanza();

	XMPP::Message m;

	if ( s.kind() == XMPP::Stanza::Message ) {
		m.fromStanza(s);
	}
	else {
		m.setFrom(s.from());
		m.setTimeStamp(QDateTime::currentDateTime());
		m.setHttpAuthRequest(HttpAuthRequest(s.element().elementsByTagNameNS("http://jabber.org/protocol/http-auth", "confirm").item(0).toElement()));
	}

	setMessage(m);
}

HttpAuthEvent::~HttpAuthEvent()
{
}

QString HttpAuthEvent::description() const
{
	return tr("HTTP Authentication Request");
}

//----------------------------------------------------------------------------
// RosterExchangeEvent
//----------------------------------------------------------------------------
RosterExchangeEvent::RosterExchangeEvent(const Jid &j, const RosterExchangeItems& i, const QString& text, PsiAccount *acc)
:PsiEvent(acc)
{
	v_from = j;
	v_items = i;
	v_text = text;
}

int RosterExchangeEvent::priority() const
{
	return eventPriorityRosterExchange;
}

Jid RosterExchangeEvent::from() const
{
	return v_from;
}

void RosterExchangeEvent::setFrom(const Jid &j)
{
	v_from = j;
}

const RosterExchangeItems& RosterExchangeEvent::rosterExchangeItems() const
{
	return v_items;
}

void RosterExchangeEvent::setRosterExchangeItems(const RosterExchangeItems& i)
{
	v_items = i;
}

const QString& RosterExchangeEvent::text() const
{
	return v_text;
}

void RosterExchangeEvent::setText(const QString& text)
{
	v_text = text;
}

QString RosterExchangeEvent::description() const
{
	return tr("This user wants to modify your roster.");
}

//----------------------------------------------------------------------------
// Status
//----------------------------------------------------------------------------
/*StatusEvent::StatusEvent(const Jid &j, const XMPP::Status& s, PsiAccount *acc)
:PsiEvent(acc)
{
	v_from = j;
	v_status = s;
}

int StatusEvent::priority() const
{
	return eventPriorityChat;
}

Jid StatusEvent::from() const
{
	return v_from;
}

void StatusEvent::setFrom(const Jid &j)
{
	v_from = j;
}

const XMPP::Status& StatusEvent::status() const
{
	return v_status;
}

void StatusEvent::setStatus(const XMPP::Status& s)
{
	v_status = s;
}*/

//----------------------------------------------------------------------------
// AvCallEvent
//----------------------------------------------------------------------------
AvCallEvent::AvCallEvent(const XMPP::Jid &j, AvCall *_sess, PsiAccount *acc)
:PsiEvent(acc)
{
	v_from = j;
	sess = _sess;
}

AvCallEvent::AvCallEvent(const AvCallEvent &from)
:PsiEvent(from.account())
{
	v_from = from.v_from;
	sess = new AvCall(*from.sess);
}

AvCallEvent::~AvCallEvent()
{
	delete sess;
}

XMPP::Jid AvCallEvent::from() const
{
	return v_from;
}

void AvCallEvent::setFrom(const XMPP::Jid &j)
{
	v_from = j;
}

AvCall *AvCallEvent::takeAvCall()
{
	AvCall *_sess = sess;
	sess = 0;
	return _sess;
}

int AvCallEvent::priority() const
{
	return eventPriorityFile; // FIXME
}

QString AvCallEvent::description() const
{
	return tr("The user is calling you.");
}

PsiEvent *AvCallEvent::copy() const
{
	return new AvCallEvent(*this);
}

//----------------------------------------------------------------------------
// EventItem
//----------------------------------------------------------------------------

EventItem::EventItem(PsiEvent *_e, int i)
{
	e = _e;
	v_id = i;
}

EventItem::EventItem(const EventItem &from)
{
	e = from.e->copy();
	v_id = from.v_id;
}

EventItem::~EventItem()
{
}

int EventItem::id() const
{
	return v_id;
}

PsiEvent* EventItem::event() const
{
	return e;
}

//----------------------------------------------------------------------------
// EventQueue
//----------------------------------------------------------------------------

class EventQueue::Private
{
public:
	Private() { }

	QList<EventItem*> list;
	PsiCon *psi;
	PsiAccount *account;
};

EventQueue::EventQueue(PsiAccount *account)
{
	d = new Private();

	d->account = account;
	d->psi = account->psi();
}

EventQueue::EventQueue(const EventQueue &from)
	: QObject()
{
	d = new Private();

	*this = from;
}

EventQueue::~EventQueue()
{
	delete d;
}

int EventQueue::nextId() const
{
	if (d->list.isEmpty())
		return -1;

	EventItem *i = d->list.first();
	if(!i)
		return -1;
	return i->id();
}

int EventQueue::count() const
{
	return d->list.count();
}

int EventQueue::count(const Jid &j, bool compareRes) const
{
	int total = 0;
	foreach(EventItem *i, d->list) {
		Jid j2(i->event()->jid());
		if(j.compare(j2, compareRes))
			++total;
	}
	return total;
}

void EventQueue::enqueue(PsiEvent *e)
{
	EventItem *i = new EventItem(e, d->psi->getId());

	int prior  = e->priority();
	bool found = false;

	// skip all with higher or equal priority
	foreach(EventItem *ei, d->list) {
		if (ei && ei->event()->priority() < prior ) {
			d->list.insert(d->list.indexOf(ei), i);
			found = true;
			break;
		}
	}

	// everything else
	if ( !found )
		d->list.append(i);

	emit queueChanged();
}

void EventQueue::dequeue(PsiEvent *e)
{
	if ( !e )
		return;

	foreach(EventItem *i, d->list) {
		if ( e == i->event() ) {
			d->list.removeAll(i);
			emit queueChanged();
			delete i;
			return;
		}
	}
}

PsiEvent *EventQueue::dequeue(const Jid &j, bool compareRes)
{
	foreach(EventItem *i, d->list) {
		PsiEvent *e = i->event();
		Jid j2(e->jid());
		if(j.compare(j2, compareRes)) {
			d->list.removeAll(i);
			emit queueChanged();
			delete i;
			return e;
		}
	}

	return 0;
}

PsiEvent *EventQueue::peek(const Jid &j, bool compareRes) const
{
	foreach(EventItem *i, d->list) {
		PsiEvent *e = i->event();
		Jid j2(e->jid());
		if(j.compare(j2, compareRes)) {
			return e;
		}
	}

	return 0;
}

PsiEvent *EventQueue::dequeueNext()
{
	if (d->list.isEmpty())
		return 0;

	EventItem *i = d->list.first();
	if(!i)
		return 0;
	PsiEvent *e = i->event();
	d->list.removeAll(i);
	emit queueChanged();
	delete i;
	return e;
}

PsiEvent *EventQueue::peekNext() const
{
	if (d->list.isEmpty())
		return 0;

	EventItem *i = d->list.first();
	if(!i)
		return 0;
	return i->event();
}

PsiEvent *EventQueue::peekFirstChat(const Jid &j, bool compareRes) const
{
	foreach(EventItem *i, d->list) {
		PsiEvent *e = i->event();
		if(e->type() == PsiEvent::Message) {
			MessageEvent *me = (MessageEvent *)e;
			if(j.compare(me->from(), compareRes) && me->message().type() == "chat")
				return e;
		}
	}

	return 0;
}

bool EventQueue::hasChats(const Jid &j, bool compareRes) const
{
	return (peekFirstChat(j, compareRes) ? true: false);
}

// this function extracts all chats from the queue, and returns a list of queue positions
void EventQueue::extractChats(QList<PsiEvent*> *el, const Jid &j, bool compareRes)
{
	bool changed = false;

	for(QList<EventItem*>::Iterator it = d->list.begin(); it != d->list.end();) {
		PsiEvent *e = (*it)->event();
		if(e->type() == PsiEvent::Message) {
			MessageEvent *me = (MessageEvent *)e;
			if(j.compare(me->from(), compareRes) && me->message().type() == "chat") {
				el->append(me);
				EventItem* ei = *it;
				it = d->list.erase(it);
				delete ei;
				changed = true;
				continue;
			}
		}
		++it;
	}

	if ( changed )
		emit queueChanged();
}

// this function extracts all messages from the queue, and returns a list of them
void EventQueue::extractMessages(QList<PsiEvent*> *el)
{
	bool changed = false;

	for(QList<EventItem*>::Iterator it = d->list.begin(); it != d->list.end();) {
		PsiEvent *e = (*it)->event();
		if(e->type() == PsiEvent::Message) {
			el->append(e);
			EventItem* ei = *it;
			it = d->list.erase(it);
			delete ei;
			changed = true;
			continue;
		}
		++it;
	}

	if ( changed )
		emit queueChanged();
}

void EventQueue::printContent() const
{
	foreach(EventItem *i, d->list) {
		PsiEvent *e = i->event();
		printf("  %d: (%d) from=[%s] jid=[%s]\n", i->id(), e->type(), qPrintable(e->from().full()), qPrintable(e->jid().full()));
	}
}

void EventQueue::clear()
{
	while(!d->list.isEmpty()) 
		delete d->list.takeFirst();

	emit queueChanged();
}

// this function removes all events associated with the input jid
void EventQueue::clear(const Jid &j, bool compareRes)
{
	bool changed = false;

	for(QList<EventItem*>::Iterator it = d->list.begin(); it != d->list.end();) {
		PsiEvent *e = (*it)->event();
		Jid j2(e->jid());
		if(j.compare(j2, compareRes)) {
			EventItem* ei = *it;
			it = d->list.erase(it);
			delete ei;
			changed = true;
		}
		else
			++it;
	}

	if ( changed )
		emit queueChanged();
}

EventQueue &EventQueue::operator= (const EventQueue &from)
{
	while(!d->list.isEmpty()) 
		delete d->list.takeFirst();
		
	d->psi = from.d->psi;

	foreach(EventItem *i, from.d->list) {
		PsiEvent *e = i->event();
		enqueue( e->copy() );
	}

	return *this;
}

QDomElement EventQueue::toXml(QDomDocument *doc) const
{
	QDomElement e = doc->createElement("eventQueue");
	e.setAttribute("version", "1.0");
	e.appendChild(textTag(doc, "progver", ApplicationInfo::version()));

	foreach(EventItem *i, d->list) {
		QDomElement event = i->event()->toXml(doc);
		e.appendChild( event );
	}

	return e;
}

bool EventQueue::fromXml(const QDomElement *q)
{
	if ( !q )
		return false;

	if ( q->tagName() != "eventQueue" )
		return false;

	if ( q->attribute("version") != "1.0" )
		return false;

	QString progver = subTagText(*q, "progver");

	for(QDomNode n = q->firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement e = n.toElement();
		if( e.isNull() )
			continue;

		if ( e.tagName() != "event" )
			continue;

		PsiEvent *event = 0;
		QString eventType = e.attribute("type");
		if ( eventType == "MessageEvent" ) {
			event = new MessageEvent(0);
			if ( !event->fromXml(d->psi, d->account, &e) ) {
				delete event;
				event = 0;
			}
		}
		else if ( eventType == "AuthEvent" ) {
			event = new AuthEvent("", "", 0);
			if ( !event->fromXml(d->psi, d->account, &e) ) {
				delete event;
				event = 0;
			}
		}

		if ( event )
			emit eventFromXml( event );
	}

	return true;
}

bool EventQueue::toFile(const QString &fname)
{
	QDomDocument doc;
	QDomElement element = toXml(&doc);
	doc.appendChild(element);

	AtomicXmlFile f(fname);
	return f.saveDocument(doc);
}

bool EventQueue::fromFile(const QString &fname)
{
	AtomicXmlFile f(fname);
	QDomDocument doc;
	if (!f.loadDocument(&doc))
		return false;

	QDomElement base = doc.documentElement();
	return fromXml(&base);
}
