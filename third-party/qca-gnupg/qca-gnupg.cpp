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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <QtCore>
#include <QtCrypto>
#ifdef Q_OS_WIN
# include<windows.h>
#endif
#include "gpgop.h"

#ifdef Q_OS_WIN
static QString find_reg_gpgProgram()
{
	HKEY root;
	root = HKEY_CURRENT_USER;

	HKEY hkey;
	const char *path = "Software\\GNU\\GnuPG";
	if(RegOpenKeyExA(HKEY_CURRENT_USER, path, 0, KEY_QUERY_VALUE, &hkey) != ERROR_SUCCESS)
	{
		if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_QUERY_VALUE, &hkey) != ERROR_SUCCESS)
			return QString::null;
	}

	char szValue[256];
	DWORD dwLen = 256;
	if(RegQueryValueExA(hkey, "gpgProgram", NULL, NULL, (LPBYTE)szValue, &dwLen) != ERROR_SUCCESS)
	{
		RegCloseKey(hkey);
		return QString::null;
	}

	RegCloseKey(hkey);
	return QString::fromLatin1(szValue);
}
#endif

static QString find_bin()
{
	QString bin = "gpg";
#ifdef Q_OS_WIN
	QString s = find_reg_gpgProgram();
	if(!s.isNull())
		bin = s;
#endif
	return bin;
}

using namespace QCA;

namespace gpgQCAPlugin {

class MyPGPKeyContext : public PGPKeyContext
{
public:
	PGPKeyContextProps _props;

	MyPGPKeyContext(Provider *p) : PGPKeyContext(p)
	{
	}

	virtual Provider::Context *clone() const
	{
		return new MyPGPKeyContext(*this);
	}

	virtual const PGPKeyContextProps *props() const
	{
		return &_props;
	}

	virtual QSecureArray toBinary() const
	{
		// TODO
		return QSecureArray();
	}

	virtual QString toAscii() const
	{
		// TODO
		return QString();
	}

	virtual ConvertResult fromBinary(const QSecureArray &a)
	{
		// TODO
		Q_UNUSED(a);
		return ErrorDecode;
	}

	virtual ConvertResult fromAscii(const QString &s)
	{
		// TODO
		Q_UNUSED(s);
		return ErrorDecode;
	}
};

class MyKeyStoreEntry : public KeyStoreEntryContext
{
public:
	KeyStoreEntry::Type item_type;
	PGPKey pub, sec;

	MyKeyStoreEntry(const PGPKey &_pub, const PGPKey &_sec, Provider *p) : KeyStoreEntryContext(p)
	{
		pub = _pub;
		sec = _sec;
		if(!sec.isNull())
			item_type = KeyStoreEntry::TypePGPSecretKey;
		else
			item_type = KeyStoreEntry::TypePGPPublicKey;
	}

	MyKeyStoreEntry(const MyKeyStoreEntry &from) : KeyStoreEntryContext(from)
	{
	}

	~MyKeyStoreEntry()
	{
	}

	virtual Provider::Context *clone() const
	{
		return new MyKeyStoreEntry(*this);
	}

	virtual KeyStoreEntry::Type type() const
	{
		return item_type;
	}

	virtual QString name() const
	{
		return pub.primaryUserId();
	}

	virtual QString id() const
	{
		return pub.keyId();
	}

	virtual QString storeId() const
	{
		return QString();
	}

	virtual QString storeName() const
	{
		return QString();
	}

	virtual PGPKey pgpSecretKey() const
	{
		return sec;
	}

	virtual PGPKey pgpPublicKey() const
	{
		return pub;
	}
};

class MyMessageContext;

GpgOp *global_gpg;

/*class MyKeyStore : public KeyStoreContext
{
	Q_OBJECT
public:
	friend class MyMessageContext;

	MyKeyStore(Provider *p) : KeyStoreContext(p) {}

	virtual Provider::Context *clone() const
	{
		return 0;
	}

	virtual int contextId() const
	{
		// TODO
		return 0; // there is only 1 context, so this can be static
	}

	virtual QString deviceId() const
	{
		// TODO
		return "qca-gnupg-(gpg)";
	}

	virtual KeyStore::Type type() const
	{
		return KeyStore::PGPKeyring;
	}

	virtual QString name() const
	{
		return "GnuPG Keyring";
	}

	virtual QList<KeyStoreEntryContext*> entryList() const
	{
		QList<KeyStoreEntryContext*> out;

		GpgOp::KeyList seckeys;
		{
			GpgOp gpg(find_bin());
			gpg.doSecretKeys();
			while(1)
			{
				GpgOp::Event e = gpg.waitForEvent(-1);
				if(e.type == GpgOp::Event::Finished)
					break;
			}
			if(!gpg.success())
				return out;
			seckeys = gpg.keys();
		}

		GpgOp::KeyList pubkeys;
		{
			GpgOp gpg(find_bin());
			gpg.doPublicKeys();
			while(1)
			{
				GpgOp::Event e = gpg.waitForEvent(-1);
				if(e.type == GpgOp::Event::Finished)
					break;
			}
			if(!gpg.success())
				return out;
			pubkeys = gpg.keys();
		}

		for(int n = 0; n < pubkeys.count(); ++n)
		{
			QString id = pubkeys[n].keyItems.first().id;
			MyPGPKeyContext *kc = new MyPGPKeyContext(provider());
			kc->_props.keyId = id;
			kc->_props.userIds = QStringList() << pubkeys[n].userIds.first();
			PGPKey pub, sec;
			pub.change(kc);
			for(int i = 0; i < seckeys.count(); ++i)
			{
				if(seckeys[i].keyItems.first().id == id)
				{
					MyPGPKeyContext *kc = new MyPGPKeyContext(provider());
					kc->_props.keyId = id;
					kc->_props.userIds = QStringList() << pubkeys[n].userIds.first();
					sec.change(kc);
				}
			}

			MyKeyStoreEntry *c = new MyKeyStoreEntry(pub, sec, provider());
			out.append(c);
		}

		return out;
	}

	virtual QList<KeyStoreEntry::Type> entryTypes() const
	{
		QList<KeyStoreEntry::Type> list;
		list += KeyStoreEntry::TypePGPSecretKey;
		list += KeyStoreEntry::TypePGPPublicKey;
		return list;
	}

	virtual void submitPassphrase(const QSecureArray &a)
	{
		global_gpg->submitPassphrase(a.toByteArray());
	}
};*/

class MyKeyStoreList;
class MyMessageContext;

static MyKeyStoreList *keyStoreList = 0;

class MyKeyStoreList : public KeyStoreListContext
{
	Q_OBJECT
public:
	friend class MyMessageContext;
	//MyKeyStore *ks;

	MyKeyStoreList(Provider *p) : KeyStoreListContext(p)
	{
		keyStoreList = this;

		//ks = 0;

		//ks = new MyKeyStore(provider());
	}

	~MyKeyStoreList()
	{
		//delete ks;

		keyStoreList = 0;
	}

	virtual Provider::Context *clone() const
	{
		return 0;
	}

	/*virtual void start()
	{
		QTimer::singleShot(0, this, SLOT(do_ready()));
	}*/

	virtual QList<int> keyStores()
	{
		QList<int> list;
		list += 0; // TODO
		return list;
	}

	virtual KeyStore::Type type(int) const
	{
		return KeyStore::PGPKeyring;
	}

	virtual QString storeId(int) const
	{
		// TODO
		return "qca-gnupg-(gpg)";
	}

	virtual QString name(int) const
	{
		return "GnuPG Keyring";
	}

	virtual QList<KeyStoreEntry::Type> entryTypes(int) const
	{
		QList<KeyStoreEntry::Type> list;
		list += KeyStoreEntry::TypePGPSecretKey;
		list += KeyStoreEntry::TypePGPPublicKey;
		return list;
	}

	virtual QList<KeyStoreEntryContext*> entryList(int)
	{
		QList<KeyStoreEntryContext*> out;

		GpgOp::KeyList seckeys;
		{
			GpgOp gpg(find_bin());
			gpg.doSecretKeys();
			while(1)
			{
				GpgOp::Event e = gpg.waitForEvent(-1);
				if(e.type == GpgOp::Event::Finished)
					break;
			}
			if(!gpg.success())
				return out;
			seckeys = gpg.keys();
		}

		GpgOp::KeyList pubkeys;
		{
			GpgOp gpg(find_bin());
			gpg.doPublicKeys();
			while(1)
			{
				GpgOp::Event e = gpg.waitForEvent(-1);
				if(e.type == GpgOp::Event::Finished)
					break;
			}
			if(!gpg.success())
				return out;
			pubkeys = gpg.keys();
		}

		for(int n = 0; n < pubkeys.count(); ++n)
		{
			QString id = pubkeys[n].keyItems.first().id;
			MyPGPKeyContext *kc = new MyPGPKeyContext(provider());
			kc->_props.keyId = id;
			kc->_props.userIds = QStringList() << pubkeys[n].userIds.first();
			PGPKey pub, sec;
			pub.change(kc);
			for(int i = 0; i < seckeys.count(); ++i)
			{
				if(seckeys[i].keyItems.first().id == id)
				{
					MyPGPKeyContext *kc = new MyPGPKeyContext(provider());
					kc->_props.keyId = id;
					kc->_props.userIds = QStringList() << pubkeys[n].userIds.first();
					sec.change(kc);
				}
			}

			MyKeyStoreEntry *c = new MyKeyStoreEntry(pub, sec, provider());
			out.append(c);
		}

		return out;
	}

	virtual void submitPassphrase(int, int, const QSecureArray &a)
	{
		global_gpg->submitPassphrase(a.toByteArray());
	}

/*private slots:
	void do_ready()
	{
		emit busyEnd();
	}*/
};

// FIXME: sloooooow
static PGPKey publicKeyFromId(const QString &id, Provider *p)
{
	GpgOp::KeyList pubkeys;
	{
		GpgOp gpg(find_bin());
		gpg.doPublicKeys();
		while(1)
		{
			GpgOp::Event e = gpg.waitForEvent(-1);
			if(e.type == GpgOp::Event::Finished)
				break;
		}
		if(!gpg.success())
			return PGPKey(); // FIXME
		pubkeys = gpg.keys();
	}

	int at = -1;
	for(int n = 0; n < pubkeys.count(); ++n)
	{
		if(pubkeys[n].keyItems.first().id == id)
		{
			at = n;
			break;
		}
	}
	if(at == -1)
		return PGPKey(); // FIXME

	MyPGPKeyContext *kc = new MyPGPKeyContext(p);
	kc->_props.keyId = pubkeys[at].keyItems.first().id;
	kc->_props.userIds = QStringList() << pubkeys[at].userIds.first();

	PGPKey pub;
	pub.change(kc);
	return pub;
}

static PGPKey secretKeyFromId(const QString &id, Provider *p)
{
	GpgOp::KeyList seckeys;
	{
		GpgOp gpg(find_bin());
		gpg.doSecretKeys();
		while(1)
		{
			GpgOp::Event e = gpg.waitForEvent(-1);
			if(e.type == GpgOp::Event::Finished)
				break;
		}
		if(!gpg.success())
			return PGPKey(); // FIXME
		seckeys = gpg.keys();
	}

	int at = -1;
	for(int n = 0; n < seckeys.count(); ++n)
	{
		const GpgOp::Key &key = seckeys[n];
		for(int k = 0; k < key.keyItems.count(); ++k)
		{
			const GpgOp::KeyItem &ki = key.keyItems[k];
			if(ki.id == id)
			{
				at = n;
				break;
			}
		}
		if(at != -1)
			break;
	}
	if(at == -1)
		return PGPKey(); // FIXME

	MyPGPKeyContext *kc = new MyPGPKeyContext(p);
	kc->_props.keyId = seckeys[at].keyItems.first().id;
	kc->_props.userIds = QStringList() << seckeys[at].userIds.first();

	PGPKey sec;
	sec.change(kc);
	return sec;
}

class MyOpenPGPContext : public SMSContext
{
public:
	MyOpenPGPContext(Provider *p) : SMSContext(p, "openpgp")
	{
		// TODO
	}

	virtual Provider::Context *clone() const
	{
		return 0;
	}

	virtual MessageContext *createMessage();
};

class MyMessageContext : public MessageContext
{
	Q_OBJECT
public:
	MyOpenPGPContext *sms;

	QString signerId;
	QStringList recipIds;
	Operation op;
	SecureMessage::SignMode signMode;
	SecureMessage::Format format;
	QByteArray in, out, sig;
	bool ok, wasSigned;
	GpgOp::Error op_err;
	SecureMessageSignature signer;

	PasswordAsker asker;

	MyMessageContext(MyOpenPGPContext *_sms, Provider *p) : MessageContext(p, "pgpmsg")
	{
		sms = _sms;
		ok = false;
		wasSigned = false;
		//connect(&asker, SIGNAL(responseReady()), SLOT(asker_responseReady()));
	}

	virtual Provider::Context *clone() const
	{
		return 0;
	}

	virtual bool canSignMultiple() const
	{
		return false;
	}

	virtual SecureMessage::Type type() const
	{
		return SecureMessage::OpenPGP;
	}

	virtual void reset()
	{
		ok = false;
		wasSigned = false;
	}

	virtual void setupEncrypt(const SecureMessageKeyList &keys)
	{
		recipIds.clear();
		for(int n = 0; n < keys.count(); ++n)
			recipIds += keys[n].pgpPublicKey().keyId();
	}

	virtual void setupSign(const SecureMessageKeyList &keys, SecureMessage::SignMode m, bool, bool)
	{
		signerId = keys.first().pgpSecretKey().keyId();
		signMode = m;
	}

	virtual void setupVerify(const QByteArray &detachedSig)
	{
		sig = detachedSig;
	}

	virtual void start(SecureMessage::Format f, Operation op)
	{
		format = f;
		this->op = op;
	}

	virtual void update(const QByteArray &in)
	{
		this->in.append(in);
	}

	virtual QByteArray read()
	{
		QByteArray a = out;
		out.clear();
		return a;
	}

	virtual void end()
	{
		bool debug_this = false;
		static int calls = 0;

		GpgOp gpg(find_bin());

		if(format == SecureMessage::Ascii)
			gpg.setAsciiFormat(true);
		else
			gpg.setAsciiFormat(false);

		if(op == Encrypt)
		{
			gpg.doEncrypt(recipIds);
		}
		else if(op == Decrypt)
		{
			gpg.doDecrypt();
		}
		else if(op == Sign)
		{
			if(signMode == SecureMessage::Message)
			{
				gpg.doSign(signerId);
			}
			else if(signMode == SecureMessage::Clearsign)
			{
				gpg.doSignClearsign(signerId);
			}
			else // SecureMessage::Detached
			{
				gpg.doSignDetached(signerId);
			}
		}
		else if(op == Verify)
		{
			if(!qgetenv("PSI_GPG_DEBUG").isEmpty()) {
				debug_this = true;
				++calls;
				qDebug() << "Start Verifying " << calls;
			}
			if(!sig.isEmpty())
				gpg.doVerifyDetached(sig);
			else
				gpg.doVerify();
		}
		else if(op == SignAndEncrypt)
		{
			gpg.doSignAndEncrypt(signerId, recipIds);
		}

		gpg.write(in);
		gpg.endWrite();
		while(1)
		{
			GpgOp::Event e = gpg.waitForEvent(-1);
			if(e.type == GpgOp::Event::NeedPassphrase)
			{
				// TODO

				QString keyId;
				PGPKey sec = secretKeyFromId(e.keyId, provider());
				if(!sec.isNull())
					keyId = sec.keyId();
				else
					keyId = e.keyId;
				//emit keyStoreList->storeNeedPassphrase(0, 0, keyId);
				asker.ask(Event::StylePassphrase, keyStoreList->storeId(0), keyId, 0);
				asker.waitForResponse();
				global_gpg = &gpg;
				keyStoreList->submitPassphrase(0, 0, asker.password());
			}
			else if(e.type == GpgOp::Event::Finished)
				break;
		}
		if (debug_this) {
			qDebug() << "End Verifying " << calls;
		}

		ok = gpg.success();
		if(ok)
		{
			if(signMode == SecureMessage::Detached)
				sig = gpg.read();
			else
				out = gpg.read();
		}

		if(ok)
		{
			if(gpg.wasSigned())
			{
				QString signerId = gpg.signerId();
				QDateTime ts = gpg.timestamp();
				GpgOp::VerifyResult vr = gpg.verifyResult();

				SecureMessageSignature::IdentityResult ir;
				Validity v;
				if(vr == GpgOp::VerifyGood)
				{
					ir = SecureMessageSignature::Valid;
					v = ValidityGood;
				}
				else if(vr == GpgOp::VerifyBad)
				{
					ir = SecureMessageSignature::InvalidSignature;
					v = ValidityGood;
				}
				else // GpgOp::VerifyNoKey
				{
					ir = SecureMessageSignature::NoKey;
					v = ErrorValidityUnknown;
				}

				SecureMessageKey key;
				PGPKey pub = publicKeyFromId(signerId, provider());
				if(pub.isNull()) {
					MyPGPKeyContext *kc = new MyPGPKeyContext(provider());
					kc->_props.keyId = signerId;
					pub.change(kc);
				}
				key.setPGPPublicKey(pub);
				signer = SecureMessageSignature(ir, v, key, ts);
				wasSigned = true;
			}
		}
		else
			op_err = gpg.errorCode();

		global_gpg = 0;
	}

	virtual bool finished() const
	{
		// TODO
		return true;
	}

	virtual void waitForFinished(int msecs)
	{
		Q_UNUSED(msecs);
	}

	virtual bool success() const
	{
		return ok;
	}

	virtual SecureMessage::Error errorCode() const
	{
		SecureMessage::Error e = SecureMessage::ErrorUnknown;
		if(op_err == GpgOp::ErrorProcess)
			e = SecureMessage::ErrorUnknown;
		else if(op_err == GpgOp::ErrorPassphrase)
			e = SecureMessage::ErrorPassphrase;
		else if(op_err == GpgOp::ErrorFormat)
			e = SecureMessage::ErrorFormat;
		else if(op_err == GpgOp::ErrorSignerExpired)
			e = SecureMessage::ErrorSignerExpired;
		else if(op_err == GpgOp::ErrorEncryptExpired)
			e = SecureMessage::ErrorEncryptExpired;
		else if(op_err == GpgOp::ErrorEncryptUntrusted)
			e = SecureMessage::ErrorEncryptUntrusted;
		else if(op_err == GpgOp::ErrorEncryptInvalid)
			e = SecureMessage::ErrorEncryptInvalid;
		else if(op_err == GpgOp::ErrorDecryptNoKey)
			e = SecureMessage::ErrorUnknown;
		else if(op_err == GpgOp::ErrorUnknown)
			e = SecureMessage::ErrorUnknown;
		return e;
	}

	virtual QByteArray signature() const
	{
		return sig;
	}

	virtual QString hashName() const
	{
		// TODO
		return "sha1";
	}

	virtual SecureMessageSignatureList signers() const
	{
		SecureMessageSignatureList list;
		if(ok && wasSigned)
			list += signer;
		return list;
	}

/*private slots:
	void asker_responseReady()
	{
		QSecureArray a = asker.password();
		global_gpg->submitPassphrase(a.toByteArray());
	}*/
};

MessageContext *MyOpenPGPContext::createMessage()
{
	return new MyMessageContext(this, provider());
}

}

using namespace gpgQCAPlugin;

class gnupgProvider : public QCA::Provider
{
public:
	virtual void init()
	{
	}

	virtual int version() const
	{
		return QCA_VERSION;
	}

	virtual QString name() const
	{
		return "qca-gnupg";
	}

	virtual QStringList features() const
	{
		QStringList list;
		list += "pgpkey";
		list += "openpgp";
		list += "keystorelist";
		return list;
	}

	virtual Context *createContext(const QString &type)
	{
		if(type == "pgpkey")
			return new MyPGPKeyContext(this);
		else if(type == "openpgp")
			return new MyOpenPGPContext(this);
		else if(type == "keystorelist")
			return new MyKeyStoreList(this);
		else
			return 0;
	}
};

class gnupgPlugin : public QCAPlugin
{
	Q_OBJECT
	Q_INTERFACES(QCAPlugin)
public:
	virtual QCA::Provider *createProvider() { return new gnupgProvider; }
};

#include "qca-gnupg.moc"

Q_EXPORT_PLUGIN2(qca_gnupg, gnupgPlugin)
