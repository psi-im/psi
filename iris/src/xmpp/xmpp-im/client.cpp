/*
 * client.cpp - IM Client
 * Copyright (C) 2003  Justin Karneges
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

#include "im.h"
#include "safedelete.h"

//! \class XMPP::Client client.h
//! \brief Communicates with the Jabber network.  Start here.
//!
//!  Client controls an active Jabber connection.  It allows you to connect,
//!  authenticate, manipulate the roster, and send / receive messages and
//!  presence.  It is the centerpiece of this library, and all Tasks must pass
//!  through it.
//!
//!  For convenience, many Tasks are handled internally to Client (such as
//!  JT_Auth).  However, for accessing features beyond the basics provided by
//!  Client, you will need to manually invoke Tasks.  Fortunately, the
//!  process is very simple.
//!
//!  The entire Task system is heavily founded on Qt.  All Tasks have a parent,
//!  except for the root Task, and are considered QObjects.  By using Qt's RTTI
//!  facilities (QObject::sender(), QObject::isA(), etc), you can use a
//!  "fire and forget" approach with Tasks.
//!
//!  \code
//!  #include "client.h"
//!  using namespace Jabber;
//!
//!  ...
//!
//!  Client *client;
//!
//!  Session::Session()
//!  {
//!    client = new Client;
//!    connect(client, SIGNAL(handshaken()), SLOT(clientHandshaken()));
//!    connect(client, SIGNAL(authFinished(bool, int, const QString &)), SLOT(authFinished(bool, int, const QString &)));
//!    client->connectToHost("jabber.org");
//!  }
//!
//!  void Session::clientHandshaken()
//!  {
//!    client->authDigest("jabtest", "12345", "Psi");
//!  }
//!
//!  void Session::authFinished(bool success, int, const QString &err)
//!  {
//!    if(success)
//!      printf("Login success!");
//!    else
//!      printf("Login failed.  Here's why: %s\n", err.toLatin1());
//!  }
//!  \endcode

#include <stdarg.h>
#include <qobject.h>
#include <QMap>
#include <qtimer.h>
#include <qpointer.h>
//Added by qt3to4:
#include <QList>
#include "xmpp_tasks.h"
#include "xmpp_xmlcommon.h"
#include "s5b.h"
#include "xmpp_ibb.h"
#include "filetransfer.h"

/*#include <stdio.h>
#include <stdarg.h>
#include <qstring.h>
#include <qdom.h>
#include <qobjectlist.h>
#include <qtimer.h>
#include "xmpp_stream.h"
#include "xmpp_tasks.h"
#include "xmpp_xmlcommon.h"
#include "xmpp_dtcp.h"
#include "xmpp_ibb.h"
#include "xmpp_jidlink.h"

using namespace Jabber;*/

#ifdef Q_WS_WIN
#define vsnprintf _vsnprintf
#endif

namespace XMPP
{

//----------------------------------------------------------------------------
// Client
//----------------------------------------------------------------------------
class Client::GroupChat
{
public:
	enum { Connecting, Connected, Closing };
	GroupChat() {}

	Jid j;
	int status;
	QString password;
};

class Client::ClientPrivate
{
public:
	ClientPrivate() {}

	ClientStream *stream;
	QDomDocument doc;
	int id_seed;
	Task *root;
	QString host, user, pass, resource;
	QString osname, tzname, clientName, clientVersion, capsNode, capsVersion, capsExt;
	DiscoItem::Identity identity;
	Features features;
	QMap<QString,Features> extension_features;
	int tzoffset;
	bool active;

	LiveRoster roster;
	ResourceList resourceList;
	S5BManager *s5bman;
	IBBManager *ibbman;
	FileTransferManager *ftman;
	bool ftEnabled;
	QList<GroupChat> groupChatList;
};


Client::Client(QObject *par)
:QObject(par)
{
	d = new ClientPrivate;
	d->tzoffset = 0;
	d->active = false;
	d->osname = "N/A";
	d->clientName = "N/A";
	d->clientVersion = "0.0";
	d->capsNode = "";
	d->capsVersion = "";
	d->capsExt = "";

	d->id_seed = 0xaaaa;
	d->root = new Task(this, true);

	d->stream = 0;

	d->s5bman = new S5BManager(this);
	connect(d->s5bman, SIGNAL(incomingReady()), SLOT(s5b_incomingReady()));

	d->ibbman = new IBBManager(this);
	connect(d->ibbman, SIGNAL(incomingReady()), SLOT(ibb_incomingReady()));

	d->ftman = 0;
}

Client::~Client()
{
	close(true);

	delete d->ftman;
	delete d->ibbman;
	delete d->s5bman;
	delete d->root;
	//delete d->stream;
	delete d;
}

void Client::connectToServer(ClientStream *s, const Jid &j, bool auth)
{
	d->stream = s;
	//connect(d->stream, SIGNAL(connected()), SLOT(streamConnected()));
	//connect(d->stream, SIGNAL(handshaken()), SLOT(streamHandshaken()));
	connect(d->stream, SIGNAL(error(int)), SLOT(streamError(int)));
	//connect(d->stream, SIGNAL(sslCertificateReady(const QSSLCert &)), SLOT(streamSSLCertificateReady(const QSSLCert &)));
	connect(d->stream, SIGNAL(readyRead()), SLOT(streamReadyRead()));
	//connect(d->stream, SIGNAL(closeFinished()), SLOT(streamCloseFinished()));
	connect(d->stream, SIGNAL(incomingXml(const QString &)), SLOT(streamIncomingXml(const QString &)));
	connect(d->stream, SIGNAL(outgoingXml(const QString &)), SLOT(streamOutgoingXml(const QString &)));

	d->stream->connectToServer(j, auth);
}

void Client::start(const QString &host, const QString &user, const QString &pass, const QString &_resource)
{
	// TODO
	d->host = host;
	d->user = user;
	d->pass = pass;
	d->resource = _resource;

	Status stat;
	stat.setIsAvailable(false);
	d->resourceList += Resource(resource(), stat);

	JT_PushPresence *pp = new JT_PushPresence(rootTask());
	connect(pp, SIGNAL(subscription(const Jid &, const QString &, const QString&)), SLOT(ppSubscription(const Jid &, const QString &, const QString&)));
	connect(pp, SIGNAL(presence(const Jid &, const Status &)), SLOT(ppPresence(const Jid &, const Status &)));

	JT_PushMessage *pm = new JT_PushMessage(rootTask());
	connect(pm, SIGNAL(message(const Message &)), SLOT(pmMessage(const Message &)));

	JT_PushRoster *pr = new JT_PushRoster(rootTask());
	connect(pr, SIGNAL(roster(const Roster &)), SLOT(prRoster(const Roster &)));

	new JT_ServInfo(rootTask());

	d->active = true;
}

void Client::setFileTransferEnabled(bool b)
{
	if(b) {
		if(!d->ftman)
			d->ftman = new FileTransferManager(this);
	}
	else {
		if(d->ftman) {
			delete d->ftman;
			d->ftman = 0;
		}
	}
}

FileTransferManager *Client::fileTransferManager() const
{
	return d->ftman;
}

S5BManager *Client::s5bManager() const
{
	return d->s5bman;
}

IBBManager *Client::ibbManager() const
{
	return d->ibbman;
}

bool Client::isActive() const
{
	return d->active;
}

QString Client::groupChatPassword(const QString& host, const QString& room) const
{
	Jid jid(room + "@" + host);
	foreach(GroupChat i, d->groupChatList) {
		if(i.j.compare(jid, false)) {
			return i.password;
		}
	}
	return QString();
}

void Client::groupChatChangeNick(const QString &host, const QString &room, const QString &nick, const Status &_s)
{
	Jid jid(room + "@" + host + "/" + nick);
	for(QList<GroupChat>::Iterator it = d->groupChatList.begin(); it != d->groupChatList.end(); it++) {
		GroupChat &i = *it;
		if(i.j.compare(jid, false)) {
			i.j = jid;

			Status s = _s;
			s.setIsAvailable(true);

			JT_Presence *j = new JT_Presence(rootTask());
			j->pres(jid, s);
			j->go(true);

			break;
		}
	}
}

bool Client::groupChatJoin(const QString &host, const QString &room, const QString &nick, const QString& password, int maxchars, int maxstanzas, int seconds, const Status& _s)
{
	Jid jid(room + "@" + host + "/" + nick);
	for(QList<GroupChat>::Iterator it = d->groupChatList.begin(); it != d->groupChatList.end();) {
		GroupChat &i = *it;
		if(i.j.compare(jid, false)) {
			// if this room is shutting down, then free it up
			if(i.status == GroupChat::Closing)
				it = d->groupChatList.erase(it);
			else
				return false;
		}
		else
			++it;
	}

	debug(QString("Client: Joined: [%1]\n").arg(jid.full()));
	GroupChat i;
	i.j = jid;
	i.status = GroupChat::Connecting;
	i.password = password;
	d->groupChatList += i;

	JT_Presence *j = new JT_Presence(rootTask());
	Status s = _s;
	s.setMUC();
	s.setMUCHistory(maxchars,maxstanzas,seconds);
	if (!password.isEmpty()) {
		s.setMUCPassword(password);
	}
	j->pres(jid,s);
	j->go(true);

	return true;
}

void Client::groupChatSetStatus(const QString &host, const QString &room, const Status &_s)
{
	Jid jid(room + "@" + host);
	bool found = false;
	for(QList<GroupChat>::ConstIterator it = d->groupChatList.begin(); it != d->groupChatList.end(); it++) {
		const GroupChat &i = *it;
		if(i.j.compare(jid, false)) {
			found = true;
			jid = i.j;
			break;
		}
	}
	if(!found)
		return;

	Status s = _s;
	s.setIsAvailable(true);

	JT_Presence *j = new JT_Presence(rootTask());
	j->pres(jid, s);
	j->go(true);
}

void Client::groupChatLeave(const QString &host, const QString &room)
{
	Jid jid(room + "@" + host);
	for(QList<GroupChat>::Iterator it = d->groupChatList.begin(); it != d->groupChatList.end(); it++) {
		GroupChat &i = *it;

		if(!i.j.compare(jid, false))
			continue;

		i.status = GroupChat::Closing;
		debug(QString("Client: Leaving: [%1]\n").arg(i.j.full()));

		JT_Presence *j = new JT_Presence(rootTask());
		Status s;
		s.setIsAvailable(false);
		j->pres(i.j, s);
		j->go(true);
	}
}

/*void Client::start()
{
	if(d->stream->old()) {
		// old has no activation step
		d->active = true;
		activated();
	}
	else {
		// TODO: IM session
	}
}*/

// TODO: fast close
void Client::close(bool)
{
	if(d->stream) {
		if(d->active) {
			for(QList<GroupChat>::Iterator it = d->groupChatList.begin(); it != d->groupChatList.end(); it++) {
				GroupChat &i = *it;
				i.status = GroupChat::Closing;

				JT_Presence *j = new JT_Presence(rootTask());
				Status s;
				s.setIsAvailable(false);
				j->pres(i.j, s);
				j->go(true);
			}
		}

		d->stream->disconnect(this);
		d->stream->close();
		d->stream = 0;
	}
	disconnected();
	cleanup();
}

void Client::cleanup()
{
	d->active = false;
	//d->authed = false;
	d->groupChatList.clear();
}

/*void Client::continueAfterCert()
{
	d->stream->continueAfterCert();
}

void Client::streamConnected()
{
	connected();
}

void Client::streamHandshaken()
{
	handshaken();
}*/

void Client::streamError(int)
{
	//StreamError e = err;
	//error(e);

	//if(!e.isWarning()) {
		disconnected();
		cleanup();
	//}
}

/*void Client::streamSSLCertificateReady(const QSSLCert &cert)
{
	sslCertReady(cert);
}

void Client::streamCloseFinished()
{
	closeFinished();
}*/

static QDomElement oldStyleNS(const QDomElement &e)
{
	// find closest parent with a namespace
	QDomNode par = e.parentNode();
	while(!par.isNull() && par.namespaceURI().isNull())
		par = par.parentNode();
	bool noShowNS = false;
	if(!par.isNull() && par.namespaceURI() == e.namespaceURI())
		noShowNS = true;

	QDomElement i;
	int x;
	//if(noShowNS)
		i = e.ownerDocument().createElement(e.tagName());
	//else
	//	i = e.ownerDocument().createElementNS(e.namespaceURI(), e.tagName());

	// copy attributes
	QDomNamedNodeMap al = e.attributes();
	for(x = 0; x < al.count(); ++x)
		i.setAttributeNode(al.item(x).cloneNode().toAttr());

	if(!noShowNS)
		i.setAttribute("xmlns", e.namespaceURI());

	// copy children
	QDomNodeList nl = e.childNodes();
	for(x = 0; x < nl.count(); ++x) {
		QDomNode n = nl.item(x);
		if(n.isElement())
			i.appendChild(oldStyleNS(n.toElement()));
		else
			i.appendChild(n.cloneNode());
	}
	return i;
}

void Client::streamReadyRead()
{
	// HACK HACK HACK
	QPointer<ClientStream> pstream = d->stream;

	while(pstream && d->stream->stanzaAvailable()) {
		Stanza s = d->stream->read();

		QString out = s.toString();
		debug(QString("Client: incoming: [\n%1]\n").arg(out));
		xmlIncoming(out);

		QDomElement x = oldStyleNS(s.element());
		distribute(x);
	}
}

void Client::streamIncomingXml(const QString &s)
{
	QString str = s;
	if(str.at(str.length()-1) != '\n')
		str += '\n';
	xmlIncoming(str);
}

void Client::streamOutgoingXml(const QString &s)
{
	QString str = s;
	if(str.at(str.length()-1) != '\n')
		str += '\n';
	xmlOutgoing(str);
}

void Client::debug(const QString &str)
{
	debugText(str);
}

QString Client::genUniqueId()
{
	QString s;
	s.sprintf("a%x", d->id_seed);
	d->id_seed += 0x10;
	return s;
}

Task *Client::rootTask()
{
	return d->root;
}

QDomDocument *Client::doc() const
{
	return &d->doc;
}

void Client::distribute(const QDomElement &x)
{
	if(x.hasAttribute("from")) {
		Jid j(x.attribute("from"));
		if(!j.isValid()) {
			debug("Client: bad 'from' JID\n");
			return;
		}
	}

	if(!rootTask()->take(x) && (x.attribute("type") == "get" || x.attribute("type") == "set") ) {
		debug("Client: Unrecognized IQ.\n");

		// Create reply element
		QDomElement reply = createIQ(doc(), "error", x.attribute("from"), x.attribute("id"));

		// Copy children
		for (QDomNode n = x.firstChild(); !n.isNull(); n = n.nextSibling()) {
			reply.appendChild(n.cloneNode());
		}
		
		// Add error
		QDomElement error = doc()->createElement("error");
		error.setAttribute("type","cancel");
		reply.appendChild(error);

		QDomElement error_type = doc()->createElement("feature-not-implemented");
		error_type.setAttribute("xmlns","urn:ietf:params:xml:ns:xmpp-stanzas");
		error.appendChild(error_type);

		send(reply);
	}
}

void Client::send(const QDomElement &x)
{
	if(!d->stream)
		return;

	//QString out;
	//QTextStream ts(&out, IO_WriteOnly);
	//x.save(ts, 0);

	//QString out = Stream::xmlToString(x);
	//debug(QString("Client: outgoing: [\n%1]\n").arg(out));
	//xmlOutgoing(out);

	QDomElement e = addCorrectNS(x);
	Stanza s = d->stream->createStanza(e);
	if(s.isNull()) {
		//printf("bad stanza??\n");
		return;
	}

	QString out = s.toString();
	debug(QString("Client: outgoing: [\n%1]\n").arg(out));
	xmlOutgoing(out);

	//printf("x[%s] x2[%s] s[%s]\n", Stream::xmlToString(x).toLatin1(), Stream::xmlToString(e).toLatin1(), s.toString().toLatin1());
	d->stream->write(s);
}

void Client::send(const QString &str)
{
	if(!d->stream)
		return;

	debug(QString("Client: outgoing: [\n%1]\n").arg(str));
	xmlOutgoing(str);
	static_cast<ClientStream*>(d->stream)->writeDirect(str);
}

Stream & Client::stream()
{
	return *d->stream;
}

QString Client::streamBaseNS() const
{
	return d->stream->baseNS();
}

const LiveRoster & Client::roster() const
{
	return d->roster;
}

const ResourceList & Client::resourceList() const
{
	return d->resourceList;
}

QString Client::host() const
{
	return d->host;
}

QString Client::user() const
{
	return d->user;
}

QString Client::pass() const
{
	return d->pass;
}

QString Client::resource() const
{
	return d->resource;
}

Jid Client::jid() const
{
	QString s;
	if(!d->user.isEmpty())
		s += d->user + '@';
	s += d->host;
	if(!d->resource.isEmpty()) {
		s += '/';
		s += d->resource;
	}

	return Jid(s);
}

void Client::ppSubscription(const Jid &j, const QString &s, const QString& n)
{
	subscription(j, s, n);
}

void Client::ppPresence(const Jid &j, const Status &s)
{
	if(s.isAvailable())
		debug(QString("Client: %1 is available.\n").arg(j.full()));
	else
		debug(QString("Client: %1 is unavailable.\n").arg(j.full()));

	for(QList<GroupChat>::Iterator it = d->groupChatList.begin(); it != d->groupChatList.end(); it++) {
		GroupChat &i = *it;

		if(i.j.compare(j, false)) {
			bool us = (i.j.resource() == j.resource() || j.resource().isEmpty()) ? true: false;

			debug(QString("for groupchat i=[%1] pres=[%2], [us=%3].\n").arg(i.j.full()).arg(j.full()).arg(us));
			switch(i.status) {
				case GroupChat::Connecting:
					if(us && s.hasError()) {
						Jid j = i.j;
						d->groupChatList.erase(it);
						groupChatError(j, s.errorCode(), s.errorString());
					}
					else {
						// don't signal success unless it is a non-error presence
						if(!s.hasError()) {
							i.status = GroupChat::Connected;
							groupChatJoined(i.j);
						}
						groupChatPresence(j, s);
					}
					break;
				case GroupChat::Connected:
					groupChatPresence(j, s);
					break;
				case GroupChat::Closing:
					if(us && !s.isAvailable()) {
						Jid j = i.j;
						d->groupChatList.erase(it);
						groupChatLeft(j);
					}
					break;
				default:
					break;
			}

			return;
		}
	}

	if(s.hasError()) {
		presenceError(j, s.errorCode(), s.errorString());
		return;
	}

	// is it me?
	if(j.compare(jid(), false)) {
		updateSelfPresence(j, s);
	}
	else {
		// update all relavent roster entries
		for(LiveRoster::Iterator it = d->roster.begin(); it != d->roster.end(); ++it) {
			LiveRosterItem &i = *it;

			if(!i.jid().compare(j, false))
				continue;

			// roster item has its own resource?
			if(!i.jid().resource().isEmpty()) {
				if(i.jid().resource() != j.resource())
					continue;
			}

			updatePresence(&i, j, s);
		}
	}
}

void Client::updateSelfPresence(const Jid &j, const Status &s)
{
	ResourceList::Iterator rit = d->resourceList.find(j.resource());
	bool found = (rit == d->resourceList.end()) ? false: true;

	// unavailable?  remove the resource
	if(!s.isAvailable()) {
		if(found) {
			debug(QString("Client: Removing self resource: name=[%1]\n").arg(j.resource()));
			(*rit).setStatus(s);
			resourceUnavailable(j, *rit);
			d->resourceList.erase(rit);
		}
	}
	// available?  add/update the resource
	else {
		Resource r;
		if(!found) {
			r = Resource(j.resource(), s);
			d->resourceList += r;
			debug(QString("Client: Adding self resource: name=[%1]\n").arg(j.resource()));
		}
		else {
			(*rit).setStatus(s);
			r = *rit;
			debug(QString("Client: Updating self resource: name=[%1]\n").arg(j.resource()));
		}

		resourceAvailable(j, r);
	}
}

void Client::updatePresence(LiveRosterItem *i, const Jid &j, const Status &s)
{
	ResourceList::Iterator rit = i->resourceList().find(j.resource());
	bool found = (rit == i->resourceList().end()) ? false: true;

	// unavailable?  remove the resource
	if(!s.isAvailable()) {
		if(found) {
			(*rit).setStatus(s);
			debug(QString("Client: Removing resource from [%1]: name=[%2]\n").arg(i->jid().full()).arg(j.resource()));
			resourceUnavailable(j, *rit);
			i->resourceList().erase(rit);
			i->setLastUnavailableStatus(s);
		}
		else {
			// create the resource just for the purpose of emit
			Resource r = Resource(j.resource(), s);
			i->resourceList() += r;
			rit = i->resourceList().find(j.resource());
			resourceUnavailable(j, *rit);
			i->resourceList().erase(rit);
			i->setLastUnavailableStatus(s);
		}
	}
	// available?  add/update the resource
	else {
		Resource r;
		if(!found) {
			r = Resource(j.resource(), s);
			i->resourceList() += r;
			debug(QString("Client: Adding resource to [%1]: name=[%2]\n").arg(i->jid().full()).arg(j.resource()));
		}
		else {
			(*rit).setStatus(s);
			r = *rit;
			debug(QString("Client: Updating resource to [%1]: name=[%2]\n").arg(i->jid().full()).arg(j.resource()));
		}

		resourceAvailable(j, r);
	}
}

void Client::pmMessage(const Message &m)
{
	debug(QString("Client: Message from %1\n").arg(m.from().full()));

	if(m.type() == "groupchat") {
		for(QList<GroupChat>::Iterator it = d->groupChatList.begin(); it != d->groupChatList.end(); it++) {
			const GroupChat &i = *it;

			if(!i.j.compare(m.from(), false))
				continue;

			if(i.status == GroupChat::Connected)
				messageReceived(m);
		}
	}
	else
		messageReceived(m);
}

void Client::prRoster(const Roster &r)
{
	importRoster(r);
}

void Client::rosterRequest()
{
	if(!d->active)
		return;

	JT_Roster *r = new JT_Roster(rootTask());
	connect(r, SIGNAL(finished()), SLOT(slotRosterRequestFinished()));
	r->get();
	d->roster.flagAllForDelete(); // mod_groups patch
	r->go(true);
}

void Client::slotRosterRequestFinished()
{
	JT_Roster *r = (JT_Roster *)sender();
	// on success, let's take it
	if(r->success()) {
		//d->roster.flagAllForDelete(); // mod_groups patch

		importRoster(r->roster());

		for(LiveRoster::Iterator it = d->roster.begin(); it != d->roster.end();) {
			LiveRosterItem &i = *it;
			if(i.flagForDelete()) {
				rosterItemRemoved(i);
				it = d->roster.erase(it);
			}
			else
				++it;
		}
	}
	else {
		// don't report a disconnect.  Client::error() will do that.
		if(r->statusCode() == Task::ErrDisc)
			return;
	}

	// report success / fail
	rosterRequestFinished(r->success(), r->statusCode(), r->statusString());
}

void Client::importRoster(const Roster &r)
{
	for(Roster::ConstIterator it = r.begin(); it != r.end(); ++it) {
		importRosterItem(*it);
	}
}

void Client::importRosterItem(const RosterItem &item)
{
	QString substr;
	switch(item.subscription().type()) {
		case Subscription::Both:
			substr = "<-->";  break;
		case Subscription::From:
			substr = "  ->";  break;
		case Subscription::To:
			substr = "<-  ";  break;
		case Subscription::Remove:
			substr = "xxxx";  break;
		case Subscription::None:
		default:
			substr = "----";  break;
	}

	QString dstr, str;
	str.sprintf("  %s %-32s", substr.toLatin1().data(), item.jid().full().toLatin1().data());
	if(!item.name().isEmpty())
		str += QString(" [") + item.name() + "]";
	str += '\n';

	// Remove
	if(item.subscription().type() == Subscription::Remove) {
		LiveRoster::Iterator it = d->roster.find(item.jid());
		if(it != d->roster.end()) {
			rosterItemRemoved(*it);
			d->roster.erase(it);
		}
		dstr = "Client: (Removed) ";
	}
	// Add/Update
	else {
		LiveRoster::Iterator it = d->roster.find(item.jid());
		if(it != d->roster.end()) {
			LiveRosterItem &i = *it;
			i.setFlagForDelete(false);
			i.setRosterItem(item);
			rosterItemUpdated(i);
			dstr = "Client: (Updated) ";
                }
		else {
			LiveRosterItem i(item);
			d->roster += i;

			// signal it
			rosterItemAdded(i);
			dstr = "Client: (Added)   ";
		}
	}

	debug(dstr + str);
}

void Client::sendMessage(const Message &m)
{
	JT_Message *j = new JT_Message(rootTask(), m);
	j->go(true);
}

void Client::sendSubscription(const Jid &jid, const QString &type, const QString& nick)
{
	JT_Presence *j = new JT_Presence(rootTask());
	j->sub(jid, type, nick);
	j->go(true);
}

void Client::setPresence(const Status &s)
{
	JT_Presence *j = new JT_Presence(rootTask());
	j->pres(s);
	j->go(true);

	// update our resourceList
	ppPresence(jid(), s);
	//ResourceList::Iterator rit = d->resourceList.find(resource());
	//Resource &r = *rit;
	//r.setStatus(s);
}

QString Client::OSName() const
{
	return d->osname;
}

QString Client::timeZone() const
{
	return d->tzname;
}

int Client::timeZoneOffset() const
{
	return d->tzoffset;
}

QString Client::clientName() const
{
	return d->clientName;
}

QString Client::clientVersion() const
{
	return d->clientVersion;
}

QString Client::capsNode() const
{
	return d->capsNode;
}

QString Client::capsVersion() const
{
	return d->capsVersion;
}

QString Client::capsExt() const
{
	return d->capsExt;
}

void Client::setOSName(const QString &name)
{
	d->osname = name;
}

void Client::setTimeZone(const QString &name, int offset)
{
	d->tzname = name;
	d->tzoffset = offset;
}

void Client::setClientName(const QString &s)
{
	d->clientName = s;
}

void Client::setClientVersion(const QString &s)
{
	d->clientVersion = s;
}

void Client::setCapsNode(const QString &s)
{
	d->capsNode = s;
}

void Client::setCapsVersion(const QString &s)
{
	d->capsVersion = s;
}

DiscoItem::Identity Client::identity()
{
	return d->identity;
}

void Client::setIdentity(DiscoItem::Identity identity)
{
	d->identity = identity;
}

void Client::setFeatures(const Features& f)
{
	d->features = f;
}

const Features& Client::features() const
{
	return d->features;
}

void Client::addExtension(const QString& ext, const Features& features)
{
	if (!ext.isEmpty()) {
		d->extension_features[ext] = features;
		d->capsExt = extensions().join(" ");
	}
}

void Client::removeExtension(const QString& ext)
{
	if (d->extension_features.contains(ext)) {
		d->extension_features.remove(ext);
		d->capsExt = extensions().join(" ");
	}
}

QStringList Client::extensions() const
{
	return d->extension_features.keys();
}

const Features& Client::extension(const QString& ext) const
{
	return d->extension_features[ext];
}

void Client::s5b_incomingReady()
{
	S5BConnection *c = d->s5bman->takeIncoming();
	if(!c)
		return;
	if(!d->ftman) {
		c->close();
		c->deleteLater();
		return;
	}
	d->ftman->s5b_incomingReady(c);
}

void Client::ibb_incomingReady()
{
	IBBConnection *c = d->ibbman->takeIncoming();
	if(!c)
		return;
	c->deleteLater();
}

//---------------------------------------------------------------------------
// LiveRosterItem
//---------------------------------------------------------------------------
LiveRosterItem::LiveRosterItem(const Jid &jid)
:RosterItem(jid)
{
	setFlagForDelete(false);
}

LiveRosterItem::LiveRosterItem(const RosterItem &i)
{
	setRosterItem(i);
	setFlagForDelete(false);
}

LiveRosterItem::~LiveRosterItem()
{
}

void LiveRosterItem::setRosterItem(const RosterItem &i)
{
	setJid(i.jid());
	setName(i.name());
	setGroups(i.groups());
	setSubscription(i.subscription());
	setAsk(i.ask());
	setIsPush(i.isPush());
}

ResourceList & LiveRosterItem::resourceList()
{
	return v_resourceList;
}

ResourceList::Iterator LiveRosterItem::priority()
{
	return v_resourceList.priority();
}

const ResourceList & LiveRosterItem::resourceList() const
{
	return v_resourceList;
}

ResourceList::ConstIterator LiveRosterItem::priority() const
{
	return v_resourceList.priority();
}

bool LiveRosterItem::isAvailable() const
{
	if(v_resourceList.count() > 0)
		return true;
	return false;
}

const Status & LiveRosterItem::lastUnavailableStatus() const
{
	return v_lastUnavailableStatus;
}

bool LiveRosterItem::flagForDelete() const
{
	return v_flagForDelete;
}

void LiveRosterItem::setLastUnavailableStatus(const Status &s)
{
	v_lastUnavailableStatus = s;
}

void LiveRosterItem::setFlagForDelete(bool b)
{
	v_flagForDelete = b;
}

//---------------------------------------------------------------------------
// LiveRoster
//---------------------------------------------------------------------------
LiveRoster::LiveRoster()
:QList<LiveRosterItem>()
{
}

LiveRoster::~LiveRoster()
{
}

void LiveRoster::flagAllForDelete()
{
	for(Iterator it = begin(); it != end(); ++it)
		(*it).setFlagForDelete(true);
}

LiveRoster::Iterator LiveRoster::find(const Jid &j, bool compareRes)
{
	Iterator it;
	for(it = begin(); it != end(); ++it) {
		if((*it).jid().compare(j, compareRes))
			break;
	}
	return it;
}

LiveRoster::ConstIterator LiveRoster::find(const Jid &j, bool compareRes) const
{
	ConstIterator it;
	for(it = begin(); it != end(); ++it) {
		if((*it).jid().compare(j, compareRes))
			break;
	}
	return it;
}

}
