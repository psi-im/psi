/*
 * Copyright (C) 2003-2007  Justin Karneges <justin@affinix.com>
 * Copyright (C) 2004,2005  Brad Hards <bradh@frogmouth.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include "qca_securelayer.h"

#include "qcaprovider.h"

#include <QTimer>
#include <QPointer>

namespace QCA {

Provider::Context *getContext(const QString &type, const QString &provider);

//----------------------------------------------------------------------------
// LayerTracker
//----------------------------------------------------------------------------
class LayerTracker
{
private:
	struct Item
	{
		int plain;
		qint64 encoded;
	};

	int p;
	QList<Item> list;

public:
	LayerTracker()
	{
		p = 0;
	}

	void reset()
	{
		p = 0;
		list.clear();
	}

	void addPlain(int plain)
	{
		p += plain;
	}

	void specifyEncoded(int encoded, int plain)
	{
		// can't specify more bytes than we have
		if(plain > p)
			plain = p;
		p -= plain;
		Item i;
		i.plain = plain;
		i.encoded = encoded;
		list += i;
	}

	int finished(qint64 encoded)
	{
		int plain = 0;
		for(QList<Item>::Iterator it = list.begin(); it != list.end();)
		{
			Item &i = *it;

			// not enough?
			if(encoded < i.encoded)
			{
				i.encoded -= encoded;
				break;
			}

			encoded -= i.encoded;
			plain += i.plain;
			it = list.erase(it);
		}
		return plain;
	}
};

//----------------------------------------------------------------------------
// SecureLayer
//----------------------------------------------------------------------------
SecureLayer::SecureLayer(QObject *parent)
:QObject(parent)
{
}

bool SecureLayer::isClosable() const
{
	return false;
}

void SecureLayer::close()
{
}

QByteArray SecureLayer::readUnprocessed()
{
	return QByteArray();
}

//----------------------------------------------------------------------------
// TLSSession
//----------------------------------------------------------------------------
TLSSession::TLSSession()
{
}

TLSSession::TLSSession(const TLSSession &from)
:Algorithm(from)
{
}

TLSSession::~TLSSession()
{
}

TLSSession & TLSSession::operator=(const TLSSession &from)
{
	Algorithm::operator=(from);
	return *this;
}

bool TLSSession::isNull() const
{
	return (!context() ? true : false);
}

//----------------------------------------------------------------------------
// TLS
//----------------------------------------------------------------------------
enum ResetMode
{
	ResetSession        = 0,
	ResetSessionAndData = 1,
	ResetAll            = 2
};

class TLS::Private : public QObject
{
	Q_OBJECT
public:
	TLS *q;
	TLSContext *c;
	TLS::Mode mode;

	bool active, server;
	CertificateChain localCert;
	PrivateKey localKey;
	CertificateCollection trusted;
	bool con_ssfMode;
	int con_minSSF, con_maxSSF;
	QStringList con_cipherSuites;
	QList<CertificateInfoOrdered> issuerList;
	TLSSession session;
	bool tryCompress;
	int packet_mtu;

	QString host;
	CertificateChain peerCert;
	Validity peerValidity;
	bool blocked;
	bool hostMismatch;
	TLSContext::SessionInfo sessionInfo;

	QByteArray in, out;
	QByteArray to_net, from_net;
	int pending_write;
	LayerTracker layer;
	bool need_finish_handshake;

	QList<QByteArray> packet_in, packet_out;
	QList<QByteArray> packet_to_net, packet_from_net;
	QList<int> packet_to_net_encoded;

	bool connect_hostNameReceived, connect_certificateRequested, connect_peerCertificateAvailable, connect_handshaken;

	enum { OpStart, OpUpdate };

	int op;

	bool handshaken, closing;
	bool tryMore;
	int bytesEncoded;
	Error errorCode;

	Private(TLS *_q, TLS::Mode _mode) : QObject(_q), q(_q), mode(_mode)
	{
		// c is 0 during initial reset, so we don't redundantly reset it
		c = 0;
		connect_hostNameReceived = false;
		connect_certificateRequested = false;
		connect_peerCertificateAvailable = false;
		connect_handshaken = false;
		server = false;

		reset(ResetAll);

		c = static_cast<TLSContext *>(q->context());

		// parent the context to us, so that moveToThread works
		c->setParent(this);

		connect(c, SIGNAL(resultsReady()), SLOT(tls_resultsReady()));
	}

	~Private()
	{
		// context is owned by Algorithm, unparent so we don't double-delete
		c->setParent(0);
	}

	void reset(ResetMode mode)
	{
		if(c)
			c->reset();

		if(!server)
			issuerList.clear();
		active = false;
		server = false;
		host = QString();
		out.clear();
		packet_out.clear();
		handshaken = false;
		closing = false;
		tryMore = false;
		bytesEncoded = 0;
		op = -1;
		pending_write = 0;
		blocked = false;
		layer.reset();
		need_finish_handshake = false;

		if(mode >= ResetSessionAndData)
		{
			peerCert = CertificateChain();
			peerValidity = ErrorValidityUnknown;
			hostMismatch = false;
			in.clear();
			to_net.clear();
			from_net.clear();
			packet_in.clear();
			packet_to_net.clear();
			packet_to_net_encoded.clear();
			packet_from_net.clear();
		}

		if(mode >= ResetAll)
		{
			localCert = CertificateChain();
			localKey = PrivateKey();
			trusted = CertificateCollection();
			con_ssfMode = true;
			con_minSSF = 128;
			con_maxSSF = -1;
			con_cipherSuites = QStringList();
			tryCompress = false;
			packet_mtu = -1;
			issuerList.clear();
			session = TLSSession();
		}
	}

	void start(bool serverMode)
	{
		active = true;
		server = serverMode;

		c->setup(serverMode, host, tryCompress);

		if(con_ssfMode)
			c->setConstraints(con_minSSF, con_maxSSF);
		else
			c->setConstraints(con_cipherSuites);

		c->setCertificate(localCert, localKey);
		c->setTrustedCertificates(trusted);
		if(serverMode)
			c->setIssuerList(issuerList);
		if(!session.isNull())
		{
			TLSSessionContext *sc = static_cast<TLSSessionContext*>(session.context());
			c->setSessionId(*sc);
		}
		c->setMTU(packet_mtu);

		op = OpStart;
		c->start();
	}

	void close()
	{
		if(!handshaken || closing)
			return;

		closing = true;
		c->shutdown();
	}

	void continueAfterStep()
	{
		//printf("continuing\n");

		blocked = false;
		update();
	}

	void update()
	{
		//printf("update attempt\n");
		if(blocked)
			return;

		// only allow one operation at a time
		if(op != -1)
			return;

		//printf("update\n");
		if(need_finish_handshake)
		{
			need_finish_handshake = false;

			handshaken = true;
			if(connect_handshaken)
			{
				blocked = true;
				QMetaObject::invokeMethod(q, "handshaken", Qt::QueuedConnection);
				return;
			}
		}

		if(!handshaken)
		{
			// FIXME: optimize this somehow.  we need to force
			//   the update after start() succeeds, but not in any
			//   other case afaict.

			if(mode == TLS::Stream)
			{
				// during handshake, only send from_net (no app data)
				//if(!from_net.isEmpty())
				//{
					op = OpUpdate;
					c->update(from_net, QByteArray());
					from_net.clear();
				//}
			}
			else
			{
				// note: there may not be a packet
				QByteArray pkt = packet_from_net.takeFirst();

				op = OpUpdate;
				c->update(pkt, QByteArray());
			}
		}
		else
		{
			if(mode == TLS::Stream)
			{
				// FIXME: we should be able to send both at once,
				//   but if we do this then qca-ossl gives us
				//   broken security layer.  probably a bug in
				//   qca-ossl?

				// otherwise, send both from_net and out
				/*if(!from_net.isEmpty() || !out.isEmpty())
				{
					op = OpUpdate;
					pending_write += out.size();
					c->update(from_net, out);
					from_net.clear();
					out.clear();
				}*/

				if(!from_net.isEmpty())
				{
					op = OpUpdate;
					c->update(from_net, QByteArray());
					from_net.clear();
				}
				else //if(!out.isEmpty())
				{
					op = OpUpdate;
					pending_write += out.size();
					c->update(QByteArray(), out);
					out.clear();
				}
			}
			else
			{
				op = OpUpdate;
				QByteArray pkta = packet_from_net.takeFirst();
				QByteArray pktb = packet_out.takeFirst();
				if(!pktb.isEmpty())
					packet_to_net_encoded += pktb.size();
				c->update(pkta, pktb);
			}
		}
	}

private slots:
	void tls_resultsReady()
	{
		QPointer<QObject> self = this;

		int last_op = op;
		op = -1;
		//printf("results ready: %d\n", last_op);

		if(last_op == OpStart)
		{
			bool ok = c->result() == TLSContext::Success;
			if(!ok)
			{
				reset(ResetSession);
				errorCode = TLS::ErrorInit;
				QMetaObject::invokeMethod(q, "error", Qt::QueuedConnection);
				//emit q->error();
				return;
			}

			update();
		}
		else // OpUpdate
		{
			TLSContext::Result r = c->result();
			QByteArray a = c->to_net();

			if(closing)
			{
				if(r == TLSContext::Error)
				{
					reset(ResetSession);
					errorCode = ErrorHandshake;
					QMetaObject::invokeMethod(q, "error", Qt::QueuedConnection);
					//emit q->error();
					return;
				}

				if(mode == TLS::Stream)
					to_net.append(a);
				else
					packet_to_net += a;

				if(!a.isEmpty())
				{
					QMetaObject::invokeMethod(q, "readyReadOutgoing", Qt::QueuedConnection);
					//emit q->readyReadOutgoing();
					if(!self)
						return;
				}

				if(r == TLSContext::Success)
				{
					from_net = c->unprocessed();
					reset(ResetSession);
					QMetaObject::invokeMethod(q, "closed", Qt::QueuedConnection);
					//emit q->closed();
					return;
				}

				return;
			}

			if(!handshaken)
			{
				if(r == TLSContext::Error)
				{
					reset(ResetSession);
					errorCode = TLS::ErrorHandshake;
					QMetaObject::invokeMethod(q, "error", Qt::QueuedConnection);
					//emit q->error();
					return;
				}

				if(mode == TLS::Stream)
					to_net.append(a);
				else
					packet_to_net += a;

				if(!a.isEmpty())
				{
					QMetaObject::invokeMethod(q, "readyReadOutgoing", Qt::QueuedConnection);
					//emit q->readyReadOutgoing();
					if(!self)
						return;
				}

				if(r == TLSContext::Success)
				{
					peerCert = c->peerCertificateChain();
					if(!peerCert.isEmpty())
					{
						peerValidity = c->peerCertificateValidity();
						if(peerValidity == ValidityGood && !host.isEmpty() && !peerCert.primary().matchesHostName(host))
							hostMismatch = true;
					}

					sessionInfo = c->sessionInfo();
					if(sessionInfo.id)
					{
						TLSSessionContext *sc = static_cast<TLSSessionContext*>(sessionInfo.id->clone());
						session.change(sc);
					}

					need_finish_handshake = true;
					if(connect_peerCertificateAvailable)
					{
						blocked = true;
						QMetaObject::invokeMethod(q, "peerCertificateAvailable", Qt::QueuedConnection);
						return;
					}

					update();
					return;
				}
				else // Continue
				{
					if(server)
					{
						bool clientHello = c->clientHelloReceived();
						if(clientHello)
						{
							host = c->hostName();
							if(!host.isEmpty())
							{
								if(connect_hostNameReceived)
								{
									blocked = true;
									emit q->hostNameReceived();
									if(!self)
										return;
								}
							}
						}
						return;
					}
					else
					{
						// TODO: check for server certificate here also, and emit peerCertificateAvailable

						bool serverHello = c->serverHelloReceived();
						if(serverHello)
						{
							if(c->certificateRequested())
							{
								issuerList = c->issuerList();
								if(connect_certificateRequested)
								{
									blocked = true;
									emit q->certificateRequested();
									if(!self)
										return;
								}
							}
						}
						return;
					}
				}

				return;
			}

			bool ok = (r == TLSContext::Success);
			if(!ok)
			{
				reset(ResetSession);
				errorCode = ErrorCrypt;
				QMetaObject::invokeMethod(q, "error", Qt::QueuedConnection);
				//emit q->error();
				return;
			}

			QByteArray b = c->to_app();
			//printf("to_app: %d (%s)\n", b.size(), b.data());
			bool eof = c->eof();
			int enc = c->encoded();

			bool more = false;
			if(mode == TLS::Stream)
			{
				pending_write -= enc;
				if(pending_write > 0)
					more = true;
			}
			else
			{
				if(!a.isEmpty() && enc > 0)
				{
					enc = packet_to_net_encoded.takeFirst();
					if(!packet_to_net_encoded.isEmpty())
						more = true;
				}

				// datagram mode might have more pending out
				if(!packet_out.isEmpty())
					more = true;
			}

			if(!out.isEmpty())
				more = true;

			if(mode == TLS::Stream)
			{
				to_net.append(a);
				in.append(b);
				bytesEncoded += enc;
			}
			else
			{
				packet_to_net += a;
				packet_in += b;
			}

			if(!a.isEmpty())
			{
				QMetaObject::invokeMethod(q, "readyReadOutgoing", Qt::QueuedConnection);
				//emit q->readyReadOutgoing();
				if(!self)
					return;
			}

			if(!b.isEmpty())
			{
				QMetaObject::invokeMethod(q, "readyRead", Qt::QueuedConnection);
				//emit q->readyRead();
				if(!self)
					return;
			}

			if(eof)
				close();

			if(eof || more)
				update();
		}
	}
};

TLS::TLS(QObject *parent, const QString &provider)
:SecureLayer(parent), Algorithm("tls", provider)
{
	d = new Private(this, TLS::Stream);
}

TLS::TLS(Mode mode, QObject *parent, const QString &provider)
:SecureLayer(parent), Algorithm(mode == Stream ? "tls" : "dtls", provider)
{
	d = new Private(this, mode);
}

TLS::~TLS()
{
	delete d;
}

void TLS::reset()
{
	d->reset(ResetAll);
}

QStringList TLS::supportedCipherSuites(const Version &version) const
{
	return d->c->supportedCipherSuites(version);
}

void TLS::setCertificate(const CertificateChain &cert, const PrivateKey &key)
{
	d->localCert = cert;
	d->localKey = key;
	if(d->active)
		d->c->setCertificate(cert, key);
}

void TLS::setCertificate(const KeyBundle &kb)
{
	setCertificate(kb.certificateChain(), kb.privateKey());
}

CertificateCollection TLS::trustedCertificates() const
{
	return d->trusted;
}

void TLS::setTrustedCertificates(const CertificateCollection &trusted)
{
	d->trusted = trusted;
	if(d->active)
		d->c->setTrustedCertificates(trusted);
}

void TLS::setConstraints(SecurityLevel s)
{
	int min = 128;
	switch(s)
	{
		case SL_None:
			min = 0;
			break;
		case SL_Integrity:
			min = 1;
			break;
		case SL_Export:
			min = 40;
			break;
		case SL_Baseline:
			min = 128;
			break;
		case SL_High:
			min = 129;
			break;
		case SL_Highest:
			min = qMax(129, d->c->maxSSF());
			break;
	}

	d->con_ssfMode = true;
	d->con_minSSF = min;
	d->con_maxSSF = -1;

	if(d->active)
		d->c->setConstraints(d->con_minSSF, d->con_maxSSF);
}

void TLS::setConstraints(int minSSF, int maxSSF)
{
	d->con_ssfMode = true;
	d->con_minSSF = minSSF;
	d->con_maxSSF = maxSSF;

	if(d->active)
		d->c->setConstraints(d->con_minSSF, d->con_maxSSF);
}

void TLS::setConstraints(const QStringList &cipherSuiteList)
{
	d->con_ssfMode = false;
	d->con_cipherSuites = cipherSuiteList;

	if(d->active)
		d->c->setConstraints(d->con_cipherSuites);
}

QList<CertificateInfoOrdered> TLS::issuerList() const
{
	return d->issuerList;
}

void TLS::setIssuerList(const QList<CertificateInfoOrdered> &issuers)
{
	d->issuerList = issuers;
	if(d->active)
		d->c->setIssuerList(issuers);
}

void TLS::setSession(const TLSSession &session)
{
	d->session = session;
}

bool TLS::canCompress() const
{
	return d->c->canCompress();
}

bool TLS::canSetHostName() const
{
	return d->c->canSetHostName();
}

bool TLS::compressionEnabled() const
{
	return d->tryCompress;
}

void TLS::setCompressionEnabled(bool b)
{
	d->tryCompress = b;
}

void TLS::startClient(const QString &host)
{
	d->reset(ResetSessionAndData);
	d->host = host;
	d->issuerList.clear();

	// client mode
	d->start(false);
}

void TLS::startServer()
{
	d->reset(ResetSessionAndData);

	// server mode
	d->start(true);
}

void TLS::continueAfterStep()
{
	d->continueAfterStep();
}

bool TLS::isHandshaken() const
{
	return d->handshaken;
}

bool TLS::isCompressed() const
{
	return d->sessionInfo.isCompressed;
}

TLS::Version TLS::version() const
{
	return d->sessionInfo.version;
}

QString TLS::cipherSuite() const
{
	return d->sessionInfo.cipherSuite;
}

int TLS::cipherBits() const
{
	return d->sessionInfo.cipherBits;
}

int TLS::cipherMaxBits() const
{
	return d->sessionInfo.cipherMaxBits;
}

TLSSession TLS::session() const
{
	return d->session;
}

TLS::Error TLS::errorCode() const
{
	return d->errorCode;
}

TLS::IdentityResult TLS::peerIdentityResult() const
{
	if(d->peerCert.isEmpty())
		return NoCertificate;

	if(d->peerValidity != ValidityGood)
		return InvalidCertificate;

	if(d->hostMismatch)
		return HostMismatch;

	return Valid;
}

Validity TLS::peerCertificateValidity() const
{
	return d->peerValidity;
}

CertificateChain TLS::localCertificateChain() const
{
	return d->localCert;
}

PrivateKey TLS::localPrivateKey() const
{
	return d->localKey;
}

CertificateChain TLS::peerCertificateChain() const
{
	return d->peerCert;
}

bool TLS::isClosable() const
{
	return true;
}

int TLS::bytesAvailable() const
{
	if(d->mode == Stream)
		return d->in.size();
	else
		return 0;
}

int TLS::bytesOutgoingAvailable() const
{
	if(d->mode == Stream)
		return d->to_net.size();
	else
		return 0;
}

void TLS::close()
{
	d->close();
	d->update();
}

void TLS::write(const QByteArray &a)
{
	if(d->mode == Stream)
	{
		d->out.append(a);
		d->layer.addPlain(a.size());
	}
	else
		d->packet_out.append(a);
	d->update();
}

QByteArray TLS::read()
{
	if(d->mode == Stream)
	{
		QByteArray a = d->in;
		d->in.clear();
		return a;
	}
	else
		return d->packet_in.takeFirst();
}

void TLS::writeIncoming(const QByteArray &a)
{
	if(d->mode == Stream)
		d->from_net.append(a);
	else
		d->packet_from_net.append(a);
	d->update();
}

QByteArray TLS::readOutgoing(int *plainBytes)
{
	if(d->mode == Stream)
	{
		QByteArray a = d->to_net;
		d->to_net.clear();
		if(plainBytes)
			*plainBytes = d->bytesEncoded;
		d->layer.specifyEncoded(a.size(), d->bytesEncoded);
		d->bytesEncoded = 0;
		return a;
	}
	else
	{
		QByteArray a = d->packet_to_net.takeFirst();
		int x = d->packet_to_net_encoded.takeFirst();
		if(plainBytes)
			*plainBytes = x;
		return a;
	}
}

QByteArray TLS::readUnprocessed()
{
	if(d->mode == Stream)
	{
		QByteArray a = d->from_net;
		d->from_net.clear();
		return a;
	}
	else
		return QByteArray();
}

int TLS::convertBytesWritten(qint64 bytes)
{
	return d->layer.finished(bytes);
}

int TLS::packetsAvailable() const
{
	return d->packet_in.count();
}

int TLS::packetsOutgoingAvailable() const
{
	return d->packet_to_net.count();
}

int TLS::packetMTU() const
{
	return d->packet_mtu;
}

void TLS::setPacketMTU(int size) const
{
	d->packet_mtu = size;
	if(d->active)
		d->c->setMTU(size);
}

void TLS::connectNotify(const char *signal)
{
	if(signal == QMetaObject::normalizedSignature(SIGNAL(hostNameReceived())))
		d->connect_hostNameReceived = true;
	else if(signal == QMetaObject::normalizedSignature(SIGNAL(certificateRequested())))
		d->connect_certificateRequested = true;
	else if(signal == QMetaObject::normalizedSignature(SIGNAL(peerCertificateAvailable())))
		d->connect_peerCertificateAvailable = true;
	else if(signal == QMetaObject::normalizedSignature(SIGNAL(handshaken())))
		d->connect_handshaken = true;
}

void TLS::disconnectNotify(const char *signal)
{
	if(signal == QMetaObject::normalizedSignature(SIGNAL(hostNameReceived())))
		d->connect_hostNameReceived = false;
	else if(signal == QMetaObject::normalizedSignature(SIGNAL(certificateRequested())))
		d->connect_certificateRequested = false;
	else if(signal == QMetaObject::normalizedSignature(SIGNAL(peerCertificateAvailable())))
		d->connect_peerCertificateAvailable = false;
	else if(signal == QMetaObject::normalizedSignature(SIGNAL(handshaken())))
		d->connect_handshaken = false;
}

//----------------------------------------------------------------------------
// SASL::Params
//----------------------------------------------------------------------------
class SASL::Params::Private
{
public:
	bool needUsername, canSendAuthzid, needPassword, canSendRealm;
};

SASL::Params::Params()
:d(new Private)
{
}

SASL::Params::Params(bool user, bool authzid, bool pass, bool realm)
:d(new Private)
{
	d->needUsername = user;
	d->canSendAuthzid = authzid;
	d->needPassword = pass;
	d->canSendRealm = realm;
}

SASL::Params::Params(const SASL::Params &from)
:d(new Private(*from.d))
{
}

SASL::Params::~Params()
{
	delete d;
}

SASL::Params & SASL::Params::operator=(const SASL::Params &from)
{
	*d = *from.d;
	return *this;
}

bool SASL::Params::needUsername() const
{
	return d->needUsername;
}

bool SASL::Params::canSendAuthzid() const
{
	return d->canSendAuthzid;
}

bool SASL::Params::needPassword() const
{
	return d->needPassword;
}

bool SASL::Params::canSendRealm() const
{
	return d->canSendRealm;
}

//----------------------------------------------------------------------------
// SASL
//----------------------------------------------------------------------------
/*
  These don't map, but I don't think it matters much..
    SASL_TRYAGAIN  (-8)  transient failure (e.g., weak key)
    SASL_BADMAC    (-9)  integrity check failed
      -- client only codes --
    SASL_WRONGMECH (-11) mechanism doesn't support requested feature
    SASL_NEWSECRET (-12) new secret needed
      -- server only codes --
    SASL_TRANS     (-17) One time use of a plaintext password will
                         enable requested mechanism for user
    SASL_PWLOCK    (-21) password locked
    SASL_NOCHANGE  (-22) requested change was not needed
*/

class SASL::Private : public QObject
{
	Q_OBJECT
public:
	SASL *q;
	SASLContext *c;

	enum
	{
		OpStart,
		OpServerFirstStep,
		OpNextStep,
		OpTryAgain,
		OpUpdate
	};

	AuthFlags auth_flags;
	int ssfmin, ssfmax;
	QString ext_authid;
	int ext_ssf;
	bool localSet, remoteSet;
	SASLContext::HostPort local, remote;

	bool set_username, set_authzid, set_password, set_realm;
	QString username, authzid, realm;
	SecureArray password;

	bool server;
	QStringList mechlist;
	QString server_realm;
	bool allowClientSendFirst;
	bool disableServerSendLast;

	int op;
	bool first;
	bool authed;
	Error errorCode;
	QByteArray out;
	QByteArray in;
	QByteArray to_net;
	QByteArray from_net;
	int bytesEncoded;
	int pending_write;
	LayerTracker layer;

	Private(SASL *_q) : QObject(_q), q(_q)
	{
		c = 0;
		set_username = false;
		set_authzid = false;
		set_password = false;
		set_realm = false;

		reset(ResetAll);

		c = static_cast<SASLContext *>(q->context());

		// parent the context to us, so that moveToThread works
		c->setParent(this);

		connect(c, SIGNAL(resultsReady()), SLOT(sasl_resultsReady()));
	}

	~Private()
	{
		// context is owned by Algorithm, unparent so we don't double-delete
		c->setParent(0);
	}

	void reset(ResetMode mode)
	{
		if(c)
		{
			c->reset();

			if(mode < ResetAll)
			{
				QString *p_username = 0;
				QString *p_authzid = 0;
				SecureArray *p_password = 0;
				QString *p_realm = 0;

				if(set_username)
					p_username = &username;
				if(set_authzid)
					p_authzid = &authzid;
				if(set_password)
					p_password = &password;
				if(set_realm)
					p_realm = &realm;

				c->setClientParams(p_username, p_authzid, p_password, p_realm);
			}
		}

		op = -1;
		authed = false;
		out.clear();
		from_net.clear();
		bytesEncoded = 0;
		pending_write = 0;
		layer.reset();

		if(mode >= ResetSessionAndData)
		{
			in.clear();
			to_net.clear();
		}

		if(mode >= ResetAll)
		{
			auth_flags = SASL::AuthFlagsNone;
			ssfmin = 0;
			ssfmax = 0;
			ext_authid = QString();
			ext_ssf = 0;
			localSet = false;
			remoteSet = false;
			local = SASLContext::HostPort();
			remote = SASLContext::HostPort();

			set_username = false;
			username = QString();
			set_authzid = false;
			authzid = QString();
			set_password = false;
			password = SecureArray();
			set_realm = false;
			realm = QString();
		}
	}

	void setup(const QString &service, const QString &host)
	{
		c->setup(service, host, localSet ? &local : 0, remoteSet ? &remote : 0, ext_authid, ext_ssf);
		c->setConstraints(auth_flags, ssfmin, ssfmax);
	}

	void start()
	{
		op = OpStart;
		first = true;

		if(server)
			c->startServer(server_realm, disableServerSendLast);
		else
			c->startClient(mechlist, allowClientSendFirst);
	}

	void putServerFirstStep(const QString &mech, const QByteArray *clientInit)
	{
		if(op != -1)
			return;

		op = OpServerFirstStep;

		c->serverFirstStep(mech, clientInit);
	}

	void putStep(const QByteArray &stepData)
	{
		if(op != -1)
			return;

		op = OpNextStep;

		c->nextStep(stepData);
	}

	void tryAgain()
	{
		if(op != -1)
			return;

		op = OpTryAgain;

		c->tryAgain();
	}

	void update()
	{
		if(op != -1)
			return;

		op = OpUpdate;

		pending_write += out.size();
		c->update(from_net, out);
		from_net.clear();
		out.clear();
	}

private slots:
	void sasl_resultsReady()
	{
		QPointer<QObject> self = this;

		int last_op = op;
		op = -1;

		SASLContext::Result r = c->result();

		if(last_op == OpStart)
		{
			if(server)
			{
				if(r != SASLContext::Success)
				{
					errorCode = SASL::ErrorInit;
					emit q->error();
					return;
				}

				emit q->serverStarted();
				return;
			}
			else // client
			{
				// fall into this logic
				last_op = OpTryAgain;
			}
		}
		else if(last_op == OpServerFirstStep)
		{
			// fall into this logic
			last_op = OpTryAgain;
		}
		else if(last_op == OpNextStep)
		{
			// fall into this logic
			last_op = OpTryAgain;
		}

		if(last_op == OpTryAgain)
		{
			if(server)
			{
				if(r == SASLContext::Continue)
				{
					emit q->nextStep(c->stepData());
					return;
				}
				else if(r == SASLContext::AuthCheck)
				{
					emit q->authCheck(c->username(), c->authzid());
					return;
				}
				else if(r == SASLContext::Success)
				{
					// FIXME: not signal safe
					emit q->nextStep(c->stepData());

					authed = true;
					emit q->authenticated();
					return;
				}
				else // error
				{
					errorCode = SASL::ErrorHandshake;
					emit q->error();
					return;
				}
			}
			else // client
			{
				if(first)
				{
					if(r == SASLContext::Error)
					{
						if(first)
							errorCode = SASL::ErrorInit;
						else
							errorCode = SASL::ErrorHandshake;
						emit q->error();
						return;
					}
					else if(r == SASLContext::Params)
					{
						Params np = c->clientParams();
						emit q->needParams(np);
						return;
					}

					first = false;
					emit q->clientStarted(c->haveClientInit(), c->stepData());
					return;
				}
				else
				{
					if(r == SASLContext::Error)
					{
						errorCode = ErrorHandshake;
						emit q->error();
						return;
					}
					else if(r == SASLContext::Params)
					{
						Params np = c->clientParams();
						emit q->needParams(np);
						return;
					}
					else if(r == SASLContext::Continue)
					{
						emit q->nextStep(c->stepData());
						return;
					}
					else if(r == SASLContext::Success)
					{
						// FIXME: not signal safe
						emit q->nextStep(c->stepData());

						authed = true;
						emit q->authenticated();
						return;
					}
				}
			}
		}
		else if(last_op == OpUpdate)
		{
			if(r != SASLContext::Success)
			{
				errorCode = ErrorCrypt;
				emit q->error();
				return;
			}

			QByteArray a = c->to_net();
			QByteArray b = c->to_app();
			int enc = c->encoded();

			to_net += a;
			in += b;

			bool more = false;
			pending_write -= enc;
			if(pending_write > 0)
				more = true;
			bytesEncoded += enc;

			if(!a.isEmpty())
			{
				emit q->readyReadOutgoing();
				if(!self)
					return;
			}

			if(!b.isEmpty())
			{
				emit q->readyRead();
				if(!self)
					return;
			}

			if(more)
				update();
		}
	}
};

SASL::SASL(QObject *parent, const QString &provider)
:SecureLayer(parent), Algorithm("sasl", provider)
{
	d = new Private(this);
}

SASL::~SASL()
{
	delete d;
}

void SASL::reset()
{
	d->reset(ResetAll);
}

SASL::Error SASL::errorCode() const
{
	return d->errorCode;
}

SASL::AuthCondition SASL::authCondition() const
{
	return d->c->authCondition();
}

void SASL::setConstraints(AuthFlags f, SecurityLevel s)
{
	int min = 0;
	if (s == SL_Integrity)
		min = 1;
	else if (s == SL_Export)
		min = 56;
	else if (s == SL_Baseline)
		min = 128;
	else if (s == SL_High)
		min = 192;
	else if (s == SL_Highest)
		min = 256;

	setConstraints(f, min, 256);
}

void SASL::setConstraints(AuthFlags f, int minSSF, int maxSSF)
{
	d->auth_flags = f;

	d->ssfmin = minSSF;
	d->ssfmax = maxSSF;
}

void SASL::setExternalAuthId(const QString &authid)
{
	d->ext_authid = authid;
}

void SASL::setExternalSSF(int strength)
{
	d->ext_ssf = strength;
}

void SASL::setLocalAddress(const QString &addr, quint16 port)
{
	d->localSet = true;
	d->local.addr = addr;
	d->local.port = port;
}

void SASL::setRemoteAddress(const QString &addr, quint16 port)
{
	d->remoteSet = true;
	d->remote.addr = addr;
	d->remote.port = port;
}

void SASL::startClient(const QString &service, const QString &host, const QStringList &mechlist, ClientSendMode mode)
{
	d->reset(ResetSessionAndData);
	d->setup(service, host);
	d->server = false;
	d->mechlist = mechlist;
	d->allowClientSendFirst = (mode == AllowClientSendFirst);
	d->start();
}

void SASL::startServer(const QString &service, const QString &host, const QString &realm, ServerSendMode mode)
{
	d->reset(ResetSessionAndData);
	d->setup(service, host);
	d->server = true;
	d->server_realm = realm;
	d->disableServerSendLast = (mode == DisableServerSendLast);
	d->start();
}

void SASL::putServerFirstStep(const QString &mech)
{
	d->putServerFirstStep(mech, 0);
}

void SASL::putServerFirstStep(const QString &mech, const QByteArray &clientInit)
{
	d->putServerFirstStep(mech, &clientInit);
}

void SASL::putStep(const QByteArray &stepData)
{
	d->putStep(stepData);
}

void SASL::setUsername(const QString &user)
{
	d->set_username = true;
	d->username = user;
	d->c->setClientParams(&user, 0, 0, 0);
}

void SASL::setAuthzid(const QString &authzid)
{
	d->set_authzid = true;
	d->authzid = authzid;
	d->c->setClientParams(0, &authzid, 0, 0);
}

void SASL::setPassword(const SecureArray &pass)
{
	d->set_password = true;
	d->password = pass;
	d->c->setClientParams(0, 0, &pass, 0);
}

void SASL::setRealm(const QString &realm)
{
	d->set_realm = true;
	d->realm = realm;
	d->c->setClientParams(0, 0, 0, &realm);
}

void SASL::continueAfterParams()
{
	d->tryAgain();
}

void SASL::continueAfterAuthCheck()
{
	d->tryAgain();
}

QString SASL::mechanism() const
{
	return d->c->mech();
}

QStringList SASL::mechanismList() const
{
	return d->c->mechlist();
}

QStringList SASL::realmList() const
{
	return d->c->realmlist();
}

int SASL::ssf() const
{
	return d->c->ssf();
}

int SASL::bytesAvailable() const
{
	return d->in.size();
}

int SASL::bytesOutgoingAvailable() const
{
	return d->to_net.size();
}

void SASL::write(const QByteArray &a)
{
	d->out.append(a);
	d->layer.addPlain(a.size());
	d->update();
}

QByteArray SASL::read()
{
	QByteArray a = d->in;
	d->in.clear();
	return a;
}

void SASL::writeIncoming(const QByteArray &a)
{
	d->from_net.append(a);
	d->update();
}

QByteArray SASL::readOutgoing(int *plainBytes)
{
	QByteArray a = d->to_net;
	d->to_net.clear();
	if(plainBytes)
		*plainBytes = d->bytesEncoded;
	d->layer.specifyEncoded(a.size(), d->bytesEncoded);
	d->bytesEncoded = 0;
	return a;
}

int SASL::convertBytesWritten(qint64 bytes)
{
	return d->layer.finished(bytes);
}

}

#include "qca_securelayer.moc"
