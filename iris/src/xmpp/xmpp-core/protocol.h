/*
 * protocol.h - XMPP-Core protocol state machine
 * Copyright (C) 2004  Justin Karneges
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

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <qpair.h>
//Added by qt3to4:
#include <QList>
#include "xmlprotocol.h"
#include "xmpp.h"

#define NS_ETHERX   "http://etherx.jabber.org/streams"
#define NS_CLIENT   "jabber:client"
#define NS_SERVER   "jabber:server"
#define NS_DIALBACK "jabber:server:dialback"
#define NS_STREAMS  "urn:ietf:params:xml:ns:xmpp-streams"
#define NS_TLS      "urn:ietf:params:xml:ns:xmpp-tls"
#define NS_SASL     "urn:ietf:params:xml:ns:xmpp-sasl"
#define NS_SESSION  "urn:ietf:params:xml:ns:xmpp-session"
#define NS_STANZAS  "urn:ietf:params:xml:ns:xmpp-stanzas"
#define NS_BIND     "urn:ietf:params:xml:ns:xmpp-bind"
#define NS_COMPRESS_FEATURE "http://jabber.org/features/compress"
#define NS_COMPRESS_PROTOCOL "http://jabber.org/protocol/compress"
#define NS_HOSTS    "http://barracuda.com/xmppextensions/hosts"

namespace XMPP
{
	class Version
	{
	public:
		Version(int maj=0, int min=0);

		int major, minor;
	};

	class StreamFeatures
	{
	public:
		StreamFeatures();

		bool tls_supported, sasl_supported, bind_supported, compress_supported;
		bool tls_required;
		QStringList sasl_mechs;
		QStringList compression_mechs;
		QStringList hosts;
	};

	class BasicProtocol : public XmlProtocol
	{
	public:
		// xmpp 1.0 error conditions
		enum SASLCond {
			Aborted,
			IncorrectEncoding,
			InvalidAuthzid,
			InvalidMech,
			MechTooWeak,
			NotAuthorized,
			TemporaryAuthFailure
		};
		enum StreamCond {
			BadFormat,
			BadNamespacePrefix,
			Conflict,
			ConnectionTimeout,
			HostGone,
			HostUnknown,
			ImproperAddressing,
			InternalServerError,
			InvalidFrom,
			InvalidId,
			InvalidNamespace,
			InvalidXml,
			StreamNotAuthorized,
			PolicyViolation,
			RemoteConnectionFailed,
			ResourceConstraint,
			RestrictedXml,
			SeeOtherHost,
			SystemShutdown,
			UndefinedCondition,
			UnsupportedEncoding,
			UnsupportedStanzaType,
			UnsupportedVersion,
			XmlNotWellFormed
		};
		enum BindCond {
			BindBadRequest,
			BindNotAllowed,
			BindConflict
		};

		// extend the XmlProtocol enums
		enum Need {
			NSASLMechs = XmlProtocol::NCustom, // need SASL mechlist
			NStartTLS,  // need to switch on TLS layer
			NCompress,  // need to switch on compression layer
			NSASLFirst, // need SASL first step
			NSASLNext,  // need SASL next step
			NSASLLayer, // need to switch on SASL layer
			NCustom = XmlProtocol::NCustom+10
		};
		enum Event {
			EFeatures = XmlProtocol::ECustom, // breakpoint after features packet is received
			ESASLSuccess, // breakpoint after successful sasl auth
			EStanzaReady, // a stanza was received
			EStanzaSent,  // a stanza was sent
			EReady,       // stream is ready for stanza use
			ECustom = XmlProtocol::ECustom+10
		};
		enum Error {
			ErrProtocol = XmlProtocol::ErrCustom, // there was an error in the xmpp-core protocol exchange
			ErrStream,   // <stream:error>, see errCond, errText, and errAppSpec for details
			ErrStartTLS, // server refused starttls
			ErrCompress, // server refused compression
			ErrAuth,     // authorization error.  errCond holds sasl condition (or numeric code for old-protocol)
			ErrBind,     // server refused resource bind
			ErrCustom = XmlProtocol::ErrCustom+10
		};

		BasicProtocol();
		~BasicProtocol();

		void reset();

		// for outgoing xml
		QDomDocument doc;

		// sasl-related
		QString saslMech() const;
		QByteArray saslStep() const;
		void setSASLMechList(const QStringList &list);
		void setSASLFirst(const QString &mech, const QByteArray &step);
		void setSASLNext(const QByteArray &step);
		void setSASLAuthed();

		// send / recv
		void sendStanza(const QDomElement &e);
		void sendDirect(const QString &s);
		void sendWhitespace();
		QDomElement recvStanza();

		// shutdown
		void shutdown();
		void shutdownWithError(int cond, const QString &otherHost="");

		// <stream> information
		QString to, from, id, lang;
		Version version;

		// error output
		int errCond;
		QString errText;
		QDomElement errAppSpec;
		QString otherHost;

		QByteArray spare; // filled with unprocessed data on NStartTLS and NSASLLayer

		bool isReady() const;

		enum { TypeElement, TypeStanza, TypeDirect, TypePing };

	protected:
		static int stringToSASLCond(const QString &s);
		static int stringToStreamCond(const QString &s);
		static QString saslCondToString(int);
		static QString streamCondToString(int);

		void send(const QDomElement &e, bool clip=false);
		void sendStreamError(int cond, const QString &text="", const QDomElement &appSpec=QDomElement());
		void sendStreamError(const QString &text); // old-style

		bool errorAndClose(int cond, const QString &text="", const QDomElement &appSpec=QDomElement());
		bool error(int code);
		void delayErrorAndClose(int cond, const QString &text="", const QDomElement &appSpec=QDomElement());
		void delayError(int code);

		// reimplemented
		QDomElement docElement();
		void handleDocOpen(const Parser::Event &pe);
		bool handleError();
		bool handleCloseFinished();
		bool doStep(const QDomElement &e);
		void itemWritten(int id, int size);

		virtual QString defaultNamespace();
		virtual QStringList extraNamespaces(); // stringlist: prefix,uri,prefix,uri, [...]
		virtual void handleStreamOpen(const Parser::Event &pe);
		virtual bool doStep2(const QDomElement &e)=0;

		void setReady(bool b);

		QString sasl_mech;
		QStringList sasl_mechlist;
		QByteArray sasl_step;
		bool sasl_authed;

		QDomElement stanzaToRecv;

	private:
		struct SASLCondEntry
		{
			const char *str;
			int cond;
		};
		static SASLCondEntry saslCondTable[];

		struct StreamCondEntry
		{
			const char *str;
			int cond;
		};
		static StreamCondEntry streamCondTable[];

		struct SendItem
		{
			QDomElement stanzaToSend;
			QString stringToSend;
			bool doWhitespace;
		};
		QList<SendItem> sendList;

		bool doShutdown, delayedError, closeError, ready;
		int stanzasPending, stanzasWritten;

		void init();
		void extractStreamError(const QDomElement &e);
	};

	class CoreProtocol : public BasicProtocol
	{
	public:
		enum {
			NPassword = NCustom,  // need password for old-mode
			EDBVerify = ECustom,  // breakpoint after db:verify request
			ErrPlain = ErrCustom  // server only supports plain, but allowPlain is false locally
		};

		CoreProtocol();
		~CoreProtocol();

		void reset();

		void startClientOut(const Jid &jid, bool oldOnly, bool tlsActive, bool doAuth, bool doCompression);
		void startServerOut(const QString &to);
		void startDialbackOut(const QString &to, const QString &from);
		void startDialbackVerifyOut(const QString &to, const QString &from, const QString &id, const QString &key);
		void startClientIn(const QString &id);
		void startServerIn(const QString &id);

		void setLang(const QString &s);
		void setAllowTLS(bool b);
		void setAllowBind(bool b);
		void setAllowPlain(bool b); // old-mode
		const Jid& jid() const;

		void setPassword(const QString &s);
		void setFrom(const QString &s);
		void setDialbackKey(const QString &s);

		// input
		QString user, host;

		// status
		bool old;

		StreamFeatures features;
		QStringList hosts;

		//static QString xmlToString(const QDomElement &e, bool clip=false);

		class DBItem
		{
		public:
			enum { ResultRequest, ResultGrant, VerifyRequest, VerifyGrant, Validated };
			int type;
			Jid to, from;
			QString key, id;
			bool ok;
		};

	private:
		enum Step {
			Start,
			Done,
			SendFeatures,
			GetRequest,
			HandleTLS,
			GetSASLResponse,
			IncHandleSASLSuccess,
			GetFeatures,        // read features packet
			HandleFeatures,     // act on features, by initiating tls, sasl, or bind
			GetTLSProceed,      // read <proceed/> tls response
			GetCompressProceed, // read <compressed/> compression response
			GetSASLFirst,       // perform sasl first step using provided data
			GetSASLChallenge,   // read server sasl challenge
			GetSASLNext,        // perform sasl next step using provided data
			HandleSASLSuccess,  // handle what must be done after reporting sasl success
			GetBindResponse,    // read bind response
			HandleAuthGet,      // send old-protocol auth-get
			GetAuthGetResponse, // read auth-get response
			HandleAuthSet,      // send old-protocol auth-set
			GetAuthSetResponse  // read auth-set response
		};

		QList<DBItem> dbrequests, dbpending, dbvalidated;

		bool server, dialback, dialback_verify;
		int step;

		bool digest;
		bool tls_started, sasl_started, compress_started;

		Jid jid_;
		bool oldOnly;
		bool allowPlain;
		bool doTLS, doAuth, doBinding, doCompress;
		QString password;

		QString dialback_id, dialback_key;
		QString self_from;

		void init();
		static int getOldErrorCode(const QDomElement &e);
		bool loginComplete();

		bool isValidStanza(const QDomElement &e) const;
		bool grabPendingItem(const Jid &to, const Jid &from, int type, DBItem *item);
		bool normalStep(const QDomElement &e);
		bool dialbackStep(const QDomElement &e);

		// reimplemented
		bool stepAdvancesParser() const;
		bool stepRequiresElement() const;
		void stringSend(const QString &s);
		void stringRecv(const QString &s);
		QString defaultNamespace();
		QStringList extraNamespaces();
		void handleStreamOpen(const Parser::Event &pe);
		bool doStep2(const QDomElement &e);
		void elementSend(const QDomElement &e);
		void elementRecv(const QDomElement &e);
	};
}

#endif
