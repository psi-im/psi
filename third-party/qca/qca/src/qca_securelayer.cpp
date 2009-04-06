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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 */

#include "qca_securelayer.h"

#include "qcaprovider.h"
#include "qca_safeobj.h"

#include <QPointer>

namespace QCA {

Provider::Context *getContext(const QString &type, const QString &provider);

enum ResetMode
{
	ResetSession        = 0,
	ResetSessionAndData = 1,
	ResetAll            = 2
};

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
class TLS::Private : public QObject
{
	Q_OBJECT
public:
	enum
	{
		OpStart,
		OpUpdate
	};

	enum State
	{
		Inactive,
		Initializing,
		Handshaking,
		Connected,
		Closing
	};

	class Action
	{
	public:
		enum Type
		{
			ReadyRead,
			ReadyReadOutgoing,
			Handshaken,
			Close,
			CheckPeerCertificate,
			CertificateRequested,
			HostNameReceived
		};

		int type;

		Action(int _type) : type(_type)
		{
		}
	};

	TLS *q;
	TLSContext *c;
	TLS::Mode mode;

	// signal connected flags
	bool connect_hostNameReceived;
	bool connect_certificateRequested;
	bool connect_peerCertificateAvailable;
	bool connect_handshaken;

	// persistent settings (survives ResetSessionAndData)
	CertificateChain localCert;
	PrivateKey localKey;
	CertificateCollection trusted;
	bool con_ssfMode;
	int con_minSSF, con_maxSSF;
	QStringList con_cipherSuites;
	bool tryCompress;
	int packet_mtu;
	QList<CertificateInfoOrdered> issuerList;
	TLSSession session;

	// session
	State state;
	bool blocked;
	bool server;
	QString host;
	TLSContext::SessionInfo sessionInfo;
	SafeTimer actionTrigger;
	int op;
	QList<Action> actionQueue;
	bool need_update;
	bool maybe_input;
	bool emitted_hostNameReceived;
	bool emitted_certificateRequested;
	bool emitted_peerCertificateAvailable;

	// data (survives ResetSession)
	CertificateChain peerCert;
	Validity peerValidity;
	bool hostMismatch;
	Error errorCode;

	// stream i/o
	QByteArray in, out;
	QByteArray to_net, from_net;
	QByteArray unprocessed;
	int out_pending;
	int to_net_encoded;
	LayerTracker layer;

	// datagram i/o
	QList<QByteArray> packet_in, packet_out;
	QList<QByteArray> packet_to_net, packet_from_net;
	int packet_out_pending; // packet count
	QList<int> packet_to_net_encoded;

	Private(TLS *_q, TLS::Mode _mode) : QObject(_q), q(_q), mode(_mode), actionTrigger(this)
	{
		// c is 0 during initial reset, so we don't redundantly reset it
		c = 0;
		connect_hostNameReceived = false;
		connect_certificateRequested = false;
		connect_peerCertificateAvailable = false;
		connect_handshaken = false;
		server = false;

		connect(&actionTrigger, SIGNAL(timeout()), SLOT(doNextAction()));
		actionTrigger.setSingleShot(true);

		reset(ResetAll);

		c = static_cast<TLSContext *>(q->context());

		// parent the context to us, so that moveToThread works
		c->setParent(this);

		connect(c, SIGNAL(resultsReady()), SLOT(tls_resultsReady()));
		connect(c, SIGNAL(dtlsTimeout()), SLOT(tls_dtlsTimeout()));
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

		// if we reset while in client mode, then clear this list
		//   (it should only persist when used for server mode)
		if(!server)
			issuerList.clear();

		state = Inactive;
		blocked = false;
		server = false;
		host = QString();
		sessionInfo = TLSContext::SessionInfo();
		actionTrigger.stop();
		op = -1;
		actionQueue.clear();
		need_update = false;
		maybe_input = false;
		emitted_hostNameReceived = false;
		emitted_certificateRequested = false;
		emitted_peerCertificateAvailable = false;

		out.clear();
		out_pending = 0;
		packet_out.clear();
		packet_out_pending = 0;

		if(mode >= ResetSessionAndData)
		{
			peerCert = CertificateChain();
			peerValidity = ErrorValidityUnknown;
			hostMismatch = false;
			errorCode = (TLS::Error)-1;

			in.clear();
			to_net.clear();
			from_net.clear();
			unprocessed.clear();
			to_net_encoded = 0;
			layer.reset();

			packet_in.clear();
			packet_to_net.clear();
			packet_from_net.clear();
			packet_to_net_encoded.clear();
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
		state = Initializing;
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

		QCA_logTextMessage(QString("tls[%1]: c->start()").arg(q->objectName()), Logger::Information);
		op = OpStart;
		c->start();
	}

	void close()
	{
		QCA_logTextMessage(QString("tls[%1]: close").arg(q->objectName()), Logger::Information);

		if(state != Connected)
			return;

		state = Closing;
		c->shutdown();
	}

	void continueAfterStep()
	{
		QCA_logTextMessage(QString("tls[%1]: continueAfterStep").arg(q->objectName()), Logger::Information);

		if(!blocked)
			return;

		blocked = false;
		update();
	}

	void processNextAction()
	{
		if(actionQueue.isEmpty())
		{
			if(need_update)
			{
				QCA_logTextMessage(QString("tls[%1]: need_update").arg(q->objectName()), Logger::Information);
				update();
			}
			return;
		}

		Action a = actionQueue.takeFirst();

		// set up for the next one, if necessary
		if(!actionQueue.isEmpty() || need_update)
		{
			if(!actionTrigger.isActive())
				actionTrigger.start();
		}

		if(a.type == Action::ReadyRead)
		{
			emit q->readyRead();
		}
		else if(a.type == Action::ReadyReadOutgoing)
		{
			emit q->readyReadOutgoing();
		}
		else if(a.type == Action::Handshaken)
		{
			state = Connected;

			// write any app data waiting during handshake
			if(!out.isEmpty())
			{
				need_update = true;
				if(!actionTrigger.isActive())
					actionTrigger.start();
			}

			QCA_logTextMessage(QString("tls[%1]: handshaken").arg(q->objectName()), Logger::Information);

			if(connect_handshaken)
			{
				blocked = true;
				emit q->handshaken();
			}
		}
		else if(a.type == Action::Close)
		{
			unprocessed = c->unprocessed();
			reset(ResetSession);
			emit q->closed();
		}
		else if(a.type == Action::CheckPeerCertificate)
		{
			peerCert = c->peerCertificateChain();
			if(!peerCert.isEmpty())
			{
				peerValidity = c->peerCertificateValidity();
				if(peerValidity == ValidityGood && !host.isEmpty() && !peerCert.primary().matchesHostName(host))
					hostMismatch = true;
			}

			if(connect_peerCertificateAvailable)
			{
				blocked = true;
				emitted_peerCertificateAvailable = true;
				emit q->peerCertificateAvailable();
			}
		}
		else if(a.type == Action::CertificateRequested)
		{
			issuerList = c->issuerList();
			if(connect_certificateRequested)
			{
				blocked = true;
				emitted_certificateRequested = true;
				emit q->certificateRequested();
			}
		}
		else if(a.type == Action::HostNameReceived)
		{
			if(connect_hostNameReceived)
			{
				blocked = true;
				emitted_hostNameReceived = true;
				emit q->hostNameReceived();
			}
		}
	}

	void update()
	{
		QCA_logTextMessage(QString("tls[%1]: update").arg(q->objectName()), Logger::Information);

		if(blocked)
		{
			QCA_logTextMessage(QString("tls[%1]: ignoring update while blocked").arg(q->objectName()), Logger::Information);
			return;
		}

		if(!actionQueue.isEmpty())
		{
			QCA_logTextMessage(QString("tls[%1]: ignoring update while processing actions").arg(q->objectName()), Logger::Information);
			need_update = true;
			return;
		}

		// only allow one operation at a time
		if(op != -1)
		{
			QCA_logTextMessage(QString("tls[%1]: ignoring update while operation active").arg(q->objectName()), Logger::Information);
			need_update = true;
			return;
		}

		need_update = false;

		QByteArray arg_from_net, arg_from_app;

		if(state == Handshaking)
		{
			// during handshake, only send from_net (no app data)

			if(mode == TLS::Stream)
			{
				arg_from_net = from_net;
				from_net.clear();
			}
			else
			{
				// note: there may not be a packet
				if(!packet_from_net.isEmpty())
					arg_from_net = packet_from_net.takeFirst();
			}
		}
		else
		{
			if(mode == TLS::Stream)
			{
				if(!from_net.isEmpty())
				{
					arg_from_net = from_net;
					from_net.clear();
				}

				if(!out.isEmpty())
				{
					out_pending += out.size();
					arg_from_app = out;
					out.clear();
				}
			}
			else
			{
				if(!packet_from_net.isEmpty())
					arg_from_net = packet_from_net.takeFirst();

				if(!packet_out.isEmpty())
				{
					arg_from_app = packet_out.takeFirst();
					++packet_out_pending;
				}
			}
		}

		if(arg_from_net.isEmpty() && arg_from_app.isEmpty() && !maybe_input)
		{
			QCA_logTextMessage(QString("tls[%1]: ignoring update: no output and no expected input").arg(q->objectName()), Logger::Information);
			return;
		}

		// clear this flag
		maybe_input = false;

		QCA_logTextMessage(QString("tls[%1]: c->update").arg(q->objectName()), Logger::Information);
		op = OpUpdate;
		c->update(arg_from_net, arg_from_app);
	}

	void start_finished()
	{
		bool ok = c->result() == TLSContext::Success;
		if(!ok)
		{
			reset(ResetSession);
			errorCode = TLS::ErrorInit;
			emit q->error();
			return;
		}

		state = Handshaking;

		// immediately update so we can get the first packet to send
		maybe_input = true;
		update();
	}

	void update_finished()
	{
		TLSContext::Result r = c->result();
		if(r == TLSContext::Error)
		{
			if(state == Handshaking || state == Closing)
			{
				reset(ResetSession);
				errorCode = ErrorHandshake;
			}
			else
			{
				reset(ResetSession);
				errorCode = ErrorCrypt;
			}

			emit q->error();
			return;
		}

		QByteArray c_to_net = c->to_net();
		if(!c_to_net.isEmpty())
		{
			QCA_logTextMessage(QString("tls[%1]: to_net %2").arg(q->objectName(), QString::number(c_to_net.size())), Logger::Information);
		}

		if(state == Closing)
		{
			if(mode == TLS::Stream)
				to_net += c_to_net;
			else
				packet_to_net += c_to_net;

			if(!c_to_net.isEmpty())
				actionQueue += Action(Action::ReadyReadOutgoing);

			if(r == TLSContext::Success)
				actionQueue += Action(Action::Close);

			processNextAction();
			return;
		}
		else if(state == Handshaking)
		{
			if(mode == TLS::Stream)
				to_net += c_to_net;
			else
				packet_to_net += c_to_net;

			if(!c_to_net.isEmpty())
				actionQueue += Action(Action::ReadyReadOutgoing);

			bool clientHello = false;
			bool serverHello = false;
			if(server)
				clientHello = c->clientHelloReceived();
			else
				serverHello = c->serverHelloReceived();

			// client specifies a host?
			if(!emitted_hostNameReceived && clientHello)
			{
				host = c->hostName();
				if(!host.isEmpty())
					actionQueue += Action(Action::HostNameReceived);
			}

			// successful handshake or server hello means there might be a peer cert
			if(!emitted_peerCertificateAvailable && (r == TLSContext::Success || (!server && serverHello)))
				actionQueue += Action(Action::CheckPeerCertificate);

			// server requests a cert from us?
			if(!emitted_certificateRequested && (serverHello && c->certificateRequested()))
				actionQueue += Action(Action::CertificateRequested);

			if(r == TLSContext::Success)
			{
				sessionInfo = c->sessionInfo();
				if(sessionInfo.id)
				{
					TLSSessionContext *sc = static_cast<TLSSessionContext*>(sessionInfo.id->clone());
					session.change(sc);
				}

				actionQueue += Action(Action::Handshaken);
			}

			processNextAction();
			return;
		}
		else // Connected
		{
			QByteArray c_to_app = c->to_app();
			if(!c_to_app.isEmpty())
			{
				QCA_logTextMessage(QString("tls[%1]: to_app %2").arg(q->objectName(), QString::number(c_to_app.size())), Logger::Information);
			}

			bool eof = c->eof();
			int enc = -1;
			if(!c_to_net.isEmpty())
				enc = c->encoded();

			bool io_pending = false;
			if(mode == TLS::Stream)
			{
				if(!c_to_net.isEmpty())
					out_pending -= enc;

				if(out_pending > 0)
				{
					maybe_input = true;
					io_pending = true;
				}

				if(!out.isEmpty())
					io_pending = true;
			}
			else
			{
				if(!c_to_net.isEmpty())
					--packet_out_pending;

				if(packet_out_pending > 0)
				{
					maybe_input = true;
					io_pending = true;
				}

				if(!packet_out.isEmpty())
					io_pending = true;
			}

			if(mode == TLS::Stream)
			{
				to_net += c_to_net;
				in += c_to_app;
				to_net_encoded += enc;
			}
			else
			{
				packet_to_net += c_to_net;
				packet_in += c_to_app;
			}

			if(!c_to_net.isEmpty())
				actionQueue += Action(Action::ReadyReadOutgoing);

			if(!c_to_app.isEmpty())
				actionQueue += Action(Action::ReadyRead);

			if(eof)
			{
				close();
				maybe_input = true;
			}

			if(eof || io_pending)
			{
				QCA_logTextMessage(QString("tls[%1]: eof || io_pending").arg(q->objectName()), Logger::Information);
				update();
			}

			processNextAction();
			return;
		}
	}

private slots:
	void tls_resultsReady()
	{
		QCA_logTextMessage(QString("tls[%1]: c->resultsReady()").arg(q->objectName()), Logger::Information);

		Q_ASSERT(op != -1);

		int last_op = op;
		op = -1;

		if(last_op == OpStart)
			start_finished();
		else // OpUpdate
			update_finished();
	}

	void tls_dtlsTimeout()
	{
		QCA_logTextMessage(QString("tls[%1]: c->dtlsTimeout()").arg(q->objectName()), Logger::Information);

		maybe_input = true;
		update();
	}

	void doNextAction()
	{
		processNextAction();
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
	if(d->state != TLS::Private::Inactive)
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
	if(d->state != TLS::Private::Inactive)
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

	if(d->state != TLS::Private::Inactive)
		d->c->setConstraints(d->con_minSSF, d->con_maxSSF);
}

void TLS::setConstraints(int minSSF, int maxSSF)
{
	d->con_ssfMode = true;
	d->con_minSSF = minSSF;
	d->con_maxSSF = maxSSF;

	if(d->state != TLS::Private::Inactive)
		d->c->setConstraints(d->con_minSSF, d->con_maxSSF);
}

void TLS::setConstraints(const QStringList &cipherSuiteList)
{
	d->con_ssfMode = false;
	d->con_cipherSuites = cipherSuiteList;

	if(d->state != TLS::Private::Inactive)
		d->c->setConstraints(d->con_cipherSuites);
}

QList<CertificateInfoOrdered> TLS::issuerList() const
{
	return d->issuerList;
}

void TLS::setIssuerList(const QList<CertificateInfoOrdered> &issuers)
{
	d->issuerList = issuers;
	if(d->state != TLS::Private::Inactive)
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
	if(d->state == TLS::Private::Connected || d->state == TLS::Private::Closing)
		return true;
	else
		return false;
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
	QCA_logTextMessage(QString("tls[%1]: write").arg(objectName()), Logger::Information);
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
	{
		if(!d->packet_in.isEmpty())
			return d->packet_in.takeFirst();
		else
			return QByteArray();
	}
}

void TLS::writeIncoming(const QByteArray &a)
{
	if(d->mode == Stream)
		d->from_net.append(a);
	else
		d->packet_from_net.append(a);
	QCA_logTextMessage(QString("tls[%1]: writeIncoming %2").arg(objectName(), QString::number(a.size())), Logger::Information);
	d->update();
}

QByteArray TLS::readOutgoing(int *plainBytes)
{
	if(d->mode == Stream)
	{
		QByteArray a = d->to_net;
		d->to_net.clear();
		if(plainBytes)
			*plainBytes = d->to_net_encoded;
		d->layer.specifyEncoded(a.size(), d->to_net_encoded);
		d->to_net_encoded = 0;
		return a;
	}
	else
	{
		if(!d->packet_to_net.isEmpty())
		{
			QByteArray a = d->packet_to_net.takeFirst();
			int x = d->packet_to_net_encoded.takeFirst();
			if(plainBytes)
				*plainBytes = x;
			return a;
		}
		else
		{
			if(plainBytes)
				*plainBytes = 0;
			return QByteArray();
		}
	}
}

QByteArray TLS::readUnprocessed()
{
	if(d->mode == Stream)
	{
		QByteArray a = d->unprocessed;
		d->unprocessed.clear();
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
	if(d->state != TLS::Private::Inactive)
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
	enum
	{
		OpStart,
		OpServerFirstStep,
		OpNextStep,
		OpTryAgain,
		OpUpdate
	};

	class Action
	{
	public:
		enum Type
		{
			ClientStarted,
			NextStep,
			Authenticated,
			ReadyRead,
			ReadyReadOutgoing
		};

		int type;
		QByteArray stepData;
		bool haveInit;

		Action(int _type) : type(_type)
		{
		}

		Action(int _type, const QByteArray &_stepData) : type(_type), stepData(_stepData)
		{
		}

		Action(int _type, bool _haveInit, const QByteArray &_stepData) : type(_type), stepData(_stepData), haveInit(_haveInit)
		{
		}
	};

	SASL *q;
	SASLContext *c;

	// persistent settings (survives ResetSessionAndData)
	AuthFlags auth_flags;
	int ssfmin, ssfmax;
	QString ext_authid;
	int ext_ssf;
	bool localSet, remoteSet;
	SASLContext::HostPort local, remote;
	bool set_username, set_authzid, set_password, set_realm;
	QString username, authzid, realm;
	SecureArray password;

	// session
	bool server;
	QStringList mechlist;
	QString server_realm;
	bool allowClientSendFirst;
	bool disableServerSendLast;
	SafeTimer actionTrigger;
	int op;
	QList<Action> actionQueue;
	bool need_update;
	bool first;
	bool authed;

	// data (survives ResetSession)
	QString mech; // selected mech
	Error errorCode;

	// stream i/o
	QByteArray in, out;
	QByteArray to_net, from_net;
	int out_pending;
	int to_net_encoded;
	LayerTracker layer;

	Private(SASL *_q) : QObject(_q), q(_q), actionTrigger(this)
	{
		c = 0;
		set_username = false;
		set_authzid = false;
		set_password = false;
		set_realm = false;

		connect(&actionTrigger, SIGNAL(timeout()), SLOT(doNextAction()));
		actionTrigger.setSingleShot(true);

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
			c->reset();

		server = false;
		mechlist.clear();
		server_realm = QString();
		allowClientSendFirst = false;
		disableServerSendLast = true;
		actionTrigger.stop();
		op = -1;
		actionQueue.clear();
		need_update = false;
		first = false;
		authed = false;

		out.clear();
		out_pending = 0;

		if(mode >= ResetSessionAndData)
		{
			mech = QString();
			errorCode = (SASL::Error)-1;

			in.clear();
			to_net.clear();
			from_net.clear();
			to_net_encoded = 0;
			layer.reset();
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

	void start()
	{
		op = OpStart;
		first = true;

		if(server)
		{
			QCA_logTextMessage(QString("sasl[%1]: c->startServer()").arg(q->objectName()), Logger::Information);
			c->startServer(server_realm, disableServerSendLast);
		}
		else
		{
			QCA_logTextMessage(QString("sasl[%1]: c->startClient()").arg(q->objectName()), Logger::Information);
			c->startClient(mechlist, allowClientSendFirst);
		}
	}

	void putServerFirstStep(const QString &mech, const QByteArray *clientInit)
	{
		if(op != -1)
			return;

		QCA_logTextMessage(QString("sasl[%1]: c->serverFirstStep()").arg(q->objectName()), Logger::Information);
		op = OpServerFirstStep;
		c->serverFirstStep(mech, clientInit);
	}

	void putStep(const QByteArray &stepData)
	{
		if(op != -1)
			return;

		QCA_logTextMessage(QString("sasl[%1]: c->nextStep()").arg(q->objectName()), Logger::Information);
		op = OpNextStep;
		c->nextStep(stepData);
	}

	void tryAgain()
	{
		if(op != -1)
			return;

		QCA_logTextMessage(QString("sasl[%1]: c->tryAgain()").arg(q->objectName()), Logger::Information);
		op = OpTryAgain;
		c->tryAgain();
	}

	void processNextAction()
	{
		if(actionQueue.isEmpty())
		{
			if(need_update)
				update();
			return;
		}

		Action a = actionQueue.takeFirst();

		// set up for the next one, if necessary
		if(!actionQueue.isEmpty() || need_update)
		{
			if(!actionTrigger.isActive())
				actionTrigger.start();
		}

		if(a.type == Action::ClientStarted)
		{
			emit q->clientStarted(a.haveInit, a.stepData);
		}
		else if(a.type == Action::NextStep)
		{
			emit q->nextStep(a.stepData);
		}
		else if(a.type == Action::Authenticated)
		{
			authed = true;

			// write any app data waiting during authentication
			if(!out.isEmpty())
			{
				need_update = true;
				if(!actionTrigger.isActive())
					actionTrigger.start();
			}

			QCA_logTextMessage(QString("sasl[%1]: authenticated").arg(q->objectName()), Logger::Information);
			emit q->authenticated();
		}
		else if(a.type == Action::ReadyRead)
		{
			emit q->readyRead();
		}
		else if(a.type == Action::ReadyReadOutgoing)
		{
			emit q->readyReadOutgoing();
		}
	}

	void update()
	{
		// defer writes while authenticating
		if(!authed)
		{
			QCA_logTextMessage(QString("sasl[%1]: ignoring update while not yet authenticated").arg(q->objectName()), Logger::Information);
			return;
		}

		if(!actionQueue.isEmpty())
		{
			QCA_logTextMessage(QString("sasl[%1]: ignoring update while processing actions").arg(q->objectName()), Logger::Information);
			need_update = true;
			return;
		}

		// only allow one operation at a time
		if(op != -1)
		{
			QCA_logTextMessage(QString("sasl[%1]: ignoring update while operation active").arg(q->objectName()), Logger::Information);
			need_update = true;
			return;
		}

		need_update = false;

		QCA_logTextMessage(QString("sasl[%1]: c->update()").arg(q->objectName()), Logger::Information);
		op = OpUpdate;
		out_pending += out.size();
		c->update(from_net, out);
		from_net.clear();
		out.clear();
	}

private slots:
	void sasl_resultsReady()
	{
		QCA_logTextMessage(QString("sasl[%1]: c->resultsReady()").arg(q->objectName()), Logger::Information);

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
				mech = c->mech();

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
					if(!disableServerSendLast)
						actionQueue += Action(Action::NextStep, c->stepData());

					actionQueue += Action(Action::Authenticated);

					processNextAction();
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
					actionQueue += Action(Action::ClientStarted, c->haveClientInit(), c->stepData());
					if(r == SASLContext::Success)
						actionQueue += Action(Action::Authenticated);

					processNextAction();
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
						actionQueue += Action(Action::NextStep, c->stepData());
						actionQueue += Action(Action::Authenticated);

						processNextAction();
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

			QByteArray c_to_net = c->to_net();
			QByteArray c_to_app = c->to_app();
			int enc = -1;
			if(!c_to_net.isEmpty())
				enc = c->encoded();

			bool io_pending = false;
			if(!c_to_net.isEmpty())
				out_pending -= enc;

			if(out_pending > 0)
				io_pending = true;

			if(!out.isEmpty())
				io_pending = true;

			to_net += c_to_net;
			in += c_to_app;
			to_net_encoded += enc;

			if(!c_to_net.isEmpty())
				actionQueue += Action(Action::ReadyReadOutgoing);

			if(!c_to_app.isEmpty())
				actionQueue += Action(Action::ReadyRead);

			if(io_pending)
				update();

			processNextAction();
			return;
		}
	}

	void doNextAction()
	{
		processNextAction();
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
	if(s == SL_Integrity)
		min = 1;
	else if(s == SL_Export)
		min = 56;
	else if(s == SL_Baseline)
		min = 128;
	else if(s == SL_High)
		min = 192;
	else if(s == SL_Highest)
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
	return d->mech;
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
		*plainBytes = d->to_net_encoded;
	d->layer.specifyEncoded(a.size(), d->to_net_encoded);
	d->to_net_encoded = 0;
	return a;
}

int SASL::convertBytesWritten(qint64 bytes)
{
	return d->layer.finished(bytes);
}

}

#include "qca_securelayer.moc"
