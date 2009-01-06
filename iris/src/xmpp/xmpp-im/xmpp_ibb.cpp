/*
 * ibb.cpp - Inband bytestream
 * Copyright (C) 2001, 2002  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "xmpp_ibb.h"

#include <qtimer.h>
#include "xmpp_xmlcommon.h"
#include <QtCrypto>

#include <stdlib.h>

#define IBB_PACKET_SIZE   4096
#define IBB_PACKET_DELAY  0

using namespace XMPP;

static int num_conn = 0;
static int id_conn = 0;

//----------------------------------------------------------------------------
// IBBConnection
//----------------------------------------------------------------------------
class IBBConnection::Private
{
public:
	Private() {}

	int state;
	Jid peer;
	QString sid;
	IBBManager *m;
	JT_IBB *j;
	QDomElement comment;
	QString iq_id;

	int blockSize;
	QByteArray recvbuf, sendbuf;
	bool closePending, closing;

	int id;
};

IBBConnection::IBBConnection(IBBManager *m)
:ByteStream(m)
{
	d = new Private;
	d->m = m;
	d->j = 0;
	reset();

	++num_conn;
	d->id = id_conn++;
	QString dstr; dstr.sprintf("IBBConnection[%d]: constructing, count=%d\n", d->id, num_conn);
	d->m->client()->debug(dstr);
}

void IBBConnection::reset(bool clear)
{
	d->m->unlink(this);
	d->state = Idle;
	d->closePending = false;
	d->closing = false;

	delete d->j;
	d->j = 0;

	d->sendbuf.resize(0);
	if(clear)
		d->recvbuf.resize(0);
}

IBBConnection::~IBBConnection()
{
	reset(true);

	--num_conn;
	QString dstr; dstr.sprintf("IBBConnection[%d]: destructing, count=%d\n", d->id, num_conn);
	d->m->client()->debug(dstr);

	delete d;
}

void IBBConnection::connectToJid(const Jid &peer, const QDomElement &comment)
{
	close();
	reset(true);

	d->state = Requesting;
	d->peer = peer;
	d->comment = comment;

	QString dstr; dstr.sprintf("IBBConnection[%d]: initiating request to %s\n", d->id, peer.full().toLatin1().data());
	d->m->client()->debug(dstr);

	d->j = new JT_IBB(d->m->client()->rootTask());
	connect(d->j, SIGNAL(finished()), SLOT(ibb_finished()));
	d->j->request(d->peer, comment);
	d->j->go(true);
}

void IBBConnection::accept()
{
	if(d->state != WaitingForAccept)
		return;

	QString dstr; dstr.sprintf("IBBConnection[%d]: accepting %s [%s]\n", d->id, d->peer.full().toLatin1().data(), d->sid.toLatin1().data());
	d->m->client()->debug(dstr);

	d->m->doAccept(this, d->iq_id);
	d->state = Active;
	d->m->link(this);
}

void IBBConnection::close()
{
	if(d->state == Idle)
		return;

	if(d->state == WaitingForAccept) {
		d->m->doReject(this, d->iq_id, 403, "Rejected");
		reset();
		return;
	}

	QString dstr; dstr.sprintf("IBBConnection[%d]: closing\n", d->id);
	d->m->client()->debug(dstr);

	if(d->state == Active) {
		// if there is data pending to be written, then pend the closing
		if(bytesToWrite() > 0) {
			d->closePending = true;
			trySend();
			return;
		}

		// send a close packet
		JT_IBB *j = new JT_IBB(d->m->client()->rootTask());
		j->sendData(d->peer, d->sid, QByteArray(), true);
		j->go(true);
	}

	reset();
}

int IBBConnection::state() const
{
	return d->state;
}

Jid IBBConnection::peer() const
{
	return d->peer;
}

QString IBBConnection::streamid() const
{
	return d->sid;
}

QDomElement IBBConnection::comment() const
{
	return d->comment;
}

bool IBBConnection::isOpen() const
{
	if(d->state == Active)
		return true;
	else
		return false;
}

void IBBConnection::write(const QByteArray &a)
{
	if(d->state != Active || d->closePending || d->closing)
		return;

	// append to the end of our send buffer
	int oldsize = d->sendbuf.size();
	d->sendbuf.resize(oldsize + a.size());
	memcpy(d->sendbuf.data() + oldsize, a.data(), a.size());

	trySend();
}

QByteArray IBBConnection::read(int)
{
	// TODO: obey argument
	QByteArray a = d->recvbuf;
	d->recvbuf.resize(0);
	return a;
}

int IBBConnection::bytesAvailable() const
{
	return d->recvbuf.size();
}

int IBBConnection::bytesToWrite() const
{
	return d->sendbuf.size();
}

void IBBConnection::waitForAccept(const Jid &peer, const QString &sid, const QDomElement &comment, const QString &iq_id)
{
	close();
	reset(true);

	d->state = WaitingForAccept;
	d->peer = peer;
	d->sid = sid;
	d->comment = comment;
	d->iq_id = iq_id;
}

void IBBConnection::takeIncomingData(const QByteArray &a, bool close)
{
	// append to the end of our recv buffer
	int oldsize = d->recvbuf.size();
	d->recvbuf.resize(oldsize + a.size());
	memcpy(d->recvbuf.data() + oldsize, a.data(), a.size());

	readyRead();

	if(close) {
		reset();
		connectionClosed();
	}
}

void IBBConnection::ibb_finished()
{
	JT_IBB *j = d->j;
	d->j = 0;

	if(j->success()) {
		if(j->mode() == JT_IBB::ModeRequest) {
			d->sid = j->streamid();

			QString dstr; dstr.sprintf("IBBConnection[%d]: %s [%s] accepted.\n", d->id, d->peer.full().toLatin1().data(), d->sid.toLatin1().data());
			d->m->client()->debug(dstr);

			d->state = Active;
			d->m->link(this);
			connected();
		}
		else {
			bytesWritten(d->blockSize);

			if(d->closing) {
				reset();
				delayedCloseFinished();
			}

			if(!d->sendbuf.isEmpty() || d->closePending)
				QTimer::singleShot(IBB_PACKET_DELAY, this, SLOT(trySend()));
		}
	}
	else {
		if(j->mode() == JT_IBB::ModeRequest) {
			QString dstr; dstr.sprintf("IBBConnection[%d]: %s refused.\n", d->id, d->peer.full().toLatin1().data());
			d->m->client()->debug(dstr);

			reset(true);
			error(ErrRequest);
		}
		else {
			reset(true);
			error(ErrData);
		}
	}
}

void IBBConnection::trySend()
{
	// if we already have an active task, then don't do anything
	if(d->j)
		return;

	QByteArray a;
	if(!d->sendbuf.isEmpty()) {
		// take a chunk
		if(d->sendbuf.size() < IBB_PACKET_SIZE)
			a.resize(d->sendbuf.size());
		else
			a.resize(IBB_PACKET_SIZE);
		memcpy(a.data(), d->sendbuf.data(), a.size());
		d->sendbuf.resize(d->sendbuf.size() - a.size());
	}

	bool doClose = false;
	if(d->sendbuf.isEmpty() && d->closePending)
		doClose = true;

	// null operation?
	if(a.isEmpty() && !doClose)
		return;

	printf("IBBConnection[%d]: sending [%d] bytes ", d->id, a.size());
	if(doClose)
		printf("and closing.\n");
	else
		printf("(%d bytes left)\n", d->sendbuf.size());

	if(doClose) {
		d->closePending = false;
		d->closing = true;
	}

	d->blockSize = a.size();
	d->j = new JT_IBB(d->m->client()->rootTask());
	connect(d->j, SIGNAL(finished()), SLOT(ibb_finished()));
	d->j->sendData(d->peer, d->sid, a, doClose);
	d->j->go(true);
}


//----------------------------------------------------------------------------
// IBBManager
//----------------------------------------------------------------------------
class IBBManager::Private
{
public:
	Private() {}

	Client *client;
	IBBConnectionList activeConns;
	IBBConnectionList incomingConns;
	JT_IBB *ibb;
};

IBBManager::IBBManager(Client *parent)
:QObject(parent)
{
	d = new Private;
	d->client = parent;
	
	d->ibb = new JT_IBB(d->client->rootTask(), true);
	connect(d->ibb, SIGNAL(incomingRequest(const Jid &, const QString &, const QDomElement &)), SLOT(ibb_incomingRequest(const Jid &, const QString &, const QDomElement &)));
	connect(d->ibb, SIGNAL(incomingData(const Jid &, const QString &, const QString &, const QByteArray &, bool)), SLOT(ibb_incomingData(const Jid &, const QString &, const QString &, const QByteArray &, bool)));
}

IBBManager::~IBBManager()
{
	while (!d->incomingConns.isEmpty()) {
		delete d->incomingConns.takeFirst();
	}
	delete d->ibb;
	delete d;
}

Client *IBBManager::client() const
{
	return d->client;
}

IBBConnection *IBBManager::takeIncoming()
{
	if(d->incomingConns.isEmpty())
		return 0;

	IBBConnection *c = d->incomingConns.takeFirst();
	return c;
}

void IBBManager::ibb_incomingRequest(const Jid &from, const QString &id, const QDomElement &comment)
{
	QString sid = genUniqueKey();

	// create a "waiting" connection
	IBBConnection *c = new IBBConnection(this);
	c->waitForAccept(from, sid, comment, id);
	d->incomingConns.append(c);
	incomingReady();
}

void IBBManager::ibb_incomingData(const Jid &from, const QString &streamid, const QString &id, const QByteArray &data, bool close)
{
	IBBConnection *c = findConnection(streamid, from);
	if(!c) {
		d->ibb->respondError(from, id, 404, "No such stream");
	}
	else {
		d->ibb->respondAck(from, id);
		c->takeIncomingData(data, close);
	}
}

QString IBBManager::genKey() const
{
	QString key = "ibb_";

	for(int i = 0; i < 4; ++i) {
		int word = rand() & 0xffff;
		for(int n = 0; n < 4; ++n) {
			QString s;
			s.sprintf("%x", (word >> (n * 4)) & 0xf);
			key.append(s);
		}
	}

	return key;
}

QString IBBManager::genUniqueKey() const
{
	// get unused key
	QString key;
	while(1) {
		key = genKey();

		if(!findConnection(key))
			break;
	}

	return key;
}

void IBBManager::link(IBBConnection *c)
{
	d->activeConns.append(c);
}

void IBBManager::unlink(IBBConnection *c)
{
	d->activeConns.removeAll(c);
}

IBBConnection *IBBManager::findConnection(const QString &sid, const Jid &peer) const
{
	foreach(IBBConnection* c, d->activeConns) {
		if(c->streamid() == sid && (peer.isEmpty() || c->peer().compare(peer)) )
			return c;
	}
	return 0;
}

void IBBManager::doAccept(IBBConnection *c, const QString &id)
{
	d->ibb->respondSuccess(c->peer(), id, c->streamid());
}

void IBBManager::doReject(IBBConnection *c, const QString &id, int code, const QString &str)
{
	d->ibb->respondError(c->peer(), id, code, str);
}


//----------------------------------------------------------------------------
// JT_IBB
//----------------------------------------------------------------------------
class JT_IBB::Private
{
public:
	Private() {}

	QDomElement iq;
	int mode;
	bool serve;
	Jid to;
	QString streamid;
};

JT_IBB::JT_IBB(Task *parent, bool serve)
:Task(parent)
{
	d = new Private;
	d->serve = serve;
}

JT_IBB::~JT_IBB()
{
	delete d;
}

void JT_IBB::request(const Jid &to, const QDomElement &comment)
{
	d->mode = ModeRequest;
	QDomElement iq;
	d->to = to;
	iq = createIQ(doc(), "set", to.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "http://jabber.org/protocol/ibb");
	iq.appendChild(query);
	query.appendChild(comment);
	d->iq = iq;
}

void JT_IBB::sendData(const Jid &to, const QString &streamid, const QByteArray &a, bool close)
{
	d->mode = ModeSendData;
	QDomElement iq;
	d->to = to;
	iq = createIQ(doc(), "set", to.full(), id());
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "http://jabber.org/protocol/ibb");
	iq.appendChild(query);
	query.appendChild(textTag(doc(), "streamid", streamid));
	if(!a.isEmpty())
		query.appendChild(textTag(doc(), "data", QCA::Base64().arrayToString(a)));
	if(close) {
		QDomElement c = doc()->createElement("close");
		query.appendChild(c);
	}
	d->iq = iq;
}

void JT_IBB::respondSuccess(const Jid &to, const QString &id, const QString &streamid)
{
	QDomElement iq = createIQ(doc(), "result", to.full(), id);
	QDomElement query = doc()->createElement("query");
	query.setAttribute("xmlns", "http://jabber.org/protocol/ibb");
	iq.appendChild(query);
	query.appendChild(textTag(doc(), "streamid", streamid));
	send(iq);
}

void JT_IBB::respondError(const Jid &to, const QString &id, int code, const QString &str)
{
	QDomElement iq = createIQ(doc(), "error", to.full(), id);
	QDomElement err = textTag(doc(), "error", str);
	err.setAttribute("code", QString::number(code));
	iq.appendChild(err);
	send(iq);
}

void JT_IBB::respondAck(const Jid &to, const QString &id)
{
	QDomElement iq = createIQ(doc(), "result", to.full(), id);
	send(iq);
}

void JT_IBB::onGo()
{
	send(d->iq);
}

bool JT_IBB::take(const QDomElement &e)
{
	if(d->serve) {
		// must be an iq-set tag
		if(e.tagName() != "iq" || e.attribute("type") != "set")
			return false;

		if(queryNS(e) != "http://jabber.org/protocol/ibb")
			return false;

		Jid from(e.attribute("from"));
		QString id = e.attribute("id");
		QDomElement q = queryTag(e);

		bool found;
		QDomElement s = findSubTag(q, "streamid", &found);
		if(!found) {
			QDomElement comment = findSubTag(q, "comment", &found);
			incomingRequest(from, id, comment);
		}
		else {
			QString sid = tagContent(s);
			QByteArray a;
			bool close = false;
			s = findSubTag(q, "data", &found);
			if(found)
				a = QCA::Base64().stringToArray(tagContent(s)).toByteArray();
			s = findSubTag(q, "close", &found);
			if(found)
				close = true;

			incomingData(from, sid, id, a, close);
		}

		return true;
	}
	else {
		Jid from(e.attribute("from"));
		if(e.attribute("id") != id() || !d->to.compare(from))
			return false;

		if(e.attribute("type") == "result") {
			QDomElement q = queryTag(e);

			// request
			if(d->mode == ModeRequest) {
				bool found;
				QDomElement s = findSubTag(q, "streamid", &found);
				if(found)
					d->streamid = tagContent(s);
				else
					d->streamid = "";
				setSuccess();
			}
			// sendData
			else {
				// thank you for the ack, kind sir
				setSuccess();
			}
		}
		else {
			setError(e);
		}

		return true;
	}
}

QString JT_IBB::streamid() const
{
	return d->streamid;
}

Jid JT_IBB::jid() const
{
	return d->to;
}

int JT_IBB::mode() const
{
	return d->mode;
}

