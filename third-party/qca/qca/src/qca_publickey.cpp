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

#include "qca_publickey.h"

#include "qcaprovider.h"

#include <QFile>
#include <QTextStream>

namespace QCA {

Provider::Context *getContext(const QString &type, const QString &provider);
Provider::Context *getContext(const QString &type, Provider *p);

bool stringToFile(const QString &fileName, const QString &content)
{
	QFile f(fileName);
	if(!f.open(QFile::WriteOnly))
		return false;
	QTextStream ts(&f);
	ts << content;
	return true;
}

bool stringFromFile(const QString &fileName, QString *s)
{
	QFile f(fileName);
	if(!f.open(QFile::ReadOnly))
		return false;
	QTextStream ts(&f);
	*s = ts.readAll();
	return true;
}

bool arrayToFile(const QString &fileName, const QByteArray &content)
{
	QFile f(fileName);
	if(!f.open(QFile::WriteOnly))
		return false;
	f.write(content.data(), content.size());
	return true;
}

bool arrayFromFile(const QString &fileName, QByteArray *a)
{
	QFile f(fileName);
	if(!f.open(QFile::ReadOnly))
		return false;
	*a = f.readAll();
	return true;
}

bool ask_passphrase(const QString &fname, void *ptr, SecureArray *answer)
{
	PasswordAsker asker;
	asker.ask(Event::StylePassphrase, fname, ptr);
	asker.waitForResponse();
	if(!asker.accepted())
		return false;
	*answer = asker.password();
	return true;
}

ProviderList allProviders()
{
	ProviderList pl = providers();
	pl += defaultProvider();
	return pl;
}

Provider *providerForName(const QString &name)
{
	ProviderList pl = allProviders();
	for(int n = 0; n < pl.count(); ++n)
	{
		if(pl[n]->name() == name)
			return pl[n];
	}
	return 0;
}

bool use_asker_fallback(ConvertResult r)
{
	// FIXME: we should only do this if we get ErrorPassphrase?
	//if(r == ErrorPassphrase)
	if(r != ConvertGood)
		return true;
	return false;
}

class Getter_GroupSet
{
public:
	static QList<DLGroupSet> getList(Provider *p)
	{
		QList<DLGroupSet> list;
		const DLGroupContext *c = static_cast<const DLGroupContext *>(getContext("dlgroup", p));
		if(!c)
			return list;
		list = c->supportedGroupSets();
		delete c;
		return list;
	}
};

class Getter_PBE
{
public:
	static QList<PBEAlgorithm> getList(Provider *p)
	{
		QList<PBEAlgorithm> list;
		const PKeyContext *c = static_cast<const PKeyContext *>(getContext("pkey", p));
		if(!c)
			return list;
		list = c->supportedPBEAlgorithms();
		delete c;
		return list;
	}
};

class Getter_Type
{
public:
	static QList<PKey::Type> getList(Provider *p)
	{
		QList<PKey::Type> list;
		const PKeyContext *c = static_cast<const PKeyContext *>(getContext("pkey", p));
		if(!c)
			return list;
		list = c->supportedTypes();
		delete c;
		return list;
	}
};

class Getter_IOType
{
public:
	static QList<PKey::Type> getList(Provider *p)
	{
		QList<PKey::Type> list;
		const PKeyContext *c = static_cast<const PKeyContext *>(getContext("pkey", p));
		if(!c)
			return list;
		list = c->supportedIOTypes();
		delete c;
		return list;
	}
};

template <typename I>
class Getter_PublicKey
{
public:
	// DER
	static ConvertResult fromData(PKeyContext *c, const QByteArray &in)
	{
		return c->publicFromDER(in);
	}

	// PEM
	static ConvertResult fromData(PKeyContext *c, const QString &in)
	{
		return c->publicFromPEM(in);
	}

	static PublicKey getKey(Provider *p, const I &in, const SecureArray &, ConvertResult *result)
	{
		PublicKey k;
		PKeyContext *c = static_cast<PKeyContext *>(getContext("pkey", p));
		if(!c)
		{
			if(result)
				*result = ErrorDecode;
			return k;
		}
		ConvertResult r = fromData(c, in);
		if(result)
			*result = r;
		if(r == ConvertGood)
			k.change(c);
		else
			delete c;
		return k;
	}
};

template <typename I>
class Getter_PrivateKey
{
public:
	// DER
	static ConvertResult fromData(PKeyContext *c, const SecureArray &in, const SecureArray &passphrase)
	{
		return c->privateFromDER(in, passphrase);
	}

	// PEM
	static ConvertResult fromData(PKeyContext *c, const QString &in, const SecureArray &passphrase)
	{
		return c->privateFromPEM(in, passphrase);
	}

	static PrivateKey getKey(Provider *p, const I &in, const SecureArray &passphrase, ConvertResult *result)
	{
		PrivateKey k;
		PKeyContext *c = static_cast<PKeyContext *>(getContext("pkey", p));
		if(!c)
		{
			if(result)
				*result = ErrorDecode;
			return k;
		}
		ConvertResult r = fromData(c, in, passphrase);
		if(result)
			*result = r;
		if(r == ConvertGood)
			k.change(c);
		else
			delete c;
		return k;
	}
};

Provider *providerForGroupSet(DLGroupSet set)
{
	ProviderList pl = allProviders();
	for(int n = 0; n < pl.count(); ++n)
	{
		if(Getter_GroupSet::getList(pl[n]).contains(set))
			return pl[n];
	}
	return 0;
}

Provider *providerForPBE(PBEAlgorithm alg, PKey::Type ioType, const PKeyContext *prefer = 0)
{
	Provider *preferProvider = 0;
	if(prefer)
	{
		preferProvider = prefer->provider();
		if(prefer->supportedPBEAlgorithms().contains(alg) && prefer->supportedIOTypes().contains(ioType))
			return preferProvider;
	}

	ProviderList pl = allProviders();
	for(int n = 0; n < pl.count(); ++n)
	{
		if(preferProvider && pl[n] == preferProvider)
			continue;

		if(Getter_PBE::getList(pl[n]).contains(alg) && Getter_IOType::getList(pl[n]).contains(ioType))
			return pl[n];
	}
	return 0;
}

Provider *providerForIOType(PKey::Type type, const PKeyContext *prefer = 0)
{
	Provider *preferProvider = 0;
	if(prefer)
	{
		preferProvider = prefer->provider();
		if(prefer && prefer->supportedIOTypes().contains(type))
			return preferProvider;
	}

	ProviderList pl = allProviders();
	for(int n = 0; n < pl.count(); ++n)
	{
		if(preferProvider && pl[n] == preferProvider)
			continue;

		if(Getter_IOType::getList(pl[n]).contains(type))
			return pl[n];
	}
	return 0;
}

template <typename T, typename G>
QList<T> getList(const QString &provider)
{
	QList<T> list;

	// single
	if(!provider.isEmpty())
	{
		Provider *p = providerForName(provider);
		if(p)
			list = G::getList(p);
	}
	// all
	else
	{
		ProviderList pl = allProviders();
		for(int n = 0; n < pl.count(); ++n)
		{
			QList<T> other = G::getList(pl[n]);
			for(int k = 0; k < other.count(); ++k)
			{
				// only add what we don't have in the list
				if(!list.contains(other[k]))
					list += other[k];
			}
		}
	}

	return list;
}

template <typename T, typename G, typename I>
T getKey(const QString &provider, const I &in, const SecureArray &passphrase, ConvertResult *result)
{
	T k;

	// single
	if(!provider.isEmpty())
	{
		Provider *p = providerForName(provider);
		if(!p)
			return k;
		k = G::getKey(p, in, passphrase, result);
	}
	// all
	else
	{
		ProviderList pl = allProviders();
		for(int n = 0; n < pl.count(); ++n)
		{
			ConvertResult r;
			k = G::getKey(pl[n], in, passphrase, &r);
			if(result)
				*result = r;
			if(!k.isNull())
				break;
			if(r == ErrorPassphrase) // don't loop if we get this
				break;
		}
	}

	return k;
}

PBEAlgorithm get_pbe_default()
{
	return PBES2_TripleDES_SHA1;
}

static PrivateKey get_privatekey_der(const SecureArray &der, const QString &fileName, void *ptr, const SecureArray &passphrase, ConvertResult *result, const QString &provider)
{
	PrivateKey out;
	ConvertResult r;
	out = getKey<PrivateKey, Getter_PrivateKey<SecureArray>, SecureArray>(provider, der, passphrase, &r);

	// error converting without passphrase?  maybe a passphrase is needed
	if(use_asker_fallback(r) && passphrase.isEmpty())
	{
		SecureArray pass;
		if(ask_passphrase(fileName, ptr, &pass))
			out = getKey<PrivateKey, Getter_PrivateKey<SecureArray>, SecureArray>(provider, der, pass, &r);
	}
	if(result)
		*result = r;
	return out;
}

static PrivateKey get_privatekey_pem(const QString &pem, const QString &fileName, void *ptr, const SecureArray &passphrase, ConvertResult *result, const QString &provider)
{
	PrivateKey out;
	ConvertResult r;
	out = getKey<PrivateKey, Getter_PrivateKey<QString>, QString>(provider, pem, passphrase, &r);

	// error converting without passphrase?  maybe a passphrase is needed
	if(use_asker_fallback(r) && passphrase.isEmpty())
	{
		SecureArray pass;
		if(ask_passphrase(fileName, ptr, &pass))
			out = getKey<PrivateKey, Getter_PrivateKey<QString>, QString>(provider, pem, pass, &r);
	}
	if(result)
		*result = r;
	return out;
}

//----------------------------------------------------------------------------
// Global
//----------------------------------------------------------------------------

// adapted from Botan
static const unsigned char pkcs_sha1[] =
{
	0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2B, 0x0E, 0x03, 0x02,
	0x1A, 0x05, 0x00, 0x04, 0x14
};

static const unsigned char pkcs_md5[] =
{
	0x30, 0x20, 0x30, 0x0C, 0x06, 0x08, 0x2A, 0x86, 0x48, 0x86,
	0xF7, 0x0D, 0x02, 0x05, 0x05, 0x00, 0x04, 0x10
};

static const unsigned char pkcs_md2[] =
{
	0x30, 0x20, 0x30, 0x0C, 0x06, 0x08, 0x2A, 0x86, 0x48, 0x86,
	0xF7, 0x0D, 0x02, 0x02, 0x05, 0x00, 0x04, 0x10
};

static const unsigned char pkcs_ripemd160[] =
{
	0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2B, 0x24, 0x03, 0x02,
	0x01, 0x05, 0x00, 0x04, 0x14
};

QByteArray get_hash_id(const QString &name)
{
	if(name == "sha1")
		return QByteArray::fromRawData((const char *)pkcs_sha1, sizeof(pkcs_sha1));
	else if(name == "md5")
		return QByteArray::fromRawData((const char *)pkcs_md5, sizeof(pkcs_md5));
	else if(name == "md2")
		return QByteArray::fromRawData((const char *)pkcs_md2, sizeof(pkcs_md2));
	else if(name == "ripemd160")
		return QByteArray::fromRawData((const char *)pkcs_ripemd160, sizeof(pkcs_ripemd160));
	else
		return QByteArray();
}

QByteArray emsa3Encode(const QString &hashName, const QByteArray &digest, int size)
{
	QByteArray hash_id = get_hash_id(hashName);
	if(hash_id.isEmpty())
		return QByteArray();

	// logic adapted from Botan
	int basesize = hash_id.size() + digest.size() + 2;
	if(size == -1)
		size = basesize + 1; // default to 1-byte pad
	int padlen = size - basesize;
	if(padlen < 1)
		return QByteArray();

	QByteArray out(size, (char)0xff); // pad with 0xff
	out[0] = 0x01;
	out[padlen + 1] = 0x00;
	int at = padlen + 2;
	memcpy(out.data() + at, hash_id.data(), hash_id.size());
	at += hash_id.size();
	memcpy(out.data() + at, digest.data(), digest.size());
	return out;
}

//----------------------------------------------------------------------------
// DLGroup
//----------------------------------------------------------------------------
class DLGroup::Private
{
public:
	BigInteger p, q, g;

	Private(const BigInteger &p1, const BigInteger &q1, const BigInteger &g1)
	:p(p1), q(q1), g(g1)
	{
	}
};

DLGroup::DLGroup()
{
	d = 0;
}

DLGroup::DLGroup(const BigInteger &p, const BigInteger &q, const BigInteger &g)
{
	d = new Private(p, q, g);
}

DLGroup::DLGroup(const BigInteger &p, const BigInteger &g)
{
	d = new Private(p, 0, g);
}

DLGroup::DLGroup(const DLGroup &from)
{
	d = 0;
	*this = from;
}

DLGroup::~DLGroup()
{
	delete d;
}

DLGroup & DLGroup::operator=(const DLGroup &from)
{
	delete d;
	d = 0;

	if(from.d)
		d = new Private(*from.d);

	return *this;
}

QList<DLGroupSet> DLGroup::supportedGroupSets(const QString &provider)
{
	return getList<DLGroupSet, Getter_GroupSet>(provider);
}

bool DLGroup::isNull() const
{
	return (d ? false: true);
}

BigInteger DLGroup::p() const
{
	return d->p;
}

BigInteger DLGroup::q() const
{
	return d->q;
}

BigInteger DLGroup::g() const
{
	return d->g;
}

//----------------------------------------------------------------------------
// PKey
//----------------------------------------------------------------------------
class PKey::Private
{
public:
};

PKey::PKey()
{
	d = new Private;
}

PKey::PKey(const QString &type, const QString &provider)
:Algorithm(type, provider)
{
	d = new Private;
}

PKey::PKey(const PKey &from)
:Algorithm(from)
{
	d = new Private;
	*this = from;
}

PKey::~PKey()
{
	delete d;
}

PKey & PKey::operator=(const PKey &from)
{
	Algorithm::operator=(from);
	*d = *from.d;
	return *this;
}

void PKey::set(const PKey &k)
{
	*this = k;
}

void PKey::assignToPublic(PKey *dest) const
{
	dest->set(*this);

	// converting private to public
	if(dest->isPrivate())
		static_cast<PKeyContext *>(dest->context())->key()->convertToPublic();
}

void PKey::assignToPrivate(PKey *dest) const
{
	dest->set(*this);
}

QList<PKey::Type> PKey::supportedTypes(const QString &provider)
{
	return getList<Type, Getter_Type>(provider);
}

QList<PKey::Type> PKey::supportedIOTypes(const QString &provider)
{
	return getList<Type, Getter_IOType>(provider);
}

bool PKey::isNull() const
{
	return (!context() ? true : false);
}

PKey::Type PKey::type() const
{
	if(isNull())
		return RSA; // some default so we don't explode
	return static_cast<const PKeyContext *>(context())->key()->type();
}

int PKey::bitSize() const
{
	return static_cast<const PKeyContext *>(context())->key()->bits();
}

bool PKey::isRSA() const
{
	return (type() == RSA);
}

bool PKey::isDSA() const
{
	return (type() == DSA);
}

bool PKey::isDH() const
{
	return (type() == DH);
}

bool PKey::isPublic() const
{
	if(isNull())
		return false;
	return !isPrivate();
}

bool PKey::isPrivate() const
{
	if(isNull())
		return false;
	return static_cast<const PKeyContext *>(context())->key()->isPrivate();
}

bool PKey::canExport() const
{
	return static_cast<const PKeyContext *>(context())->key()->canExport();
}

bool PKey::canKeyAgree() const
{
	return isDH();
}

PublicKey PKey::toPublicKey() const
{
	PublicKey k;
	if(!isNull())
		assignToPublic(&k);
	return k;
}

PrivateKey PKey::toPrivateKey() const
{
	PrivateKey k;
	if(!isNull() && isPrivate())
		assignToPrivate(&k);
	return k;
}

RSAPublicKey PKey::toRSAPublicKey() const
{
	RSAPublicKey k;
	if(!isNull() && isRSA())
		assignToPublic(&k);
	return k;
}

RSAPrivateKey PKey::toRSAPrivateKey() const
{
	RSAPrivateKey k;
	if(!isNull() && isRSA() && isPrivate())
		assignToPrivate(&k);
	return k;
}

DSAPublicKey PKey::toDSAPublicKey() const
{
	DSAPublicKey k;
	if(!isNull() && isDSA())
		assignToPublic(&k);
	return k;
}

DSAPrivateKey PKey::toDSAPrivateKey() const
{
	DSAPrivateKey k;
	if(!isNull() && isDSA() && isPrivate())
		assignToPrivate(&k);
	return k;
}

DHPublicKey PKey::toDHPublicKey() const
{
	DHPublicKey k;
	if(!isNull() && isDH())
		assignToPublic(&k);
	return k;
}

DHPrivateKey PKey::toDHPrivateKey() const
{
	DHPrivateKey k;
	if(!isNull() && isDH() && isPrivate())
		assignToPrivate(&k);
	return k;
}

bool PKey::operator==(const PKey &a) const
{
	if(isNull() || a.isNull() || type() != a.type())
		return false;

	if(a.isPrivate())
		return (toPrivateKey().toDER() == a.toPrivateKey().toDER());
	else
		return (toPublicKey().toDER() == a.toPublicKey().toDER());
}

bool PKey::operator!=(const PKey &a) const
{
	return !(*this == a);
}

//----------------------------------------------------------------------------
// PublicKey
//----------------------------------------------------------------------------
PublicKey::PublicKey()
{
}

PublicKey::PublicKey(const QString &type, const QString &provider)
:PKey(type, provider)
{
}

PublicKey::PublicKey(const PrivateKey &k)
{
	set(k.toPublicKey());
}

PublicKey::PublicKey(const QString &fileName)
{
	*this = fromPEMFile(fileName, 0, QString());
}

PublicKey::PublicKey(const PublicKey &from)
:PKey(from)
{
}

PublicKey::~PublicKey()
{
}

PublicKey & PublicKey::operator=(const PublicKey &from)
{
	PKey::operator=(from);
	return *this;
}

RSAPublicKey PublicKey::toRSA() const
{
	return toRSAPublicKey();
}

DSAPublicKey PublicKey::toDSA() const
{
	return toDSAPublicKey();
}

DHPublicKey PublicKey::toDH() const
{
	return toDHPublicKey();
}

bool PublicKey::canEncrypt() const
{
	return isRSA();
}

bool PublicKey::canVerify() const
{
	return (isRSA() || isDSA());
}

int PublicKey::maximumEncryptSize(EncryptionAlgorithm alg) const
{
	return static_cast<const PKeyContext *>(context())->key()->maximumEncryptSize(alg);
}

SecureArray PublicKey::encrypt(const SecureArray &a, EncryptionAlgorithm alg)
{
	return static_cast<PKeyContext *>(context())->key()->encrypt(a, alg);
}

void PublicKey::startVerify(SignatureAlgorithm alg, SignatureFormat format)
{
	if(isDSA() && format == DefaultFormat)
		format = IEEE_1363;
	static_cast<PKeyContext *>(context())->key()->startVerify(alg, format);
}

void PublicKey::update(const MemoryRegion &a)
{
	static_cast<PKeyContext *>(context())->key()->update(a);
}

bool PublicKey::validSignature(const QByteArray &sig)
{
	return static_cast<PKeyContext *>(context())->key()->endVerify(sig);
}

bool PublicKey::verifyMessage(const MemoryRegion &a, const QByteArray &sig, SignatureAlgorithm alg, SignatureFormat format)
{
	startVerify(alg, format);
	update(a);
	return validSignature(sig);
}

QByteArray PublicKey::toDER() const
{
	QByteArray out;
	const PKeyContext *cur = static_cast<const PKeyContext *>(context());
	Provider *p = providerForIOType(type(), cur);
	if(!p)
		return out;
	if(cur->provider() == p)
	{
		out = cur->publicToDER();
	}
	else
	{
		PKeyContext *pk = static_cast<PKeyContext *>(getContext("pkey", p));
		if(pk->importKey(cur->key()))
			out = pk->publicToDER();
		delete pk;
	}
	return out;
}

QString PublicKey::toPEM() const
{
	QString out;
	const PKeyContext *cur = static_cast<const PKeyContext *>(context());
	Provider *p = providerForIOType(type(), cur);
	if(!p)
		return out;
	if(cur->provider() == p)
	{
		out = cur->publicToPEM();
	}
	else
	{
		PKeyContext *pk = static_cast<PKeyContext *>(getContext("pkey", p));
		if(pk->importKey(cur->key()))
			out = pk->publicToPEM();
		delete pk;
	}
	return out;
}

bool PublicKey::toPEMFile(const QString &fileName) const
{
	return stringToFile(fileName, toPEM());
}

PublicKey PublicKey::fromDER(const QByteArray &a, ConvertResult *result, const QString &provider)
{
	return getKey<PublicKey, Getter_PublicKey<QByteArray>, QByteArray>(provider, a, SecureArray(), result);
}

PublicKey PublicKey::fromPEM(const QString &s, ConvertResult *result, const QString &provider)
{
	return getKey<PublicKey, Getter_PublicKey<QString>, QString>(provider, s, SecureArray(), result);
}

PublicKey PublicKey::fromPEMFile(const QString &fileName, ConvertResult *result, const QString &provider)
{
	QString pem;
	if(!stringFromFile(fileName, &pem))
	{
		if(result)
			*result = ErrorFile;
		return PublicKey();
	}
	return fromPEM(pem, result, provider);
}

//----------------------------------------------------------------------------
// PrivateKey
//----------------------------------------------------------------------------
PrivateKey::PrivateKey()
{
}

PrivateKey::PrivateKey(const QString &type, const QString &provider)
:PKey(type, provider)
{
}

PrivateKey::PrivateKey(const QString &fileName, const SecureArray &passphrase)
{
	*this = fromPEMFile(fileName, passphrase, 0, QString());
}

PrivateKey::PrivateKey(const PrivateKey &from)
:PKey(from)
{
}

PrivateKey::~PrivateKey()
{
}

PrivateKey & PrivateKey::operator=(const PrivateKey &from)
{
	PKey::operator=(from);
	return *this;
}

RSAPrivateKey PrivateKey::toRSA() const
{
	return toRSAPrivateKey();
}

DSAPrivateKey PrivateKey::toDSA() const
{
	return toDSAPrivateKey();
}

DHPrivateKey PrivateKey::toDH() const
{
	return toDHPrivateKey();
}

bool PrivateKey::canDecrypt() const
{
	return isRSA();
}

bool PrivateKey::canSign() const
{
	return (isRSA() || isDSA());
}

bool PrivateKey::decrypt(const SecureArray &in, SecureArray *out, EncryptionAlgorithm alg)
{
	return static_cast<PKeyContext *>(context())->key()->decrypt(in, out, alg);
}

void PrivateKey::startSign(SignatureAlgorithm alg, SignatureFormat format)
{
	if(isDSA() && format == DefaultFormat)
		format = IEEE_1363;
	static_cast<PKeyContext *>(context())->key()->startSign(alg, format);
}

void PrivateKey::update(const MemoryRegion &a)
{
	static_cast<PKeyContext *>(context())->key()->update(a);
}

QByteArray PrivateKey::signature()
{
	return static_cast<PKeyContext *>(context())->key()->endSign();
}

QByteArray PrivateKey::signMessage(const MemoryRegion &a, SignatureAlgorithm alg, SignatureFormat format)
{
	startSign(alg, format);
	update(a);
	return signature();
}

SymmetricKey PrivateKey::deriveKey(const PublicKey &theirs)
{
	const PKeyContext *theirContext = static_cast<const PKeyContext *>(theirs.context());
	return static_cast<PKeyContext *>(context())->key()->deriveKey(*(theirContext->key()));
}

QList<PBEAlgorithm> PrivateKey::supportedPBEAlgorithms(const QString &provider)
{
	return getList<PBEAlgorithm, Getter_PBE>(provider);
}

SecureArray PrivateKey::toDER(const SecureArray &passphrase, PBEAlgorithm pbe) const
{
	SecureArray out;
	if(pbe == PBEDefault)
		pbe = get_pbe_default();
	const PKeyContext *cur = static_cast<const PKeyContext *>(context());
	Provider *p = providerForPBE(pbe, type(), cur);
	if(!p)
		return out;
	if(cur->provider() == p)
	{
		out = cur->privateToDER(passphrase, pbe);
	}
	else
	{
		PKeyContext *pk = static_cast<PKeyContext *>(getContext("pkey", p));
		if(pk->importKey(cur->key()))
			out = pk->privateToDER(passphrase, pbe);
		delete pk;
	}
	return out;
}

QString PrivateKey::toPEM(const SecureArray &passphrase, PBEAlgorithm pbe) const
{
	QString out;
	if(pbe == PBEDefault)
		pbe = get_pbe_default();
	const PKeyContext *cur = static_cast<const PKeyContext *>(context());
	Provider *p = providerForPBE(pbe, type(), cur);
	if(!p)
		return out;
	if(cur->provider() == p)
	{
		out = cur->privateToPEM(passphrase, pbe);
	}
	else
	{
		PKeyContext *pk = static_cast<PKeyContext *>(getContext("pkey", p));
		if(pk->importKey(cur->key()))
			out = pk->privateToPEM(passphrase, pbe);
		delete pk;
	}
	return out;
}

bool PrivateKey::toPEMFile(const QString &fileName, const SecureArray &passphrase, PBEAlgorithm pbe) const
{
	return stringToFile(fileName, toPEM(passphrase, pbe));
}

PrivateKey PrivateKey::fromDER(const SecureArray &a, const SecureArray &passphrase, ConvertResult *result, const QString &provider)
{
	return get_privatekey_der(a, QString(), (void *)&a, passphrase, result, provider);
}

PrivateKey PrivateKey::fromPEM(const QString &s, const SecureArray &passphrase, ConvertResult *result, const QString &provider)
{
	return get_privatekey_pem(s, QString(), (void *)&s, passphrase, result, provider);
}

PrivateKey PrivateKey::fromPEMFile(const QString &fileName, const SecureArray &passphrase, ConvertResult *result, const QString &provider)
{
	QString pem;
	if(!stringFromFile(fileName, &pem))
	{
		if(result)
			*result = ErrorFile;
		return PrivateKey();
	}
	return get_privatekey_pem(pem, fileName, 0, passphrase, result, provider);
}

//----------------------------------------------------------------------------
// KeyGenerator
//----------------------------------------------------------------------------
class KeyGenerator::Private : public QObject
{
	Q_OBJECT
public:
	KeyGenerator *parent;
	bool blocking, wasBlocking;
	PrivateKey key;
	DLGroup group;

	PKeyBase *k;
	PKeyContext *dest;
	DLGroupContext *dc;

	Private(KeyGenerator *_parent) : QObject(_parent), parent(_parent)
	{
		k = 0;
		dest = 0;
		dc = 0;
	}

	~Private()
	{
		delete k;
		delete dest;
		delete dc;
	}

public slots:
	void done()
	{
		if(!k->isNull())
		{
			if(!wasBlocking)
			{
				k->setParent(0);
				k->moveToThread(0);
			}
			dest->setKey(k);
			k = 0;

			key.change(dest);
			dest = 0;
		}
		else
		{
			delete k;
			k = 0;
			delete dest;
			dest = 0;
		}

		if(!wasBlocking)
			emit parent->finished();
	}

	void done_group()
	{
		if(!dc->isNull())
		{
			BigInteger p, q, g;
			dc->getResult(&p, &q, &g);
			group = DLGroup(p, q, g);
		}
		delete dc;
		dc = 0;

		if(!wasBlocking)
			emit parent->finished();
	}
};

KeyGenerator::KeyGenerator(QObject *parent)
:QObject(parent)
{
	d = new Private(this);
	d->blocking = true;
}

KeyGenerator::~KeyGenerator()
{
	delete d;
}

bool KeyGenerator::blockingEnabled() const
{
	return d->blocking;
}

void KeyGenerator::setBlockingEnabled(bool b)
{
	d->blocking = b;
}

bool KeyGenerator::isBusy() const
{
	return (d->k ? true: false);
}

PrivateKey KeyGenerator::createRSA(int bits, int exp, const QString &provider)
{
	if(isBusy())
		return PrivateKey();

	d->key = PrivateKey();
	d->wasBlocking = d->blocking;
	d->k = static_cast<RSAContext *>(getContext("rsa", provider));
	d->dest = static_cast<PKeyContext *>(getContext("pkey", d->k->provider()));

	if(!d->blocking)
	{
		d->k->moveToThread(thread());
		d->k->setParent(d);
		connect(d->k, SIGNAL(finished()), d, SLOT(done()));
		static_cast<RSAContext *>(d->k)->createPrivate(bits, exp, false);
	}
	else
	{
		static_cast<RSAContext *>(d->k)->createPrivate(bits, exp, true);
		d->done();
	}

	return d->key;
}

PrivateKey KeyGenerator::createDSA(const DLGroup &domain, const QString &provider)
{
	if(isBusy())
		return PrivateKey();

	d->key = PrivateKey();
	d->wasBlocking = d->blocking;
	d->k = static_cast<DSAContext *>(getContext("dsa", provider));
	d->dest = static_cast<PKeyContext *>(getContext("pkey", d->k->provider()));

	if(!d->blocking)
	{
		d->k->moveToThread(thread());
		d->k->setParent(d);
		connect(d->k, SIGNAL(finished()), d, SLOT(done()));
		static_cast<DSAContext *>(d->k)->createPrivate(domain, false);
	}
	else
	{
		static_cast<DSAContext *>(d->k)->createPrivate(domain, true);
		d->done();
	}

	return d->key;
}

PrivateKey KeyGenerator::createDH(const DLGroup &domain, const QString &provider)
{
	if(isBusy())
		return PrivateKey();

	d->key = PrivateKey();
	d->wasBlocking = d->blocking;
	d->k = static_cast<DHContext *>(getContext("dh", provider));
	d->dest = static_cast<PKeyContext *>(getContext("pkey", d->k->provider()));

	if(!d->blocking)
	{
		d->k->moveToThread(thread());
		d->k->setParent(d);
		connect(d->k, SIGNAL(finished()), d, SLOT(done()));
		static_cast<DHContext *>(d->k)->createPrivate(domain, false);
	}
	else
	{
		static_cast<DHContext *>(d->k)->createPrivate(domain, true);
		d->done();
	}

	return d->key;
}

PrivateKey KeyGenerator::key() const
{
	return d->key;
}

DLGroup KeyGenerator::createDLGroup(QCA::DLGroupSet set, const QString &provider)
{
	if(isBusy())
		return DLGroup();

	Provider *p;
	if(!provider.isEmpty())
		p = providerForName(provider);
	else
		p = providerForGroupSet(set);

	d->group = DLGroup();
	d->wasBlocking = d->blocking;
	d->dc = static_cast<DLGroupContext *>(getContext("dlgroup", p));

	if(!d->blocking)
	{
		connect(d->dc, SIGNAL(finished()), d, SLOT(done_group()));
		d->dc->fetchGroup(set, false);
	}
	else
	{
		d->dc->fetchGroup(set, true);
		d->done_group();
	}

	return d->group;
}

DLGroup KeyGenerator::dlGroup() const
{
	return d->group;
}

//----------------------------------------------------------------------------
// RSAPublicKey
//----------------------------------------------------------------------------
RSAPublicKey::RSAPublicKey()
{
}

RSAPublicKey::RSAPublicKey(const BigInteger &n, const BigInteger &e, const QString &provider)
{
	RSAContext *k = static_cast<RSAContext *>(getContext("rsa", provider));
	k->createPublic(n, e);
	PKeyContext *c = static_cast<PKeyContext *>(getContext("pkey", k->provider()));
	c->setKey(k);
	change(c);
}

RSAPublicKey::RSAPublicKey(const RSAPrivateKey &k)
:PublicKey(k)
{
}

BigInteger RSAPublicKey::n() const
{
	return static_cast<const RSAContext *>(static_cast<const PKeyContext *>(context())->key())->n();
}

BigInteger RSAPublicKey::e() const
{
	return static_cast<const RSAContext *>(static_cast<const PKeyContext *>(context())->key())->e();
}

//----------------------------------------------------------------------------
// RSAPrivateKey
//----------------------------------------------------------------------------
RSAPrivateKey::RSAPrivateKey()
{
}

RSAPrivateKey::RSAPrivateKey(const BigInteger &n, const BigInteger &e, const BigInteger &p, const BigInteger &q, const BigInteger &d, const QString &provider)
{
	RSAContext *k = static_cast<RSAContext *>(getContext("rsa", provider));
	k->createPrivate(n, e, p, q, d);
	PKeyContext *c = static_cast<PKeyContext *>(getContext("pkey", k->provider()));
	c->setKey(k);
	change(c);
}

BigInteger RSAPrivateKey::n() const
{
	return static_cast<const RSAContext *>(static_cast<const PKeyContext *>(context())->key())->n();
}

BigInteger RSAPrivateKey::e() const
{
	return static_cast<const RSAContext *>(static_cast<const PKeyContext *>(context())->key())->e();
}

BigInteger RSAPrivateKey::p() const
{
	return static_cast<const RSAContext *>(static_cast<const PKeyContext *>(context())->key())->p();
}

BigInteger RSAPrivateKey::q() const
{
	return static_cast<const RSAContext *>(static_cast<const PKeyContext *>(context())->key())->q();
}

BigInteger RSAPrivateKey::d() const
{
	return static_cast<const RSAContext *>(static_cast<const PKeyContext *>(context())->key())->d();
}

//----------------------------------------------------------------------------
// DSAPublicKey
//----------------------------------------------------------------------------
DSAPublicKey::DSAPublicKey()
{
}

DSAPublicKey::DSAPublicKey(const DLGroup &domain, const BigInteger &y, const QString &provider)
{
	DSAContext *k = static_cast<DSAContext *>(getContext("dsa", provider));
	k->createPublic(domain, y);
	PKeyContext *c = static_cast<PKeyContext *>(getContext("pkey", k->provider()));
	c->setKey(k);
	change(c);
}

DSAPublicKey::DSAPublicKey(const DSAPrivateKey &k)
:PublicKey(k)
{
}

DLGroup DSAPublicKey::domain() const
{
	return static_cast<const DSAContext *>(static_cast<const PKeyContext *>(context())->key())->domain();
}

BigInteger DSAPublicKey::y() const
{
	return static_cast<const DSAContext *>(static_cast<const PKeyContext *>(context())->key())->y();
}

//----------------------------------------------------------------------------
// DSAPrivateKey
//----------------------------------------------------------------------------
DSAPrivateKey::DSAPrivateKey()
{
}

DSAPrivateKey::DSAPrivateKey(const DLGroup &domain, const BigInteger &y, const BigInteger &x, const QString &provider)
{
	DSAContext *k = static_cast<DSAContext *>(getContext("dsa", provider));
	k->createPrivate(domain, y, x);
	PKeyContext *c = static_cast<PKeyContext *>(getContext("pkey", k->provider()));
	c->setKey(k);
	change(c);
}

DLGroup DSAPrivateKey::domain() const
{
	return static_cast<const DSAContext *>(static_cast<const PKeyContext *>(context())->key())->domain();
}

BigInteger DSAPrivateKey::y() const
{
	return static_cast<const DSAContext *>(static_cast<const PKeyContext *>(context())->key())->y();
}

BigInteger DSAPrivateKey::x() const
{
	return static_cast<const DSAContext *>(static_cast<const PKeyContext *>(context())->key())->x();
}

//----------------------------------------------------------------------------
// DHPublicKey
//----------------------------------------------------------------------------
DHPublicKey::DHPublicKey()
{
}

DHPublicKey::DHPublicKey(const DLGroup &domain, const BigInteger &y, const QString &provider)
{
	DHContext *k = static_cast<DHContext *>(getContext("dh", provider));
	k->createPublic(domain, y);
	PKeyContext *c = static_cast<PKeyContext *>(getContext("pkey", k->provider()));
	c->setKey(k);
	change(c);
}

DHPublicKey::DHPublicKey(const DHPrivateKey &k)
:PublicKey(k)
{
}

DLGroup DHPublicKey::domain() const
{
	return static_cast<const DHContext *>(static_cast<const PKeyContext *>(context())->key())->domain();
}

BigInteger DHPublicKey::y() const
{
	return static_cast<const DHContext *>(static_cast<const PKeyContext *>(context())->key())->y();
}

//----------------------------------------------------------------------------
// DHPrivateKey
//----------------------------------------------------------------------------
DHPrivateKey::DHPrivateKey()
{
}

DHPrivateKey::DHPrivateKey(const DLGroup &domain, const BigInteger &y, const BigInteger &x, const QString &provider)
{
	DHContext *k = static_cast<DHContext *>(getContext("dh", provider));
	k->createPrivate(domain, y, x);
	PKeyContext *c = static_cast<PKeyContext *>(getContext("pkey", k->provider()));
	c->setKey(k);
	change(c);
}

DLGroup DHPrivateKey::domain() const
{
	return static_cast<const DHContext *>(static_cast<const PKeyContext *>(context())->key())->domain();
}

BigInteger DHPrivateKey::y() const
{
	return static_cast<const DHContext *>(static_cast<const PKeyContext *>(context())->key())->y();
}

BigInteger DHPrivateKey::x() const
{
	return static_cast<const DHContext *>(static_cast<const PKeyContext *>(context())->key())->x();
}

}

#include "qca_publickey.moc"
