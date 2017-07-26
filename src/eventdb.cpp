/*
 * eventdb.cpp - asynchronous I/O event database
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

#include "eventdb.h"

#include <QVector>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QTextStream>
#include <QDateTime>

#include "psievent.h"
#include "jidutil.h"

using namespace XMPP;

//----------------------------------------------------------------------------
// EDBItem
//----------------------------------------------------------------------------
EDBItem::EDBItem(const PsiEvent::Ptr &event, const QString &id)
{
	e = event;
	v_id = id;
}

EDBItem::~EDBItem()
{
}

PsiEvent::Ptr EDBItem::event() const
{
	return e;
}

const QString & EDBItem::id() const
{
	return v_id;
}


//----------------------------------------------------------------------------
// EDBHandle
//----------------------------------------------------------------------------
class EDBHandle::Private
{
public:
	Private() {}

	EDB *edb;
	int beginRow_;
	EDBResult r;
	bool busy;
	bool writeSuccess;
	int listeningFor;
	int lastRequestType;
};

EDBHandle::EDBHandle(EDB *edb)
:QObject(0)
{
	d = new Private;
	d->edb = edb;
	d->beginRow_ = 0;
	d->busy = false;
	d->writeSuccess = false;
	d->listeningFor = -1;
	d->lastRequestType = Read;

	d->edb->reg(this);
}

EDBHandle::~EDBHandle()
{
	d->edb->unreg(this);

	delete d;
}

void EDBHandle::get(const QString &accId, const XMPP::Jid &jid, const QDateTime date, int direction, int begin, int len)
{
	d->busy = true;
	d->lastRequestType = Read;
	d->listeningFor = d->edb->op_get(accId, jid, date, direction, begin, len);
}

void EDBHandle::find(const QString &accId, const QString &str, const XMPP::Jid &jid, const QDateTime date, int direction)
{
	d->busy = true;
	d->lastRequestType = Read;
	d->listeningFor = d->edb->op_find(accId, str, jid, date, direction);
}

void EDBHandle::append(const QString &accId, const Jid &j, const PsiEvent::Ptr &e, int type)
{
	d->busy = true;
	d->lastRequestType = Write;
	d->listeningFor = d->edb->op_append(accId, j, e, type);
}

void EDBHandle::erase(const QString &accId, const Jid &j)
{
	d->busy = true;
	d->lastRequestType = Erase;
	d->listeningFor = d->edb->op_erase(accId, j);
}

bool EDBHandle::busy() const
{
	return d->busy;
}

const EDBResult EDBHandle::result() const
{
	return d->r;
}

bool EDBHandle::writeSuccess() const
{
	return d->writeSuccess;
}

void EDBHandle::edb_resultReady(EDBResult r)
{
	d->busy = false;
	d->r = r;
	d->listeningFor = -1;
	finished();
}

void EDBHandle::edb_writeFinished(bool b)
{
	d->busy = false;
	d->writeSuccess = b;
	d->listeningFor = -1;
	finished();
}

int EDBHandle::listeningFor() const
{
	return d->listeningFor;
}

int EDBHandle::lastRequestType() const
{
	return d->lastRequestType;
}

int EDBHandle::beginRow() const
{
	return d->beginRow_;
}


//----------------------------------------------------------------------------
// EDB
//----------------------------------------------------------------------------
class EDB::Private
{
public:
	Private() {}

	QList<EDBHandle*> list;
	int reqid_base;
	PsiCon *psi;
};

EDB::EDB(PsiCon *psi)
{
	d = new Private;
	d->reqid_base = 0;
	d->psi = psi;
}

EDB::~EDB()
{
	qDeleteAll(d->list);
	d->list.clear();
	delete d;
}

int EDB::genUniqueId() const
{
	return d->reqid_base++;
}

void EDB::reg(EDBHandle *h)
{
	d->list.append(h);
}

void EDB::unreg(EDBHandle *h)
{
	d->list.removeAll(h);
}

int EDB::op_get(const QString &accId, const Jid &jid, const QDateTime date, int direction, int start, int len)
{
	return get(accId, jid, date, direction, start, len);
}

int EDB::op_find(const QString &accId, const QString &str, const Jid &j, const QDateTime date, int direction)
{
	return find(accId, str, j, date, direction);
}

int EDB::op_append(const QString &accId, const Jid &j, const PsiEvent::Ptr &e, int type)
{
	return append(accId, j, e, type);
}

int EDB::op_erase(const QString &accId, const Jid &j)
{
	return erase(accId, j);
}

void EDB::resultReady(int req, EDBResult r, int begin_row)
{
	// deliver
	foreach(EDBHandle* h, d->list) {
		if(h->listeningFor() == req) {
			h->d->beginRow_ = begin_row;
			h->edb_resultReady(r);
			return;
		}
	}
}

void EDB::writeFinished(int req, bool b)
{
	// deliver
	foreach(EDBHandle* h, d->list) {
		if(h->listeningFor() == req) {
			h->edb_writeFinished(b);
			return;
		}
	}
}

PsiCon *EDB::psi()
{
	return d->psi;
}
