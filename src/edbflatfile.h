/*
 * edbflatfile.h - asynchronous I/O event database
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

#ifndef EDBFLATFILE_H
#define EDBFLATFILE_H

#include <QObject>
#include <QTimer>
#include <QFile>
#include <QDateTime>

#include "eventdb.h"
#include "xmpp_jid.h"
#include "psievent.h"

class EDBFlatFile : public EDB
{
	Q_OBJECT
public:
	EDBFlatFile(PsiCon *psi);
	~EDBFlatFile();

	int features() const;
	int get(const QString &accId, const XMPP::Jid &jid, const QDateTime date, int direction, int start, int len);
	int find(const QString &accId, const QString &, const XMPP::Jid &, const QDateTime date, int direction);
	int append(const QString &accId, const XMPP::Jid &, const PsiEvent::Ptr &, int);
	int erase(const QString &accId, const XMPP::Jid &);
	QList<EDB::ContactItem> contacts(const QString &accId, int type);
	quint64 eventsCount(const QString &accId, const XMPP::Jid &jid);
	QString getStorageParam(const QString &) { return QString(); }
	void setStorageParam(const QString &, const QString &) {}

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
	int getId(QDateTime &date, int dir, int offset);
	void touch();
	PsiEvent::Ptr get(int);
	bool append(const PsiEvent::Ptr &);
	int findNearestDate(const QDateTime &date);

	static QString jidToFileName(const XMPP::Jid &);
	static QString strToFileName(const QString &s);
	static QList<EDB::ContactItem> contacts(const QString &accId, int type);

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
	PsiEvent::Ptr lineToEvent(const QString &);
	QString eventToLine(const PsiEvent::Ptr&);
	void ensureIndex();
	QString getLine(int id);
	QDateTime getDate(int id);
};

#endif // EDBFLATFILE_H
