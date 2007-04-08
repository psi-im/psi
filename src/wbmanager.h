/*
 * wbmanager.h - Whiteboard manager
 * Copyright (C) 2006  Joonas Govenius
 *
 * Influenced by:
 * pepmanager.h - Classes for PEP
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

#ifndef WBMANAGER_H
#define WBMANAGER_H

#include "wbdlg.h"
#include <QInputDialog>

namespace XMPP {
	class Client;
	class Jid;
	class Message;
}

using namespace XMPP;

/*! \brief A simple class to keep track of the state of a session negotiation process.*/
class WbNegotiation{
public:
	/*! \brief Describes our role in the negotiation.*/
	enum Role {Participant, Joiner};
	/*! \brief Describes the current state of the negotiation.*/
	enum State {NotStarted, InvitationSent, ConnectionRequested, InvitationAccepted, DocumentBegan, /*DocumentEnded,*/ HistoryOffered, HistoryAccepted, Finished, Aborted};
	Role role;
	State state;
	/*! \brief The JID that will be used for delivering the wb messages once session is established.*/
	Jid target;
	/*! \brief The JID where negotiation messages are sent.*/
	Jid peer;
	/*! \brief The user's own JID used in the session.*/
	Jid ownJid;
	/*! \brief Set if the target is a groupchat.*/
	bool groupChat;
	/*! \brief The dialog created for this session.*/
	WbDlg* dialog;
};

/*! \brief The manager for whiteboard sessions and dialogs.
 *  This class listens to incoming messages and picks up any that contain a
 *  whiteboard element. If the whiteboard element contains a protocol element
 *  it's preprocessed by the manager in case it is a new session request or
 *  a session left notification. If the whiteboard element doesn't contain
 *  a protocol element and belongs to an existing session it's passed on to
 *  the corresponding dialog.
 *
 *  The manager also provides a possibillity for the local client to start
 *  a new session negotiation with the desired contact.
 *
 *  TODO: The manager also takes care of notifications of changes to whiteboards.
 *
 *  One class per PsiAccount should be created.
 *
 *  \sa WbDlg
 */
class WbManager : public QObject
{
	Q_OBJECT

	struct detectedSession {
		QString session;
		Jid jid;
		QTime time;
	};

public:
	/*! \brief Constructor.
	 *  Creates a new manager for the specified Client and PsiAccount
	 */
	WbManager(XMPP::Client* client, PsiAccount* pa);

public slots:
	/*! \brief Opens the existing dialog to the specified contact or starts a new session negotiation if necessary.*/
	void openWhiteboard(const Jid &target, const Jid &ownJid, bool groupChat);

private:
	/*! \brief Process a message that contains a protocol element.
	 *  Returns a pointer to the new dialog if negotiation finished.
	 */
	WbDlg* negotiateSession(const Message &message);
	/*! \brief Starts a new session negotiation to the specified contact with given list of features.*/
	void startNegotiation(const Jid &target, const Jid &ownJid, bool groupChat, QList<QString> features = QList<QString>());
	/*! \brief Return a pointer to a dialog to the specified contact.
	 *  If such dialog doesn't exits, returns 0.
	 */
	WbDlg* findWbDlg(const Jid &target);
	/*! \brief Return a pointer to a dialog of the specified session.
	 *  If such dialog doesn't exits, returns 0.
	 */
	WbDlg* findWbDlg(const QString &session);
	/*! \brief Returns a pointer to a new dialog to with given contact and session set.*/
	WbDlg* createWbDlg(const Jid &target, QString session, const Jid &ownJid, bool groupChat);
	/*! \brief Aborts the negotiation of \a session started with \a peer.*/
	void sendAbortNegotiation(QString session, const Jid &peer, bool groupChat);

	/*! \brief A pointer to the Client to listen to.*/
	XMPP::Client* client_;
	/*! \brief A pointer to the PsiAccount to listen to.*/
	PsiAccount* pa_;
	/*! \brief A list of dialogs of established sessions.*/
	QList<WbDlg*> dialogs_;
	/*! \brief A list of negotiations in process.*/
	QHash<QString, WbNegotiation*> negotiations_;
	/*! \brief A timer used to remove unfinished negotiations after a timeout.*/
	QTimer negotiationTimer_;
	/*! \brief A list of dialogs of established sessions.*/
	QList<detectedSession> detectedSessions_;
	/*! \brief A list of Jids corresponding to self.*/
	QList<QString> ownJids_;
	/*! \brief A list of supported SVG features.*/
	QList<QString> supportedFeatures_;
	/*! \brief A counter used for including a unique hash in each sent wb element.*/
	int wbHash_;

private slots:
	/*! \brief Receives incoming message and determines what to do with them.*/
	void messageReceived(const Message &message);
	/*! \brief Send given whiteboard element to receiver in a message.*/
	void sendMessage(QDomElement wb, const Jid & receiver, bool groupChat);
	/*! \brief Removes and deletes the dialog for the given session.*/
	void removeSession(const QString &session);
	/*! \brief Removes the "detected session" record of the given session.*/
	void removeDetectedSession(const QString &session);
	/*! \brief Removes and deletes the possible dialogs for the groupchat.*/
	void groupChatLeft(const Jid &);
	/*! \brief Keeps a record of groupchats.*/
	void groupChatJoined(const Jid &, const Jid &ownJid);
	/*! \brief Removes inactive and unfinished session negotiations.*/
	void negotiationTimeout();
};

#endif
