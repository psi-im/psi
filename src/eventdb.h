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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef EVENTDB_H
#define EVENTDB_H

#include <QObject>
#include <QTimer>
#include <QFile>

#include "xmpp_jid.h"

class PsiEvent;

class EDBItem
{
public:
	EDBItem(PsiEvent *, const QString &id, const QString &nextId, const QString &prevId);
	~EDBItem();

	PsiEvent *event() const;
	const QString & id() const;
	const QString & nextId() const;
	const QString & prevId() const;

private:
	QString v_id, v_prevId, v_nextId;
	PsiEvent *e;
};

class EDBResult : public QList<EDBItem*>
{
public:
	EDBResult()
		: autoDelete_(false)
	{}

	~EDBResult()
	{
		if (autoDelete_) {
			qDeleteAll(*this);
		}
	}

	void setAutoDelete(bool autoDelete)
	{
		autoDelete_ = autoDelete;
	}

private:
	bool autoDelete_;
};

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
	void find(const QString &, const XMPP::Jid &, const QString &id, int direction);
	void append(const XMPP::Jid &, PsiEvent *);
	void erase(const XMPP::Jid &);

	bool busy() const;
	const EDBResult *result() const;
	bool writeSuccess() const;
	int lastRequestType() const;

signals:
	void finished();

private:
	class Private;
	Private *d;

	friend class EDB;
	void edb_resultReady(EDBResult *);
	void edb_writeFinished(bool);
	int listeningFor() const;
};

class EDB : public QObject
{
	Q_OBJECT
public:
	enum { Forward, Backward };
	EDB();
	virtual ~EDB()=0;

protected:
	int genUniqueId() const;
	virtual int getLatest(const XMPP::Jid &, int len)=0;
	virtual int getOldest(const XMPP::Jid &, int len)=0;
	virtual int get(const XMPP::Jid &jid, const QString &id, int direction, int len)=0;
	virtual int append(const XMPP::Jid &, PsiEvent *)=0;
	virtual int find(const QString &, const XMPP::Jid &, const QString &id, int direction)=0;
	virtual int erase(const XMPP::Jid &)=0;
	void resultReady(int, EDBResult *);
	void writeFinished(int, bool);

private:
	class Private;
	Private *d;

	friend class EDBHandle;
	void reg(EDBHandle *);
	void unreg(EDBHandle *);

	int op_getLatest(const XMPP::Jid &, int len);
	int op_getOldest(const XMPP::Jid &, int len);
	int op_get(const XMPP::Jid &, const QString &id, int direction, int len);
	int op_find(const QString &, const XMPP::Jid &, const QString &id, int direction);
	int op_append(const XMPP::Jid &, PsiEvent *);
	int op_erase(const XMPP::Jid &);
};

class EDBFlatFile : public EDB
{
	Q_OBJECT
public:
	EDBFlatFile();
	~EDBFlatFile();

	int getLatest(const XMPP::Jid &, int len);
	int getOldest(const XMPP::Jid &, int len);
	int get(const XMPP::Jid &jid, const QString &id, int direction, int len);
	int find(const QString &, const XMPP::Jid &, const QString &id, int direction);
	int append(const XMPP::Jid &, PsiEvent *);
	int erase(const XMPP::Jid &);

	class File;

private slots:
	void performRequests();
	void file_timeout();

private:
	class Private;
	Private *d;

	File *findFile(const XMPP::Jid &) const;
	File *ensureFile(const XMPP::Jid &);
	bool deleteFile(const XMPP::Jid &);
};

class EDBFlatFile::File : public QObject
{
	Q_OBJECT
public:
	File(const XMPP::Jid &_j);
	~File();

	int total() const;
	void touch();
	PsiEvent *get(int);
	bool append(PsiEvent *);

	static QString jidToFileName(const XMPP::Jid &);

signals:
	void timeout();

private slots:
	void timer_timeout();

public:
	XMPP::Jid j;
	QString fname;
	QFile f;
	bool valid;
	QTimer *t;

	class Private;
	Private *d;

private:
	PsiEvent *lineToEvent(const QString &);
	QString eventToLine(PsiEvent *);
	void ensureIndex();
};

#endif
