/*
 * sxemanager.h - Whiteboard manager
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

#ifndef SXDEMANAGER_H
#define SXDEMANAGER_H

#include "QTimer"

#include "sxesession.h"

namespace XMPP {
	class Client;
	class Jid;
	class Message;
}

using namespace XMPP;

/*! \brief The manager for SXE sessions and negotiations.
 *  This class listens to incoming messages and picks up any that contain an
 *  SxdE element. If the element belongs to an already established session,
 *  the element is passed to the session. If the element is a negotiation element,
 *  the manager handles it. Accepting invitations is determined based on callback
 *  functions provided to the class.
 *
 *  The manager also provides a possibillity for the local client to start
 *  a new session negotiation with the desired contact.
 *
 *  One instance per PsiAccount should be created.
 *
 *  \sa SxeSession
 */
class SxeManager : public QObject
{
	Q_OBJECT

	/*! \brief A simple struct used to keep track of the detected SXE sessions.*/
	struct DetectedSession {
		QString session;
		Jid jid;
		QTime time;
	};

	/*! \brief A simple class to keep track of the state of a session negotiation process.*/
	class SxeNegotiation{
	public:
		enum Role {Participant, Joiner};
		enum State {NotStarted, InvitationSent, ConnectionRequested, InvitationAccepted, DocumentBegan, /*DocumentEnded,*/ HistoryOffered, HistoryAccepted, Finished, Aborted};

		/*! \brief Describes our role in the negotiation.*/
		Role role;
		/*! \brief Describes the current state of the negotiation.*/
		State state;
		/*! \brief The identifier for the session.*/
		QString sessionId;
		/*! \brief The JID that will be used for delivering the sxe messages once session is established.*/
		Jid target;
		/*! \brief The JID where negotiation messages are sent.*/
		Jid peer;
		/*! \brief The user's own JID used in the session.*/
		Jid ownJid;
		/*! \brief Set if the target is a groupchat.*/
		bool groupChat;
		/*! \brief The document to be sent as the initial document during the negotiation.
		 *	  This is only relevant when Role == Participant.
		 */
		QDomDocument initialDoc;
		/*! \brief A list of features proposed for the session.*/
		QList<QString> features;
		/*! \brief The session created for this negotiation.*/
		QPointer<SxeSession> session;
	};

public:
	/*! \brief Constructor.
	 *  Creates a new manager for the specified Client and PsiAccount
	 */
	SxeManager(XMPP::Client* client, PsiAccount* pa);
	/*! \brief Return a list of pointers to sessions to the specified contact.
	 *  If no such session exits, returns 0.
	 */
	QList< QPointer<SxeSession> > findSession(const Jid &target);
	/*! \brief Return a pointer to the specified session.
	 *  If such session doesn't exits, returns 0.
	 */
	QPointer<SxeSession> findSession(const QString &session);
	/*! \brief Add a callback for invitations.*/
	void addInvitationCallback(bool (*callback)(const Jid &peer, const QList<QString> &features));
	/*! \brief Starts a new session negotiation to the specified contact with given list of features.*/
	void startNewSession(const Jid &target, const Jid &ownJid, bool groupChat, const QDomDocument &initialDoc, QList<QString> features = QList<QString>());
	/*! \brief Join an existing session.*/
	void joinSession(const Jid &target, const Jid &ownJid, bool groupChat, const QString &session);
	/*! \brief Checks that \a jid supports SXE and \a features. */
	bool checkSupport(const Jid &jid, const QList<QString> &features);

signals:
	/*! \brief Emitted when \a session has been established.*/
	void sessionNegotiated(SxeSession* session);
	/*! \brief Emitted when \a an invitation to \a jid was declined and joining an alternative session \a session was suggested.*/
	void alternativeSession(const Jid &jid, const QString &session);

private:
	/*! \brief Process a message that contains a negotiation element.
	 *  Returns a pointer to the new session if negotiation finished.
	 */
	QPointer<SxeSession> processNegotiationMessage(const Message &message);
	/*! \brief Process a negotiation element as a Participant.
		Appends the appropriate responses to \a response.
		Returns false iff the negotiation object was deleted. */
	bool processNegotiationAsParticipant(const QDomNode &negotiationElement, SxeNegotiation* negotiation, QDomNode response);
	/*! \brief Process a negotiation element as a Joiner.
		Appends the appropriate responses to \a response.
		Returns false iff the negotiation object was deleted. */
	bool processNegotiationAsJoiner(const QDomNode &negotiationElement, SxeNegotiation* negotiation, QDomNode response, const Message &message);
	/*! \brief Returns a pointer to a new session instance.*/
	QPointer<SxeSession> createSxeSession(const Jid &target, QString session, const Jid &ownJid, bool groupChat, const QList<QString> &features);
	/*! \brief Aborts the negotiation of \a session started with \a peer.*/
	void abortNegotiation(QString session, const Jid &peer, bool groupChat);
	/*! \brief Aborts the negotiation with the peer and deletes the session.*/
	void abortNegotiation(SxeNegotiation* negotiation);
	/*! \brief Records the session that the message refers to as a DetectedSession if no entry for the session exists yet.*/
	void recordDetectedSession(const Message &message);
	/*! \brief Returns a pointer to a new negotiation instance based on \a message.*/
	SxeNegotiation* createNegotiation(const Message &message);
	/*! \brief Returns a pointer to a new negotiation instance.*/
	SxeNegotiation* createNegotiation(SxeNegotiation::Role role, SxeNegotiation::State state, const QString &sessionId, const Jid &target, const Jid &ownJid, bool groupChat);
	/*! \brief Returns a pointer to an existing negotiation object of \a session with \a jid.*/
	SxeNegotiation* findNegotiation(const Jid &jid, const QString &session);
	/*! \brief Remove the negotiation object of \a session with \a jid.*/
	void removeNegotiation(SxeNegotiation* negotiation);


	/*! \brief A pointer to the Client to listen to.*/
	XMPP::Client* client_;
	/*! \brief A pointer to the PsiAccount to listen to.*/
	PsiAccount* pa_;
	/*! \brief A list of of established sessions.*/
	QList< QPointer<SxeSession> > sessions_;
	/*! \brief A list of negotiations in process.*/
	QMultiHash<QString, SxeNegotiation* > negotiations_;
	/*! \brief A timer used to remove unfinished negotiations after a timeout.*/
	QTimer negotiationTimer_;
	/*! \brief A list of of detected sessions.*/
	QList<DetectedSession> DetectedSessions_;
	/*! \brief A list of Jids corresponding to self.*/
	QList<QString> ownJids_;
	/*! \brief A list of callbacks used to determine whether an invitation should be accepted.*/
	QList<bool (*)(const Jid &peer, const QList<QString> &features)> invitationCallbacks_;
	/*! \brief A counter used for including a unique id in each sent sxe element.*/
	int sxeId_;
	/*! \brief A list of messages waiting to be sent out.*/
	QList<Message> queuedMessages_;

private slots:
	/*! \brief Receives incoming message and determines what to do with them.*/
	void messageReceived(const Message &message);
	/*! \brief Send given whiteboard element to receiver in a message.*/
	void sendSxe(QDomElement sxe, const Jid & receiver, bool groupChat);
	/*! \brief Removes and deletes the session.*/
	void removeSession(SxeSession* session);
	/*! \brief Removes the "detected session" record of the given session.*/
	void removeDetectedSession(SxeSession* session);
	/*! \brief Removes and deletes the possible sessions for the groupchat.*/
	void groupChatLeft(const Jid &);
	/*! \brief Keeps a record of groupchats.*/
	void groupChatJoined(const Jid &, const Jid &ownJid);
	/*! \brief Removes inactive and unfinished session negotiations.*/
	void negotiationTimeout();
};

#endif
