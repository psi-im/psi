/*
 * sxemanager.cpp - Whiteboard manager
 * Copyright (C) 2006  Joonas Govenius
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
 
#include "sxemanager.h"
#include "psipopup.h"
#include "psioptions.h"
#include "common.h"
#include "capsmanager.h"
#include <QUrl>

#define ONETOONEPREFIXSELF "0"
#define ONETOONEPREFIXOTHER "1"

using namespace XMPP;

//----------------------------------------------------------------------------
// SxeManager
//----------------------------------------------------------------------------

SxeManager::SxeManager(Client* client, PsiAccount* pa) : client_(client) {
	sxeId_ = QTime::currentTime().toString("z").toInt();

	pa_ = pa;

	client_->addExtension("sxe", Features(SXENS));

	connect(client_, SIGNAL(messageReceived(const Message &)), SLOT(messageReceived(const Message &)));
	connect(client_, SIGNAL(groupChatLeft(const Jid &)), SLOT(groupChatLeft(const Jid &)));
	// connect(client_, SIGNAL(groupChatJoined(const Jid &, const Jid &)), SLOT(groupChatJoined(const Jid &, const Jid &)));

	negotiationTimer_.setSingleShot(true);
	negotiationTimer_.setInterval(120000);
	connect(&negotiationTimer_, SIGNAL(timeout()), SLOT(negotiationTimeout()));
}

void SxeManager::addInvitationCallback(bool (*callback)(const Jid &peer, const QList<QString> &features)) {
	invitationCallbacks_ += callback;
}

void SxeManager::messageReceived(const Message &message) {
	// only process messages that contain a <sxe/> with a nonempty
	// 'session' attribute and that are addressed to this particular account
	if(!message.sxe().attribute("session").isEmpty()) {

		// skip messages from self
		if(ownJids_.contains(message.from().full())) {
			qDebug("from self");
			return;
		}

		// Check if the <sxe/> contains a <negotiation/>
		if(message.sxe().elementsByTagName("negotiation").length() > 0) {
			processNegotiationMessage(message);
			// processNegotiationMessage() will also pass regular SXE edits in the message
			// to the session so we're done
			return;
		}

		// otherwise, try finding a matching session for the session if new one not negotiated
		SxeSession* w = findSession(message.sxe().attribute("session"));

		if(w) {
			// pass the message to the session if already established
			w->processIncomingSxeElement(message.sxe(), message.sxe().attribute("id"));
		} else {
			// otherwise record the session id as a "detected session"
			recordDetectedSession(message);
		}

	}
}

void SxeManager::recordDetectedSession(const Message &message) {
	// check if a record of the session exists
	foreach(DetectedSession d, DetectedSessions_) {
		if(d.session == message.sxe().attribute("session")
			&& d.jid.compare(message.from(), message.type() != "groupchat"))
			return;
	}

	// store a record of a detected session
	DetectedSession detected;
	detected.session = message.sxe().attribute("session");
	if(message.type() == "groupchat")
		detected.jid = message.from().bare();
	else
		detected.jid = message.from();
	detected.time = QTime::currentTime();
	DetectedSessions_.append(detected);
}

void SxeManager::removeSession(SxeSession* session) {
	sessions_.removeAll(session);

	// cancel possible negotiations
	foreach(SxeNegotiation* negotiation, negotiations_.values(session->session())) {
		if(negotiation->target.compare(session->target(), true))
			abortNegotiation(negotiation);
	}

	// notify the target
	QDomDocument doc;
	QDomElement sxe = doc.createElementNS(SXENS, "sxe");
	sxe.setAttribute("session", session->session());
	QDomElement negotiation = doc.createElementNS(SXENS, "negotiation");
	negotiation.appendChild(doc.createElementNS(SXENS, "left-session"));
	sxe.appendChild(negotiation);
	sendSxe(sxe, session->target(), session->groupChat());

	// delete the session
	session->deleteLater();
}

bool SxeManager::processNegotiationAsParticipant(const QDomNode &negotiationElement, SxeNegotiation* negotiation, QDomNode response) {
	QDomDocument doc = QDomDocument();

	if(negotiationElement.nodeName() == "left-session") {

		if(negotiation->state == SxeNegotiation::Finished)
			emit negotiation->session->peerLeftSession(negotiation->peer);

	} else if(negotiationElement.nodeName() == "abort-negotiation") {

		if(negotiation->state < SxeNegotiation::HistoryOffered
			&& negotiation->state != SxeNegotiation::DocumentBegan) {
			// Abort, as in delete session, if still establishing it and not trying to create a new groupchat session
			if(!(negotiation->groupChat && negotiation->peer.resource().isEmpty())) {
				removeNegotiation(negotiation);
				return false;
			}
		} else {
			// Just remove the "negotation" but keep the session
			negotiation->state = SxeNegotiation::Finished;
		}

	} else if(negotiationElement.nodeName() == "connect-request"
			&& negotiation->state == SxeNegotiation::Finished) {

		// accept all <connect-request/>'s automatically
		// if currently not negotiating with someone else
		negotiation->state = SxeNegotiation::HistoryOffered;
		response.appendChild(doc.createElementNS(SXENS, "history-offer"));

	} else if((negotiationElement.nodeName() == "accept-history"
					&& negotiation->state == SxeNegotiation::HistoryOffered)
			|| (negotiationElement.nodeName() == "accept-invitation"
					&& negotiation->state == SxeNegotiation::InvitationSent)) {

		// If this is a new session (negotiation->state == SxeNegotiation::HistoryOffered),
		// create a new SxeSession
		if(!negotiation->session) {
			negotiation->session = createSxeSession(negotiation->target, negotiation->sessionId, negotiation->ownJid, negotiation->groupChat, negotiation->features);
			negotiation->session->setUUIDPrefix(ONETOONEPREFIXSELF);
			negotiation->session->initializeDocument(negotiation->initialDoc);
		}

		if(!negotiation->session) {
			// Creating a new session failed for some reason.
			abortNegotiation(negotiation);
			return false;
		}

		// Retrieve all the edits to the session so far and start queueing new edits
		QList<const SxeEdit*> snapshot = negotiation->session->startQueueing();

		// append <document-begin/>
		QDomElement documentBegin = doc.createElementNS(SXENS, "document-begin");
		response.appendChild(documentBegin);
		QString prolog = SxeSession::parseProlog(negotiation->session->document());
		if(!prolog.isEmpty()) {
			QUrl::encode(prolog);
			documentBegin.setAttribute("prolog", QString("data:text/xml,%1").arg(prolog));
		}
		if(!negotiation->groupChat) {
			// It's safe to give the other participant a prefix to use in 1-to-1 sessions
			documentBegin.setAttribute("available-uuid-prefix", ONETOONEPREFIXOTHER);
		}
		response.appendChild(documentBegin);

		// append all the SxeEdit's returned by startQueueing()
		foreach(const SxeEdit* e, snapshot) {
			response.appendChild(e->xml(doc));
		}

		// append <documend-end/>
		QDomElement documentEnd = doc.createElementNS(SXENS, "document-end");

		QString usedIds;
		foreach(const QString usedId, negotiation->session->usedSxeIds())
			usedIds += usedId + ";";
		if(usedIds.size() > 0)
			usedIds = usedIds.left(usedIds.size() - 1); // strip the last ";"
		documentEnd.setAttribute("used-sxe-ids", usedIds);

		response.appendChild(documentEnd);

		// Need to "flush" the sxe here before stopping queueing
		if(response.hasChildNodes()) {
			QDomElement sxe = doc.createElementNS(SXENS, "sxe");
			sxe.setAttribute("session", negotiation->sessionId);
			sxe.appendChild(response);
			sendSxe(sxe.toElement(), negotiation->peer, negotiation->groupChat);
			sxe.removeChild(response);
		}
		while(response.hasChildNodes())
			response.removeChild(response.firstChild());

		// we're all set and can stop queueing new edits to the session
		negotiation->state = SxeNegotiation::Finished;
		negotiation->session->stopQueueing();

		// signal that a peer joined
		emit negotiation->session->peerJoinedSession(negotiation->peer);

	} else if(negotiationElement.nodeName() == "decline-invitation" && negotiation->state == SxeNegotiation::InvitationSent) {

		QDomNodeList alternatives = negotiationElement.toElement().elementsByTagName("alternative-session");
		for(int i = 0; i < alternatives.size(); i++) {
			emit alternativeSession(negotiation->target, alternatives.at(i).toElement().text());
		}

		if(!negotiation->groupChat || alternatives.size() > 0) {
			abortNegotiation(negotiation);
			return false;
		}

	}

	return true;
}

bool SxeManager::processNegotiationAsJoiner(const QDomNode &negotiationElement, SxeNegotiation* negotiation, QDomNode response, const Message &message) {
	QDomDocument doc = QDomDocument();

	if(negotiationElement.nodeName() == "abort-negotiation") {

		// Abort, as in delete session, if not trying to join a groupchat session
		if(!(negotiation->groupChat && negotiation->peer.resource().isEmpty())) {
			removeNegotiation(negotiation);
			return false;
		}

	} else if(negotiationElement.nodeName() == "invitation"
				&& negotiation->state == SxeNegotiation::NotStarted) {

		// copy the feature strings to negotiation-features
		for(uint k = 0; k < negotiationElement.childNodes().length(); k++) {
			if(negotiationElement.childNodes().at(k).nodeName() == "feature") {
				negotiation->features += negotiationElement.childNodes().at(k).toElement().text();
			}
		}

		// check if one of the invitation callbacks accepts the invitation.
		foreach(bool (*callback)(const Jid &peer, const QList<QString> &features), invitationCallbacks_) {
			if(callback(negotiation->peer, negotiation->features)) {
				response.appendChild(doc.createElementNS(SXENS, "accept-invitation"));
				negotiation->state = SxeNegotiation::InvitationAccepted;
				return true;
			}
		}
		// othewise abort negotiation
		abortNegotiation(negotiation);
		return false;

	} else if(negotiationElement.nodeName() == "history-offer"
			&& negotiation->state == SxeNegotiation::ConnectionRequested) {

		// accept the first <history-offer/> that arrives in response to a <connect-request/>
		negotiation->state = SxeNegotiation::HistoryAccepted;
		negotiation->peer = message.from();
		response.appendChild(doc.createElementNS(SXENS, "accept-history"));

	} else if(negotiationElement.nodeName() == "document-begin"
			&& (negotiation->state == SxeNegotiation::HistoryAccepted
				|| negotiation->state == SxeNegotiation::InvitationAccepted)) {

		// Create the new SxeSession
		if(!negotiation->session) {
			negotiation->session = createSxeSession(negotiation->target, negotiation->sessionId, negotiation->ownJid, negotiation->groupChat, negotiation->features);

			// The offer may contain a UUID prefix reserved for us
			if(negotiationElement.toElement().hasAttribute("available-uuid-prefix"))
				negotiation->session->setUUIDPrefix(negotiationElement.toElement().attribute("available-uuid-prefix"));
		}

		if(negotiation->session) {
			// set the session to "importing" state which bypasses some version control
			QDomDocument doc;
			if(negotiationElement.toElement().hasAttribute("prolog")) {
				QString prolog = negotiationElement.toElement().attribute("prolog");
				if(prolog.startsWith("data:")) {
					// Assuming non-base64
					prolog = prolog.mid(prolog.indexOf(",") + 1);
					QUrl::decode(prolog);
					doc.setContent(prolog);
				}
			}

			negotiation->session->startImporting(doc);
			negotiation->state = SxeNegotiation::DocumentBegan;
		} else {
			// creating the session failed for some reason
			qDebug("Failed to create session.");
			abortNegotiation(negotiation);
			return false;
		}

	} else if(negotiationElement.nodeName() != "document-end"
			&& negotiation->state == SxeNegotiation::DocumentBegan) {

		// pass the edit to the session
		QDomElement sxe = doc.createElementNS(SXENS, "sxe");
		sxe.setAttribute("session", negotiation->sessionId);
		sxe.appendChild(negotiationElement.cloneNode());
		negotiation->session->processIncomingSxeElement(sxe, QString());

	} else if(negotiationElement.nodeName() == "document-end"
			&& negotiation->state == SxeNegotiation::DocumentBegan) {

		// The initial document has been received and we're done
		negotiation->state = SxeNegotiation::Finished;

		// Decode the 'used-sxe-ids' field
		foreach(QString usedId, negotiationElement.toElement().attribute("used-sxe-ids").split(";"))
			if (usedId.size() > 0) negotiation->session->addUsedSxeId(usedId);

		// Exit the "importing" state so that normal version control resumes
		negotiation->session->stopImporting();

	}

	return true;
}

QPointer<SxeSession> SxeManager::processNegotiationMessage(const Message &message) {

	if(PsiOptions::instance()->getOption("options.messages.ignore-non-roster-contacts").toBool() && message.type() != "groupchat") {
		// Ignore the message if contact not in roster
		if(!pa_->find(message.from())) {
			qDebug("SXE invitation received from contact that is not in roster.");
			return 0;
		}
	}

	// Find or create a negotiation object
	SxeNegotiation* negotiation = findNegotiation(message.from(), message.sxe().attribute("session"));
	if(negotiation) {

		// Only accept further negotiation messages from the source we are already negotiationing with or if we've requested connection to a groupchat session
		if(!negotiation->peer.compare(message.from()) && !(negotiation->groupChat && negotiation->peer.resource().isEmpty())) {
			abortNegotiation(negotiation->sessionId, message.from(), true);
			return 0;
		}

	} else
		negotiation = createNegotiation(message);

	// Prepare the response <sxe/>
	QDomDocument doc;
	QDomElement sxe = doc.createElementNS(SXENS, "sxe");
	sxe.setAttribute("session", negotiation->sessionId);
	QDomElement response = doc.createElementNS(SXENS, "negotiation");

	// Process each child of the <sxe/>
	QDomNode n;
	for(int i = 0; i < message.sxe().childNodes().count(); i++) {
		n = message.sxe().childNodes().at(i);

		// skip non-elements
		if(!n.isElement())
			continue;

		if(n.nodeName() == "negotiation") {

			// Process each child element of <negotiation/>
			for(int j = 0; j < n.childNodes().count(); j++) {

				if(negotiation->role == SxeNegotiation::Participant) {
					if(!processNegotiationAsParticipant(n.childNodes().at(j), negotiation, response))
						return 0;
				} else if(negotiation->role == SxeNegotiation::Joiner) {
					if(!processNegotiationAsJoiner(n.childNodes().at(j), negotiation, response, message))
						return 0;
				} else {
					Q_ASSERT(false);
				}

				// Send any responses that were generated
				if(response.hasChildNodes()) {
					sxe.appendChild(response);
					sendSxe(sxe.cloneNode().toElement(), negotiation->peer, negotiation->groupChat);
					sxe.removeChild(response);
				}

				while(response.hasChildNodes())
					response.removeChild(response.firstChild());
			}

		} else if(negotiation->state == SxeNegotiation::Finished
				&& negotiation->session) {

			// There should be no more children after <negotiation/>...
			qDebug("Children after <negotiation/> in <sxe/>.");

		}
	}


	// Cleanup:
	// Delete negotation objects that are no longer needed
	if(negotiation) {

		if(negotiation->state == SxeNegotiation::NotStarted) {

			removeNegotiation(negotiation);

		} else if(negotiation->state == SxeNegotiation::Finished) {

			// Save session for successful negotiations but delete the negotiation object
			SxeSession* sxesession = negotiation->session;
			if(sxesession) {
				if(!sessions_.contains(sxesession)) {

					// store and emit a signal about the session only if it's new
					sessions_.append(sxesession);
					emit sessionNegotiated(sxesession);

				}
			}

			removeNegotiation(negotiation);

			// return a handle to the session
			return sxesession;

		}
	}
	return 0;
}

SxeManager::SxeNegotiation* SxeManager::findNegotiation(const Jid &jid, const QString &session) {
	QList<SxeNegotiation*> negotiations = negotiations_.values(session);
	foreach(SxeNegotiation* negotiation, negotiations) {
		if(negotiation->state != SxeNegotiation::Aborted
			&& negotiation->peer.compare(jid, negotiation->state != SxeNegotiation::ConnectionRequested))
			return negotiation;
	}

	return 0;
}

void SxeManager::removeNegotiation(SxeNegotiation* negotiation) {
	negotiations_.remove(negotiation->sessionId, negotiation);
	delete negotiation;
}

SxeManager::SxeNegotiation* SxeManager::createNegotiation(SxeNegotiation::Role role, SxeNegotiation::State state, const QString &sessionId, const Jid &target, const Jid &ownJid, bool groupChat) {
	SxeNegotiation* negotiation = new SxeNegotiation;
	negotiation->role = role;
	negotiation->state = state;
	negotiation->sessionId = sessionId;
	negotiation->target = target;
	negotiation->peer = target;
	negotiation->ownJid = ownJid;
	negotiation->groupChat = groupChat;
	negotiation->session = 0;

	negotiations_.insert(sessionId, negotiation);

	return negotiation;
}

SxeManager::SxeNegotiation* SxeManager::createNegotiation(const Message &message) {

	// Create a new negotiation object

	SxeNegotiation* negotiation = new SxeNegotiation;

	negotiation->sessionId = message.sxe().attribute("session");
	negotiation->session = findSession(negotiation->sessionId);

	negotiation->peer = message.from();

	if(negotiation->session) {

		// If negotiation exists, we're going to be the "server" for the negotiation
		negotiation->role = SxeNegotiation::Participant;
		negotiation->state = SxeNegotiation::Finished;
		negotiation->target = negotiation->session->target();
		negotiation->groupChat = negotiation->session->groupChat();
		negotiation->ownJid = negotiation->session->ownJid();
		negotiation->features = negotiation->session->features();

	} else {

		// Otherwise we're joining a session
		negotiation->role = SxeNegotiation::Joiner;
		negotiation->state = SxeNegotiation::NotStarted;

		if(message.type() == "groupchat") {

			// If we're being invited from a groupchat,
			// ownJid is determined based on the bare part of ownJids_

			negotiation->groupChat = true;
			foreach(QString j, ownJids_) {
				if(message.from().bare() == j.left(j.indexOf("/"))) {
					negotiation->ownJid = j;
					break;
				}
			}
			// Also, the target is just the bare JID in a groupchat
			negotiation->target = message.from().bare();

		} else {

			negotiation->groupChat = false;
			negotiation->target = negotiation->peer;

		}

		if(negotiation->ownJid.isEmpty())
			negotiation->ownJid = message.to();

	}

	// Reset the timeout
	negotiationTimer_.start();

	// Store the session
	negotiations_.insert(negotiation->sessionId, negotiation);

	return negotiation;
}

void SxeManager::joinSession(const Jid &target, const Jid &ownJid, bool groupChat, const QString &session) {
	// Prepare the <connect-request/>
	QDomDocument doc;
	QDomElement sxe = doc.createElementNS(SXENS, "sxe");
	sxe.setAttribute("session", session);
	QDomElement negotiationElement = doc.createElementNS(SXENS, "negotiation");
	QDomElement request = doc.createElementNS(SXENS, "connect-request");
	negotiationElement.appendChild(request);
	sxe.appendChild(negotiationElement);

	// Create the negotiation object
	createNegotiation(SxeNegotiation::Joiner, SxeNegotiation::ConnectionRequested, session, target, ownJid, groupChat);

	sendSxe(sxe, target, groupChat);

	// Reset the timeout for negotiations
	negotiationTimer_.start();

	return;
}

void SxeManager::startNewSession(const Jid &target, const Jid &ownJid, bool groupChat, const QDomDocument &initialDoc, QList<QString> features) {

	// check that the target supports SXE and all specified features
	if(!checkSupport(target, features)) {
		qDebug() << QString("Tried to start an SXE session with %1 but the client doesn't support all features.").arg(target.full()).toAscii();
		return;
	}

	// generate a session identifier
	QString session;
	do {
		session = SxeSession::generateUUID();
	} while (findSession(session));

	if(features.size() == 0) {
		// some features must be specified
		return;
	}

	// Prepare the <invitation/>
	QDomDocument doc;
	QDomElement sxe = doc.createElementNS(SXENS, "sxe");
	sxe.setAttribute("session", session);
	QDomElement negotiationElement = doc.createElementNS(SXENS, "negotiation");
	QDomElement request = doc.createElementNS(SXENS, "invitation");
	QDomElement feature = doc.createElementNS(SXENS, "feature");
	foreach(QString f, features) {
		feature = feature.cloneNode(false).toElement();
		feature.appendChild(doc.createTextNode(f));
		request.appendChild(feature);
	}
	negotiationElement.appendChild(request);
	sxe.appendChild(negotiationElement);

	// Create the negotiation object
	SxeNegotiation* negotiation = createNegotiation(SxeNegotiation::Participant, SxeNegotiation::InvitationSent, session, target, ownJid, groupChat);
	negotiation->initialDoc = initialDoc;
	negotiation->features = features;

	sendSxe(sxe, target, groupChat);

	// Reset the timeout for negotiations
	negotiationTimer_.start();

	return;
}

void SxeManager::negotiationTimeout() {
	foreach(SxeNegotiation* negotiation, negotiations_.values()){
		if(negotiation->role == SxeNegotiation::Participant && negotiation->state < SxeNegotiation::HistoryOffered && negotiation->state != SxeNegotiation::DocumentBegan) {
			if(negotiation->session)
				negotiation->session->endSession();
			abortNegotiation(negotiation->sessionId, negotiation->peer, negotiation->groupChat);
		}
		delete negotiation;
	}

	negotiations_.clear();
}

// #include <QTextStream>
void SxeManager::sendSxe(QDomElement sxe, const Jid & receiver, bool groupChat) {

	SxeSession* session = qobject_cast<SxeSession*>(sender());

	// Add a unique id to each sent sxe element
	if(session) {
		QString id = session->generateUUIDForSession();
		sxe.setAttribute("id", id);
		session->addUsedSxeId(id);
	} else
		sxe.setAttribute("id", SxeSession::generateUUID());

	Message m(receiver);
	m.setSxe(sxe);
	if(groupChat && receiver.resource().isEmpty())
		m.setType("groupchat");

	if(client_->isActive()) {
		// send queued messages first
		while(!queuedMessages_.isEmpty())
			client_->sendMessage(queuedMessages_.takeFirst());

		client_->sendMessage(m);
	} else {
		queuedMessages_.append(m);
	}
}

QList< QPointer<SxeSession> > SxeManager::findSession(const Jid &jid) {
	// find if a session for the jid already exists
	QList< QPointer<SxeSession> > matching;
	foreach(QPointer<SxeSession> w, sessions_) {
		// does the jid match?
		if(w->target().compare(jid)) {
			matching.append(w);
		}
	}
	return matching;
}

QPointer<SxeSession> SxeManager::findSession(const QString &session) {
	// find if a session for the session already exists
	foreach(SxeSession* w, sessions_) {
		// does the session match?
		if(w->session() == session)
			return w;
	}
	return 0;
}

QPointer<SxeSession> SxeManager::createSxeSession(const Jid &target, QString session, const Jid &ownJid, bool groupChat, const QList<QString> &features) {
	if(session.isEmpty() || !target.isValid())
		return 0;
	if(!ownJids_.contains(ownJid.full()))
		ownJids_.append(ownJid.full());
	// FIXME: detect serverside support
	bool serverSupport = false;
	// create the SxeSession
	QPointer<SxeSession> w = new SxeSession(target, session, ownJid, groupChat, serverSupport, features);
	// connect the signals
	connect(w, SIGNAL(newSxeElement(QDomElement, Jid, bool)), SLOT(sendSxe(const QDomElement &, const Jid &, bool)));
	connect(w, SIGNAL(sessionEnded(SxeSession*)), SLOT(removeSession(SxeSession*)));
	removeDetectedSession(w);
	return w;
	// Note: the session should be added to sessions_ once negotiation is finished
}

void SxeManager::abortNegotiation(QString session, const Jid &peer, bool groupChat) {
	QDomDocument doc = QDomDocument();
	QDomElement sxe = doc.createElementNS(SXENS, "sxe");
	sxe.setAttribute("session", session);
	QDomElement negotiationElement = doc.createElementNS(SXENS, "negotiation");
	negotiationElement.appendChild(doc.createElementNS(SXENS, "abort-negotiation"));
	sxe.appendChild(negotiationElement);
	sendSxe(sxe, peer, groupChat);
}

void SxeManager::abortNegotiation(SxeNegotiation* negotiation) {
	abortNegotiation(negotiation->sessionId, negotiation->peer, negotiation->groupChat);

	removeNegotiation(negotiation);
}

void SxeManager::removeDetectedSession(SxeSession* session) {
	for(int i = 0; i < DetectedSessions_.size(); i++) {
		DetectedSession detected = DetectedSessions_.at(i);
		// Remove the specified session from the list
		if(detected.session == session->session() && detected.jid.compare(session->target(), true))
			DetectedSessions_.removeAt(i);
		else if(detected.time.secsTo(QTime::currentTime()) > 1800)
			// Remove detected session that are old
			DetectedSessions_.removeAt(i);
	}
}

void SxeManager::groupChatLeft(const Jid &jid) {
	for(int i = 0; i < ownJids_.size(); i++) {
		if(jid.bare() == ownJids_.at(i).left(ownJids_.at(i).indexOf("/")))
			ownJids_.removeAt(i);
	}
	QList< QPointer<SxeSession> > matching = findSession(jid);
	foreach(QPointer<SxeSession> w, matching)
		w->endSession();
}

void SxeManager::groupChatJoined(const Jid &, const Jid &ownJid) {
	if(!ownJids_.contains(ownJid.full()))
		ownJids_.append(ownJid.full());
}

bool SxeManager::checkSupport(const Jid &jid, const QList<QString> &features) {
	QStringList supported = pa_->capsManager()->features(jid).list();

	if(!supported.contains(SXENS))
		return false;

	foreach(QString f, features) {
		if(!supported.contains(f))
			return false;
	}

	return true;
}
