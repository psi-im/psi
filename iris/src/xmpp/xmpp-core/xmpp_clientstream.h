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

#ifndef XMPP_CLIENTSTREAM_H
#define XMPP_CLIENTSTREAM_H

#include <QtCrypto>

#include "xmpp_stream.h"

class QByteArray;
class QString;
class QDomDocument;
class QDomElement;
class QObject;
class ByteStream;
class QHostAddress;

namespace XMPP
{
	class TLSHandler;
	class Connector;

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
		enum AllowPlainType {
			NoAllowPlain,
			AllowPlain,
			AllowPlainOverTLS
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
		void setAuthzid(const QString &s);
		void continueAfterParams();

		// SASL information
		QString saslMechanism() const;
		int saslSSF() const;

		// binding
		void setResourceBinding(bool);

		// Language
		void setLang(const QString&);

		// security options (old protocol only uses the first !)
		void setAllowPlain(AllowPlainType);
		void setRequireMutualAuth(bool);
		void setSSFRange(int low, int high);
		void setOldOnly(bool);
		void setSASLMechanism(const QString &s);
		void setLocalAddr(const QHostAddress &addr, quint16 port);

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

		// barracuda extension
		QStringList hosts() const;

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
}

#endif
