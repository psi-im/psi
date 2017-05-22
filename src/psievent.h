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

#ifndef PSIEVENT_H
#define PSIEVENT_H

#include <QList>
#include <QDateTime>
#include <QObject>
#include <QDomDocument>
#include <QDomElement>
#include <QPointer>

#include "xmpp_jid.h"
#include "xmpp_rosterx.h"
#include "xmpp_message.h"
#include "psihttpauthrequest.h"

namespace XMPP {
	class FileTransfer;
};

class PsiCon;
class PsiAccount;
class QDomElement;

class AvCall;

class PsiEvent : public QObject
{
	Q_OBJECT
public:
	PsiEvent(PsiAccount *);
	PsiEvent(const PsiEvent &);
	virtual ~PsiEvent() = 0;

	typedef QSharedPointer<PsiEvent> Ptr;
	enum {
		Message,
#ifdef WHITEBOARDING
		Sxe,
#endif
		Auth,
		PGP,
		File,
		RosterExchange,
		//Status
		HttpAuth,
		AvCallType
#ifdef PSI_PLUGINS
		,Plugin
#endif

	};
	virtual int type() const = 0;

	virtual XMPP::Jid from() const = 0;
	virtual void setFrom(const XMPP::Jid &j) = 0;

	XMPP::Jid jid() const;
	void setJid(const XMPP::Jid &);

	bool originLocal() const;
	bool late() const;
	QDateTime timeStamp() const;

	void setOriginLocal(bool b);
	void setLate(bool b);
	void setTimeStamp(const QDateTime &t);

	PsiAccount *account() const;
	void setAccount(PsiAccount* account);

	virtual QDomElement toXml(QDomDocument *) const;
	virtual bool fromXml(PsiCon *, PsiAccount *, const QDomElement *);

	virtual int priority() const;

	virtual QString description() const;

	virtual PsiEvent *copy() const;

private:
	bool v_originLocal, v_late;
	QDateTime v_ts;
	XMPP::Jid v_jid;
	PsiAccount *v_account;
};


#ifdef PSI_PLUGINS
class PluginEvent : public PsiEvent
{
	Q_OBJECT
public:
	PluginEvent(int account, const QString& jid, const QString& descr, PsiAccount *acc);
	~PluginEvent();

	typedef QSharedPointer<PluginEvent> Ptr;
	int type() const;
	XMPP::Jid from() const;
	virtual void setFrom(const XMPP::Jid &j);
	virtual QString description() const;
	void activate();

signals:
	void activated(const QString &jid, int account);

private:
	XMPP::Jid from_;
	QString descr_;
	int _account;
};
#endif


// normal, chat, error, headline, etc
class MessageEvent : public PsiEvent
{
	Q_OBJECT
public:
	MessageEvent(PsiAccount *acc);
	MessageEvent(const MessageEvent &from);
	MessageEvent(const XMPP::Message &, PsiAccount *acc);
	~MessageEvent();

	typedef QSharedPointer<MessageEvent> Ptr;
	int type() const;
	XMPP::Jid from() const;
	void setFrom(const XMPP::Jid &j);

	const QString& nick() const;
	void setNick(const QString&);

	bool sentToChatWindow() const;
	const XMPP::Message & message() const;

	void setSentToChatWindow(bool b);
	void setMessage(const XMPP::Message &m);

	QDomElement toXml(QDomDocument *) const;
	bool fromXml(PsiCon *, PsiAccount *, const QDomElement *);

	virtual int priority() const;

	virtual QString description() const;

	virtual PsiEvent *copy() const;

private:
	XMPP::Message v_m;
	bool v_sentToChatWindow;
};

// subscribe, subscribed, unsubscribe, unsubscribed
class AuthEvent : public PsiEvent
{
	Q_OBJECT
public:
	AuthEvent(const XMPP::Jid &j, const QString &authType, PsiAccount *acc);
	AuthEvent(const AuthEvent &from);
	~AuthEvent();

	typedef QSharedPointer<AuthEvent> Ptr;
	int type() const;
	XMPP::Jid from() const;
	void setFrom(const XMPP::Jid &j);

	const QString& nick() const;
	void setNick(const QString&);

	QString authType() const;

	QDomElement toXml(QDomDocument *) const;
	bool fromXml(PsiCon *, PsiAccount *, const QDomElement *);

	virtual int priority() const;

	virtual QString description() const;

	virtual PsiEvent *copy() const;

	void setSentToChatWindow(bool b);
	bool sentToChatWindow() const;

private:
	XMPP::Jid v_from;
	QString v_nick;
	QString v_at;
	bool v_sentToChatWindow;
};

#ifdef WHITEBOARDING
class SxeEvent : public MessageEvent
{
	Q_OBJECT
public:
	SxeEvent(int id, PsiAccount *acc)
		: MessageEvent(acc), id_(id) {}
	SxeEvent(const SxeEvent &from)
		: MessageEvent(from), id_(from.id()) {}
	~SxeEvent() {}
	typedef QSharedPointer<SxeEvent> Ptr;
	int type() const { return Sxe; }
//	XMPP::Jid from() const { return jid(); }
//	void setFrom(const XMPP::Jid &) {  }
	int id() const { return id_; }

private:
	int id_;
};
#endif

// request pgp passphrase
class PGPEvent : public PsiEvent
{
	Q_OBJECT
public:
	PGPEvent(PsiAccount *acc) : PsiEvent(acc) {}
	PGPEvent(const PGPEvent &from)
	: PsiEvent(from) {}
	~PGPEvent() {}
	typedef QSharedPointer<PGPEvent> Ptr;
	int type() const { return PGP; }
	XMPP::Jid from() const { return QString(); }
	void setFrom(const XMPP::Jid &) {}
};

// incoming file transfer
class FileEvent : public PsiEvent
{
	Q_OBJECT
public:
	FileEvent(const XMPP::Jid &j, XMPP::FileTransfer *ft, PsiAccount *acc);
	FileEvent(const FileEvent &from);
	~FileEvent();

	typedef QSharedPointer<FileEvent> Ptr;
	int type() const { return File; }
	XMPP::Jid from() const;
	void setFrom(const XMPP::Jid &);
	XMPP::FileTransfer *takeFileTransfer();

	virtual int priority() const;

	virtual QString description() const;

	virtual PsiEvent *copy() const;

private:
	XMPP::Jid v_from;
	QPointer<XMPP::FileTransfer> ft;
};

// roster item exchange event
class RosterExchangeEvent : public PsiEvent
{
	Q_OBJECT
public:
	RosterExchangeEvent(const XMPP::Jid &j, const XMPP::RosterExchangeItems& i, const QString& body, PsiAccount *acc);

	typedef QSharedPointer<RosterExchangeEvent> Ptr;
	int type() const { return RosterExchange; }
	XMPP::Jid from() const;
	void setFrom(const XMPP::Jid &);
	const XMPP::RosterExchangeItems& rosterExchangeItems() const;
	void setRosterExchangeItems(const XMPP::RosterExchangeItems&);
	const QString& text() const;
	void setText(const QString& text);

	virtual int priority() const;

	virtual QString description() const;

private:
	XMPP::Jid v_from;
	XMPP::RosterExchangeItems v_items;
	QString v_text;
};

/*class StatusEvent : public PsiEvent
{
	Q_OBJECT
public:
	StatusEvent(const XMPP::Jid &j, const XMPP::Status& i, PsiAccount *acc);

	int type() const { return Status; }
	XMPP::Jid from() const;
	void setFrom(const XMPP::Jid &);
	const XMPP::Status& status() const;
	void setStatus(const XMPP::Status&);

	virtual int priority() const;

private:
	XMPP::Jid v_from;
	XMPP::Status v_status;
};*/

// http auth
class HttpAuthEvent : public MessageEvent
{
	Q_OBJECT
public:
	HttpAuthEvent(const PsiHttpAuthRequest &req, PsiAccount *acc);
	~HttpAuthEvent();

	typedef QSharedPointer<HttpAuthEvent> Ptr;
	int type() const { return HttpAuth; }

	const PsiHttpAuthRequest & request() { return v_req; }

	virtual QString description() const;

private:
	PsiHttpAuthRequest v_req;

};

// incoming avcall
class AvCallEvent : public PsiEvent
{
	Q_OBJECT
public:
	AvCallEvent(const XMPP::Jid &j, AvCall *sess, PsiAccount *acc);
	AvCallEvent(const AvCallEvent &from);
	~AvCallEvent();

	typedef QSharedPointer<AvCallEvent> Ptr;
	int type() const { return AvCallType; }
	XMPP::Jid from() const;
	void setFrom(const XMPP::Jid &);
	AvCall *takeAvCall();

	virtual int priority() const;

	virtual QString description() const;

	virtual PsiEvent *copy() const;

private:
	XMPP::Jid v_from;
	QPointer<AvCall> sess;
};

class EventItem
{
public:
	EventItem(const PsiEvent::Ptr &_e);
	EventItem(const EventItem &from);
	~EventItem();
	int id() const;
	PsiEvent::Ptr event() const;

private:
	PsiEvent::Ptr e;
	int v_id;
};

// event queue
class EventQueue : public QObject
{
	Q_OBJECT
public:
	EventQueue(PsiAccount *);
	EventQueue(const EventQueue &);
	~EventQueue();

	EventQueue &operator= (const EventQueue &);

	bool enabled() const;
	void setEnabled(bool enabled);

	int nextId() const;
	int count() const;
	int contactCount() const;
	int count(const XMPP::Jid &, bool compareRes=true) const;
	void enqueue(const PsiEvent::Ptr &);
	void dequeue(const PsiEvent::Ptr &);
	PsiEvent::Ptr dequeue(const XMPP::Jid &, bool compareRes=true);
	PsiEvent::Ptr peek(const XMPP::Jid &, bool compareRes=true) const;
	PsiEvent::Ptr dequeueNext();
	PsiEvent::Ptr peekNext() const;
	bool hasChats(const XMPP::Jid &, bool compareRes=true) const;
	PsiEvent::Ptr peekFirstChat(const XMPP::Jid &, bool compareRes=true) const;
	void extractByType(int type, QList<PsiEvent::Ptr> *el);
	void extractMessages(QList<PsiEvent::Ptr> *list);
	void extractChats(QList<PsiEvent::Ptr> *list, const XMPP::Jid &, bool compareRes, bool removeEvents);
	void extractByJid(QList<PsiEvent::Ptr> *list, const XMPP::Jid &jid);
	void printContent() const;
	void clear();
	void clear(const XMPP::Jid &, bool compareRes=true);
	typedef QPair<int, PsiEvent::Ptr> PsiEventId;
	QList<PsiEventId> eventsFor(const XMPP::Jid& jid, bool compareRes=true);

	QDomElement toXml(QDomDocument *) const; // these work with pointers, to save inclusion of qdom.h, which is pretty large
	bool fromXml(const QDomElement *);

	bool toFile(const QString &fname);
	bool fromFile(const QString &fname);

signals:
	void eventFromXml(const PsiEvent::Ptr &);
	void queueChanged();

private:
	QList<EventItem*> list_;
	PsiCon* psi_;
	PsiAccount* account_;
	bool enabled_;
};



#endif
