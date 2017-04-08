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
	EDBFlatFile();
	~EDBFlatFile();

	int getLatest(const XMPP::Jid &, int len);
	int getOldest(const XMPP::Jid &, int len);
	int get(const XMPP::Jid &jid, const QString &id, int direction, int len);
	int getByDate(const XMPP::Jid &jid, QDateTime first, QDateTime last);
	int find(const QString &, const XMPP::Jid &, const QString &id, int direction);
	int append(const XMPP::Jid &, const PsiEvent::Ptr&);
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
	PsiEvent::Ptr get(int);
	bool append(const PsiEvent::Ptr &);

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
	PsiEvent::Ptr lineToEvent(const QString &);
	QString eventToLine(const PsiEvent::Ptr&);
	void ensureIndex();
};

#endif // EDBFLATFILE_H
