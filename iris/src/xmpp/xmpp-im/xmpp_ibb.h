/*
 * ibb.h - Inband bytestream
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

#ifndef JABBER_IBB_H
#define JABBER_IBB_H

#include <QList>
#include <qobject.h>
#include <qdom.h>
#include <qstring.h>
#include "bytestream.h"
#include "im.h"

namespace XMPP
{
	class Client;
	class IBBManager;

	// this is an IBB connection.  use it much like a qsocket
	class IBBConnection : public ByteStream
	{
		Q_OBJECT
	public:
		enum { ErrRequest, ErrData };
		enum { Idle, Requesting, WaitingForAccept, Active };
		IBBConnection(IBBManager *);
		~IBBConnection();

		void connectToJid(const Jid &peer, const QDomElement &comment);
		void accept();
		void close();

		int state() const;
		Jid peer() const;
		QString streamid() const;
		QDomElement comment() const;

		bool isOpen() const;
		void write(const QByteArray &);
		QByteArray read(int bytes=0);
		int bytesAvailable() const;
		int bytesToWrite() const;

	signals:
		void connected();

	private slots:
		void ibb_finished();
		void trySend();

	private:
		class Private;
		Private *d;

		void reset(bool clear=false);

		friend class IBBManager;
		void waitForAccept(const Jid &peer, const QString &sid, const QDomElement &comment, const QString &iq_id);
		void takeIncomingData(const QByteArray &, bool close);
	};

	typedef QList<IBBConnection*> IBBConnectionList;
	class IBBManager : public QObject
	{
		Q_OBJECT
	public:
		IBBManager(Client *);
		~IBBManager();

		Client *client() const;

		IBBConnection *takeIncoming();

	signals:
		void incomingReady();

	private slots:
		void ibb_incomingRequest(const Jid &from, const QString &id, const QDomElement &);
		void ibb_incomingData(const Jid &from, const QString &streamid, const QString &id, const QByteArray &data, bool close);

	private:
		class Private;
		Private *d;

		QString genKey() const;

		friend class IBBConnection;
		IBBConnection *findConnection(const QString &sid, const Jid &peer="") const;
		QString genUniqueKey() const;
		void link(IBBConnection *);
		void unlink(IBBConnection *);
		void doAccept(IBBConnection *c, const QString &id);
		void doReject(IBBConnection *c, const QString &id, int, const QString &);
	};

	class JT_IBB : public Task
	{
		Q_OBJECT
	public:
		enum { ModeRequest, ModeSendData };
		JT_IBB(Task *, bool serve=false);
		~JT_IBB();

		void request(const Jid &, const QDomElement &comment);
		void sendData(const Jid &, const QString &streamid, const QByteArray &data, bool close);
		void respondSuccess(const Jid &, const QString &id, const QString &streamid);
		void respondError(const Jid &, const QString &id, int code, const QString &str);
		void respondAck(const Jid &to, const QString &id);

		void onGo();
		bool take(const QDomElement &);

		QString streamid() const;
		Jid jid() const;
		int mode() const;

	signals:
		void incomingRequest(const Jid &from, const QString &id, const QDomElement &);
		void incomingData(const Jid &from, const QString &streamid, const QString &id, const QByteArray &data, bool close);

	private:
		class Private;
		Private *d;
	};
}

#endif
