/*
 * Copyright (C) 2003-2005  Justin Karneges <justin@affinix.com>
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

#include <QtCore>
#include "qcaprovider.h"

namespace QCA {

Provider::Context *getContext(const QString &type, const QString &provider);

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

	CertificateChain localCert;
	PrivateKey localKey;
	CertificateCollection trusted;
	bool con_ssfMode;
	int con_minSSF, con_maxSSF;
	QStringList con_cipherSuites;
	bool tryCompress;

	QString host;
	CertificateChain peerCert;
	Validity peerValidity;
	bool hostMismatch;
	TLSContext::SessionInfo sessionInfo;

	QByteArray in, out;
	QByteArray to_net, from_net;

	enum { OpStart, OpHandshake, OpShutdown, OpEncode, OpDecode };
	int last_op;

	bool handshaken, closing, closed, error;
	bool tryMore;
	int bytesEncoded;
	Error errorCode;

	Private(TLS *_q) : QObject(_q), q(_q)
	{
		c = 0;

		reset(ResetAll);
	}

	void reset(ResetMode mode)
	{
		if(c)
			c->reset();

		host = QString();
		out.clear();
		handshaken = false;
		closing = false;
		closed = false;
		error = false;
		tryMore = false;
		bytesEncoded = 0;

		if(mode >= ResetSessionAndData)
		{
			peerCert = CertificateChain();
			peerValidity = ErrorValidityUnknown;
			hostMismatch = false;
			in.clear();
			to_net.clear();
			from_net.clear();
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
		}
	}

	bool start(bool serverMode)
	{
		if(con_ssfMode)
			c->setConstraints(con_minSSF, con_maxSSF);
		else
			c->setConstraints(con_cipherSuites);

		c->setup(trusted, localCert, localKey, serverMode, host, tryCompress, false);

		bool ok;
		c->start();
		last_op = OpStart;
		c->waitForResultsReady(-1);
		ok = c->result() == TLSContext::Success;
		if(!ok)
			return false;

		update();
		return true;
	}

	void close()
	{
		if(!handshaken || closing)
			return;

		closing = true;
	}

	void update()
	{
		bool wasHandshaken = handshaken;
		//q->layerUpdateBegin();
		int _read = q->bytesAvailable();
		int _readout = q->bytesOutgoingAvailable();
		bool _closed = closed;
		bool _error = error;

		if(closing)
			updateClosing();
		else
			updateMain();

		if(!wasHandshaken && handshaken)
			QMetaObject::invokeMethod(q, "handshaken", Qt::QueuedConnection);

		if(q->bytesAvailable() > _read)
		{
			emit q->readyRead();
			//QMetaObject::invokeMethod(q, "readyRead", Qt::QueuedConnection);
		}
		if(q->bytesOutgoingAvailable() > _readout)
			QMetaObject::invokeMethod(q, "readyReadOutgoing", Qt::QueuedConnection);
		if(!_closed && closed)
			QMetaObject::invokeMethod(q, "closed", Qt::QueuedConnection);
		if(!_error && error)
			QMetaObject::invokeMethod(q, "error", Qt::QueuedConnection);
		//q->layerUpdateEnd();
	}

	void updateClosing()
	{
		QByteArray a;
		TLSContext::Result r;
		c->update(from_net, QByteArray());
		last_op = OpShutdown;
		c->waitForResultsReady(-1);
		a = c->to_net();
		r = c->result();
		from_net.clear();

		if(r == TLSContext::Error)
		{
			reset(ResetSession);
			error = true;
			errorCode = ErrorHandshake;
			return;
		}

		to_net.append(a);

		if(r == TLSContext::Success)
		{
			from_net = c->unprocessed();
			reset(ResetSession);
			closed = true;
			return;
		}
	}

	void updateMain()
	{
		bool force_read = false;

		if(!handshaken)
		{
			QByteArray a;
			TLSContext::Result r;
			c->update(from_net, QByteArray());
			last_op = OpHandshake;
			c->waitForResultsReady(-1);
			a = c->to_net();
			r = c->result();
			from_net.clear();

			if(r == TLSContext::Error)
			{
				reset(ResetSession);
				error = true;
				errorCode = ErrorHandshake;
				return;
			}

			to_net.append(a);

			if(r == TLSContext::Success)
			{
				peerCert = c->peerCertificateChain();
				if(!peerCert.isEmpty())
				{
					peerValidity = c->peerCertificateValidity();
					if(peerValidity == ValidityGood && !host.isEmpty() && !peerCert.primary().matchesHostname(host))
						hostMismatch = true;
				}
				sessionInfo = c->sessionInfo();
				handshaken = true;
				force_read = true;
			}
		}

		if(handshaken)
		{
			bool eof = false;

			if(!out.isEmpty() || tryMore)
			{
				tryMore = false;
				QByteArray a;
				int enc;
				bool more = false;
				c->update(QByteArray(), out);
				c->waitForResultsReady(-1);
				bool ok = c->result() == TLSContext::Success;
				a = c->to_net();
				enc = c->encoded();
				eof = c->eof();
				if(ok && enc < out.size())
					more = true;
				out.clear();
				if(!eof)
				{
					if(!ok)
					{
						reset(ResetSession);
						error = true;
						errorCode = ErrorCrypt;
						return;
					}
					bytesEncoded += enc;
					if(more)
						tryMore = true;
					to_net.append(a);
				}
			}

			if(!from_net.isEmpty() || force_read)
			{
				QByteArray a;
				QByteArray b;
				c->update(from_net, QByteArray());
				c->waitForResultsReady(-1);
				bool ok = c->result() == TLSContext::Success;
				a = c->to_app();
				b = c->to_net();
				eof = c->eof();
				from_net.clear();
				if(!ok)
				{
					reset(ResetSession);
					error = true;
					errorCode = ErrorCrypt;
					return;
				}
				in.append(a);
				to_net.append(b);
			}

			if(eof)
			{
				close();
				updateClosing();
				return;
			}
		}
	}

private slots:
	void tls_resultsReady()
	{
		//printf("results ready\n");
	}
};

TLS::TLS(QObject *parent, const QString &provider)
:SecureLayer(parent), Algorithm("tls", provider)
{
	d = new Private(this);
	d->c = static_cast<TLSContext *>(context());
	d->c->setParent(d);
	connect(d->c, SIGNAL(resultsReady()), d, SLOT(tls_resultsReady()));
}

TLS::TLS(Mode mode, QObject *parent, const QString &provider)
:SecureLayer(parent), Algorithm(mode == Stream ? "tls" : "dtls", provider)
{
	d = new Private(this);
	d->c = static_cast<TLSContext *>(context());
	d->c->setParent(d);
	connect(d->c, SIGNAL(resultsReady()), d, SLOT(tls_resultsReady()));
}

TLS::~TLS()
{
	d->c->setParent(0);
	delete d;
}

void TLS::reset()
{
	d->reset(ResetAll);
}

QStringList TLS::supportedCipherSuites(const Version &version, const QString &provider)
{
	QStringList list;
	const TLSContext *c = static_cast<const TLSContext *>(getContext(version == DTLS_v1 ? "dtls" : "tls", provider));
	if(!c)
		return list;
	list = c->supportedCipherSuites(version);
	delete c;
	return list;
}

void TLS::setCertificate(const CertificateChain &cert, const PrivateKey &key)
{
	d->localCert = cert;
	d->localKey = key;
}

void TLS::setTrustedCertificates(const CertificateCollection &trusted)
{
	d->trusted = trusted;
}

void TLS::setConstraints(SecurityLevel s)
{
	int min = SL_Baseline;
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
}

void TLS::setConstraints(int minSSF, int maxSSF)
{
	d->con_ssfMode = true;
	d->con_minSSF = minSSF;
	d->con_maxSSF = maxSSF;
}

void TLS::setConstraints(const QStringList &cipherSuiteList)
{
	d->con_ssfMode = false;
	d->con_cipherSuites = cipherSuiteList;
}

bool TLS::canCompress(Mode mode, const QString &provider)
{
	bool ok = false;
	const TLSContext *c = static_cast<const TLSContext *>(getContext(mode == Stream ? "tls" : "dtls", provider));
	if(!c)
		return ok;
	ok = c->canCompress();
	delete c;
	return ok;
}

void TLS::setCompressionEnabled(bool b)
{
	d->tryCompress = b;
}

void TLS::startClient(const QString &host)
{
	d->reset(ResetSessionAndData);
	d->host = host;
	//layerUpdateBegin();
	if(!d->start(false))
	{
		d->reset(ResetSession);
		d->error = true;
		d->errorCode = ErrorInit;
		QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection);
	}
	//layerUpdateEnd();
}

void TLS::startServer()
{
	d->reset(ResetSessionAndData);
	//layerUpdateBegin();
	if(!d->start(true))
	{
		d->reset(ResetSession);
		d->error = true;
		d->errorCode = ErrorInit;
		QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection);
	}
	//layerUpdateEnd();
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
	return d->in.size();
}

int TLS::bytesOutgoingAvailable() const
{
	return d->to_net.size();
}

void TLS::close()
{
	d->close();
	d->update();
}

void TLS::write(const QByteArray &a)
{
	d->out.append(a);
	d->update();
}

QByteArray TLS::read()
{
	QByteArray a = d->in;
	d->in.clear();
	return a;
}

void TLS::writeIncoming(const QByteArray &a)
{
	d->from_net.append(a);
	d->update();
}

QByteArray TLS::readOutgoing(int *plainBytes)
{
	QByteArray a = d->to_net;
	d->to_net.clear();
	if(plainBytes)
		*plainBytes = d->bytesEncoded;
	d->bytesEncoded = 0;
	return a;
}

QByteArray TLS::readUnprocessed()
{
	QByteArray a = d->from_net;
	d->from_net.clear();
	return a;
}

int TLS::packetsAvailable() const
{
	// TODO
	return 0;
}

int TLS::packetsOutgoingAvailable() const
{
	// TODO
	return 0;
}

void TLS::setPacketMTU(int size) const
{
	// TODO
	Q_UNUSED(size);
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

QString *saslappname = 0;
class SASL::Private : public QObject
{
	Q_OBJECT
private:
	SASL *sasl;
public:
	Private(SASL *parent)
	{
		sasl = parent;
	}

	void setup(const QString &service, const QString &host)
	{
		c->setup(service, host, localSet ? &local : 0, remoteSet ? &remote : 0, ext_authid, ext_ssf);
		c->setConstraints(auth_flags, ssfmin, ssfmax);
	}

	void handleServerFirstStep()
	{
		errorCode = ErrorHandshake;

		if(c->result() == SASLContext::Success)
			QMetaObject::invokeMethod(sasl, "authenticated", Qt::QueuedConnection);
		else if(c->result() == SASLContext::Continue)
			QMetaObject::invokeMethod(sasl, "nextStep", Qt::QueuedConnection, Q_ARG(QByteArray, c->stepData())); // TODO: double-check this!
		else if(c->result() == SASLContext::AuthCheck ||
		        c->result() == SASLContext::NeedParams)
			QMetaObject::invokeMethod(this, "tryAgain", Qt::QueuedConnection);
		else
			QMetaObject::invokeMethod(sasl, "error", Qt::QueuedConnection);
	}

	void update()
	{
		int _read    = sasl->bytesAvailable();
		int _readout = sasl->bytesOutgoingAvailable();

	// 	bool force_read = false;
	//
	// 	if(!handshaken)
	// 	{
	// 		QByteArray a;
	// 		TLSContext::Result r;
	// 		c->update(from_net, QByteArray());
	// 		last_op = OpHandshake;
	// 		c->waitForResultsReady(-1);
	// 		a = c->to_net();
	// 		r = c->result();
	// 		from_net.clear();
	//
	// 		if(r == TLSContext::Error)
	// 		{
	// 			reset(ResetSession);
	// 			error = true;
	// 			errorCode = ErrorHandshake;
	// 			return;
	// 		}
	//
	// 		to_net.append(a);
	//
	// 		if(r == TLSContext::Success)
	// 		{
	// 			peerCert = c->peerCertificateChain();
	// 			if(!peerCert.isEmpty())
	// 			{
	// 				peerValidity = c->peerCertificateValidity();
	// 				if(peerValidity == ValidityGood && !host.isEmpty() && !peerCert.primary().matchesHostname(host))
	// 					hostMismatch = true;
	// 			}
	// 			sessionInfo = c->sessionInfo();
	// 			handshaken = true;
	// 			force_read = true;
	// 		}
	// 	}
	//
	// 	if(handshaken)
	// 	{
			bool eof        = false;
			bool tryMore    = false;
			bool force_read = false;

			if(!out.isEmpty() || tryMore)
			{
				tryMore = false;
				QByteArray a;
				int enc;
				bool more = false;
				c->update(QByteArray(), out);
				c->waitForResultsReady(-1);
				bool ok = c->result() == SASLContext::Success;
				a = c->to_net();
				enc = c->encoded();
				// eof = c->eof();
				if(ok && enc < out.size())
					more = true;
				out.clear();
				if(!eof)
				{
					if(!ok)
					{
						sasl->reset();
						// error = true;
						// errorCode = ErrorCrypt;
						return;
					}
					bytesEncoded += enc;
					if(more)
						tryMore = true;
					to_net.append(a);
				}
			}

			if(!from_net.isEmpty() || force_read)
			{
				QByteArray a;
				QByteArray b;
				c->update(from_net, QByteArray());
				c->waitForResultsReady(-1);
				bool ok = c->result() == SASLContext::Success;
				a = c->to_app();
				b = c->to_net();
				// eof = c->eof();
				from_net.clear();
				if(!ok)
				{
					sasl->reset();
					// error = true;
					// errorCode = ErrorCrypt;
					return;
				}
				in.append(a);
				to_net.append(b);
			}

	// 		if(eof)
	// 		{
	// 			close();
	// 			updateClosing();
	// 			return;
	// 		}
	// 	}

		if(sasl->bytesAvailable() > _read)
		{
			//emit sasl->readyRead();
			QMetaObject::invokeMethod(sasl, "readyRead", Qt::QueuedConnection);
		}
		if(sasl->bytesOutgoingAvailable() > _readout)
			QMetaObject::invokeMethod(sasl, "readyReadOutgoing", Qt::QueuedConnection);
	}

	QByteArray out;
	QByteArray in;
	QByteArray to_net;
	QByteArray from_net;
	int bytesEncoded;

	// security opts
	AuthFlags auth_flags;
	int ssfmin, ssfmax;
	QString ext_authid;
	int ext_ssf;

	bool tried;
	SASLContext *c;
	SASLContext::HostPort local, remote;
	bool localSet, remoteSet;
	QByteArray stepData; // mblsha: wtf is this for?
	bool allowClientSendFirst;
	bool disableServerSendLast;
	bool first, server;
	Error errorCode;

public slots:
	void tryAgain();
};

SASL::SASL(QObject *parent, const QString &provider)
:SecureLayer(parent), Algorithm("sasl", provider)
{
	d = new Private(this);
	d->c = (SASLContext *)context();
	reset();
}

SASL::~SASL()
{
	delete d;
}

void SASL::reset()
{
	d->localSet  = false;
	d->remoteSet = false;

	d->ssfmin     = 0;
	d->ssfmax     = 0;
	d->ext_authid = QString();
	d->ext_ssf    = 0;

	d->out.clear();
	d->in.clear();
	d->to_net.clear();
	d->from_net.clear();
	d->bytesEncoded = 0;

	d->c->reset();
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

void SASL::setExternalSSF(int x)
{
	d->ext_ssf = x;
}

void SASL::setLocalAddr(const QString &addr, quint16 port)
{
	d->localSet   = true;
	d->local.addr = addr;
	d->local.port = port;
}

void SASL::setRemoteAddr(const QString &addr, quint16 port)
{
	d->remoteSet   = true;
	d->remote.addr = addr;
	d->remote.port = port;
}

void SASL::startClient(const QString &service, const QString &host, const QStringList &mechlist, ClientSendMode mode)
{
	d->setup(service, host);
	d->allowClientSendFirst = (mode == AllowClientSendFirst);
	d->c->startClient(mechlist, d->allowClientSendFirst);
	d->first  = true;
	d->server = false;
	d->tried  = false;
	QTimer::singleShot(0, d, SLOT(tryAgain()));
}

void SASL::startServer(const QString &service, const QString &host, const QString &realm, ServerSendMode mode)
{
	d->setup(service, host);

	QString appname;
	if(saslappname)
		appname = *saslappname;
	else
		appname = "qca";

	// TODO: use appname!!

	d->disableServerSendLast = (mode == DisableServerSendLast);
	d->c->startServer(realm, d->disableServerSendLast);
	d->first  = true;
	d->server = true;
	d->tried  = false;
	if(d->c->result() == SASLContext::Success)
		QMetaObject::invokeMethod(this, "serverStarted", Qt::QueuedConnection);
}

void SASL::putServerFirstStep(const QString &mech)
{
	d->c->serverFirstStep(mech, 0);
	d->handleServerFirstStep();
}

void SASL::putServerFirstStep(const QString &mech, const QByteArray &clientInit)
{
	d->c->serverFirstStep(mech, &clientInit);
	d->handleServerFirstStep();
}

void SASL::putStep(const QByteArray &stepData)
{
	d->stepData = stepData;
	d->tryAgain();
}

void SASL::setUsername(const QString &user)
{
	d->c->setClientParams(&user, 0, 0, 0);
}

void SASL::setAuthzid(const QString &authzid)
{
	d->c->setClientParams(0, &authzid, 0, 0);
}

void SASL::setPassword(const QSecureArray &pass)
{
	d->c->setClientParams(0, 0, &pass, 0);
}

void SASL::setRealm(const QString &realm)
{
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
	return d->c->mechlist().split(" ");
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
	d->bytesEncoded = 0;
	return a;
}

void SASL::Private::tryAgain()
{
	Private *d = this;
	SASL *q = sasl;

	if(d->server) {
		if(!d->tried) {
			d->c->nextStep(d->stepData);
			d->tried = true;
		}
		else {
			d->c->tryAgain();
		}

		if(d->c->result() == SASLContext::Error) {
			d->errorCode = ErrorHandshake;
			emit q->error();
			return;
		}
		else if(d->c->result() == SASLContext::Continue) {
			d->tried = false;
			emit q->nextStep(d->c->stepData());
			return;
		}
		else if(d->c->result() == SASLContext::AuthCheck) {
			emit q->authCheck(d->c->username(), d->c->authzid());
			return;
		}
	}
	else {
		if(d->first) {
			if(d->c->result() == SASLContext::Error) {
				d->errorCode = ErrorInit;
				emit q->error();
				return;
			}

			d->c->tryAgain();

			if(d->c->result() == SASLContext::Error) {
				d->errorCode = ErrorHandshake;
				emit q->error();
				return;
			}
			else if(d->c->result() == SASLContext::NeedParams) {
				//d->tried = false;
				Params np = d->c->clientParamsNeeded();
				emit q->needParams(np);
				return;
			}

			d->first = false;
			d->tried = false;
			emit q->clientStarted(d->c->haveClientInit(), d->c->stepData());
		}
		else {
			if(!d->tried) {
				d->c->nextStep(d->stepData);
				d->tried = true;
			}
			else
				d->c->tryAgain();

			if(d->c->result() == SASLContext::Error) {
				d->errorCode = ErrorHandshake;
				emit q->error();
				return;
			}
			else if(d->c->result() == SASLContext::NeedParams) {
				//d->tried = false;
				Params np = d->c->clientParamsNeeded();
				emit q->needParams(np);
				return;
			}
			// else if(d->c->result() == SASLContext::Continue) {
				d->tried = false;
				emit q->nextStep(d->c->stepData());
			// 	return;
			// }
		}
	}

	if(d->c->result() == SASLContext::Success)
		emit q->authenticated();
	else if(d->c->result() == SASLContext::Error) {
		d->errorCode = ErrorHandshake;
		emit q->error();
	}
}

}

#include "qca_securelayer.moc"
