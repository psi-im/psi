/*
 * Copyright (C) 2003-2005  Justin Karneges <justin@affinix.com>
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

#ifndef GPGOP_H
#define GPGOP_H

#include <QtCrypto>
#include "qpipe.h"

namespace gpgQCAPlugin {

class GpgOp : public QObject
{
	Q_OBJECT
public:
	enum Type
	{
		Check,             // --version
		SecretKeyringFile, // --list-secret-keys
		PublicKeyringFile, // --list-public-keys
		SecretKeys,        // --fixed-list-mode --with-colons --list-secret-keys
		PublicKeys,        // --fixed-list-mode --with-colons --list-public-keys
		Encrypt,           // --encrypt
		Decrypt,           // --decrypt
		Sign,              // --sign
		SignAndEncrypt,    // --sign --encrypt
		SignClearsign,     // --clearsign
		SignDetached,      // --detach-sign
		Verify,            // --verify
		VerifyDetached,    // --verify
		Import,            // --import
		Export,            // --export
		DeleteKey          // --delete-key
	};

	enum VerifyResult
	{
		VerifyGood,        // good sig
		VerifyBad,         // bad sig
		VerifyNoKey        // we don't have signer's public key
	};

	enum Error
	{
		ErrorProcess,          // startup, process, or ipc error
		ErrorPassphrase,       // passphrase was either wrong or not provided
		ErrorFormat,           // input format was bad
		ErrorSignerExpired,    // signing key is expired
		ErrorEncryptExpired,   // encrypting key is expired
		ErrorEncryptUntrusted, // encrypting key is untrusted
		ErrorEncryptInvalid,   // encrypting key is invalid in some way
		ErrorDecryptNoKey,     // missing decrypt key
		ErrorUnknown           // other error
	};

	class Event
	{
	public:
		enum Type
		{
			None,
			ReadyRead,
			BytesWritten,
			Finished,
			NeedPassphrase,
			NeedCard,
			ReadyReadDiagnosticText
		};

		Type type;
		int written;   // BytesWritten
		QString keyId; // NeedPassphrase

		Event() : type(None), written(0) {}
	};

	class KeyItem
	{
	public:
		enum Type
		{
			RSA,
			DSA,
			ElGamal,
			Unknown
		};

		enum Caps
		{
			Encrypt = 0x01,
			Sign    = 0x02,
			Certify = 0x04,
			Auth    = 0x08
		};

		QString id;
		Type type;
		int bits;
		QDateTime creationDate;
		QDateTime expirationDate;
		int caps; // flags OR'd together
		QString fingerprint;

		KeyItem() : type(Unknown), bits(0), caps(0) {}
	};

	class Key
	{
	public:
		QList<KeyItem> keyItems; // first item is primary
		QStringList userIds;
		bool isTrusted;

		Key() : isTrusted(false) {}
	};
	typedef QList<Key> KeyList;

	explicit GpgOp(const QString &bin, QObject *parent = 0);
	~GpgOp();

	void reset();

	bool isActive() const;
	Type op() const;

	void setAsciiFormat(bool b);
	void setDisableAgent(bool b);
	void setAlwaysTrust(bool b);
	void setKeyrings(const QString &pubfile, const QString &secfile); // for keylists and import

	void doCheck();
	void doSecretKeyringFile();
	void doPublicKeyringFile();
	void doSecretKeys();
	void doPublicKeys();
	void doEncrypt(const QStringList &recip_ids);
	void doDecrypt();
	void doSign(const QString &signer_id);
	void doSignAndEncrypt(const QString &signer_id, const QStringList &recip_ids);
	void doSignClearsign(const QString &signer_id);
	void doSignDetached(const QString &signer_id);
	void doVerify();
	void doVerifyDetached(const QByteArray &sig);
	void doImport(const QByteArray &in);
	void doExport(const QString &key_id);
	void doDeleteKey(const QString &key_fingerprint);

#ifdef QPIPE_SECURE
	void submitPassphrase(const QCA::SecureArray &a);
#else
	void submitPassphrase(const QByteArray &a);
#endif
	void cardOkay();

	// for encrypt, decrypt, sign, verify, export
	QByteArray read();
	void write(const QByteArray &in);
	void endWrite();

	QString readDiagnosticText();

	// for synchronous operation
	Event waitForEvent(int msecs = -1);

	// results
	bool success() const;
	Error errorCode() const;
	KeyList keys() const;              // Keys
	QString keyringFile() const;       // KeyringFile
	QString encryptedToId() const;     // Decrypt (for ErrorDecryptNoKey)
	bool wasSigned() const;            // Decrypt
	QString signerId() const;          // Verify
	QDateTime timestamp() const;       // Verify
	VerifyResult verifyResult() const; // Verify

Q_SIGNALS:
	void readyRead();
	void bytesWritten(int bytes);
	void finished();
	void needPassphrase(const QString &keyId);
	void needCard();
	void readyReadDiagnosticText();

private:
	class Private;
	friend class Private;
	Private *d;
};

}

#endif
