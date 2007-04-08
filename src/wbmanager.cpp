/*
 * wbmanager.cpp - Whiteboard manager
 * Copyright (C) 2006  Joonas Govenius
 *
 * Influenced by:
 * pepmanager.cpp - Classes for PEP
 * Copyright (C) 2006  Remko Troncon
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
 
#include "wbmanager.h"
#include "psipopup.h"

using namespace XMPP;

// -----------------------------------------------------------------------------

WbManager::WbManager(Client* client, PsiAccount* pa) : client_(client) {
	wbHash_ = QTime::currentTime().toString("z").toInt();

	// Supported SVG features
	supportedFeatures_.append("http://www.w3.org/TR/SVG11/feature#CoreAttribute");
	supportedFeatures_.append("http://www.w3.org/TR/SVG11/feature#BasicStructure");
	supportedFeatures_.append("http://www.w3.org/TR/SVG11/feature#BasicPaintAttribute");
	supportedFeatures_.append("http://www.w3.org/TR/SVG11/feature#Shape");
	supportedFeatures_.append("http://www.w3.org/TR/SVG11/feature#BasicText");
	supportedFeatures_.append("http://www.w3.org/TR/SVG11/feature#Image");

	pa_ = pa;
	connect(client_, SIGNAL(messageReceived(const Message &)), SLOT(messageReceived(const Message &)));
	connect(client_, SIGNAL(groupChatLeft(const Jid &)), SLOT(groupChatLeft(const Jid &)));
	connect(client_, SIGNAL(groupChatJoined(const Jid &, const Jid &)), SLOT(groupChatJoined(const Jid &, const Jid &)));

	negotiationTimer_.setSingleShot(true);
	negotiationTimer_.setInterval(120000);
	connect(&negotiationTimer_, SIGNAL(timeout()), SLOT(negotiationTimeout()));
}

void WbManager::openWhiteboard(const Jid &target, const Jid &ownJid, bool groupChat) {
	// See if we have a session for the JID
	WbDlg* w = findWbDlg(target);
	// else negotiate a new session and return null
	if(!w)
		startNegotiation(target, ownJid, groupChat);
	else
		bringToFront(w);
}

void WbManager::messageReceived(const Message &message) {
	// only process messages that contain a <wb/> with a nonempty 'session' attribute and that are addressed to this particular account
	if(!message.wb().attribute("session").isEmpty()) {
		// skip messages from self
		if(ownJids_.contains(message.from().full())) {
			qDebug("from self");
			return;
		}
		bool recordSessionId = false;
		// Don't process delayed messages (chat history) but remember the session id
		if(!message.spooled()) {
	// 		qDebug(QString("<wb/> to session %1 from %2").arg(message.wb().attribute("session")).arg(message.from().full()).toAscii());
			WbDlg* w = 0;
			// Check if the <wb/> contains a <protocol/>
			QDomNodeList children = message.wb().childNodes();
			for(uint i=0; i < children.length(); i++) {
				if(children.item(i).nodeName() == "protocol") {
					w = negotiateSession(message);
				}
			}
			// try finding a matching dialog for the session if new one not negotiated
			if(!w) {
				w = findWbDlg(message.wb().attribute("session"));
				if(w) {
					// only pass the message to existing sessions, not to newly negotiated ones.
					w->incomingWbElement(message.wb(), message.from());
				} else
					recordSessionId = true;
			}
			if (w) {
	// 			PsiPopup::PopupType popupType = PsiPopup::AlertNone;
	//			bool doPopup = false;
				pa_->playSound(option.onevent[eChat2]);
	// 			if(!w->isActiveWindow() || option.alertOpenChats) {
	// 				popupType = PsiPopup::AlertChat;
	// 				if(option.popupChats) {
	// 					if(!option.noUnlistedPopup && message.type() != "groupchat") {
	// 						// don't popup wb's from unlisted contacts
	// 						if(!pa_->find(message.from()))
	// 							return;
	// 					}
						w->show();
	// 				}
	// 			}
				if(option.raiseChatWindow) {
					bringToFront(w);
				}
	// 			if ((popupType == PsiPopup::AlertChat && option.ppChat) /*&& makeSTATUS(status()) != STATUS_DND*/) {
	// 				PsiPopup *popup = new PsiPopup(popupType, pa_);
	// 				popup->setData(j, r, u, e);
	// 			}
	// #if defined(Q_WS_MAC) && defined(HAVE_GROWL)
	// 			PsiGrowlNotifier::instance()->popup(this, popupType, j, r, u, e);
	// #endif
			}
		} else
			recordSessionId = true;
		if(recordSessionId) {
			// check if a record of the session exists
			bool alreadyRecorded = false;
			foreach(detectedSession d, detectedSessions_) {
				if(d.session == message.wb().attribute("session")) {
					alreadyRecorded = true;
					break;
				}
			}
			if(!alreadyRecorded) {
				// store a record of a detected session
				detectedSession detected;
				detected.session = message.wb().attribute("session");
				if(message.type() == "groupchat")
					detected.jid = message.from().bare();
				else
					detected.jid = message.from();
				detected.time = QTime::currentTime();
				detectedSessions_.append(detected);
			}
		}
	}
}

void WbManager::removeSession(const QString &session) {
	WbDlg* d;
	for(int i = 0; i < dialogs_.size(); i++) {
		// does the session match?
		if(dialogs_.at(i)->session() == session) {
			d = dialogs_.takeAt(i);

			QDomDocument doc;
			QDomElement wb = doc.createElementNS("http://jabber.org/protocol/svgwb", "wb");
			wb.setAttribute("session", session);
			QDomElement protocol = doc.createElement("protocol");
			protocol.appendChild(doc.createElement("left-session"));
			wb.appendChild(protocol);
			sendMessage(wb, d->target(), d->groupChat());
			break;
		}
	}
	if(d) {
		if(negotiations_.contains(session)) {
			sendAbortNegotiation(session, negotiations_[session]->peer, d->groupChat());
			negotiations_.remove(session);
		}
		// FIXME: Delete the dialog
		setAttribute(Qt::WA_DeleteOnClose);
		d->close();
//		d->deleteLater();

	}
}

WbDlg* WbManager::negotiateSession(const Message &message) {
	QString session = message.wb().attribute("session");
	if(session.isEmpty())
		return 0;

	if(option.ignoreNonRoster && message.type() != "groupchat") {
		// Ignore the message if contact not in roster
		if(!pa_->find(message.from())) {
			qDebug("Whiteboard invitation received from contact that is not in roster.");
			return 0;
		}
	}

	QDomDocument doc;
	QDomElement wb = doc.createElementNS("http://jabber.org/protocol/svgwb", "wb");
	wb.setAttribute("session", session);
	QDomElement protocol = doc.createElement("protocol");
	wb.appendChild(protocol);

	// Find or create a negotiation process
qDebug("1");
	WbNegotiation* negotiation = negotiations_[session];
	if(negotiation) {
		// Only accept further negotiation messages from the source we are already negotiationing with or if we've requested connection to a groupchat session
		if(!negotiation->peer.compare(message.from()) && !(negotiation->groupChat && negotiation->peer.resource().isEmpty())) {
			qDebug("1.1");
// 			if(negotiation->groupChat) {
// 				// If it is a history offer, turn it down
// 				for(uint i = 0; i < message.wb().childNodes().length(); i++) {
// 					if(message.wb().childNodes().at(i).nodeName() == "protocol") {
// 						for(uint j = 0; j < message.wb().childNodes().at(i).childNodes().length(); j++) {
// 							if(message.wb().childNodes().at(i).childNodes().at(j).nodeName() == "history-offer") {
								sendAbortNegotiation(session, message.from(), true);
// 							}
// 						}
// 					}
// 				}
// 			}
			return 0;
		}
	} else {
// qDebug("2");
		negotiation = new WbNegotiation;
		negotiation->dialog = findWbDlg(session);
		negotiation->peer = message.from();
		if(negotiation->dialog) {
// qDebug("3");
			negotiation->role = WbNegotiation::Participant;
			negotiation->state = WbNegotiation::Finished;
			negotiation->target = negotiation->dialog->target();
			negotiation->groupChat = negotiation->dialog->groupChat();
			negotiation->ownJid = negotiation->dialog->ownJid();
		} else {
			if(message.type() == "groupchat") {
				negotiation->groupChat = true;
				// If we're being invited from a groupchat,
				// ownJid is determined based on the "bare" part of ownJids_
				foreach(QString j, ownJids_) {
					if(message.from().bare() == j.left(j.indexOf("/"))) {
						negotiation->ownJid = j;
						break;
					}
				}
				// Also, the target is just the "bare" JID in a groupchat
				negotiation->target = message.from().bare();
			} else {
				negotiation->groupChat = false;
				negotiation->target = negotiation->peer;
			}
			if(negotiation->ownJid.isEmpty())
				negotiation->ownJid = message.to();
			negotiation->role = WbNegotiation::Joiner;
			negotiation->state = WbNegotiation::NotStarted;
		}
		// Reset the timeout
		negotiationTimer_.start();
		// Store the session
		negotiations_[session] = negotiation;
	}

	// Process the children of the <wb/>
	QDomNode n, m;
	for(int i = 0; i < message.wb().childNodes().count(); i++) {
		n = message.wb().childNodes().at(i);
// qDebug("4.121");
		if(!n.isElement())
			continue;
// qDebug("4.12");
		if(n.nodeName() == "protocol") {
// qDebug("4.123");
			for(int j = 0; j < n.childNodes().count(); j++) {
				m = n.childNodes().at(j);
// qDebug("4.1234");
// 				protocol = protocol.cloneNode(false).toElement();
				while(protocol.hasChildNodes())
					protocol.removeChild(protocol.firstChild());
				// Check if <protocol/> contains <left-session/>
				if(m.nodeName() == "left-session") {
					if(negotiation->state == WbNegotiation::Finished) {
						// Only end session if it's a 1-to-1 session
						if(!negotiation->groupChat) {
							negotiation->dialog->peerLeftSession();
						}
					}
				} else if(m.nodeName() == "abort-negotiation") {
					if(negotiation->role == WbNegotiation::Participant && negotiation->state < WbNegotiation::HistoryOffered && negotiation->state != WbNegotiation::DocumentBegan) {
						// Abort, as in delete session, if still establishing it and not trying to create a new groupchat session
						if(!(negotiation->groupChat && negotiation->peer.resource().isEmpty()))
							negotiation->state = WbNegotiation::Aborted;
					} else {
						// Just remove the "negotation" but keep the dialog if sending history was cancelled
						negotiation->state = WbNegotiation::Finished;
					}
				} else if(m.nodeName() == "invitation" && negotiation->state == WbNegotiation::NotStarted) {
// qDebug("4.1234567");
					bool accept = true;
					// Reject invitations from a source that we already have a session to
					if(findWbDlg(negotiation->target))
						accept = false;
					else {
						// Check if the proposed features are supported
						for(uint k = 0; k < m.childNodes().length(); k++) {
							if(m.childNodes().at(k).nodeName() == "feature") {
								if(!supportedFeatures_.contains(m.childNodes().at(k).toElement().text()))
									accept = false;
							}
						}
					}
					if(accept) {
						protocol.appendChild(doc.createElement("accept-invitation"));
						negotiation->state = WbNegotiation::InvitationAccepted;
					} else {
						protocol.appendChild(doc.createElement("abort-negotiation"));
						negotiation->state = WbNegotiation::Aborted;
					}
				} else if(m.nodeName() == "connect-request" && negotiation->state == WbNegotiation::Finished) {
					negotiation->state = WbNegotiation::HistoryOffered;
					protocol.appendChild(doc.createElement("history-offer"));
// qDebug("7");
				} else if(m.nodeName() == "history-offer" && negotiation->state == WbNegotiation::ConnectionRequested) {
// qDebug("5");
					negotiation->state = WbNegotiation::HistoryAccepted;
					negotiation->peer = message.from();
					protocol.appendChild(doc.createElement("accept-history"));
				} else if(m.nodeName() == "document-begin" && (negotiation->state == WbNegotiation::HistoryAccepted || negotiation->state == WbNegotiation::InvitationAccepted)) {
					if(!negotiation->dialog) {
// 						qDebug(QString("%1  %2  %3  %4").arg(negotiation->target.full()).arg(session).arg(negotiation->ownJid.full()).toAscii());
						negotiation->dialog = createWbDlg(negotiation->target, session, negotiation->ownJid, negotiation->groupChat);
					}
// 					negotiation->dialog->setAllowEdits(false);
					if(negotiation->dialog) {
						negotiation->dialog->setImporting(true);
						negotiation->state = WbNegotiation::DocumentBegan;
					} else
						negotiation->state = WbNegotiation::Aborted;
				} else if(m.nodeName() == "document-end" && negotiation->state == WbNegotiation::DocumentBegan) {
					negotiation->state = WbNegotiation::Finished;
					for(int k = 0; k < m.childNodes().count(); k++) {
						if(m.childNodes().at(k).nodeName() == "last-wb") {
							QDomElement last = m.childNodes().at(k).toElement();
							negotiation->dialog->eraseQueueUntil(last.attribute("sender"), last.attribute("hash"));
							break;
						}
					}
					negotiation->dialog->setImporting(false);
// 						negotiation->dialog->setAllowEdits(true);
				} else if ((m.nodeName() == "accept-history" && negotiation->state == WbNegotiation::HistoryOffered) || (m.nodeName() == "accept-invitation" && negotiation->state == WbNegotiation::InvitationSent)) {
// qDebug("8");
					if(!negotiation->dialog) {
						negotiation->dialog = createWbDlg(negotiation->target, session, negotiation->ownJid, negotiation->groupChat);
						if(!negotiation->dialog) {
							sendAbortNegotiation(session, negotiation->peer, negotiation->groupChat);
							return 0;
						}
					}
					negotiation->dialog->setQueueing(true);
					// create the wb element with the history of the whiteboard
					QDomElement history = doc.createElementNS("http://jabber.org/protocol/svgwb", "wb");
					history.setAttribute("session", session);
					QDomElement documentBegin = doc.createElement("protocol");
					documentBegin.appendChild(doc.createElement("document-begin"));
					QDomElement documentEnd = doc.createElement("protocol");
					documentEnd.appendChild(doc.createElement("document-end"));
					QDomElement lastWb = doc.createElement("last-wb");
					lastWb.setAttribute("sender", negotiation->dialog->lastWb()["sender"]);
					lastWb.setAttribute("hash", negotiation->dialog->lastWb()["hash"]);
					documentEnd.appendChild(lastWb);
// qDebug("9");
					// append <document-begin/>
					history.appendChild(documentBegin);
					// append <new/> and <configure/>s or each element
					foreach(WbItem* item, negotiation->dialog->snapshot()) {
// qDebug("10");
						QString oldParent = item->parentWbItem();
						QDomNode insertReference = history.lastChild();
						QDomElement newElement = item->svg();
						QDomElement configure;
						int version = -1;
						// do each of the undos and simultaneously create <configure/>'s that represent the edits
						for(int k = item->undos.size() - 1; k >= 0; k--) {
// qDebug("11");
							// Note: don't remove the undos
							EditUndo u = item->undos.at(k);
							// only create one <configure/> per version
							if(version != u.version) {
// qDebug("12");
								// append the <configure/> for previous version
								if(configure.hasChildNodes()) {
// qDebug("13");
									if(insertReference.isNull()) {
// qDebug("14");
										history.insertBefore(configure.cloneNode(), insertReference);
									} else
										history.insertAfter(configure.cloneNode(), insertReference);
								}
								// create the <configure/> for the older version
								version = u.version;
								configure = doc.createElement("configure");
								configure.setAttribute("target", item->id());
								configure.setAttribute("version", version);
							}
							if(u.type == Edit::AttributeEdit) {
// qDebug("15");
								// Create the edit
								QDomElement attributeEdit = doc.createElement("attribute");
								attributeEdit.setAttribute("name", u.attribute);
								attributeEdit.appendChild(doc.createTextNode(newElement.attribute(u.attribute)));
								configure.insertBefore(attributeEdit, QDomNode());
								// Do the undo
								if(u.oldValue.isNull()) {
// qDebug("16");
									newElement.removeAttribute(u.attribute);
								} else
									newElement.setAttribute(u.attribute, u.oldValue);
							} else if(u.type == Edit::ContentEdit) {
// qDebug("17");
								// Create the edit and do the undo
								QDomElement contentEdit = doc.createElement("content");
								while(newElement.hasChildNodes()) {
// qDebug("18");
									contentEdit.appendChild(newElement.firstChild());
									newElement.removeChild(newElement.firstChild());
								}
								configure.insertBefore(contentEdit, QDomNode());
								for(uint j=0; j < u.oldContent.length(); j++) {
// qDebug("19");
									newElement.appendChild(u.oldContent.at(j));
								}
							} else if(u.type == Edit::ParentEdit) {
// qDebug("20");
								// Create the edit
								QDomElement parentEdit = doc.createElement("parent");
								parentEdit.appendChild(doc.createTextNode(oldParent));
								configure.insertBefore(parentEdit, QDomNode());
								// Do the undo
								oldParent = u.oldParent;
							}
						}
// qDebug("21");
						// Append the last <configure/>
						if(configure.hasChildNodes()) {
// qDebug("22");
							if(insertReference.isNull()) {
								history.insertBefore(configure.cloneNode(), insertReference);
							} else
								history.insertAfter(configure.cloneNode(), insertReference);
						}
// qDebug("23");
						// Create and insert the <new/> element before the <configure/>s created above.
						if(item->id() != "root") {
							QDomElement newEdit = doc.createElement("new");
							newEdit.setAttribute("id", item->id());
							newEdit.setAttribute("parent", oldParent);
							newEdit.setAttribute("index", item->index());
							newEdit.appendChild(newElement);
							if(insertReference.isNull()) {
// qDebug("24");
								history.insertBefore(newEdit, insertReference);
							} else
								history.insertAfter(newEdit, insertReference);
						}
					}
// qDebug("25");
					// append <protocol><documend-end/><last-edit/></protocol>
					history.appendChild(documentEnd);

					sendMessage(history, negotiation->peer, negotiation->groupChat);
					negotiation->state = WbNegotiation::Finished;
					negotiation->dialog->setQueueing(false);
				} /*else if(m.nodeName() == "decline-invitation" && negotiation->state == WbNegotiation::ConnectionRequested) {
					// Create the WbDlg
					negotiation->state = WbNegotiation::Aborted;
				}*/
				if(protocol.hasChildNodes()) {
					sendMessage(wb.cloneNode().toElement(), negotiation->peer, negotiation->groupChat);
				}
			}
		} else if((negotiation->state == WbNegotiation::DocumentBegan || negotiation->state == WbNegotiation::Finished) && negotiation->dialog) {
			// If state after <document-begin/>,
			// pass the edits to the dialog
			wb.removeChild(protocol);
			wb.appendChild(n);
			negotiation->dialog->incomingWbElement(wb, negotiation->peer);
			wb.removeChild(n);
			wb.appendChild(protocol);
		}
	}
	// Delete erroneous attempts and finished negotiations
	if(negotiation) {
		if(negotiation->state == WbNegotiation::NotStarted) {
			negotiations_.remove(session);
			delete negotiation;
		} else if(negotiation->state == WbNegotiation::Finished) {
			WbDlg* w = 0;
			if(negotiation->dialog) {
				w = negotiation->dialog;
				if(!dialogs_.contains(negotiation->dialog))
					dialogs_.append(negotiation->dialog);
			}
			negotiations_.remove(session);
			delete negotiation;
			return w;
		} else if(negotiation->state == WbNegotiation::Aborted) {
			if(negotiation->dialog)
				negotiation->dialog->endSession();
			negotiations_.remove(session);
			delete negotiation;
			return 0;
		}
	}
	return 0;
}

void WbManager::startNegotiation(const Jid &target, const Jid &ownJid, bool groupChat, QList<QString> features) {
	// generate a session identifier
	QString session;
	// Check if there's detected activity of a session to the specified jid
	// TODO: Make a custom dialog.
	QList<QString> potentialSessions;
	// Remove old detected sessions
	removeDetectedSession(QString());
// 	potentialSessions.append(tr("New session"));
// 	potentialSessions.append(tr("Manual..."));
	foreach(detectedSession detected, detectedSessions_) {
		if(detected.jid.compare(target))
			potentialSessions.append(detected.session);
	}
	bool ok;
	session = QInputDialog::getItem(0, tr("Specify Session"),tr("If you wish to join and existing session,\nchoose from one of the recently detected sessions\nor type in your own. Else leave the field empty."), potentialSessions, 0, true, &ok);
	if(!ok)
		return;
	// else, generate an arbitrary session
	// TODO: this could conflict with other clients starting a session at the same time though it's extremely unlikely
	bool existing = true;
	if(session.isEmpty()) {
		existing = false;
		do {
			session = QTime::currentTime().toString("msz");
		} while (findWbDlg(session));
	}
	// Prepare the list of features
	if(!features.size()) {
		// If none is specified, try all supported features;
		features = supportedFeatures_;
	} else foreach(QString f, features) {
		// Check that all features are actually supported
		if(!supportedFeatures_.contains(f))
			features.removeAll(f);
	}
	// Prepare the connect request
	QDomDocument doc;
	QDomElement wb = doc.createElementNS("http://jabber.org/protocol/svgwb", "wb");
	wb.setAttribute("session", session);
	QDomElement protocol = doc.createElement("protocol");
	QDomElement request;
	if(existing)
		request = doc.createElement("connect-request");
	else {
		request = doc.createElement("invitation");
		QDomElement feature = doc.createElement("feature");
		foreach(QString f, features) {
			feature = feature.cloneNode(false).toElement();
			feature.appendChild(doc.createTextNode(f));
			request.appendChild(feature);
		}
	}
	protocol.appendChild(request);
	wb.appendChild(protocol);
	// Create the negotiation object
	WbNegotiation* negotiation = new WbNegotiation;
	if(existing) {
		negotiation->role = WbNegotiation::Participant;
		negotiation->state = WbNegotiation::ConnectionRequested;
	} else {
		negotiation->role = WbNegotiation::Joiner;
		negotiation->state = WbNegotiation::InvitationSent;
	}
	negotiation->target = target;
	negotiation->peer = target;
	negotiation->ownJid = ownJid;
	negotiation->groupChat = groupChat;
	negotiation->dialog = 0;
	negotiations_[session] = negotiation;

	sendMessage(wb, target, groupChat);

	// Reset the timeout for negotiations
	negotiationTimer_.start();

	return;
}

void WbManager::negotiationTimeout() {
	WbNegotiation* negotiation;
	foreach(QString session, negotiations_.keys()){
		negotiation = negotiations_.take(session);
		if(negotiation->role == WbNegotiation::Participant && negotiation->state < WbNegotiation::HistoryOffered && negotiation->state != WbNegotiation::DocumentBegan) {		
			if(negotiation->dialog)
				negotiation->dialog->endSession();
			sendAbortNegotiation(session, negotiation->peer, negotiation->groupChat);
		}
		delete negotiation;
	}
}

// #include <QTextStream>
void WbManager::sendMessage(QDomElement wb, const Jid & receiver, bool groupChat) {
	// DEBUG:
// 	QString debug;
// 	QTextStream s(&debug);
// 	wb.save(s, 1);
// 	qDebug(debug.toAscii());

// 	// Split large wb elements to smaller ones if possible
// 	QString str;
// 	QTextStream stream(&debug);
// 	wb.save(stream, 0);
// 	QList<QDomElement> wbs;
// 	while(str > 6000) {
// 		QDomElement
// 		wb.save(stream, 0);
// 	}
	wbHash_--;
	WbDlg* dialog = qobject_cast<WbDlg*>(sender());
	if(dialog)
		dialog->setLastWb(dialog->ownJid().full(), QString("%1").arg(wbHash_));
	// Add a unique hash to each sent wb element
	wb.setAttribute("hash", wbHash_);
	Message m(receiver);
	m.setWb(wb);
	if(groupChat && receiver.resource().isEmpty())
		m.setType("groupchat");
	if(client_->isActive())
		client_->sendMessage(m);
	else {
		//TODO: queue the message
	}
}

WbDlg* WbManager::findWbDlg(const Jid &jid) {
	// find if a dialog for the jid already exists
	foreach(WbDlg* w, dialogs_) {
		// does the jid match?
		if(w->target().compare(jid)) {
			return w;
		}
	}
	return 0;
}

WbDlg* WbManager::findWbDlg(const QString &session) {
	// find if a dialog for the session already exists
	foreach(WbDlg* w, dialogs_) {
		// does the session match?
		if(w->session() == session)
			return w;
	}
	return 0;
}

WbDlg* WbManager::createWbDlg(const Jid &target, QString session, const Jid &ownJid, bool groupChat) {
	if(session.isEmpty() || !target.isValid())
		return 0;
	if(!ownJids_.contains(ownJid.full()))
		ownJids_.append(ownJid.full());
	// create the WbDlg
	WbDlg* w = new WbDlg(target, session, ownJid, groupChat, pa_);
	// connect the signals
	connect(w, SIGNAL(newWbElement(QDomElement, Jid, bool)), SLOT(sendMessage(const QDomElement &, const Jid &, bool)));
	connect(w, SIGNAL(sessionEnded(QString)), SLOT(removeSession(const QString &)));
	removeDetectedSession(session);
	return w;
	// Note: the dialog should be added to dialogs_ once negotiation is finished
}

void WbManager::sendAbortNegotiation(QString session, const Jid &peer, bool groupChat) {
	QDomDocument doc = QDomDocument();
	QDomElement wb = doc.createElementNS("http://jabber.org/protocol/svgwb", "wb");
	wb.setAttribute("session", session);
	QDomElement protocol = doc.createElement("protocol");
	protocol.appendChild(doc.createElement("abort-negotiation"));
	wb.appendChild(protocol);
	sendMessage(wb, peer, groupChat);
}

void WbManager::removeDetectedSession(const QString &session) {
	for(int i = 0; i < detectedSessions_.size(); i++) {
		// Remove the specified session from the list
		if(detectedSessions_.at(i).session == session)
			detectedSessions_.removeAt(i);
		else if(detectedSessions_.at(i).time.secsTo(QTime::currentTime()) > 1800)
			// Remove detected session that are old
			detectedSessions_.removeAt(i);
	}
}

void WbManager::groupChatLeft(const Jid &jid) {
	for(int i = 0; i < ownJids_.size(); i++) {
		if(jid.bare() == ownJids_.at(i).left(ownJids_.at(i).indexOf("/")))
			ownJids_.removeAt(i);
	}
	WbDlg* w = findWbDlg(jid);
	if(w)
		w->endSession();
}

void WbManager::groupChatJoined(const Jid &, const Jid &ownJid) {
	if(!ownJids_.contains(ownJid.full()))
		ownJids_.append(ownJid.full());
}
