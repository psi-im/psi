/*
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

#ifndef XMPP_CLIENT_H
#define XMPP_CLIENT_H

#include <QObject>
#include <QStringList>

#include "xmpp/jid/jid.h"
#include "xmpp_status.h"
#include "xmpp_discoitem.h"

class QString;
class QDomElement;
class QDomDocument;
namespace XMPP {
	class ClientStream;
	class Features;
	class FileTransferManager;
	class IBBManager;
	class JidLinkManager;
	class LiveRoster;
	class LiveRosterItem;
	class Message;
	class Resource;
	class ResourceList;
	class Roster;
	class RosterItem;
	class S5BManager;
	class Stream;
	class Task;
}

namespace XMPP
{
	class Client : public QObject
	{
		Q_OBJECT

	public:
		Client(QObject *parent=0);
		~Client();

		bool isActive() const;
		void connectToServer(ClientStream *s, const Jid &j, bool auth=true);
		void start(const QString &host, const QString &user, const QString &pass, const QString &resource);
		void close(bool fast=false);

		Stream & stream();
		QString streamBaseNS() const;
		const LiveRoster & roster() const;
		const ResourceList & resourceList() const;

		void send(const QDomElement &);
		void send(const QString &);

		QString host() const;
		QString user() const;
		QString pass() const;
		QString resource() const;
		Jid jid() const;

		void rosterRequest();
		void sendMessage(const Message &);
		void sendSubscription(const Jid &, const QString &, const QString& nick = QString());
		void setPresence(const Status &);

		void debug(const QString &);
		QString genUniqueId();
		Task *rootTask();
		QDomDocument *doc() const;

		QString OSName() const;
		QString timeZone() const;
		int timeZoneOffset() const;
		QString clientName() const;
		QString clientVersion() const;
		QString capsNode() const;
		QString capsVersion() const;
		QString capsExt() const;

		void setOSName(const QString &);
		void setTimeZone(const QString &, int);
		void setClientName(const QString &);
		void setClientVersion(const QString &);
		void setCapsNode(const QString &);
		void setCapsVersion(const QString &);

		void setIdentity(DiscoItem::Identity);
		DiscoItem::Identity identity();

		void setFeatures(const Features& f);
		const Features& features() const;

		void addExtension(const QString& ext, const Features& f);
		void removeExtension(const QString& ext);
		const Features& extension(const QString& ext) const;
		QStringList extensions() const;
		
		S5BManager *s5bManager() const;
		IBBManager *ibbManager() const;
		JidLinkManager *jidLinkManager() const;

		void setFileTransferEnabled(bool b);
		FileTransferManager *fileTransferManager() const;

		QString groupChatPassword(const QString& host, const QString& room) const;
		bool groupChatJoin(const QString &host, const QString &room, const QString &nick, const QString& password = QString(), int maxchars = -1, int maxstanzas = -1, int seconds = -1, const Status& = Status());
		void groupChatSetStatus(const QString &host, const QString &room, const Status &);
		void groupChatChangeNick(const QString &host, const QString &room, const QString &nick, const Status &);
		void groupChatLeave(const QString &host, const QString &room);

	signals:
		void activated();
		void disconnected();
		//void authFinished(bool, int, const QString &);
		void rosterRequestFinished(bool, int, const QString &);
		void rosterItemAdded(const RosterItem &);
		void rosterItemUpdated(const RosterItem &);
		void rosterItemRemoved(const RosterItem &);
		void resourceAvailable(const Jid &, const Resource &);
		void resourceUnavailable(const Jid &, const Resource &);
		void presenceError(const Jid &, int, const QString &);
		void subscription(const Jid &, const QString &, const QString &);
		void messageReceived(const Message &);
		void debugText(const QString &);
		void xmlIncoming(const QString &);
		void xmlOutgoing(const QString &);
		void groupChatJoined(const Jid &);
		void groupChatLeft(const Jid &);
		void groupChatPresence(const Jid &, const Status &);
		void groupChatError(const Jid &, int, const QString &);

		void incomingJidLink();

	private slots:
		//void streamConnected();
		//void streamHandshaken();
		//void streamError(const StreamError &);
		//void streamSSLCertificateReady(const QSSLCert &);
		//void streamCloseFinished();
		void streamError(int);
		void streamReadyRead();
		void streamIncomingXml(const QString &);
		void streamOutgoingXml(const QString &);

		void slotRosterRequestFinished();

		// basic daemons
		void ppSubscription(const Jid &, const QString &, const QString&);
		void ppPresence(const Jid &, const Status &);
		void pmMessage(const Message &);
		void prRoster(const Roster &);

		void s5b_incomingReady();
		void ibb_incomingReady();

	public:
		class GroupChat;
	private:
		void cleanup();
		void distribute(const QDomElement &);
		void importRoster(const Roster &);
		void importRosterItem(const RosterItem &);
		void updateSelfPresence(const Jid &, const Status &);
		void updatePresence(LiveRosterItem *, const Jid &, const Status &);

		class ClientPrivate;
		ClientPrivate *d;
	};
}

#endif
