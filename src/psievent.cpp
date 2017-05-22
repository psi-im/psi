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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "psievent.h"

#include <QDomElement>
#include <QTextStream>
#include <QList>
#include <QCoreApplication>
#include <QSet>

#include "psicon.h"
#include "psiaccount.h"
#include "xmpp_xmlcommon.h"
#include "dummystream.h"
#include "filetransfer.h"
#include "applicationinfo.h"
#include "psicontactlist.h"
#include "atomicxmlfile/atomicxmlfile.h"
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

void PsiEvent::setAccount(PsiAccount* account)
{
	v_account = account;
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

#ifdef PSI_PLUGINS
//----------------------------------------------------------------------------
// PluginEvent
//----------------------------------------------------------------------------
PluginEvent::PluginEvent(int account, const QString& jid, const QString& descr, PsiAccount *acc)
	: PsiEvent(acc)
	, descr_(descr)
	, _account(account)
{
	from_ = XMPP::Jid(jid);
}

PluginEvent::~PluginEvent()
{

}

int PluginEvent::type() const
{
	return Plugin;
}

XMPP::Jid PluginEvent::from() const
{
	return from_;
}

void PluginEvent::setFrom(const XMPP::Jid &j)
{
	from_ = j;
}

void PluginEvent::activate()
{
	emit activated(from_.full(), _account);
}

QString PluginEvent::description() const
{
	return descr_;
}
#endif

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
#ifdef GROUPCHAT
	if (v_m.type() == "groupchat")
		return v_m.from().bare();
#endif
	return v_m.carbonDirection() == XMPP::Message::Sent ? v_m.to() : v_m.from();
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

	QDomElement msg = (*e).firstChildElement("message");
	if (!msg.isNull()) {
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
	v_sentToChatWindow = false;
}

AuthEvent::AuthEvent(const AuthEvent &from)
	: PsiEvent(from)
	, v_from(from.v_from)
	, v_at(from.v_at)
	, v_sentToChatWindow(from.v_sentToChatWindow)
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
	v_nick = subTagText(*e, "nick");

	return true;
}

int AuthEvent::priority() const
{
	return eventPriorityAuth;
}

QString AuthEvent::description() const
{
	QString result;
	if (authType() == "subscribe")
		result = tr("%1 wants to subscribe to your presence.").arg(from().bare());
	else if (authType() == "subscribed")
		result = tr("%1 authorized you to view his status.").arg(from().bare());
	else if (authType() == "unsubscribed" || authType() == "unsubscribe")
		result = tr("%1 removed your authorization to view his status!").arg(from().bare());
	else
		Q_ASSERT(false);

	return result;
}

PsiEvent *AuthEvent::copy() const
{
	return new AuthEvent( *this );
}

void AuthEvent::setSentToChatWindow(bool b)
{
	v_sentToChatWindow = b;
}

bool AuthEvent::sentToChatWindow() const
{
	return v_sentToChatWindow;
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
// EventIdGenerator
//----------------------------------------------------------------------------

class EventIdGenerator : public QObject
{
	Q_OBJECT
public:
	static EventIdGenerator* instance();

	int getId();

private:
	static EventIdGenerator* instance_;
	int id_;

	EventIdGenerator();
};

EventIdGenerator* EventIdGenerator::instance_ = 0;

EventIdGenerator* EventIdGenerator::instance()
{
	if (!instance_) {
		instance_ = new EventIdGenerator();
	}
	return instance_;
}

static const QString idGeneratorOptionPath = "options.last-event-id";

EventIdGenerator::EventIdGenerator()
	: QObject(QCoreApplication::instance())
{
	id_ = 0;
}

int EventIdGenerator::getId()
{
	int result = id_;
	++id_;

	if (id_ > 0x7FFFFFFF) {
		id_ = 0;
	}

	Q_ASSERT(id_ >= 0);
	Q_ASSERT(result >= 0);
	return result;
}

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

EventItem::EventItem(const PsiEvent::Ptr &_e)
{
	e = _e;
	Q_ASSERT(e->account());
	v_id = EventIdGenerator::instance()->getId();
}

EventItem::EventItem(const EventItem &from)
{
	e = from.e;
	v_id = from.v_id;
}

EventItem::~EventItem()
{
}

int EventItem::id() const
{
	return v_id;
}

PsiEvent::Ptr EventItem::event() const
{
	return e;
}

//----------------------------------------------------------------------------
// EventQueue
//----------------------------------------------------------------------------

EventQueue::EventQueue(PsiAccount *account)
	: psi_(0)
	, account_(0)
	, enabled_(false)
{
	account_ = account;
	psi_ = account_->psi();
}

EventQueue::EventQueue(const EventQueue &from)
	: QObject()
	, list_()
	, psi_(0)
	, account_(0)
	, enabled_(false)
{
	Q_ASSERT(false);
	Q_UNUSED(from);
}

EventQueue::~EventQueue()
{
	setEnabled(false);
	qDeleteAll(list_);
	list_.clear();
}

bool EventQueue::enabled() const
{
	return enabled_;
}

void EventQueue::setEnabled(bool enabled)
{
	enabled_ = enabled;
}

EventQueue &EventQueue::operator= (const EventQueue &from)
{
	while(!list_.isEmpty())
		delete list_.takeFirst();

	psi_ = from.psi_;
	account_ = from.account_;

	foreach(EventItem *i, from.list_) {
		PsiEvent::Ptr e = i->event();
		enqueue( e );
	}

	return *this;
}

int EventQueue::nextId() const
{
	if (list_.isEmpty())
		return -1;

	EventItem *i = list_.first();
	if(!i)
		return -1;
	return i->id();
}

int EventQueue::count() const
{
	return list_.count();
}

int EventQueue::contactCount() const
{
	QSet<QString> set;
	foreach(EventItem *i, list_) {
		set.insert(i->event()->jid().bare());
	}
	return set.size();
}

int EventQueue::count(const Jid &j, bool compareRes) const
{
	int total = 0;
	foreach(EventItem *i, list_) {
		Jid j2(i->event()->jid());
		if(j.compare(j2, compareRes))
			++total;
	}
	return total;
}

void EventQueue::enqueue(const PsiEvent::Ptr &e)
{
	EventItem *i = new EventItem(e);

	int prior  = e->priority();
	bool found = false;

	// skip all with higher or equal priority
	foreach(EventItem *ei, list_) {
		if (ei && ei->event()->priority() < prior ) {
			list_.insert(list_.indexOf(ei), i);
			found = true;
			break;
		}
	}

	// everything else
	if ( !found )
		list_.append(i);

	emit queueChanged();
}

void EventQueue::dequeue(const PsiEvent::Ptr &e)
{
	if ( !e )
		return;

	foreach(EventItem *i, list_) {
		if ( e == i->event() ) {
			list_.removeAll(i);
			emit queueChanged();
			delete i;
			return;
		}
	}
}

PsiEvent::Ptr EventQueue::dequeue(const Jid &j, bool compareRes)
{
	foreach(EventItem *i, list_) {
		PsiEvent::Ptr e = i->event();
		Jid j2(e->jid());
		if(j.compare(j2, compareRes)) {
			list_.removeAll(i);
			emit queueChanged();
			delete i;
			return e;
		}
	}

	return PsiEvent::Ptr();
}

PsiEvent::Ptr EventQueue::peek(const Jid &j, bool compareRes) const
{
	foreach(EventItem *i, list_) {
		PsiEvent::Ptr e = i->event();
		Jid j2(e->jid());
		if(j.compare(j2, compareRes)) {
			return e;
		}
	}

	return PsiEvent::Ptr();
}

PsiEvent::Ptr EventQueue::dequeueNext()
{
	if (list_.isEmpty())
		return PsiEvent::Ptr();

	EventItem *i = list_.first();
	if(!i)
		return PsiEvent::Ptr();
	PsiEvent::Ptr e = i->event();
	list_.removeAll(i);
	emit queueChanged();
	delete i;
	return e;
}

PsiEvent::Ptr EventQueue::peekNext() const
{
	if (list_.isEmpty())
		return PsiEvent::Ptr();

	EventItem *i = list_.first();
	if(!i)
		return PsiEvent::Ptr();
	return i->event();
}

PsiEvent::Ptr EventQueue::peekFirstChat(const Jid &j, bool compareRes) const
{
	foreach(EventItem *i, list_) {
		PsiEvent::Ptr e = i->event();
		if(e->type() == PsiEvent::Message) {
			MessageEvent::Ptr me = e.staticCast<MessageEvent>();
			if(j.compare(me->from(), compareRes) && me->message().type() == "chat")
				return e;
		}
	}

	return PsiEvent::Ptr();
}

bool EventQueue::hasChats(const Jid &j, bool compareRes) const
{
	return (peekFirstChat(j, compareRes) ? true: false);
}

// this function extracts all chats from the queue, and returns a list of queue positions
void EventQueue::extractChats(QList<PsiEvent::Ptr> *el, const Jid &j, bool compareRes, bool removeEvents)
{
	bool changed = false;

	for(QList<EventItem*>::Iterator it = list_.begin(); it != list_.end();) {
		PsiEvent::Ptr e = (*it)->event();
		bool extract = false;
		if(e->type() == PsiEvent::Message) {
			MessageEvent::Ptr me = e.staticCast<MessageEvent>();
			if(j.compare(me->from(), compareRes) && me->message().type() == "chat") { // FIXME: refactor-refactor-refactor
				extract = true;
			}
		}

		if (extract) {
			el->append(e);
		}

		if (extract && removeEvents) {
			EventItem* ei = *it;
			it = list_.erase(it);
			delete ei;
			changed = true;
			continue;
		}

		++it;
	}

	if ( changed )
		emit queueChanged();
}

void EventQueue::extractByJid(QList<PsiEvent::Ptr> *list, const XMPP::Jid &jid)
{
	for (QList<EventItem*>::Iterator it = list_.begin(); it != list_.end(); it++) {
		PsiEvent::Ptr e = (*it)->event();
		if (jid.compare(e->from(), false)) {
			list->append(e);
		}
	}
}


// this function extracts all messages from the queue, and returns a list of them
void EventQueue::extractMessages(QList<PsiEvent::Ptr> *el)
{
	extractByType(PsiEvent::Message, el);
}

// this function extracts all auths from the queue, and returns a list of them
void EventQueue::extractByType(int type, QList<PsiEvent::Ptr> *el)
{
	bool changed = false;

	for(QList<EventItem*>::Iterator it = list_.begin(); it != list_.end();) {
		PsiEvent::Ptr e = (*it)->event();
		if(e->type() == type) {
			el->append(e);
			EventItem* ei = *it;
			it = list_.erase(it);
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
	foreach(EventItem *i, list_) {
		PsiEvent::Ptr e = i->event();
		printf("  %d: (%d) from=[%s] jid=[%s]\n", i->id(), e->type(), qPrintable(e->from().full()), qPrintable(e->jid().full()));
	}
}

void EventQueue::clear()
{
	while(!list_.isEmpty())
	{
		EventItem *i = list_.takeFirst();
		delete i;
	}

	emit queueChanged();
}

// this function removes all events associated with the input jid
void EventQueue::clear(const Jid &j, bool compareRes)
{
	bool changed = false;

	for(QList<EventItem*>::Iterator it = list_.begin(); it != list_.end();) {
		PsiEvent::Ptr e = (*it)->event();
		Jid j2(e->jid());
		if(j.compare(j2, compareRes)) {
			EventItem* ei = *it;
			it = list_.erase(it);
			delete ei;
			changed = true;
		}
		else
			++it;
	}

	if ( changed )
		emit queueChanged();
}

QDomElement EventQueue::toXml(QDomDocument *doc) const
{
	QDomElement e = doc->createElement("eventQueue");
	e.setAttribute("version", "1.0");
	e.appendChild(textTag(doc, "progver", ApplicationInfo::version()));

	foreach(EventItem *i, list_) {
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

		PsiEvent::Ptr event;
		QString eventType = e.attribute("type");
		if ( eventType == "MessageEvent" ) {
			event = MessageEvent::Ptr(new MessageEvent(0));
			if ( !event->fromXml(psi_, account_, &e) ) {
				//delete event;
				event.clear();
			}
		}
		else if ( eventType == "AuthEvent" ) {
			event = AuthEvent::Ptr(new AuthEvent("", "", 0));
			if ( !event->fromXml(psi_, account_, &e) ) {
				//delete event;
				event.clear();
			}
		}

		if ( event )
			emit eventFromXml( event );
	}

	return true;
}

QList<EventQueue::PsiEventId> EventQueue::eventsFor(const XMPP::Jid& jid, bool compareRes)
{
	QList<PsiEventId> result;

	foreach(EventItem* i, list_) {
		if (i->event()->from().compare(jid, compareRes))
			result << QPair<int, PsiEvent::Ptr>(i->id(), i->event());
	}

	return result;
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

#include "psievent.moc"
