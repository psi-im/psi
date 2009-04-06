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

#include "qca_tools.h"

#include "qdebug.h"

#ifdef Q_OS_UNIX
# include <stdlib.h>
# include <sys/mman.h>
#endif
#include "botantools/botantools.h"

namespace QCA {

static bool can_lock()
{
#ifdef Q_OS_UNIX
	bool ok = false;
#ifdef MLOCK_NOT_VOID_PTR
# define MLOCK_TYPE char *
# define MLOCK_TYPE_CAST (MLOCK_TYPE)
#else
# define MLOCK_TYPE void *
# define MLOCK_TYPE_CAST
#endif

	MLOCK_TYPE d = MLOCK_TYPE_CAST malloc(256);
	if(mlock(d, 256) == 0)
	{
		munlock(d, 256);
		ok = true;
	}
	free(d);
	return ok;
#else
	return true;
#endif
}

// Botan shouldn't throw any exceptions in our init/deinit.

static Botan::Allocator *alloc = 0;

void botan_throw_abort()
{
	fprintf(stderr, "QCA: Exception from internal Botan\n");
	abort();
}

bool botan_init(int prealloc, bool mmap)
{
	// 64k minimum
	if(prealloc < 64)
		prealloc = 64;

	bool secmem = false;

	try
	{
		Botan::Builtin_Modules modules;
		Botan::Library_State *libstate = new Botan::Library_State(modules.mutex_factory());
		libstate->prealloc_size = prealloc * 1024;
		Botan::set_global_state(libstate);
		Botan::global_state().load(modules);

		if(can_lock())
		{
			Botan::global_state().set_default_allocator("locking");
			secmem = true;
		}
		else if(mmap)
		{
			Botan::global_state().set_default_allocator("mmap");
			secmem = true;
		}
		alloc = Botan::Allocator::get(true);
	}
	catch(std::exception &)
	{
		fprintf(stderr, "QCA: Error initializing internal Botan\n");
		abort();
	}

	return secmem;
}

void botan_deinit()
{
	try
	{
		alloc = 0;
		Botan::set_global_state(0);
	}
	catch(std::exception &)
	{
		botan_throw_abort();
	}
}

void *botan_secure_alloc(int bytes)
{
	try
	{
		return alloc->allocate((Botan::u32bit)bytes);
	}
	catch(std::exception &)
	{
		botan_throw_abort();
	}
	return 0; // never get here
}

void botan_secure_free(void *p, int bytes)
{
	try
	{
		alloc->deallocate(p, (Botan::u32bit)bytes);
	}
	catch(std::exception &)
	{
		botan_throw_abort();
	}
}

} // end namespace QCA

void *qca_secure_alloc(int bytes)
{
	// allocate enough room to store a size value in front, return a pointer after it
	char *c = (char *)QCA::botan_secure_alloc(bytes + sizeof(int));
	((int *)c)[0] = bytes + sizeof(int);
	return c + sizeof(int);
}

void qca_secure_free(void *p)
{
	// backtrack to read the size value
	char *c = (char *)p;
	c -= sizeof(int);
	int bytes = ((int *)c)[0];
	QCA::botan_secure_free(c, bytes);
}

void *qca_secure_realloc(void *p, int bytes)
{
	// if null, do a plain alloc (just like how realloc() works)
	if(!p)
		return qca_secure_alloc(bytes);

	// backtrack to read the size value
	char *c = (char *)p;
	c -= sizeof(int);
	int oldsize = ((int *)c)[0] - sizeof(int);

	// alloc the new chunk
	char *new_p = (char *)qca_secure_alloc(bytes);
	if(!new_p)
		return 0;

	// move over the memory from the original block
	memmove(new_p, p, qMin(oldsize, bytes));

	// free the original
	qca_secure_free(p);

	// done
	return new_p;
}

namespace QCA {

// secure or non-secure buffer, with trailing 0-byte.
// buffer size of 0 is okay (sbuf/qbuf will be 0).
struct alloc_info
{
	bool sec;
	char *data;
	int size;

	// internal
	Botan::SecureVector<Botan::byte> *sbuf;
	QByteArray *qbuf;
};

// note: these functions don't return error if memory allocation/resizing
//   fails..  maybe fix this someday?

// ai: uninitialized
// size: >= 0
// note: memory will be initially zero'd out
static bool ai_new(alloc_info *ai, int size, bool sec);

// ai: uninitialized
// from: initialized
static bool ai_copy(alloc_info *ai, const alloc_info *from);

// ai: initialized
// new_size: >= 0
static bool ai_resize(alloc_info *ai, int new_size);

// ai: initialized
static void ai_delete(alloc_info *ai);

bool ai_new(alloc_info *ai, int size, bool sec)
{
	if(size < 0)
		return false;

	ai->size = size;
	ai->sec = sec;

	if(size == 0)
	{
		ai->sbuf = 0;
		ai->qbuf = 0;
		ai->data = 0;
		return true;
	}

	if(sec)
	{
		try
		{
			ai->sbuf = new Botan::SecureVector<Botan::byte>((Botan::u32bit)size + 1);
		}
		catch(std::exception &)
		{
			botan_throw_abort();
			return false; // never get here
		}

		(*(ai->sbuf))[size] = 0;
		ai->qbuf = 0;
		Botan::byte *bp = (Botan::byte *)(*(ai->sbuf));
		ai->data = (char *)bp;
	}
	else
	{
		ai->sbuf = 0;
		ai->qbuf = new QByteArray(size, 0);
		ai->data = ai->qbuf->data();
	}

	return true;
}

bool ai_copy(alloc_info *ai, const alloc_info *from)
{
	ai->size = from->size;
	ai->sec = from->sec;

	if(ai->size == 0)
	{
		ai->sbuf = 0;
		ai->qbuf = 0;
		ai->data = 0;
		return true;
	}

	if(ai->sec)
	{
		try
		{
			ai->sbuf = new Botan::SecureVector<Botan::byte>(*(from->sbuf));
		}
		catch(std::exception &)
		{
			botan_throw_abort();
			return false; // never get here
		}

		ai->qbuf = 0;
		Botan::byte *bp = (Botan::byte *)(*(ai->sbuf));
		ai->data = (char *)bp;
	}
	else
	{
		ai->sbuf = 0;
		ai->qbuf = new QByteArray(*(from->qbuf));
		ai->data = ai->qbuf->data();
	}

	return true;
}

bool ai_resize(alloc_info *ai, int new_size)
{
	if(new_size < 0)
		return false;

	// new size is empty
	if(new_size == 0)
	{
		// we currently aren't empty
		if(ai->size > 0)
		{
			if(ai->sec)
			{
				delete ai->sbuf;
				ai->sbuf = 0;
			}
			else
			{
				delete ai->qbuf;
				ai->qbuf = 0;
			}

			ai->size = 0;
			ai->data = 0;
		}

		return true;
	}

	if(ai->sec)
	{
		Botan::SecureVector<Botan::byte> *new_buf;
		try
		{
			new_buf = new Botan::SecureVector<Botan::byte>((Botan::u32bit)new_size + 1);
		}
		catch(std::exception &)
		{
			botan_throw_abort();
			return false; // never get here
		}

		Botan::byte *new_p = (Botan::byte *)(*new_buf);
		if(ai->size > 0)
		{
			const Botan::byte *old_p = (const Botan::byte *)(*(ai->sbuf));
			memcpy(new_p, old_p, qMin(new_size, ai->size));
			delete ai->sbuf;
		}
		ai->sbuf = new_buf;
		ai->size = new_size;
		(*(ai->sbuf))[new_size] = 0;
		ai->data = (char *)new_p;
	}
	else
	{
		if(ai->size > 0)
			ai->qbuf->resize(new_size);
		else
			ai->qbuf = new QByteArray(new_size, 0);

		ai->size = new_size;
		ai->data = ai->qbuf->data();
	}

	return true;
}

void ai_delete(alloc_info *ai)
{
	if(ai->size > 0)
	{
		if(ai->sec)
			delete ai->sbuf;
		else
			delete ai->qbuf;
	}
}

//----------------------------------------------------------------------------
// MemoryRegion
//----------------------------------------------------------------------------
static char blank[] = "";

class MemoryRegion::Private : public QSharedData
{
public:
	alloc_info ai;

	Private(int size, bool sec)
	{
		ai_new(&ai, size, sec);
	}

	Private(const QByteArray &from, bool sec)
	{
		ai_new(&ai, from.size(), sec);
		memcpy(ai.data, from.data(), ai.size);
	}

	Private(const Private &from) : QSharedData(from)
	{
		ai_copy(&ai, &from.ai);
	}

	~Private()
	{
		ai_delete(&ai);
	}

	bool resize(int new_size)
	{
		return ai_resize(&ai, new_size);
	}

	void setSecure(bool sec)
	{
		// if same mode, do nothing
		if(ai.sec == sec)
			return;

		alloc_info other;
		ai_new(&other, ai.size, sec);
		memcpy(other.data, ai.data, ai.size);
		ai_delete(&ai);
		ai = other;
	}
};

MemoryRegion::MemoryRegion()
:_secure(false), d(0)
{
}

MemoryRegion::MemoryRegion(const char *str)
:_secure(false), d(new Private(QByteArray::fromRawData(str, strlen(str)), false))
{
}

MemoryRegion::MemoryRegion(const QByteArray &from)
:_secure(false), d(new Private(from, false))
{
}

MemoryRegion::MemoryRegion(const MemoryRegion &from)
:_secure(from._secure), d(from.d)
{
}

MemoryRegion::~MemoryRegion()
{
}

MemoryRegion & MemoryRegion::operator=(const MemoryRegion &from)
{
	_secure = from._secure;
	d = from.d;
	return *this;
}

MemoryRegion & MemoryRegion::operator=(const QByteArray &from)
{
	set(from, false);
	return *this;
}

bool MemoryRegion::isNull() const
{
	return (d ? false : true);
}

bool MemoryRegion::isSecure() const
{
	return _secure;
}

QByteArray MemoryRegion::toByteArray() const
{
	if(!d)
		return QByteArray();

	if(d->ai.sec)
	{
		QByteArray buf(d->ai.size, 0);
		memcpy(buf.data(), d->ai.data, d->ai.size);
		return buf;
	}
	else
	{
		if(d->ai.size > 0)
			return *(d->ai.qbuf);
		else
			return QByteArray((int)0, (char)0);
	}
}

MemoryRegion::MemoryRegion(bool secure)
:_secure(secure), d(0)
{
}

MemoryRegion::MemoryRegion(int size, bool secure)
:_secure(secure), d(new Private(size, secure))
{
}

MemoryRegion::MemoryRegion(const QByteArray &from, bool secure)
:_secure(secure), d(new Private(from, secure))
{
}

char *MemoryRegion::data()
{
	if(!d)
		return blank;
	return d->ai.data;
}

const char *MemoryRegion::data() const
{
	if(!d)
		return blank;
	return d->ai.data;
}

const char *MemoryRegion::constData() const
{
	if(!d)
		return blank;
	return d->ai.data;
}

char & MemoryRegion::at(int index)
{
	return *(d->ai.data + index);
}

const char & MemoryRegion::at(int index) const
{
	return *(d->ai.data + index);
}

int MemoryRegion::size() const
{
	if(!d)
		return 0;
	return d->ai.size;
}

bool MemoryRegion::isEmpty() const
{
	if(!d)
		return true;
	return (d->ai.size > 0 ? false : true);
}

bool MemoryRegion::resize(int size)
{
	if(!d)
	{
		d = new Private(size, _secure);
		return true;
	}

	if(d->ai.size == size)
		return true;

	return d->resize(size);
}

void MemoryRegion::set(const QByteArray &from, bool secure)
{
	_secure = secure;

	if(!from.isEmpty())
		d = new Private(from, secure);
	else
		d = new Private(0, secure);
}

void MemoryRegion::setSecure(bool secure)
{
	_secure = secure;

	if(!d)
	{
		d = new Private(0, secure);
		return;
	}

	d->setSecure(secure);
}

//----------------------------------------------------------------------------
// SecureArray
//----------------------------------------------------------------------------
SecureArray::SecureArray()
:MemoryRegion(true)
{
}

SecureArray::SecureArray(int size, char ch)
:MemoryRegion(size, true)
{
	// ai_new fills with zeros for us
	if(ch != 0)
		fill(ch, size);
}

SecureArray::SecureArray(const char *str)
:MemoryRegion(QByteArray::fromRawData(str, strlen(str)), true)
{
}

SecureArray::SecureArray(const QByteArray &a)
:MemoryRegion(a, true)
{
}

SecureArray::SecureArray(const MemoryRegion &a)
:MemoryRegion(a)
{
	setSecure(true);
}

SecureArray::SecureArray(const SecureArray &from)
:MemoryRegion(from)
{
}

SecureArray::~SecureArray()
{
}

SecureArray & SecureArray::operator=(const SecureArray &from)
{
	MemoryRegion::operator=(from);
	return *this;
}

SecureArray & SecureArray::operator=(const QByteArray &from)
{
	MemoryRegion::set(from, true);
	return *this;
}

void SecureArray::clear()
{
	MemoryRegion::resize(0);
}

bool SecureArray::resize(int size)
{
	return MemoryRegion::resize(size);
}

char & SecureArray::operator[](int index)
{
	return at(index);
}

const char & SecureArray::operator[](int index) const
{
	return at(index);
}

char & SecureArray::at(int index)
{
	return MemoryRegion::at(index);
}

const char & SecureArray::at(int index) const
{
	return MemoryRegion::at(index);
}

char *SecureArray::data()
{
	return MemoryRegion::data();
}

const char *SecureArray::data() const
{
	return MemoryRegion::data();
}

const char *SecureArray::constData() const
{
	return MemoryRegion::constData();
}

int SecureArray::size() const
{
	return MemoryRegion::size();
}

bool SecureArray::isEmpty() const
{
	return MemoryRegion::isEmpty();
}

QByteArray SecureArray::toByteArray() const
{
	return MemoryRegion::toByteArray();
}

SecureArray & SecureArray::append(const SecureArray &a)
{
	int oldsize = size();
	resize(oldsize + a.size());
	memcpy(data() + oldsize, a.data(), a.size());
	return *this;
}

bool SecureArray::operator==(const MemoryRegion &other) const
{
	if(this == &other)
		return true;
	if(size() == other.size() && memcmp(data(), other.data(), size()) == 0)
		return true;
	return false;
}

SecureArray & SecureArray::operator+=(const SecureArray &a)
{
	return append(a);
}

void SecureArray::fill(char fillChar, int fillToPosition)
{
	int len = (fillToPosition == -1) ? size() : qMin(fillToPosition, size());
	if(len > 0)
		memset(data(), (int)fillChar, len);
}

void SecureArray::set(const SecureArray &from)
{
	*this = from;
}

void SecureArray::set(const QByteArray &from)
{
	*this = from;
}

const SecureArray operator+(const SecureArray &a, const SecureArray &b)
{
	SecureArray c = a;
	return c.append(b);
}

//----------------------------------------------------------------------------
// BigInteger
//----------------------------------------------------------------------------
static void negate_binary(char *a, int size)
{
	// negate = two's compliment + 1
	bool done = false;
	for(int n = size - 1; n >= 0; --n)
	{
		a[n] = ~a[n];
		if(!done)
		{
			if((unsigned char)a[n] < 0xff)
			{
				++a[n];
				done = true;
			}
			else
				a[n] = 0;
		}
	}
}

class BigInteger::Private : public QSharedData
{
public:
	Botan::BigInt n;
};

BigInteger::BigInteger()
{
	d = new Private;
}

BigInteger::BigInteger(int i)
{
	d = new Private;
	if(i < 0)
	{
		d->n = Botan::BigInt(i * (-1));
		d->n.set_sign(Botan::BigInt::Negative);
	}
	else
	{
		d->n = Botan::BigInt(i);
		d->n.set_sign(Botan::BigInt::Positive);
	}
}

BigInteger::BigInteger(const char *c)
{
	d = new Private;
	fromString(QString(c));
}

BigInteger::BigInteger(const QString &s)
{
	d = new Private;
	fromString(s);
}

BigInteger::BigInteger(const SecureArray &a)
{
	d = new Private;
	fromArray(a);
}

BigInteger::BigInteger(const BigInteger &from)
{
	*this = from;
}

BigInteger::~BigInteger()
{
}

BigInteger & BigInteger::operator=(const BigInteger &from)
{
	d = from.d;
	return *this;
}

BigInteger & BigInteger::operator+=(const BigInteger &i)
{
	d->n += i.d->n;
	return *this;
}

BigInteger & BigInteger::operator-=(const BigInteger &i)
{
	d->n -= i.d->n;
	return *this;
}

BigInteger & BigInteger::operator*=(const BigInteger &i)
{
	d->n *= i.d->n;
	return *this;
}

BigInteger & BigInteger::operator/=(const BigInteger &i)
{
	try
	{
		d->n /= i.d->n;
	}
	catch(std::exception &)
	{
		fprintf(stderr, "QCA: Botan integer division error\n");
		abort();
	}
	return *this;
}

BigInteger & BigInteger::operator%=(const BigInteger &i)
{
	try
	{
		d->n %= i.d->n;
	}
	catch(std::exception &)
	{
		fprintf(stderr, "QCA: Botan integer division error\n");
		abort();
	}
	return *this;
}

BigInteger & BigInteger::operator=(const QString &s)
{
	fromString(s);
	return *this;
}

int BigInteger::compare(const BigInteger &n) const
{
	return ( (d->n).cmp( n.d->n, true) );
}

QTextStream &operator<<(QTextStream &stream, const BigInteger& b)
{
	stream << b.toString();
	return stream;
}

SecureArray BigInteger::toArray() const
{
	int size = d->n.encoded_size(Botan::BigInt::Binary);

	// return at least 8 bits
	if(size == 0)
	{
		SecureArray a(1);
		a[0] = 0;
		return a;
	}

	int offset = 0;
	SecureArray a;

	// make room for a sign bit if needed
	if(d->n.get_bit((size * 8) - 1))
	{
		++size;
		a.resize(size);
		a[0] = 0;
		++offset;
	}
	else
		a.resize(size);

	Botan::BigInt::encode((Botan::byte *)a.data() + offset, d->n, Botan::BigInt::Binary);

	if(d->n.is_negative())
		negate_binary(a.data(), a.size());

	return a;
}

void BigInteger::fromArray(const SecureArray &_a)
{
	if(_a.isEmpty())
	{
		d->n = Botan::BigInt(0);
		return;
	}
	SecureArray a = _a;

	Botan::BigInt::Sign sign = Botan::BigInt::Positive;
	if(a[0] & 0x80)
		sign = Botan::BigInt::Negative;

	if(sign == Botan::BigInt::Negative)
		negate_binary(a.data(), a.size());

	d->n = Botan::BigInt::decode((const Botan::byte *)a.data(), a.size(), Botan::BigInt::Binary);
	d->n.set_sign(sign);
}

QString BigInteger::toString() const
{
	QByteArray cs;
	try
	{
		cs.resize(d->n.encoded_size(Botan::BigInt::Decimal));
		Botan::BigInt::encode((Botan::byte *)cs.data(), d->n, Botan::BigInt::Decimal);
	}
	catch(std::exception &)
	{
		return QString();
	}

	QString str;
	if(d->n.is_negative())
		str += '-';
	str += QString::fromLatin1(cs);
	return str;
}

bool BigInteger::fromString(const QString &s)
{
	if(s.isEmpty())
		return false;
	QByteArray cs = s.toLatin1();

	bool neg = false;
	if(s[0] == '-')
		neg = true;

	try
	{
		d->n = Botan::BigInt::decode((const Botan::byte *)cs.data() + (neg ? 1 : 0), cs.length() - (neg ? 1 : 0), Botan::BigInt::Decimal);
	}
	catch(std::exception &)
	{
		return false;
	}

	if(neg)
		d->n.set_sign(Botan::BigInt::Negative);
	else
		d->n.set_sign(Botan::BigInt::Positive);
	return true;
}

}
