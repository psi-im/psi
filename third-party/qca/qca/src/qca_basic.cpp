/*
 * Copyright (C) 2003-2007  Justin Karneges <justin@affinix.com>
 * Copyright (C) 2004,2005,2007  Brad Hards <bradh@frogmouth.net>
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

#include "qca_basic.h"

#include "qcaprovider.h"

#include <QMutexLocker>
#include <QtGlobal>

namespace QCA {

// from qca_core.cpp
QMutex *global_random_mutex();
Random *global_random();
Provider::Context *getContext(const QString &type, Provider *p);

// from qca_publickey.cpp
ProviderList allProviders();
Provider *providerForName(const QString &name);

static void mergeList(QStringList *a, const QStringList &b)
{
	foreach(const QString &s, b)
	{
		if(!a->contains(s))
			a->append(s);
	}
}

static QStringList get_hash_types(Provider *p)
{
	QStringList out;
	InfoContext *c = static_cast<InfoContext *>(getContext("info", p));
	if(!c)
		return out;
	out = c->supportedHashTypes();
	delete c;
	return out;
}

static QStringList get_cipher_types(Provider *p)
{
	QStringList out;
	InfoContext *c = static_cast<InfoContext *>(getContext("info", p));
	if(!c)
		return out;
	out = c->supportedCipherTypes();
	delete c;
	return out;
}

static QStringList get_mac_types(Provider *p)
{
	QStringList out;
	InfoContext *c = static_cast<InfoContext *>(getContext("info", p));
	if(!c)
		return out;
	out = c->supportedMACTypes();
	delete c;
	return out;
}

static QStringList get_types(QStringList (*get_func)(Provider *p), const QString &provider)
{
	QStringList out;
	if(!provider.isEmpty())
	{
		Provider *p = providerForName(provider);
		if(p)
			out = get_func(p);
	}
	else
	{
		ProviderList pl = allProviders();
		foreach(Provider *p, pl)
			mergeList(&out, get_func(p));
	}
	return out;
}

static QStringList supportedHashTypes(const QString &provider)
{
	return get_types(get_hash_types, provider);
}

static QStringList supportedCipherTypes(const QString &provider)
{
	return get_types(get_cipher_types, provider);
}

static QStringList supportedMACTypes(const QString &provider)
{
	return get_types(get_mac_types, provider);
}

//----------------------------------------------------------------------------
// Random
//----------------------------------------------------------------------------
Random::Random(const QString &provider)
:Algorithm("random", provider)
{
}

Random::Random(const Random &from)
:Algorithm(from)
{
}

Random::~Random()
{
}

Random & Random::operator=(const Random &from)
{
	Algorithm::operator=(from);
	return *this;
}

uchar Random::nextByte()
{
	return (uchar)(nextBytes(1)[0]);
}

SecureArray Random::nextBytes(int size)
{
	return static_cast<RandomContext *>(context())->nextBytes(size);
}

uchar Random::randomChar()
{
	QMutexLocker locker(global_random_mutex());
	return global_random()->nextByte();
}

int Random::randomInt()
{
	QMutexLocker locker(global_random_mutex());
	SecureArray a = global_random()->nextBytes(sizeof(int));
	int x;
	memcpy(&x, a.data(), a.size());
	return x;
}

SecureArray Random::randomArray(int size)
{
	QMutexLocker locker(global_random_mutex());
	return global_random()->nextBytes(size);
}

//----------------------------------------------------------------------------
// Hash
//----------------------------------------------------------------------------
Hash::Hash(const QString &type, const QString &provider)
:Algorithm(type, provider)
{
}

Hash::Hash(const Hash &from)
:Algorithm(from), BufferedComputation(from)
{
}

Hash::~Hash()
{
}

Hash & Hash::operator=(const Hash &from)
{
	Algorithm::operator=(from);
	return *this;
}

QStringList Hash::supportedTypes(const QString &provider)
{
	return supportedHashTypes(provider);
}

QString Hash::type() const
{
	// algorithm type is the same as the hash type
	return Algorithm::type();
}

void Hash::clear()
{
	static_cast<HashContext *>(context())->clear();
}

void Hash::update(const MemoryRegion &a)
{
	static_cast<HashContext *>(context())->update(a);
}

void Hash::update(const QByteArray &a)
{
	update(MemoryRegion(a));
}

void Hash::update(const char *data, int len)
{
	if(len < 0)
		len = qstrlen(data);
	if(len == 0)
		return;

	update(MemoryRegion(QByteArray::fromRawData(data, len)));
}

// Reworked from KMD5, from KDE's kdelibs
void Hash::update(QIODevice *file)
{
	char buffer[1024];
	int len;

	while ((len=file->read(reinterpret_cast<char*>(buffer), sizeof(buffer))) > 0)
		update(buffer, len);
}

MemoryRegion Hash::final()
{
	return static_cast<HashContext *>(context())->final();
}

MemoryRegion Hash::hash(const MemoryRegion &a)
{
	return process(a);
}

QString Hash::hashToString(const MemoryRegion &a)
{
	return arrayToHex(hash(a).toByteArray());
}

//----------------------------------------------------------------------------
// Cipher
//----------------------------------------------------------------------------
class Cipher::Private
{
public:
	QString type;
	Cipher::Mode mode;
	Cipher::Padding pad;
	Direction dir;
	SymmetricKey key;
	InitializationVector iv;

	bool ok, done;
};

Cipher::Cipher(const QString &type, Mode mode, Padding pad,
	Direction dir, const SymmetricKey &key,
	const InitializationVector &iv,
	const QString &provider)
:Algorithm(withAlgorithms(type, mode, pad), provider)
{
	d = new Private;
	d->type = type;
	d->mode = mode;
	d->pad = pad;
	if(!key.isEmpty())
		setup(dir, key, iv);
}

Cipher::Cipher(const Cipher &from)
:Algorithm(from), Filter(from)
{
	d = new Private(*from.d);
}

Cipher::~Cipher()
{
	delete d;
}

Cipher & Cipher::operator=(const Cipher &from)
{
	Algorithm::operator=(from);
	*d = *from.d;
	return *this;
}

QStringList Cipher::supportedTypes(const QString &provider)
{
	return supportedCipherTypes(provider);
}

QString Cipher::type() const
{
	return d->type;
}

Cipher::Mode Cipher::mode() const
{
	return d->mode;
}

Cipher::Padding Cipher::padding() const
{
	return d->pad;
}

Direction Cipher::direction() const
{
	return d->dir;
}

KeyLength Cipher::keyLength() const
{
	return static_cast<const CipherContext *>(context())->keyLength();
}

bool Cipher::validKeyLength(int n) const
{
	KeyLength len = keyLength();
	return ((n >= len.minimum()) && (n <= len.maximum()) && (n % len.multiple() == 0));
}

int Cipher::blockSize() const
{
	return static_cast<const CipherContext *>(context())->blockSize();
}

void Cipher::clear()
{
	d->done = false;
	static_cast<CipherContext *>(context())->setup(d->dir, d->key, d->iv);
}

MemoryRegion Cipher::update(const MemoryRegion &a)
{
	SecureArray out;
	if(d->done)
		return out;
	d->ok = static_cast<CipherContext *>(context())->update(a, &out);
	return out;
}

MemoryRegion Cipher::final()
{
	SecureArray out;
	if(d->done)
		return out;
	d->done = true;
	d->ok = static_cast<CipherContext *>(context())->final(&out);
	return out;
}

bool Cipher::ok() const
{
	return d->ok;
}

void Cipher::setup(Direction dir, const SymmetricKey &key, const InitializationVector &iv)
{
	d->dir = dir;
	d->key = key;
	d->iv = iv;
	clear();
}

QString Cipher::withAlgorithms(const QString &cipherType, Mode modeType, Padding paddingType)
{
	QString mode;
	switch(modeType) {
	case CBC:
		mode = "cbc";
		break;
	case CFB:
		mode = "cfb";
		break;
	case OFB:
		mode = "ofb";
		break;
	case ECB:
		mode = "ecb";
		break;
	default:
		Q_ASSERT(0);
	}

	// do the default
	if(paddingType == DefaultPadding)
	{
		// logic from Botan
		if(modeType == CBC)
			paddingType = PKCS7;
		else
			paddingType = NoPadding;
	}

	QString pad;
	if(paddingType == NoPadding)
		pad = "";
	else
		pad = "pkcs7";

	QString result = cipherType + '-' + mode;
	if(!pad.isEmpty())
		result += QString("-") + pad;

	return result;
}

//----------------------------------------------------------------------------
// MessageAuthenticationCode
//----------------------------------------------------------------------------
class MessageAuthenticationCode::Private
{
public:
	SymmetricKey key;

	bool done;
	MemoryRegion buf;
};


MessageAuthenticationCode::MessageAuthenticationCode(const QString &type,
	const SymmetricKey &key,
	const QString &provider)
:Algorithm(type, provider)
{
	d = new Private;
	setup(key);
}

MessageAuthenticationCode::MessageAuthenticationCode(const MessageAuthenticationCode &from)
:Algorithm(from), BufferedComputation(from)
{
	d = new Private(*from.d);
}

MessageAuthenticationCode::~MessageAuthenticationCode()
{
	delete d;
}

MessageAuthenticationCode & MessageAuthenticationCode::operator=(const MessageAuthenticationCode &from)
{
	Algorithm::operator=(from);
	*d = *from.d;
	return *this;
}

QStringList MessageAuthenticationCode::supportedTypes(const QString &provider)
{
	return supportedMACTypes(provider);
}

QString MessageAuthenticationCode::type() const
{
	// algorithm type is the same as the mac type
	return Algorithm::type();
}

KeyLength MessageAuthenticationCode::keyLength() const
{
	return static_cast<const MACContext *>(context())->keyLength();
}

bool MessageAuthenticationCode::validKeyLength(int n) const
{
	KeyLength len = keyLength();
	return ((n >= len.minimum()) && (n <= len.maximum()) && (n % len.multiple() == 0));
}

void MessageAuthenticationCode::clear()
{
	d->done = false;
	static_cast<MACContext *>(context())->setup(d->key);
}

void MessageAuthenticationCode::update(const MemoryRegion &a)
{
	if(d->done)
		return;
	static_cast<MACContext *>(context())->update(a);
}

MemoryRegion MessageAuthenticationCode::final()
{
	if(!d->done)
	{
		d->done = true;
		static_cast<MACContext *>(context())->final(&d->buf);
	}
	return d->buf;
}

void MessageAuthenticationCode::setup(const SymmetricKey &key)
{
	d->key = key;
	clear();
}

//----------------------------------------------------------------------------
// Key Derivation Function
//----------------------------------------------------------------------------
KeyDerivationFunction::KeyDerivationFunction(const QString &type, const QString &provider)
:Algorithm(type, provider)
{
}

KeyDerivationFunction::KeyDerivationFunction(const KeyDerivationFunction &from)
:Algorithm(from)
{
}

KeyDerivationFunction::~KeyDerivationFunction()
{
}

KeyDerivationFunction & KeyDerivationFunction::operator=(const KeyDerivationFunction &from)
{
	Algorithm::operator=(from);
	return *this;
}

SymmetricKey KeyDerivationFunction::makeKey(const SecureArray &secret, const InitializationVector &salt, unsigned int keyLength, unsigned int iterationCount)
{
	return static_cast<KDFContext *>(context())->makeKey(secret, salt, keyLength, iterationCount);
}

QString KeyDerivationFunction::withAlgorithm(const QString &kdfType, const QString &algType)
{
	return (kdfType + '(' + algType + ')');
}

}
