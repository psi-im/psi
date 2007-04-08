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

	class Jid
	{
	public:
		Jid();
		~Jid();

		Jid(const QString &s);
		Jid(const char *s);
		Jid & operator=(const QString &s);
		Jid & operator=(const char *s);

		void set(const QString &s);
		void set(const QString &domain, const QString &node, const QString &resource="");

		void setDomain(const QString &s);
		void setNode(const QString &s);
		void setResource(const QString &s);

		const QString & domain() const { return d; }
		const QString & node() const { return n; }
		const QString & resource() const { return r; }
		const QString & bare() const { return b; }
		const QString & full() const { return f; }

		Jid withNode(const QString &s) const;
		Jid withResource(const QString &s) const;

		bool isValid() const;
		bool isEmpty() const;
		bool compare(const Jid &a, bool compareRes=true) const;

		static bool validDomain(const QString &s, QString *norm=0);
		static bool validNode(const QString &s, QString *norm=0);
		static bool validResource(const QString &s, QString *norm=0);

		// TODO: kill these later
		const QString & host() const { return d; }
		const QString & user() const { return n; }
		const QString & userHost() const { return b; }

	private:
		void reset();
		void update();

		QString f, b, d, n, r;
		bool valid;
	};

	class Stream;
	class Stanza
	{
	public:
		enum Kind { Message, Presence, IQ };

		Stanza();
		Stanza(const Stanza &from);
		Stanza & operator=(const Stanza &from);
		virtual ~Stanza();

		class Error
		{
		public:
			enum ErrorType { Cancel = 1, Continue, Modify, Auth, Wait };
			enum ErrorCond
			{
				BadRequest = 1,
				Conflict,
				FeatureNotImplemented,
				Forbidden,
				Gone,
				InternalServerError,
				ItemNotFound,
				JidMalformed,
				NotAcceptable,
				NotAllowed,
				NotAuthorized,
				PaymentRequired,
				RecipientUnavailable,
				Redirect,
				RegistrationRequired,
				RemoteServerNotFound,
				RemoteServerTimeout,
				ResourceConstraint,
				ServiceUnavailable,
				SubscriptionRequired,
				UndefinedCondition,
				UnexpectedRequest
			};

			Error(int type=Cancel, int condition=UndefinedCondition, const QString &text="", const QDomElement &appSpec=QDomElement());

			int type;
			int condition;
			QString text;
			QDomElement appSpec;

			int code() const;
			bool fromCode(int code);

			QPair<QString, QString> description() const;

			QDomElement toXml(QDomDocument &doc, const QString &baseNS) const;
			bool fromXml(const QDomElement &e, const QString &baseNS);
		private:
			class Private;
			int originalCode;

		};

		bool isNull() const;

		QDomElement element() const;
		QString toString() const;

		QDomDocument & doc() const;
		QString baseNS() const;
		QDomElement createElement(const QString &ns, const QString &tagName);
		QDomElement createTextElement(const QString &ns, const QString &tagName, const QString &text);
		void appendChild(const QDomElement &e);

		Kind kind() const;
		void setKind(Kind k);

		Jid to() const;
		Jid from() const;
		QString id() const;
		QString type() const;
		QString lang() const;

		void setTo(const Jid &j);
		void setFrom(const Jid &j);
		void setId(const QString &id);
		void setType(const QString &type);
		void setLang(const QString &lang);

		Error error() const;
		void setError(const Error &err);
		void clearError();

	private:
		friend class Stream;
		Stanza(Stream *s, Kind k, const Jid &to, const QString &type, const QString &id);
		Stanza(Stream *s, const QDomElement &e);

		class Private;
		Private *d;
	};

	class Stream : public QObject
	{
		Q_OBJECT
	public:
		enum Error { ErrParse, ErrProtocol, ErrStream, ErrCustom = 10 };
		enum StreamCond {
			GenericStreamError,
			Conflict,
			ConnectionTimeout,
			InternalServerError,
			InvalidFrom,
			InvalidXml,
			PolicyViolation,
			ResourceConstraint,
			SystemShutdown
		};

		Stream(QObject *parent=0);
		virtual ~Stream();

		virtual QDomDocument & doc() const=0;
		virtual QString baseNS() const=0;
		virtual bool old() const=0;

		virtual void close()=0;
		virtual bool stanzaAvailable() const=0;
		virtual Stanza read()=0;
		virtual void write(const Stanza &s)=0;

		virtual int errorCondition() const=0;
		virtual QString errorText() const=0;
		virtual QDomElement errorAppSpec() const=0;

		Stanza createStanza(Stanza::Kind k, const Jid &to="", const QString &type="", const QString &id="");
		Stanza createStanza(const QDomElement &e);

		static QString xmlToString(const QDomElement &e, bool clip=false);

	signals:
		void connectionClosed();
		void delayedCloseFinished();
		void readyRead();
		void stanzaWritten();
		void error(int);
	};

	class ClientStream : public Stream
	{
		Q_OBJECT
	public:
		enum Error {
			ErrConnection = ErrCustom,  // Connection error, ask Connector-subclass what's up
			ErrNeg,                     // Negotiation error, see condition
			ErrTLS,                     // TLS error, see condition
			ErrAuth,                    // Auth error, see condition
			ErrSecurityLayer,           // broken SASL security layer
			ErrBind                     // Resource binding error
		};
		enum Warning {
			WarnOldVersion,             // server uses older XMPP/Jabber "0.9" protocol
			WarnNoTLS                   // there is no chance for TLS at this point
		};
		enum NegCond {
			HostGone,                   // host no longer hosted
			HostUnknown,                // unknown host
			RemoteConnectionFailed,     // unable to connect to a required remote resource
			SeeOtherHost,               // a 'redirect', see errorText() for other host
			UnsupportedVersion          // unsupported XMPP version
		};
		enum TLSCond {
			TLSStart,                   // server rejected STARTTLS
			TLSFail                     // TLS failed, ask TLSHandler-subclass what's up
		};
		enum SecurityLayer {
			LayerTLS,
			LayerSASL
		};
		enum AuthCond {
			GenericAuthError,           // all-purpose "can't login" error
			NoMech,                     // No appropriate auth mech available
			BadProto,                   // Bad SASL auth protocol
			BadServ,                    // Server failed mutual auth
			EncryptionRequired,         // can't use mech without TLS
			InvalidAuthzid,             // bad input JID
			InvalidMech,                // bad mechanism
			InvalidRealm,               // bad realm
			MechTooWeak,                // can't use mech with this authzid
			NotAuthorized,              // bad user, bad password, bad creditials
			TemporaryAuthFailure        // please try again later!
		};
		enum BindCond {
			BindNotAllowed,             // not allowed to bind a resource
			BindConflict                // resource in-use
		};

		ClientStream(Connector *conn, TLSHandler *tlsHandler=0, QObject *parent=0);
		ClientStream(const QString &host, const QString &defRealm, ByteStream *bs, QCA::TLS *tls=0, QObject *parent=0); // server
		~ClientStream();

		Jid jid() const;
		void connectToServer(const Jid &jid, bool auth=true);
		void accept(); // server
		bool isActive() const;
		bool isAuthenticated() const;

		// login params
		void setUsername(const QString &s);
		void setPassword(const QString &s);
		void setRealm(const QString &s);
		void continueAfterParams();

		// SASL information
		QString saslMechanism() const;
		int saslSSF() const;

		// binding
		void setResourceBinding(bool);

		// security options (old protocol only uses the first !)
		void setAllowPlain(bool);
		void setRequireMutualAuth(bool);
		void setSSFRange(int low, int high);
		void setOldOnly(bool);
		void setSASLMechanism(const QString &s);
		void setLocalAddr(const QHostAddress &addr, Q_UINT16 port);

		// Compression
		void setCompress(bool);

		// reimplemented
		QDomDocument & doc() const;
		QString baseNS() const;
		bool old() const;

		void close();
		bool stanzaAvailable() const;
		Stanza read();
		void write(const Stanza &s);

		int errorCondition() const;
		QString errorText() const;
		QDomElement errorAppSpec() const;

		// extra
		void writeDirect(const QString &s);
		void setNoopTime(int mills);

	signals:
		void connected();
		void securityLayerActivated(int);
		void needAuthParams(bool user, bool pass, bool realm);
		void authenticated();
		void warning(int);
		void incomingXml(const QString &s);
		void outgoingXml(const QString &s);

	public slots:
		void continueAfterWarning();

	private slots:
		void cr_connected();
		void cr_error();

		void bs_connectionClosed();
		void bs_delayedCloseFinished();
		void bs_error(int); // server only

		void ss_readyRead();
		void ss_bytesWritten(int);
		void ss_tlsHandshaken();
		void ss_tlsClosed();
		void ss_error(int);

		void sasl_clientFirstStep(bool, const QByteArray&);
		void sasl_nextStep(const QByteArray &stepData);
		void sasl_needParams(const QCA::SASL::Params&);
		void sasl_authCheck(const QString &user, const QString &authzid);
		void sasl_authenticated();
		void sasl_error();

		void doNoop();
		void doReadyRead();

	private:
		class Private;
		Private *d;

		void reset(bool all=false);
		void processNext();
		int convertedSASLCond() const;
		bool handleNeed();
		void handleError();
		void srvProcessNext();
	};
};

#endif
