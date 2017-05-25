/*
 * eventdb.h - asynchronous I/O event database
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

#ifndef EVENTDB_H
#define EVENTDB_H

#include <QObject>
#include <QTimer>
#include <QFile>
#include <QSharedPointer>
#include <QDateTime>

#include "xmpp_jid.h"
#include "psievent.h"
#include "psicon.h"

class EDBItem
{
public:
	EDBItem(const PsiEvent::Ptr &, const QString &id, const QString &nextId, const QString &prevId);
	~EDBItem();

	PsiEvent::Ptr event() const;
	const QString & id() const;
	const QString & nextId() const;
	const QString & prevId() const;

private:
	QString v_id, v_prevId, v_nextId;
	PsiEvent::Ptr e;
};

typedef QSharedPointer<EDBItem> EDBItemPtr;
typedef QList<EDBItemPtr> EDBResult;

class EDB;
class EDBHandle : public QObject
{
	Q_OBJECT
public:
	enum { Read, Write, Erase };
	EDBHandle(EDB *);
	~EDBHandle();

	// operations
	void getLatest(const XMPP::Jid &, int len);
	void getOldest(const XMPP::Jid &, int len);
	void get(const XMPP::Jid &jid, const QString &id, int direction, int len);
	void getByDate(const XMPP::Jid &jid, QDateTime first, QDateTime last);
	void find(const QString &, const XMPP::Jid &, const QString &id, int direction);
	void append(const XMPP::Jid &, const PsiEvent::Ptr &);
	void erase(const XMPP::Jid &);

	bool busy() const;
	const EDBResult result() const;
	bool writeSuccess() const;
	int lastRequestType() const;

signals:
	void finished();

private:
	class Private;
	Private *d;

	friend class EDB;
	void edb_resultReady(EDBResult);
	void edb_writeFinished(bool);
	int listeningFor() const;
};

class EDB : public QObject
{
	Q_OBJECT
public:
	enum { Forward, Backward };
	enum { Contact = 1, GroupChatContact = 2 };
	enum { SeparateAccounts = 1, PrivateContacts = 2, AllContacts = 4, AllAccounts = 8 };
	struct ContactItem
	{
		QString   accId;
		XMPP::Jid jid;
		ContactItem(const QString &aId, XMPP::Jid j) { accId = aId; jid = j; }
	};

	EDB(PsiCon *psi);
	virtual ~EDB()=0;
	virtual int features() const = 0;
	virtual QList<ContactItem> contacts(const QString &accId, int type) = 0;

protected:
	int genUniqueId() const;
	virtual int getLatest(const XMPP::Jid &, int len)=0;
	virtual int getOldest(const XMPP::Jid &, int len)=0;
	virtual int get(const XMPP::Jid &jid, const QString &id, int direction, int len)=0;
	virtual int getByDate(const XMPP::Jid &jid, QDateTime first, QDateTime last) = 0;
	virtual int append(const XMPP::Jid &, const PsiEvent::Ptr &)=0;
	virtual int find(const QString &, const XMPP::Jid &, const QString &id, int direction)=0;
	virtual int erase(const XMPP::Jid &)=0;
	void resultReady(int, EDBResult);
	void writeFinished(int, bool);
	PsiCon *psi();

private:
	class Private;
	Private *d;

	friend class EDBHandle;
	void reg(EDBHandle *);
	void unreg(EDBHandle *);

	int op_getLatest(const XMPP::Jid &, int len);
	int op_getOldest(const XMPP::Jid &, int len);
	int op_get(const XMPP::Jid &, const QString &id, int direction, int len);
	int op_getByDate(const XMPP::Jid &jid, QDateTime first, QDateTime last);
	int op_find(const QString &, const XMPP::Jid &, const QString &id, int direction);
	int op_append(const XMPP::Jid &, const PsiEvent::Ptr&);
	int op_erase(const XMPP::Jid &);
};

#endif
