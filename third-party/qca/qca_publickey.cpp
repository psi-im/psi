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

#include "qca_publickey.h"

#include <QtCore>
#include "qcaprovider.h"

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

bool ask_passphrase(const QString &fname, void *ptr, QSecureArray *answer)
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
	static ConvertResult fromData(PKeyContext *c, const QSecureArray &in)
	{
		return c->publicFromDER(in);
	}

	// PEM
	static ConvertResult fromData(PKeyContext *c, const QString &in)
	{
		return c->publicFromPEM(in);
	}

	static PublicKey getKey(Provider *p, const I &in, const QSecureArray &, ConvertResult *result)
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
		return k;
	}
};

template <typename I>
class Getter_PrivateKey
{
public:
	// DER
	static ConvertResult fromData(PKeyContext *c, const QSecureArray &in, const QSecureArray &passphrase)
	{
		return c->privateFromDER(in, passphrase);
	}

	// PEM
	static ConvertResult fromData(PKeyContext *c, const QString &in, const QSecureArray &passphrase)
	{
		return c->privateFromPEM(in, passphrase);
	}

	static PrivateKey getKey(Provider *p, const I &in, const QSecureArray &passphrase, ConvertResult *result)
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
		return k;
	}
};

Provider *providerForPBE(PBEAlgorithm alg)
{
	ProviderList pl = allProviders();
	for(int n = 0; n < pl.count(); ++n)
	{
		if(Getter_PBE::getList(pl[n]).contains(alg))
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
			list += G::getList(pl[n]);
	}

	return list;
}

template <typename T, typename G, typename I>
T getKey(const QString &provider, const I &in, const QSecureArray &passphrase, ConvertResult *result)
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

//----------------------------------------------------------------------------
// DLGroup
//----------------------------------------------------------------------------
class DLGroup::Private
{
public:
	QBigInteger p, q, g;

	Private(const QBigInteger &p1, const QBigInteger &q1, const QBigInteger &g1)
	:p(p1), q(q1), g(g1)
	{
	}
};

DLGroup::DLGroup()
{
	d = 0;
}

DLGroup::DLGroup(const QBigInteger &p, const QBigInteger &q, const QBigInteger &g)
{
	d = new Private(p, q, g);
}

DLGroup::DLGroup(const QBigInteger &p, const QBigInteger &g)
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
	//DLGroupContext *c = static_cast<DLGroupContext *>(getContext("dlgroup", provider));
	//QList<DLGroupSet> list = c->supportedGroupSets();
	//delete c;
	//return list;
}

bool DLGroup::isNull() const
{
	return (d ? false: true);
}

QBigInteger DLGroup::p() const
{
	return d->p;
}

QBigInteger DLGroup::q() const
{
	return d->q;
}

QBigInteger DLGroup::g() const
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
	//const PKeyContext *c = static_cast<const PKeyContext *>(getContext("pkey", provider));
	//QList<Type> list = c->supportedTypes();
	//delete c;
	//return list;
}

QList<PKey::Type> PKey::supportedIOTypes(const QString &provider)
{
	return getList<Type, Getter_IOType>(provider);
	//const PKeyContext *c = static_cast<const PKeyContext *>(getContext("pkey", provider));
	//QList<Type> list = c->supportedIOTypes();
	//delete c;
	//return list;
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

QSecureArray PublicKey::encrypt(const QSecureArray &a, EncryptionAlgorithm alg)
{
	return static_cast<PKeyContext *>(context())->key()->encrypt(a, alg);
}

void PublicKey::startVerify(SignatureAlgorithm alg, SignatureFormat format)
{
	if(isDSA() && format == DefaultFormat)
		format = IEEE_1363;
	static_cast<PKeyContext *>(context())->key()->startVerify(alg, format);
}

void PublicKey::update(const QSecureArray &a)
{
	static_cast<PKeyContext *>(context())->key()->update(a);
}

bool PublicKey::validSignature(const QSecureArray &sig)
{
	return static_cast<PKeyContext *>(context())->key()->endVerify(sig);
}

bool PublicKey::verifyMessage(const QSecureArray &a, const QSecureArray &sig, SignatureAlgorithm alg, SignatureFormat format)
{
	startVerify(alg, format);
	update(a);
	return validSignature(sig);
}

QSecureArray PublicKey::toDER() const
{
	return static_cast<const PKeyContext *>(context())->publicToDER();
}

QString PublicKey::toPEM() const
{
	return static_cast<const PKeyContext *>(context())->publicToPEM();
}

bool PublicKey::toPEMFile(const QString &fileName) const
{
	return stringToFile(fileName, toPEM());
}

PublicKey PublicKey::fromDER(const QSecureArray &a, ConvertResult *result, const QString &provider)
{
	return getKey<PublicKey, Getter_PublicKey<QSecureArray>, QSecureArray>(provider, a, QSecureArray(), result);

	/*PublicKey k;
	PKeyContext *c = static_cast<PKeyContext *>(getContext("pkey", provider));
	ConvertResult r = c->publicFromDER(a);
	if(result)
		*result = r;
	if(r == ConvertGood)
		k.change(c);
	return k;*/
}

PublicKey PublicKey::fromPEM(const QString &s, ConvertResult *result, const QString &provider)
{
	return getKey<PublicKey, Getter_PublicKey<QString>, QString>(provider, s, QSecureArray(), result);

	/*PublicKey k;
	PKeyContext *c = static_cast<PKeyContext *>(getContext("pkey", provider));
	ConvertResult r = c->publicFromPEM(s);
	if(result)
		*result = r;
	if(r == ConvertGood)
		k.change(c);
	return k;*/
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

PrivateKey::PrivateKey(const QString &fileName, const QSecureArray &passphrase)
{
	*this = fromPEMFile(fileName, passphrase, 0, QString());
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

bool PrivateKey::decrypt(const QSecureArray &in, QSecureArray *out, EncryptionAlgorithm alg)
{
	return static_cast<PKeyContext *>(context())->key()->decrypt(in, out, alg);
}

void PrivateKey::startSign(SignatureAlgorithm alg, SignatureFormat format)
{
	if(isDSA() && format == DefaultFormat)
		format = IEEE_1363;
	static_cast<PKeyContext *>(context())->key()->startSign(alg, format);
}

void PrivateKey::update(const QSecureArray &a)
{
	static_cast<PKeyContext *>(context())->key()->update(a);
}

QSecureArray PrivateKey::signature()
{
	return static_cast<PKeyContext *>(context())->key()->endSign();
}

QSecureArray PrivateKey::signMessage(const QSecureArray &a, SignatureAlgorithm alg, SignatureFormat format)
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

	//QList<PBEAlgorithm> list = getList<PBEAlgorithm, Getter_PBE>(provider);

	// single
	/*if(!provider.isEmpty())
	{
		Provider *p = providerForName(provider);
		if(p)
			list = getPBEList(p);
	}
	// all
	else
	{
		ProviderList pl = allProviders();
		for(int n = 0; n < pl.count(); ++n)
			list += getPBEList(pl[n]);
	}*/

	//return list;
}

QSecureArray PrivateKey::toDER(const QSecureArray &passphrase, PBEAlgorithm pbe) const
{
	QSecureArray out;
	if(pbe == PBEDefault)
		pbe = get_pbe_default();
	Provider *p = providerForPBE(pbe);
	if(!p)
		return out;
	const PKeyContext *cur = static_cast<const PKeyContext *>(context());
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

QString PrivateKey::toPEM(const QSecureArray &passphrase, PBEAlgorithm pbe) const
{
	//return static_cast<const PKeyContext *>(context())->privateToPEM(passphrase, pbe);

	QString out;
	if(pbe == PBEDefault)
		pbe = get_pbe_default();
	Provider *p = providerForPBE(pbe);
	if(!p)
		return out;
	const PKeyContext *cur = static_cast<const PKeyContext *>(context());
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

bool PrivateKey::toPEMFile(const QString &fileName, const QSecureArray &passphrase, PBEAlgorithm pbe) const
{
	return stringToFile(fileName, toPEM(passphrase, pbe));
}

PrivateKey PrivateKey::fromDER(const QSecureArray &a, const QSecureArray &passphrase, ConvertResult *result, const QString &provider)
{
	PrivateKey out;
	ConvertResult r;
	out = getKey<PrivateKey, Getter_PrivateKey<QSecureArray>, QSecureArray>(provider, a, passphrase, &r);

	// error converting without passphrase?  maybe a passphrase is needed
	// FIXME: we should only do this if we get ErrorPassphrase?
	if(r != ConvertGood && passphrase.isEmpty())
	{
		QSecureArray pass;
		if(ask_passphrase(QString(), 0, &pass))
			out = getKey<PrivateKey, Getter_PrivateKey<QSecureArray>, QSecureArray>(provider, a, pass, &r);
	}
	if(result)
		*result = r;
	return out;

	/*PrivateKey k;

	// single
	if(!provider.isEmpty())
	{
		Provider *p = providerForName(provider);
		if(!p)
			return k;
		PKeyContext *c = static_cast<PKeyContext *>(p->createContext("pkey"));
		if(!c)
			return k;
		ConvertResult r = c->privateFromDER(a, passphrase);
		if(result)
			*result = r;
		if(r == ConvertGood)
			k.change(c);
	}
	// all
	else
	{
		ProviderList pl = allProviders();
		for(int n = 0; n < pl.count(); ++n)
		{
			PKeyContext *c = static_cast<PKeyContext *>(pl[n]->createContext("pkey"));
			if(!c)
				continue;
			ConvertResult r = c->privateFromDER(a, passphrase);
			if(result)
				*result = r;
			if(r == ConvertGood)
				k.change(c);
			if(!k.isNull())
				break;
		}
	}
	return k;*/
}

PrivateKey PrivateKey::fromPEM(const QString &s, const QSecureArray &passphrase, ConvertResult *result, const QString &provider)
{
	PrivateKey out;
	ConvertResult r;
	out = getKey<PrivateKey, Getter_PrivateKey<QString>, QString>(provider, s, passphrase, &r);

	// error converting without passphrase?  maybe a passphrase is needed
	// FIXME: we should only do this if we get ErrorPassphrase?
	if(r != ConvertGood && passphrase.isEmpty())
	{
		QSecureArray pass;
		if(ask_passphrase(QString(), 0, &pass))
			out = getKey<PrivateKey, Getter_PrivateKey<QString>, QString>(provider, s, pass, &r);
	}
	if(result)
		*result = r;
	return out;

	/*PrivateKey k;
	PKeyContext *c = static_cast<PKeyContext *>(getContext("pkey", provider));
	ConvertResult r = c->privateFromPEM(s, passphrase);
	if(result)
		*result = r;
	if(r == ConvertGood)
		k.change(c);
	return k;*/
}

PrivateKey PrivateKey::fromPEMFile(const QString &fileName, const QSecureArray &passphrase, ConvertResult *result, const QString &provider)
{
	QString pem;
	if(!stringFromFile(fileName, &pem))
	{
		if(result)
			*result = ErrorFile;
		return PrivateKey();
	}
	return fromPEM(pem, passphrase, result, provider);
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
			QBigInteger p, q, g;
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

bool KeyGenerator::blocking() const
{
	return d->blocking;
}

void KeyGenerator::setBlocking(bool b)
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

	d->group = DLGroup();
	d->wasBlocking = d->blocking;
	d->dc = static_cast<DLGroupContext *>(getContext("dlgroup", provider));

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

RSAPublicKey::RSAPublicKey(const QBigInteger &n, const QBigInteger &e, const QString &provider)
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

QBigInteger RSAPublicKey::n() const
{
	return static_cast<const RSAContext *>(static_cast<const PKeyContext *>(context())->key())->n();
}

QBigInteger RSAPublicKey::e() const
{
	return static_cast<const RSAContext *>(static_cast<const PKeyContext *>(context())->key())->e();
}

//----------------------------------------------------------------------------
// RSAPrivateKey
//----------------------------------------------------------------------------
RSAPrivateKey::RSAPrivateKey()
{
}

RSAPrivateKey::RSAPrivateKey(const QBigInteger &n, const QBigInteger &e, const QBigInteger &p, const QBigInteger &q, const QBigInteger &d, const QString &provider)
{
	RSAContext *k = static_cast<RSAContext *>(getContext("rsa", provider));
	k->createPrivate(n, e, p, q, d);
	PKeyContext *c = static_cast<PKeyContext *>(getContext("pkey", k->provider()));
	c->setKey(k);
	change(c);
}

QBigInteger RSAPrivateKey::n() const
{
	return static_cast<const RSAContext *>(static_cast<const PKeyContext *>(context())->key())->n();
}

QBigInteger RSAPrivateKey::e() const
{
	return static_cast<const RSAContext *>(static_cast<const PKeyContext *>(context())->key())->e();
}

QBigInteger RSAPrivateKey::p() const
{
	return static_cast<const RSAContext *>(static_cast<const PKeyContext *>(context())->key())->p();
}

QBigInteger RSAPrivateKey::q() const
{
	return static_cast<const RSAContext *>(static_cast<const PKeyContext *>(context())->key())->q();
}

QBigInteger RSAPrivateKey::d() const
{
	return static_cast<const RSAContext *>(static_cast<const PKeyContext *>(context())->key())->d();
}

//----------------------------------------------------------------------------
// DSAPublicKey
//----------------------------------------------------------------------------
DSAPublicKey::DSAPublicKey()
{
}

DSAPublicKey::DSAPublicKey(const DLGroup &domain, const QBigInteger &y, const QString &provider)
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

QBigInteger DSAPublicKey::y() const
{
	return static_cast<const DSAContext *>(static_cast<const PKeyContext *>(context())->key())->y();
}

//----------------------------------------------------------------------------
// DSAPrivateKey
//----------------------------------------------------------------------------
DSAPrivateKey::DSAPrivateKey()
{
}

DSAPrivateKey::DSAPrivateKey(const DLGroup &domain, const QBigInteger &y, const QBigInteger &x, const QString &provider)
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

QBigInteger DSAPrivateKey::y() const
{
	return static_cast<const DSAContext *>(static_cast<const PKeyContext *>(context())->key())->y();
}

QBigInteger DSAPrivateKey::x() const
{
	return static_cast<const DSAContext *>(static_cast<const PKeyContext *>(context())->key())->x();
}

//----------------------------------------------------------------------------
// DHPublicKey
//----------------------------------------------------------------------------
DHPublicKey::DHPublicKey()
{
}

DHPublicKey::DHPublicKey(const DLGroup &domain, const QBigInteger &y, const QString &provider)
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

QBigInteger DHPublicKey::y() const
{
	return static_cast<const DHContext *>(static_cast<const PKeyContext *>(context())->key())->y();
}

//----------------------------------------------------------------------------
// DHPrivateKey
//----------------------------------------------------------------------------
DHPrivateKey::DHPrivateKey()
{
}

DHPrivateKey::DHPrivateKey(const DLGroup &domain, const QBigInteger &y, const QBigInteger &x, const QString &provider)
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

QBigInteger DHPrivateKey::y() const
{
	return static_cast<const DHContext *>(static_cast<const PKeyContext *>(context())->key())->y();
}

QBigInteger DHPrivateKey::x() const
{
	return static_cast<const DHContext *>(static_cast<const PKeyContext *>(context())->key())->x();
}

}

#include "qca_publickey.moc"
