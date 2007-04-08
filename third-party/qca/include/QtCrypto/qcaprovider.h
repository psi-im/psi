/*
 * qcaprovider.h - QCA Plugin API
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

// WARNING!
//
// Do not use the Plugin interface in applications!  This interface is
// for plugin developers ONLY.  It may change between QCA versions.

#ifndef QCAPROVIDER_H
#define QCAPROVIDER_H

#include <QtCore>
#include "qca_core.h"
#include "qca_basic.h"
#include "qca_publickey.h"
#include "qca_cert.h"
#include "qca_keystore.h"
#include "qca_securelayer.h"
#include "qca_securemessage.h"

#include <limits>

class QCA_EXPORT QCAPlugin : public QObject
{
	Q_OBJECT
public:
	virtual QCA::Provider *createProvider() = 0;
};

Q_DECLARE_INTERFACE(QCAPlugin, "com.affinix.qca.Plugin/1.0")

namespace QCA {

class QCA_EXPORT RandomContext : public Provider::Context
{
public:
	RandomContext(Provider *p) : Provider::Context(p, "random") {}
	virtual QSecureArray nextBytes(int size, Random::Quality q) = 0;
};

class QCA_EXPORT HashContext : public Provider::Context
{
public:
	HashContext(Provider *p, const QString &type) : Provider::Context(p, type) {}
	virtual void clear() = 0;
	virtual void update(const QSecureArray &a) = 0;
	virtual QSecureArray final() = 0;
};

class QCA_EXPORT CipherContext : public Provider::Context
{
public:
	//enum Mode { CBC, CFB, ECB };
	CipherContext(Provider *p, const QString &type) : Provider::Context(p, type) {}
	virtual void setup(Direction dir, const SymmetricKey &key, const InitializationVector &iv) = 0;
	virtual KeyLength keyLength() const = 0;
	virtual unsigned int blockSize() const = 0;

	virtual bool update(const QSecureArray &in, QSecureArray *out) = 0;
	virtual bool final(QSecureArray *out) = 0;
};

class QCA_EXPORT MACContext : public Provider::Context
{
public:
	MACContext(Provider *p, const QString &type) : Provider::Context(p, type) {}
	virtual void setup(const SymmetricKey &key) = 0;
	virtual KeyLength keyLength() const = 0;

	virtual void update(const QSecureArray &in) = 0;
	virtual void final(QSecureArray *out) = 0;

protected:
	KeyLength anyKeyLength() const
	{
		// this is used instead of a default implementation to make sure that
		// provider authors think about it, at least a bit.
		// See Meyers, Effective C++, Effective C++ (2nd Ed), Item 36
		return KeyLength( 0, INT_MAX, 1 );
	}
};

class QCA_EXPORT KDFContext : public Provider::Context
{
public:
	KDFContext(Provider *p, const QString &type) : Provider::Context(p, type) {}
	virtual SymmetricKey makeKey(const QSecureArray &secret, const InitializationVector &salt, unsigned int keyLength, unsigned int iterationCount) = 0;
};

class QCA_EXPORT DLGroupContext : public QObject, public Provider::Context
{
	Q_OBJECT
public:
	DLGroupContext(Provider *p) : Provider::Context(p, "dlgroup") {}
	virtual QList<DLGroupSet> supportedGroupSets() const = 0;
	virtual bool isNull() const = 0;
	virtual void fetchGroup(DLGroupSet set, bool block) = 0;
	virtual void getResult(QBigInteger *p, QBigInteger *q, QBigInteger *g) const = 0;

signals:
	void finished();
};

class QCA_EXPORT PKeyBase : public QObject, public Provider::Context
{
	Q_OBJECT
public:
	PKeyBase(Provider *p, const QString &type);
	virtual bool isNull() const = 0;
	virtual PKey::Type type() const = 0;
	virtual bool isPrivate() const = 0;
	virtual bool canExport() const = 0;
	virtual void convertToPublic() = 0;
	virtual int bits() const = 0;

	// encrypt/decrypt
	virtual int maximumEncryptSize(EncryptionAlgorithm alg) const;
	virtual QSecureArray encrypt(const QSecureArray &in, EncryptionAlgorithm alg);
	virtual bool decrypt(const QSecureArray &in, QSecureArray *out, EncryptionAlgorithm alg);

	// sign / verify
	virtual void startSign(SignatureAlgorithm alg, SignatureFormat format);
	virtual void startVerify(SignatureAlgorithm alg, SignatureFormat format);
	virtual void update(const QSecureArray &in);
	virtual QSecureArray endSign();
	virtual bool endVerify(const QSecureArray &sig);

	// key agreement
	virtual SymmetricKey deriveKey(const PKeyBase &theirs);

signals:
	void finished();
};

class QCA_EXPORT RSAContext : public PKeyBase
{
	Q_OBJECT
public:
	RSAContext(Provider *p) : PKeyBase(p, "rsa") {}
	virtual void createPrivate(int bits, int exp, bool block) = 0;
	virtual void createPrivate(const QBigInteger &n, const QBigInteger &e, const QBigInteger &p, const QBigInteger &q, const QBigInteger &d) = 0;
	virtual void createPublic(const QBigInteger &n, const QBigInteger &e) = 0;
	virtual QBigInteger n() const = 0;
	virtual QBigInteger e() const = 0;
	virtual QBigInteger p() const = 0;
	virtual QBigInteger q() const = 0;
	virtual QBigInteger d() const = 0;
};

class QCA_EXPORT DSAContext : public PKeyBase
{
	Q_OBJECT
public:
	DSAContext(Provider *p) : PKeyBase(p, "dsa") {}
	virtual void createPrivate(const DLGroup &domain, bool block) = 0;
	virtual void createPrivate(const DLGroup &domain, const QBigInteger &y, const QBigInteger &x) = 0;
	virtual void createPublic(const DLGroup &domain, const QBigInteger &y) = 0;
	virtual DLGroup domain() const = 0;
	virtual QBigInteger y() const = 0;
	virtual QBigInteger x() const = 0;
};

class QCA_EXPORT DHContext : public PKeyBase
{
	Q_OBJECT
public:
	DHContext(Provider *p) : PKeyBase(p, "dh") {}
	virtual void createPrivate(const DLGroup &domain, bool block) = 0;
	virtual void createPrivate(const DLGroup &domain, const QBigInteger &y, const QBigInteger &x) = 0;
	virtual void createPublic(const DLGroup &domain, const QBigInteger &y) = 0;
	virtual DLGroup domain() const = 0;
	virtual QBigInteger y() const = 0;
	virtual QBigInteger x() const = 0;
};

class QCA_EXPORT PKeyContext : public Provider::Context
{
public:
	PKeyContext(Provider *p) : Provider::Context(p, "pkey") {}

	virtual QList<PKey::Type> supportedTypes() const = 0;
	virtual QList<PKey::Type> supportedIOTypes() const = 0;
	virtual QList<PBEAlgorithm> supportedPBEAlgorithms() const = 0;

	virtual PKeyBase *key() = 0;
	virtual const PKeyBase *key() const = 0;
	virtual void setKey(PKeyBase *key) = 0;
	virtual bool importKey(const PKeyBase *key) = 0;

	// import / export
	virtual QSecureArray publicToDER() const = 0;
	virtual QString publicToPEM() const = 0;
	virtual ConvertResult publicFromDER(const QSecureArray &a) = 0;
	virtual ConvertResult publicFromPEM(const QString &s) = 0;
	virtual QSecureArray privateToDER(const QSecureArray &passphrase, PBEAlgorithm pbe) const = 0;
	virtual QString privateToPEM(const QSecureArray &passphrase, PBEAlgorithm pbe) const = 0;
	virtual ConvertResult privateFromDER(const QSecureArray &a, const QSecureArray &passphrase) = 0;
	virtual ConvertResult privateFromPEM(const QString &s, const QSecureArray &passphrase) = 0;
};

class QCA_EXPORT CertBase : public Provider::Context
{
public:
	CertBase(Provider *p, const QString &type) : Provider::Context(p, type) {}

	// import / export
	virtual QSecureArray toDER() const = 0;
	virtual QString toPEM() const = 0;
	virtual ConvertResult fromDER(const QSecureArray &a) = 0;
	virtual ConvertResult fromPEM(const QString &s) = 0;
};

class QCA_EXPORT CertContextProps
{
public:
	int version;                     // cert only
	QDateTime start, end;            // cert only
	CertificateInfo subject;
	CertificateInfo issuer;          // cert only
	Constraints constraints;
	QStringList policies;
	QBigInteger serial;              // cert only
	bool isCA;
	bool isSelfSigned;               // cert only
	int pathLimit;
	QSecureArray sig;
	SignatureAlgorithm sigalgo;
	QByteArray subjectId, issuerId;  // cert only
	QString challenge;               // csr only
	CertificateRequestFormat format; // csr only
};

class QCA_EXPORT CRLContextProps
{
public:
	CertificateInfo issuer;
	int number;
	QDateTime thisUpdate, nextUpdate;
	QList<CRLEntry> revoked;
	QSecureArray sig;
	SignatureAlgorithm sigalgo;
	QByteArray issuerId;
};

class CRLContext;

class QCA_EXPORT CertContext : public CertBase
{
public:
	CertContext(Provider *p) : CertBase(p, "cert") {}

	virtual bool createSelfSigned(const CertificateOptions &opts, const PKeyContext &priv) = 0;
	virtual const CertContextProps *props() const = 0;
	virtual PKeyContext *subjectPublicKey() const = 0; // caller must delete
	virtual bool isIssuerOf(const CertContext *other) const = 0;

	// ownership of items IS NOT passed
	virtual Validity validate(const QList<CertContext*> &trusted, const QList<CertContext*> &untrusted, const QList<CRLContext*> &crls, UsageMode u) const = 0;
	virtual Validity validate_chain(const QList<CertContext*> &chain, const QList<CertContext*> &trusted, const QList<CRLContext*> &crls, UsageMode u) const = 0;
};

class QCA_EXPORT CSRContext : public CertBase
{
public:
	CSRContext(Provider *p) : CertBase(p, "csr") {}

	virtual bool canUseFormat(CertificateRequestFormat f) const = 0;
	virtual bool createRequest(const CertificateOptions &opts, const PKeyContext &priv) = 0;
	virtual const CertContextProps *props() const = 0;
	virtual PKeyContext *subjectPublicKey() const = 0; // caller must delete
	virtual QString toSPKAC() const = 0;
	virtual ConvertResult fromSPKAC(const QString &s) = 0;
};

class QCA_EXPORT CRLContext : public CertBase
{
public:
	CRLContext(Provider *p) : CertBase(p, "crl") {}

	virtual const CRLContextProps *props() const = 0;
};

class QCA_EXPORT CertCollectionContext : public Provider::Context
{
public:
	CertCollectionContext(Provider *p) : Provider::Context(p, "certcollection") {}

	// ownership of items IS NOT passed
	virtual QByteArray toPKCS7(const QList<CertContext*> &certs, const QList<CRLContext*> &crls) const = 0;

	// ownership of items IS passed
	virtual ConvertResult fromPKCS7(const QByteArray &a, QList<CertContext*> *certs, QList<CRLContext*> *crls) const = 0;
};

class QCA_EXPORT CAContext : public Provider::Context
{
public:
	CAContext(Provider *p) : Provider::Context(p, "ca") {}

	virtual void setup(const CertContext &cert, const PKeyContext &priv) = 0;

	// caller must delete all return values here
	virtual CertContext *certificate() const = 0;
	virtual CertContext *signRequest(const CSRContext &req, const QDateTime &notValidAfter) const = 0;
	virtual CertContext *createCertificate(const PKeyContext &pub, const CertificateOptions &opts) const = 0;
	virtual CRLContext *createCRL(const QDateTime &nextUpdate) const = 0;
	virtual CRLContext *updateCRL(const CRLContext &crl, const QList<CRLEntry> &entries, const QDateTime &nextUpdate) const = 0;
};

class QCA_EXPORT PIXContext : public Provider::Context
{
public:
	PIXContext(Provider *p) : Provider::Context(p, "pix") {}

	virtual QByteArray toPKCS12(const QString &name, const QList<const CertContext*> &chain, const PKeyContext &priv, const QSecureArray &passphrase) const = 0;

	// caller must delete
	virtual ConvertResult fromPKCS12(const QByteArray &in, const QSecureArray &passphrase, QString *name, QList<CertContext*> *chain, PKeyContext **priv) const = 0;
};

class QCA_EXPORT PGPKeyContextProps
{
public:
	QString keyId;
	QStringList userIds;
	bool isSecret;
	QDateTime creationDate, expirationDate;
	QString fingerprint;
	bool inKeyring;
	bool isTrusted;
};

class QCA_EXPORT PGPKeyContext : public Provider::Context
{
public:
	PGPKeyContext(Provider *p) : Provider::Context(p, "pgpkey") {}

	virtual const PGPKeyContextProps *props() const = 0;

	virtual QSecureArray toBinary() const = 0;
	virtual QString toAscii() const = 0;
	virtual ConvertResult fromBinary(const QSecureArray &a) = 0;
	virtual ConvertResult fromAscii(const QString &s) = 0;
};

class QCA_EXPORT KeyStoreEntryContext : public Provider::Context
{
public:
	KeyStoreEntryContext(Provider *p) : Provider::Context(p, "keystoreentry") {}

	virtual KeyStoreEntry::Type type() const = 0;
	virtual QString id() const = 0;
	virtual QString name() const = 0;
	virtual QString storeId() const = 0;
	virtual QString storeName() const = 0;

	virtual KeyBundle keyBundle() const;
	virtual Certificate certificate() const;
	virtual CRL crl() const;
	virtual PGPKey pgpSecretKey() const;
	virtual PGPKey pgpPublicKey() const;

	virtual bool ensureAccess();
};

class QCA_EXPORT KeyStoreListContext : public QObject, public Provider::Context
{
	Q_OBJECT
public:
	KeyStoreListContext(Provider *p) : Provider::Context(p, "keystorelist") {}

	virtual void start();

	// enable/disable update events
	virtual void setUpdatesEnabled(bool enabled);

	// returns a list of integer context ids (for keystores)
	virtual QList<int> keyStores() = 0;

	// null/empty return values mean the context id is gone

	virtual KeyStore::Type type(int id) const = 0;
	virtual QString storeId(int id) const = 0;
	virtual QString name(int id) const = 0;
	virtual bool isReadOnly(int id) const;

	virtual QList<KeyStoreEntry::Type> entryTypes(int id) const = 0;

	// caller must delete any returned KeyStoreEntryContexts

	virtual QList<KeyStoreEntryContext*> entryList(int id) = 0;

	// return 0 if no such entry
	virtual KeyStoreEntryContext *entry(int id, const QString &entryId);

	// return 0 if the provider doesn't handle this type of storeId
	virtual KeyStoreEntryContext *entryPassive(const QString &storeId, const QString &entryId);

	virtual bool writeEntry(int id, const KeyBundle &kb);
	virtual bool writeEntry(int id, const Certificate &cert);
	virtual bool writeEntry(int id, const CRL &crl);
	virtual PGPKey writeEntry(int id, const PGPKey &key);
	virtual bool removeEntry(int id, const QString &entryId);

	virtual void submitPassphrase(int id, int requestId, const QSecureArray &passphrase);
	virtual void rejectPassphraseRequest(int id, int requestId);

signals:
	// note: busyStart is assumed after calling start(), no need to emit
	void busyStart();
	void busyEnd();

	void updated();
	void diagnosticText(const QString &str);
	void storeUpdated(int id);
};

class QCA_EXPORT TLSContext : public QObject, public Provider::Context
{
	Q_OBJECT
public:
	class SessionInfo
	{
	public:
		bool isCompressed;
		TLS::Version version;
		QString cipherSuite;
		int cipherBits, cipherMaxBits;
	};

	enum Result
	{
		Success,
		Error,
		Continue
	};

	TLSContext(Provider *p, const QString &type) : Provider::Context(p, type) {}

	virtual void reset() = 0;

	virtual QStringList supportedCipherSuites(const TLS::Version &version) const = 0;
	virtual bool canCompress() const = 0;
	virtual int maxSSF() const = 0;

	virtual void setConstraints(int minSSF, int maxSSF) = 0;
	virtual void setConstraints(const QStringList &cipherSuiteList) = 0;
	virtual void setup(const CertificateCollection &trusted, const CertificateChain &cert, const PrivateKey &key, bool server, bool compress, bool dtls) = 0;

	virtual void shutdown() = 0; // flag for shutdown, call update next
	virtual void setMTU(int size); // for dtls

	// start() results:
	//   result (Success or Error)
	virtual void start() = 0;

	// update() results:
	//   during handshake:
	//     result
	//     to_net
	//   during shutdown:
	//     result
	//     to_net
	//   else
	//     result (Success or Error)
	//     to_net
	//     encoded
	//     to_app
	//     eof
	// note: for dtls, this function only operates with single
	//       packets.  perform the operation repeatedly to send/recv
	//       multiple packets.
	virtual void update(const QByteArray &from_net, const QByteArray &from_app) = 0;

	virtual void waitForResultsReady(int msecs) = 0; // -1 means wait forever

	// results
	virtual Result result() const = 0;
	virtual QByteArray to_net() = 0;
	virtual int encoded() const = 0;
	virtual QByteArray to_app() = 0;
	virtual bool eof() const = 0;

	// call after successful handshake
	virtual Validity peerCertificateValidity() const = 0;
	virtual CertificateChain peerCertificateChain() const = 0;
	virtual SessionInfo sessionInfo() const = 0;

	// call after shutdown
	virtual QByteArray unprocessed() = 0;

signals:
	void resultsReady();
	void dtlsTimeout(); // call update, even with empty args
};

class QCA_EXPORT SASLContext : public QObject, public Provider::Context
{
	Q_OBJECT
public:
	class HostPort
	{
	public:
		QString addr;
		quint16 port;
	};

	enum Result
	{
		Success,
		Error,
		NeedParams,
		AuthCheck,
		Continue
	};

	SASLContext(Provider *p) : Provider::Context(p, "sasl") {}

	virtual void reset() = 0;

	virtual void setConstraints(SASL::AuthFlags f, int minSSF, int maxSSF) = 0;
	virtual void setup(const QString &service, const QString &host, const HostPort *local, const HostPort *remote, const QString &ext_id, int ext_ssf) = 0;

	// startClient() results:
	//   result
	//   mech
	//   haveClientInit
	//   stepData
	virtual void startClient(const QStringList &mechlist, bool allowClientSendFirst) = 0;

	// startServer() results:
	//   result (Success or Error)
	//   mechlist
	virtual void startServer(const QString &realm, bool disableServerSendLast) = 0;

	// serverFirstStep() results:
	//   result
	//   stepData
	virtual void serverFirstStep(const QString &mech, const QByteArray *clientInit) = 0;

	// nextStep() results:
	//   result
	//   stepData
	virtual void nextStep(const QByteArray &from_net) = 0;

	// tryAgain() results:
	//   result
	//   stepData
	virtual void tryAgain() = 0;

	// update() results:
	//   result (Success or Error)
	//   to_net
	//   encoded
	//   to_app
	virtual void update(const QByteArray &from_net, const QByteArray &from_app) = 0;

	virtual void waitForResultsReady(int msecs) = 0; // -1 means wait forever

	// results
	virtual Result result() const = 0;
	virtual QString mechlist() const = 0;
	virtual QString mech() const = 0;
	virtual bool haveClientInit() const = 0;
	virtual QByteArray stepData() const = 0;
	virtual QByteArray to_net() = 0;
	virtual int encoded() const = 0;
	virtual QByteArray to_app() = 0;

	// call after auth success
	virtual int ssf() const = 0;

	// call after auth fail
	virtual SASL::AuthCondition authCondition() const = 0;

	// call after NeedParams
	virtual SASL::Params clientParamsNeeded() const = 0;
	virtual void setClientParams(const QString *user, const QString *authzid, const QSecureArray *pass, const QString *realm) = 0;

	// call after AuthCheck
	virtual QString username() const = 0;
	virtual QString authzid() const = 0;

signals:
	void resultsReady();
};

class QCA_EXPORT MessageContext : public QObject, public Provider::Context
{
	Q_OBJECT
public:
	enum Operation
	{
		Encrypt,
		Decrypt,
		Sign,
		Verify,
		SignAndEncrypt
	};

	MessageContext(Provider *p, const QString &type) : Provider::Context(p, type) {}

	virtual bool canSignMultiple() const = 0;

	virtual SecureMessage::Type type() const = 0;

	virtual void reset() = 0;
	virtual void setupEncrypt(const SecureMessageKeyList &keys) = 0;
	virtual void setupSign(const SecureMessageKeyList &keys, SecureMessage::SignMode m, bool bundleSigner, bool smime) = 0;
	virtual void setupVerify(const QByteArray &detachedSig) = 0;

	virtual void start(SecureMessage::Format f, Operation op) = 0;
	virtual void update(const QByteArray &in) = 0;
	virtual QByteArray read() = 0;
	virtual void end() = 0;

	virtual bool finished() const = 0;
	virtual void waitForFinished(int msecs) = 0; // -1 means wait forever

	virtual bool success() const = 0;
	virtual SecureMessage::Error errorCode() const = 0;
	virtual QByteArray signature() const = 0;
	virtual QString hashName() const = 0;
	virtual SecureMessageSignatureList signers() const = 0;

signals:
	void updated();
};

class QCA_EXPORT SMSContext : public Provider::Context
{
public:
	SMSContext(Provider *p, const QString &type) : Provider::Context(p, type) {}

	virtual void setTrustedCertificates(const CertificateCollection &trusted);
	virtual void setPrivateKeys(const QList<SecureMessageKey> &keys);
	virtual MessageContext *createMessage() = 0;
};

}

#endif
