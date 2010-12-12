/*
 * sxesession.h - Sxe Session
 * Copyright (C) 2007  Joonas Govenius
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

#ifndef SXDESESSION_H
#define SXDESESSION_H

#define SXENS "http://jabber.org/protocol/sxe" 
/*  ^^^^ make sure corresponds to NS used for parsing in iris/src/xmpp/xmpp-im/types.cpp ^^^^ */

#include <QObject>
#include <QList>
#include <QPointer>
#include <QDomNode>
#include "im.h"
#include "psiaccount.h"

#include "sxerecord.h"

namespace XMPP {
	class Client;
	class Jid;
	class Message;
}

using namespace XMPP;

/*! \brief Class for storing the record and the XML document for an established SXE session.*/
class SxeSession : public QObject {
	Q_OBJECT

		// Make SxeManager a friend class so it can emit peerJoined/Left session.
		friend class SxeManager;

	private:
		struct IncomingEdit {
			QString id;
			QDomElement xml;
		};

	public:
		/*! \brief Constructor.
		*  Creates a new session for the specified jid and session identifier.
		*/
		SxeSession(const Jid &target, const QString &session, const Jid &ownJid, bool groupChat, bool serverSupport, const QList<QString> &features);
		/*! \brief Destructor.
		*  Emits sessionEnded()
		*/
		~SxeSession();

		/*! \brief Initializes the shared document. Only used if starting a new session; not when joining one. */
		void initializeDocument(const QDomDocument &doc);
		/*! \brief Processes the incoming SXE element and remembers its identifying information.*/
		void processIncomingSxeElement(const QDomElement &, const QString &id);
		/*! \brief Returns a const reference to the target document.*/
		const QDomDocument& document() const;
		/*! \brief Returns true if the target is a groupchat.*/
		bool groupChat() const;
		/*! \brief Returns true if the target is a groupchat.*/
		bool serverSupport() const;
		/*! \brief Returns the target contact's JID.*/
		const Jid target() const;
		/*! \brief Returns the session identifier.*/
		const QString session() const;
		/*! \brief Returns the JID used by the user in the session.*/
		const Jid ownJid() const;
		/*! \brief Returns the session identifier.*/
		const QList<QString> features() const;

		/*! \brief Starts queueing new edits to the document.
		 *  Queueing should be started just before sending <document-begin/>.
		 */
		QList<const SxeEdit*> startQueueing();
		/*! \brief Stop queueing new edits to the document and process the queued ones.
		 *  Queueing should be stopped after sending <document-end/>.
		 */
		void stopQueueing();

		/*! \brief Initializes the document with the prolog of \a doc if provided.
		 * Should be used when <document-begin/> is received when joining.
		 */
		void startImporting(const QDomDocument &doc = QDomDocument());
		/*! \brief Enters the normal editing mode.
		 * Should be used when <document-end/> is received when joining.
		 */
		void stopImporting();

		/*! \brief Add the given ID to the list of used IDs for <sxe/> elements.*/
		void addUsedSxeId(QString id);
		/*! \brief Return the list of used IDs for <sxe/> elements.*/
		QList<QString> usedSxeIds();

		void setUUIDPrefix(const QString uuidPrefix = QString());
		/*! \brief Returns a random UUID without enclosing { }. */
		static QString generateUUID();
		/*! \brief Returns the prolog of the document as a string. */
		static QString parseProlog(const QDomDocument &doc);

	public slots:
		/*! \brief Ends the session.*/
		void endSession();
		/*! \brief Inserts or moves the given node so that it is before the reference element.
		 *  If the reference element is the null element the element is inserted as the first child of the parent.
		 *  Returns \a node if the node was already in the document. Otherwise returns the created node.
		 */
		const QDomNode insertNodeBefore(const QDomNode &node, const QDomNode &parent, const QDomNode &referenceNode = QDomNode());
		/*! \brief Inserts or moves the given node so that it is after the reference element.
		 *  If the reference element is the null element the element is inserted as the last child of the parent.
		 *  Returns \a node if the node was already in the document. Otherwise returns the created node.
		 */
		const QDomNode insertNodeAfter(const QDomNode &node, const QDomNode &parent, const QDomNode &referenceNode = QDomNode());
		/*! \brief Removes the given element.*/
		void removeNode(const QDomNode &node);
		/*! \brief Sets the value of \a attribute of \a node to \a value. */
		void setAttribute(const QDomNode &node, const QString &attribute, const QString &value, int from = -1, int n = 0);
		/*! \brief Sets the value of \a node to \a value. */
		void setNodeValue(const QDomNode &node, const QString &value, int from = -1, int n = 0);

		/*! \brief Sends all queued edits.*/
		void flush();

	signals:
		/*! \brief used to pass the new <sxe/> elements to sxemanager.*/
		void newSxeElement(const QDomElement &element, const Jid &, bool groupChat);

		/*! \brief Emitted after each processed SXE element.*/
		void documentUpdated(bool remote);
		/*! \brief Emitted just before \a node is inserted.*/
		void nodeToBeAdded(const QDomNode &node, bool remote);
		/*! \brief Emitted after \a node is inserted.*/
		void nodeAdded(const QDomNode &node, bool remote);
		/*! \brief Emitted just before \a node is moved in the document tree due to processing of an SXE element.*/
		void nodeToBeMoved(const QDomNode &node, bool remote);
		/*! \brief Emitted after \a node has been moved in the document tree due to processing of an SXE element.*/
		void nodeMoved(const QDomNode &node, bool remote);
		/*! \brief Emitted just before \a node is removed from the document tree due to processing of an SXE element.*/
		void nodeToBeRemoved(const QDomNode &node, bool remote);
		/*! \brief Emitted after \a node has been removed from the document tree due to processing of an SXE element.*/
		void nodeRemoved(const QDomNode &node, bool remote);
		/*! \brief Emitted when the name of \a node has been changed. */
		// void nameChanged(const QDomNode &node, bool remote);
		/*! \brief Emitted just before the chdata of \a node is changed. */
		void chdataToBeChanged(const QDomNode &node, bool remote);
		/*! \brief Emitted when the chdata of \a node has been changed. */
		void chdataChanged(const QDomNode &node, bool remote);

		/*! \brief Signals that a peer joined the session.*/
		void peerJoinedSession(const Jid &);
		/*! \brief Signals that a peer left the session.*/
		void peerLeftSession(const Jid &);
		/*! \brief Signals that the session ended and the session is to be deleted.*/
		void sessionEnded(SxeSession*);

	private slots:
		/*! \brief Adds \a node to the document tree and emits the appropriate public signals. */
		void handleNodeToBeAdded(const QDomNode &node, bool remote);
		/*! \brief Moves \a node in the document tree and emits the appropriate public signals. */
		void handleNodeToBeMoved(const QDomNode &node, bool remote);
		/*! \brief Remove the record entry from the lookup tables and emit the appropriate public signals. */
		void handleNodeToBeRemoved(const QDomNode &node, bool remote);
		/*! \brief Add a node node to the lookup table. */
		// void addToLookup(const QDomNode &node, bool, const QString &rid);

	private:
		/*! \brief Inserts or moves a node according to it's record (parent and primary-weight). */
		void reposition(const QDomNode &node, bool remote);
		/*! \brief Remove the record associated with \a node from the lookup tables. */
		void removeRecord(const QDomNode &node);
		/*! \brief Remove the item with smaller secondary weight.
			Returns true iff \a meta1 was removed. */
		bool removeSmaller(SxeRecord* meta1, SxeRecord* meta2);
		/*! \brief Processes an incoming sxe element.*/
		bool processSxe(const QDomElement &sxe, const QString &id);
		/*! \brief Queues an outgoing edit to be sent when flushed.*/
		void queueOutgoingEdit(SxeEdit* edit);
		/*! \brief Creates the record of node with rid \a id. Returns a pointer to it. */
		SxeRecord* createRecord(const QString &id);
		/*! \brief Returns a pointer to the record of node with rid \a id. */
		SxeRecord* record(const QString &id);
		/*! \brief Returns a pointer to the record of \a node. */
		SxeRecord* record(const QDomNode &node) const;
		/*! \brief Generates SxeNewEdits for \a node and its children.
		 *  Returns the created node. */
		QDomNode generateNewNode(const QDomNode &node, const QString &parent, double primaryWeight);
		/*! \brief Generates SxeRemoveEdits for \a node and its children. */
		void generateRemoves(const QDomNode &node);
		/*! \brief Recursive helper method for arranging edits for the snapshot. */
		void arrangeEdits(QHash<QString, QString> &ridByParent, QList<const SxeEdit*> &output, const QString &iterator);
		/*! \brief Insert node with the given primaryWeight.
		 *  Returns node if the node was already in the document. Otherwise returns the created node. */
		const QDomNode insertNode(const QDomNode &node, const QString &parentId, double primaryWeight);
		/*! \brief Returns a random UUID without enclosing { } and checks that it's not used as a rid.
			Necessary because you often generate the same UUID on two different processes on the same computer (non-windows). */
		QString generateUUIDForSession();

		/*! \brief The string identifying the session.*/
		QString session_;
		/*! \brief The target JID.*/
		Jid target_;
		/*! \brief The target JID.*/
		Jid ownJid_;
		/*! \brief The target XML document.*/
		QDomDocument document_;

		/*! \brief Hash used for rid -> SxeRecord* lookups.*/
		QHash<
				QString,
				SxeRecord*
			 > recordByNodeId_;
		/*! \brief List of queued incoming sxe elements.*/
		QList<IncomingEdit> queuedIncomingEdits_;
		/*! \brief List of queued outgoing sxe elements.*/
		QList<QDomNode> queuedOutgoingEdits_;
		/*! \brief QDomDocument representing the the contents when queueing_ was set true.*/
		QList<SxeEdit*> snapshot_;
		/*! \brief True if the target is a groupchat.*/
		bool groupChat_;
		/*! \brief True if the session was not established with a server supporting SXE.*/
		bool serverSupport_;
		/*! \brief If true, new sxe elements are queued rather than processed.*/
		bool queueing_;
		/*! \brief True while initial contents of the document are being imported.*/
		bool importing_;
		/*! \brief A list of supported features for the session.*/
		QList<QString> features_;
		 /*! \brief Identifiers for the <sxe/> elements that have been processed already.*/
		QList<QString> usedSxeIds_;
		/*! \brief A unique id is generated as "uuidPrefix.counter".*/
		QString uuidPrefix_;
		int uuidMaxPostfix_;
		/*! \brief The main DOM document.*/
		QDomDocument doc_;
};

#endif
