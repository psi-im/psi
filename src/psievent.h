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

#ifndef PSIEVENT_H
#define PSIEVENT_H

#include <QList>
#include <QDateTime>
#include <QObject>
#include <QDomDocument>
#include <QDomElement>
#include <QPointer>

#include "xmpp.h"
#include "im.h"
#include "psihttpauthrequest.h"

namespace XMPP
{
	class FileTransfer;
};

class PsiCon;
class PsiAccount;


class PsiEvent : public QObject
{
	Q_OBJECT
public:
	PsiEvent(PsiAccount *);
	PsiEvent(const PsiEvent &);
	virtual ~PsiEvent() = 0;

	enum {
		Message,
		Auth,
		PGP,
		File,
		RosterExchange,
		//Status
		HttpAuth
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

	virtual QDomElement toXml(QDomDocument *) const;
	virtual bool fromXml(PsiCon *, PsiAccount *, const QDomElement *);

	virtual int priority() const;

	virtual PsiEvent *copy() const;

private:
	bool v_originLocal, v_late;
	QDateTime v_ts;
	XMPP::Jid v_jid;
	PsiAccount *v_account;
};

// normal, chat, error, headline, etc
class MessageEvent : public PsiEvent
{
	Q_OBJECT
public:
	MessageEvent(PsiAccount *acc);
	MessageEvent(const MessageEvent &from);
	MessageEvent(const XMPP::Message &, PsiAccount *acc);
	~MessageEvent();

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

	int type() const;
	XMPP::Jid from() const;
	void setFrom(const XMPP::Jid &j);

	const QString& nick() const;
	void setNick(const QString&);

	QString authType() const;

	QDomElement toXml(QDomDocument *) const;
	bool fromXml(PsiCon *, PsiAccount *, const QDomElement *);

	virtual int priority() const;

	virtual PsiEvent *copy() const;

private:
	XMPP::Jid v_from;
	QString v_nick;
	QString v_at;
};

// request pgp passphrase
class PGPEvent : public PsiEvent
{
	Q_OBJECT
public:
	PGPEvent(PsiAccount *acc) : PsiEvent(acc) {}
	PGPEvent(const PGPEvent &from)
	: PsiEvent(from) {}
	~PGPEvent() {}
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
	~FileEvent();

	int type() const { return File; }
	XMPP::Jid from() const;
	void setFrom(const XMPP::Jid &);
	XMPP::FileTransfer *takeFileTransfer();

	virtual int priority() const;

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

	int type() const { return RosterExchange; }
	XMPP::Jid from() const;
	void setFrom(const XMPP::Jid &);
	const XMPP::RosterExchangeItems& rosterExchangeItems() const;
	void setRosterExchangeItems(const XMPP::RosterExchangeItems&);
	const QString& text() const;
	void setText(const QString& text);
	
	virtual int priority() const;

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

	int type() const { return HttpAuth; }

	const PsiHttpAuthRequest & request() { return v_req; }

private:
	PsiHttpAuthRequest v_req;

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

	int nextId() const;
	int count() const;
	int count(const XMPP::Jid &, bool compareRes=true) const;
	void enqueue(PsiEvent *);
	void dequeue(PsiEvent *);
	PsiEvent *dequeue(const XMPP::Jid &, bool compareRes=true);
	PsiEvent *peek(const XMPP::Jid &, bool compareRes=true) const;
	PsiEvent *dequeueNext();
	PsiEvent *peekNext() const;
	bool hasChats(const XMPP::Jid &, bool compareRes=true) const;
	PsiEvent *peekFirstChat(const XMPP::Jid &, bool compareRes=true) const;
	void extractMessages(QList<PsiEvent*> *list);
	void extractChats(QList<PsiEvent*> *list, const XMPP::Jid &, bool compareRes=true);
	void printContent() const;
	void clear();
	void clear(const XMPP::Jid &, bool compareRes=true);

	QDomElement toXml(QDomDocument *) const; // these work with pointers, to save inclusion of qdom.h, which is pretty large
	bool fromXml(const QDomElement *);

	bool toFile(const QString &fname);
	bool fromFile(const QString &fname);

signals:
	void handleEvent(PsiEvent *);
	void queueChanged();

private:
	class Private;
	Private *d;
};



#endif
