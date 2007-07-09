/*
 * xmpp.h - XMPP "core" library API
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

#ifndef XMPP_H
#define XMPP_H

#include <QPair>
#include <qobject.h>
#include <qstring.h>
#include <qhostaddress.h>
#include <qstring.h>
#include <q3cstring.h>
#include <qxml.h>
#include <qdom.h>

#include "xmpp_jid.h"
#include "xmpp_stanza.h"
#include "xmpp_stream.h"
#include "xmpp_clientstream.h"

namespace QCA
{
	class TLS;
};

#ifndef CS_XMPP
class ByteStream;
#endif

#include <QtCrypto> // For QCA::SASL::Params

namespace XMPP
{
	// CS_IMPORT_BEGIN cutestuff/bytestream.h
#ifdef CS_XMPP
	class ByteStream;
#endif
	// CS_IMPORT_END

	class Debug
	{
	public:
		virtual ~Debug();

		virtual void msg(const QString &)=0;
		virtual void outgoingTag(const QString &)=0;
		virtual void incomingTag(const QString &)=0;
		virtual void outgoingXml(const QDomElement &)=0;
		virtual void incomingXml(const QDomElement &)=0;
	};

	void setDebug(Debug *);

	class Connector : public QObject
	{
		Q_OBJECT
	public:
		Connector(QObject *parent=0);
		virtual ~Connector();

		virtual void connectToServer(const QString &server)=0;
		virtual ByteStream *stream() const=0;
		virtual void done()=0;

		bool useSSL() const;
		bool havePeerAddress() const;
		QHostAddress peerAddress() const;
		Q_UINT16 peerPort() const;

	signals:
		void connected();
		void error();

	protected:
		void setUseSSL(bool b);
		void setPeerAddressNone();
		void setPeerAddress(const QHostAddress &addr, Q_UINT16 port);

	private:
		bool ssl;
		bool haveaddr;
		QHostAddress addr;
		Q_UINT16 port;
	};

	class AdvancedConnector : public Connector
	{
		Q_OBJECT
	public:
		enum Error { ErrConnectionRefused, ErrHostNotFound, ErrProxyConnect, ErrProxyNeg, ErrProxyAuth, ErrStream };
		AdvancedConnector(QObject *parent=0);
		virtual ~AdvancedConnector();

		class Proxy
		{
		public:
			enum { None, HttpConnect, HttpPoll, Socks };
			Proxy();
			~Proxy();

			int type() const;
			QString host() const;
			Q_UINT16 port() const;
			QString url() const;
			QString user() const;
			QString pass() const;
			int pollInterval() const;

			void setHttpConnect(const QString &host, Q_UINT16 port);
			void setHttpPoll(const QString &host, Q_UINT16 port, const QString &url);
			void setSocks(const QString &host, Q_UINT16 port);
			void setUserPass(const QString &user, const QString &pass);
			void setPollInterval(int secs);

		private:
			int t;
			QString v_host, v_url;
			Q_UINT16 v_port;
			QString v_user, v_pass;
			int v_poll;
		};

		void setProxy(const Proxy &proxy);
		void setOptHostPort(const QString &host, Q_UINT16 port);
		void setOptProbe(bool);
		void setOptSSL(bool);

		void changePollInterval(int secs);

		void connectToServer(const QString &server);
		ByteStream *stream() const;
		void done();

		int errorCode() const;

	signals:
		void srvLookup(const QString &server);
		void srvResult(bool success);
		void httpSyncStarted();
		void httpSyncFinished();

	private slots:
		void dns_done();
		void srv_done();
		void bs_connected();
		void bs_error(int);
		void http_syncStarted();
		void http_syncFinished();

	private:
		class Private;
		Private *d;

		void cleanup();
		void do_resolve();
		void do_connect();
		void tryNextSrv();
	};

	class TLSHandler : public QObject
	{
		Q_OBJECT
	public:
		TLSHandler(QObject *parent=0);
		virtual ~TLSHandler();

		virtual void reset()=0;
		virtual void startClient(const QString &host)=0;
		virtual void write(const QByteArray &a)=0;
		virtual void writeIncoming(const QByteArray &a)=0;

	signals:
		void success();
		void fail();
		void closed();
		void readyRead(const QByteArray &a);
		void readyReadOutgoing(const QByteArray &a, int plainBytes);
	};

	class QCATLSHandler : public TLSHandler
	{
		Q_OBJECT
	public:
		QCATLSHandler(QCA::TLS *parent);
		~QCATLSHandler();

		QCA::TLS *tls() const;
		int tlsError() const;

		void setXMPPCertCheck(bool enable);
		bool XMPPCertCheck();
		bool certMatchesHostname();

		void reset();
		void startClient(const QString &host);
		void write(const QByteArray &a);
		void writeIncoming(const QByteArray &a);

	signals:
		void tlsHandshaken();

	public slots:
		void continueAfterHandshake();

	private slots:
		void tls_handshaken();
		void tls_readyRead();
		void tls_readyReadOutgoing();
		void tls_closed();
		void tls_error();

	private:
		class Private;
		Private *d;
	};
};

#endif
