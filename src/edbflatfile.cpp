/*
 * edbflatfile.cpp - asynchronous I/O event database
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

#include <QVector>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QTextStream>
#include <QDateTime>

#include "edbflatfile.h"
#include "xmpp_jid.h"
#include "jidutil.h"
#include "common.h"
#include "applicationinfo.h"

#define FAKEDELAY 0

using namespace XMPP;

//----------------------------------------------------------------------------
// EDBFlatFile
//----------------------------------------------------------------------------
struct item_file_req
{
	Jid j;
	int type; // 0 = latest, 1 = oldest, 2 = random, 3 = write
	int len;
	int dir;
	int id;
	int eventId;
	QString findStr;
	PsiEvent::Ptr event;

	QDateTime first, last;
	enum Type {
		Type_getLatest = 0,
		Type_getOldest,
		Type_get,
		Type_append,
		Type_find,
		Type_getByDate,
		Type_erase
	};
};

class EDBFlatFile::Private
{
public:
	Private() {}

	QList<File*> flist;
	QList<item_file_req*> rlist;
};

EDBFlatFile::EDBFlatFile()
:EDB()
{
	d = new Private;
}

EDBFlatFile::~EDBFlatFile()
{
	qDeleteAll(d->rlist);
	qDeleteAll(d->flist);
	d->flist.clear();

	delete d;
}

int EDBFlatFile::getLatest(const Jid &j, int len)
{
	item_file_req *r = new item_file_req;
	r->j = j;
	r->type = item_file_req::Type_getLatest;
	r->len = len < 1 ? 1: len;
	r->id = genUniqueId();
	d->rlist.append(r);

	QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));
	return r->id;
}

int EDBFlatFile::getOldest(const Jid &j, int len)
{
	item_file_req *r = new item_file_req;
	r->j = j;
	r->type = item_file_req::Type_getOldest;
	r->len = len < 1 ? 1: len;
	r->id = genUniqueId();
	d->rlist.append(r);

	QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));
	return r->id;
}

int EDBFlatFile::get(const Jid &j, const QString &id, int direction, int len)
{
	item_file_req *r = new item_file_req;
	r->j = j;
	r->type = item_file_req::Type_get;
	r->len = len < 1 ? 1: len;
	r->dir = direction;
	r->eventId = id.toInt();
	r->id = genUniqueId();
	d->rlist.append(r);

	QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));
	return r->id;
}


int EDBFlatFile::getByDate(const XMPP::Jid &jid, QDateTime first, QDateTime last)
{
	item_file_req *r = new item_file_req;
	r->j = jid;
	r->type = item_file_req::Type_getByDate;
	r->len = 1;
	r->id = genUniqueId();
	d->rlist.append(r);
	r->first = first;
	r->last = last;

	QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));
	return r->id;
}

int EDBFlatFile::find(const QString &str, const Jid &j, const QString &id, int direction)
{
	item_file_req *r = new item_file_req;
	r->j = j;
	r->type = item_file_req::Type_find;
	r->len = 1;
	r->dir = direction;
	r->findStr = str;
	r->eventId = id.toInt();
	r->id = genUniqueId();
	d->rlist.append(r);

	QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));
	return r->id;
}

int EDBFlatFile::append(const Jid &j, const PsiEvent::Ptr &e)
{
	item_file_req *r = new item_file_req;
	r->j = j;
	r->type = item_file_req::Type_append;
	r->event = e;
	if ( !r->event ) {
		qWarning("EDBFlatFile::append(): Attempted to append incompatible type.");
		delete r;
		return 0;
	}
	r->id = genUniqueId();
	d->rlist.append(r);

	QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));
	return r->id;
}

int EDBFlatFile::erase(const Jid &j)
{
	item_file_req *r = new item_file_req;
	r->j = j;
	r->type = item_file_req::Type_erase;
	r->id = genUniqueId();
	d->rlist.append(r);

	QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));
	return r->id;
}

EDBFlatFile::File *EDBFlatFile::findFile(const Jid &j) const
{
	foreach(File* i, d->flist) {
		if(i->j.compare(j, false))
			return i;
	}
	return 0;
}

EDBFlatFile::File *EDBFlatFile::ensureFile(const Jid &j)
{
	File *i = findFile(j);
	if(!i) {
		i = new File(Jid(j.bare()));
		connect(i, SIGNAL(timeout()), SLOT(file_timeout()));
		d->flist.append(i);
	}
	return i;
}

bool EDBFlatFile::deleteFile(const Jid &j)
{
	File *i = findFile(j);

	QString fname;

	if (i) {
		fname = i->fname;
		d->flist.removeAll(i);
		delete i;
	}
	else {
		fname = File::jidToFileName(j);
	}

	QFileInfo fi(fname);
	if(fi.exists()) {
		QDir dir = fi.dir();
		return dir.remove(fi.fileName());
	}
	else
		return true;
}

void EDBFlatFile::performRequests()
{
	if(d->rlist.isEmpty())
		return;

	item_file_req *r = d->rlist.takeFirst();

	File *f = ensureFile(r->j);
	int type = r->type;
	if(type >= item_file_req::Type_getLatest && type <= item_file_req::Type_get) {
		int id, direction;

		if(type == item_file_req::Type_getLatest) {
			direction = Backward;
			id = f->total()-1;
		}
		else if(type == item_file_req::Type_getOldest) {
			direction = Forward;
			id = 0;
		}
		else if(type == item_file_req::Type_get) {
			direction = r->dir;
			id = r->eventId;
		}
		else {
			qWarning("EDBFlatFile::performRequests(): Invalid type.");
			return;
		}

		int len;
		if(direction == Forward) {
			if(id + r->len > f->total())
				len = f->total() - id;
			else
				len = r->len;
		}
		else {
			if((id+1) - r->len < 0)
				len = id+1;
			else
				len = r->len;
		}

		EDBResult result;
		for(int n = 0; n < len; ++n) {
			PsiEvent::Ptr e(f->get(id));
			if(e) {
				QString prevId, nextId;
				if(id > 0)
					prevId = QString::number(id-1);
				if(id < f->total()-1)
					nextId = QString::number(id+1);
				EDBItemPtr ei = EDBItemPtr(new EDBItem(e, QString::number(id), prevId, nextId));
				result.append(ei);
			}

			if(direction == Forward)
				++id;
			else
				--id;
		}
		resultReady(r->id, result);
	}
	else if(type == item_file_req::Type_append) {
		writeFinished(r->id, f->append(r->event));
	}
	else if(type == item_file_req::Type_find) {
		int id = r->eventId;
		EDBResult result;
		while(1) {
			PsiEvent::Ptr e(f->get(id));
			if(!e)
				break;

			QString prevId, nextId;
			if(id > 0)
				prevId = QString::number(id-1);
			if(id < f->total()-1)
				nextId = QString::number(id+1);

			if(e->type() == PsiEvent::Message) {
				MessageEvent::Ptr me = e.staticCast<MessageEvent>();
				const Message &m = me->message();
				if(m.body().indexOf(r->findStr, 0, Qt::CaseInsensitive) != -1) {
					EDBItemPtr ei = EDBItemPtr(new EDBItem(e, QString::number(id), prevId, nextId));
					result.append(ei);
					//commented line below to return ALL(instead of just first) messages that contain findStr
					//break;
				}
			}

			if(r->dir == Forward)
				++id;
			else
				--id;
		}
		resultReady(r->id, result);
	}
	else if(type == item_file_req::Type_getByDate ) {
		EDBResult result;
		for (int id = f->findNearestDate(r->first); id < f->total(); ++id) {
			PsiEvent::Ptr e(f->get(id));
			if(!e)
				continue;

			QString prevId, nextId;
			if(id > 0)
				prevId = QString::number(id-1);
			if(id < f->total()-1)
				nextId = QString::number(id+1);

			if(e->type() == PsiEvent::Message) {
				MessageEvent::Ptr me = e.staticCast<MessageEvent>();
				const Message &m = me->message();
				if(m.timeStamp() > r->first) {
					if (m.timeStamp() >= r->last )
						break;
					EDBItemPtr ei = EDBItemPtr(new EDBItem(e, QString::number(id), prevId, nextId));
					result.append(ei);
				}
			}
		}
		resultReady(r->id, result);
	}

	else if(type == item_file_req::Type_erase) {
		writeFinished(r->id, deleteFile(f->j));
	}

	delete r;
}

void EDBFlatFile::file_timeout()
{
	File *i = (File *)sender();
	d->flist.removeAll(i);
	i->deleteLater();
}


//----------------------------------------------------------------------------
// EDBFlatFile::File
//----------------------------------------------------------------------------
class EDBFlatFile::File::Private
{
public:
	Private() {}

	QVector<quint64> index;
	bool indexed;
};

EDBFlatFile::File::File(const Jid &_j)
{
	d = new Private;
	d->indexed = false;

	j = _j;
	valid = false;
	t = new QTimer(this);
	connect(t, SIGNAL(timeout()), SLOT(timer_timeout()));

	//printf("[EDB opening -- %s]\n", j.full().latin1());
	fname = jidToFileName(_j);
	f.setFileName(fname);
	valid = f.open(QIODevice::ReadWrite);

	touch();
}

EDBFlatFile::File::~File()
{
	if(valid)
		f.close();
	//printf("[EDB closing -- %s]\n", j.full().latin1());

	delete d;
}

QString EDBFlatFile::File::jidToFileName(const XMPP::Jid &j)
{
	return ApplicationInfo::historyDir() + "/" + JIDUtil::encode(j.bare()).toLower() + ".history";
}

void EDBFlatFile::File::ensureIndex()
{
	if ( valid && !d->indexed ) {
		if (f.isSequential()) {
			qWarning("EDBFlatFile::File::ensureIndex(): Can't index sequential files.");
			return;
		}

		f.reset(); // go to beginning
		d->index.clear();

		//printf(" file: %s\n", fname.latin1());
		// build index
		while(1) {
			quint64 at = f.pos();

			// locate a newline
			bool found = false;
			char c;
			while (f.getChar(&c)) {
				if (c == '\n') {
					found = true;
					break;
				}
			}

			if(!found)
				break;

			int oldsize = d->index.size();
			d->index.resize(oldsize+1);
			d->index[oldsize] = at;
		}

		d->indexed = true;
	}
	else {
		//printf(" file: can't open\n");
	}

	//printf(" messages: %d\n\n", d->index.size());
}

int EDBFlatFile::File::total() const
{
	((EDBFlatFile::File *)this)->ensureIndex();
	return d->index.size();
}

/*
 * This method returns an index of a string with the event
 * which has the nearest date to the specified one.
 * Returned date may be earlier than that is passed as an argument.
 */
int EDBFlatFile::File::findNearestDate(const QDateTime &date)
{
	int cnt = total();
	if (cnt == 0)
		return 0;

	// Binary search algorithm
	int left  = 0;
	int right = cnt;
	while (right - left > 0) {
		int idx = left + (right - left) / 2;
		const QDateTime mid = getDate(idx);
		if (date <= mid)
			right = idx;
		else
			left = idx + 1;
	}
	// --
	if (right == cnt) // Specified date is later than the latest one in the history
		return cnt - 1;

	// Now `right` is pointing to the index with an identical or later date
	while (right > 0 && getDate(right - 1) == date) // in case of there are more than one identical date
		--right;
	if (right == 0)
		return 0;
	if (getDate(right - 1).secsTo(date) <= date.secsTo(getDate(right))) // compares with earlier one
		--right;
	return right;
}

void EDBFlatFile::File::touch()
{
	t->start(30000);
}

void EDBFlatFile::File::timer_timeout()
{
	timeout();
}

PsiEvent::Ptr EDBFlatFile::File::get(int id)
{
	QString line = getLine(id);
	if (line.isNull())
		return PsiEvent::Ptr();
	return lineToEvent(line);
}

bool EDBFlatFile::File::append(const PsiEvent::Ptr &e)
{
	touch();

	if(!valid)
		return false;

	QString line = eventToLine(e);
	if(line.isEmpty())
		return false;

	f.seek(f.size());
	quint64 at = f.pos();

	QTextStream t;
	t.setDevice(&f);
	t.setCodec("UTF-8");
	t << line << endl;
	f.flush();

	if ( d->indexed ) {
		int oldsize = d->index.size();
		d->index.resize(oldsize+1);
		d->index[oldsize] = at;
	}

	return true;
}

PsiEvent::Ptr EDBFlatFile::File::lineToEvent(const QString &line)
{
	// -- read the line --
	QString sTime, sType, sOrigin, sFlags, sText, sSubj, sUrl, sUrlDesc;
	int x1, x2;
	x1 = line.indexOf('|') + 1;

	x2 = line.indexOf('|', x1);
	sTime = line.mid(x1, x2-x1);
	x1 = x2 + 1;

	x2 = line.indexOf('|', x1);
	sType = line.mid(x1, x2-x1);
	x1 = x2 + 1;

	x2 = line.indexOf('|', x1);
	sOrigin = line.mid(x1, x2-x1);
	x1 = x2 + 1;

	x2 = line.indexOf('|', x1);
	sFlags = line.mid(x1, x2-x1);
	x1 = x2 + 1;

	// check for extra fields
	if(sFlags[1] != '-') {
		int subflags = QString(sFlags[1]).toInt(NULL,16);

		// have subject?
		if(subflags & 1) {
			x2 = line.indexOf('|', x1);
			sSubj = line.mid(x1, x2-x1);
			x1 = x2 + 1;
		}
		// have url?
		if(subflags & 2) {
			x2 = line.indexOf('|', x1);
			sUrl = line.mid(x1, x2-x1);
			x1 = x2 + 1;
			x2 = line.indexOf('|', x1);
			sUrlDesc = line.mid(x1, x2-x1);
			x1 = x2 + 1;
		}
	}

	// body text is last
	sText = line.mid(x1);

	// -- read end --

	int type = sType.toInt();
	if(type == 0 || type == 1 || type == 4 || type == 5) {
		Message m;
		m.setTimeStamp(QDateTime::fromString(sTime, Qt::ISODate));
		if(type == 1)
			m.setType("chat");
		else if(type == 4)
			m.setType("error");
		else if(type == 5)
			m.setType("headline");
		else
			m.setType("");

		bool originLocal = (sOrigin == "to") ? true: false;
		m.setFrom(j);
		if(sFlags[0] == 'N')
			m.setBody(logdecode(sText));
		else
			m.setBody(logdecode(QString::fromUtf8(sText.toLatin1())));
		m.setSubject(logdecode(sSubj));

		QString url = logdecode(sUrl);
		if(!url.isEmpty())
			m.urlAdd(Url(url, logdecode(sUrlDesc)));
		m.setSpooled(true);

		MessageEvent::Ptr me(new MessageEvent(m, 0));
		me->setOriginLocal(originLocal);

		return me.staticCast<PsiEvent>();
	}
	else if(type == 2 || type == 3 || type == 6 || type == 7 || type == 8) {
		QString subType = "subscribe";
		if(type == 2) {
			// stupid "system message" from Psi <= 0.8.6
			// try to figure out what kind it REALLY is based on the text
			if(sText == tr("<big>[System Message]</big><br>You are now authorized."))
				subType = "subscribed";
			else if(sText == tr("<big>[System Message]</big><br>Your authorization has been removed!"))
				subType = "unsubscribed";
		}
		else if(type == 3)
			subType = "subscribe";
		else if(type == 6)
			subType = "subscribed";
		else if(type == 7)
			subType = "unsubscribe";
		else if(type == 8)
			subType = "unsubscribed";

		AuthEvent::Ptr ae(new AuthEvent(j, subType, 0));
		ae->setTimeStamp(QDateTime::fromString(sTime, Qt::ISODate));
		return ae.staticCast<PsiEvent>();
	}

	return PsiEvent::Ptr();
}

QString EDBFlatFile::File::eventToLine(const PsiEvent::Ptr &e)
{
	int subflags = 0;
	QString sTime, sType, sOrigin, sFlags;

	if(e->type() == PsiEvent::Message) {
		MessageEvent::Ptr me = e.staticCast<MessageEvent>();
		const Message &m = me->message();
		const UrlList urls = m.urlList();

		if(!m.subject().isEmpty())
			subflags |= 1;
		if(!urls.isEmpty())
			subflags |= 2;

		sTime = m.timeStamp().toString(Qt::ISODate);
		int n = 0;
		if(m.type() == "chat")
			n = 1;
		else if(m.type() == "error")
			n = 4;
		else if(m.type() == "headline")
			n = 5;
		sType.setNum(n);
		sOrigin = e->originLocal() ? "to": "from";
		sFlags = "N---";

		if(subflags != 0)
			sFlags[1] = QString::number(subflags,16)[0];

		//  | date | type | To/from | flags | text
		QString line = "|" + sTime + "|" + sType + "|" + sOrigin + "|" + sFlags + "|";

		if(subflags & 1) {
			line += logencode(m.subject()) + "|";
		}
		if(subflags & 2) {
			const Url &url = urls.first();
			line += logencode(url.url()) + "|";
			line += logencode(url.desc()) + "|";
		}
		line += logencode(m.body());

		return line;
	}
	else if(e->type() == PsiEvent::Auth) {
		AuthEvent::Ptr ae = e.staticCast<AuthEvent>();
		sTime = ae->timeStamp().toString(Qt::ISODate);
		QString subType = ae->authType();
		int n = 0;
		if(subType == "subscribe")
			n = 3;
		else if(subType == "subscribed")
			n = 6;
		else if(subType == "unsubscribe")
			n = 7;
		else if(subType == "unsubscribed")
			n = 8;
		sType.setNum(n);
		sOrigin = e->originLocal() ? "to": "from";
		sFlags = "N---";

		//  | date | type | To/from | flags | text
		QString line = "|" + sTime + "|" + sType + "|" + sOrigin + "|" + sFlags + "|";
		line += logencode(subType);

		return line;
	}

	return "";
}

QString EDBFlatFile::File::getLine(int id)
{
	touch();

	if(!valid)
		return QString();

	ensureIndex();
	if(id < 0 || id >= (int)d->index.size())
		return QString();

	f.seek(d->index[id]);

	QTextStream t;
	t.setDevice(&f);
	t.setCodec("UTF-8");
	return t.readLine();
}

QDateTime EDBFlatFile::File::getDate(int id)
{
	QString line = getLine(id);
	if (line.isNull())
		return QDateTime();

	int p1 = line.indexOf('|') + 1;
	if (p1 == -1)
		return QDateTime();
	int p2 = line.indexOf('|', p1);
	if (p2 == -1)
		return QDateTime();

	QString sTime = line.mid(p1, p2 - p1);
	return QDateTime::fromString(sTime, Qt::ISODate);
}
