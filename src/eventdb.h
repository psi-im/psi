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
	EDBItem(const PsiEvent::Ptr &, const QString &id);
	~EDBItem();

	PsiEvent::Ptr event() const;
	const QString & id() const;

private:
	QString v_id;
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
	void get(const QString &accId, const XMPP::Jid &jid, const QDateTime date, int direction, int begin, int len);
	void find(const QString &accId, const QString &, const XMPP::Jid &, const QDateTime date, int direction);
	void append(const QString &accId, const XMPP::Jid &, const PsiEvent::Ptr &, int);
	void erase(const QString &accId, const XMPP::Jid &);

	bool busy() const;
	const EDBResult result() const;
	bool writeSuccess() const;
	int lastRequestType() const;
	int beginRow() const;

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
	virtual quint64 eventsCount(const QString &accId, const XMPP::Jid &jid) = 0;
	virtual QString getStorageParam(const QString &key) = 0;
	virtual void setStorageParam(const QString &key, const QString &val) = 0;

protected:
	int genUniqueId() const;
	virtual int get(const QString &accId, const XMPP::Jid &jid, const QDateTime date, int direction, int start, int len)=0;
	virtual int append(const QString &accId, const XMPP::Jid &, const PsiEvent::Ptr &, int)=0;
	virtual int find(const QString &accId, const QString &, const XMPP::Jid &, const QDateTime date, int direction)=0;
	virtual int erase(const QString &accId, const XMPP::Jid &)=0;
	void resultReady(int, EDBResult, int);
	void writeFinished(int, bool);
	PsiCon *psi();

private:
	class Private;
	Private *d;

	friend class EDBHandle;
	void reg(EDBHandle *);
	void unreg(EDBHandle *);

	int op_get(const QString &accId, const XMPP::Jid &, const QDateTime date, int direction, int start, int len);
	int op_find(const QString &accId, const QString &, const XMPP::Jid &, const QDateTime date, int direction);
	int op_append(const QString &accId, const XMPP::Jid &, const PsiEvent::Ptr &, int);
	int op_erase(const QString &accId, const XMPP::Jid &);
};

#endif
