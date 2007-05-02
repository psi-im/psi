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
	void *d = malloc(256);
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

bool botan_init(int prealloc, bool mmap)
{
	// 64k minimum
	if(prealloc < 64)
		prealloc = 64;

	{
		Botan::Builtin_Modules modules;
		Botan::Library_State *libstate = new Botan::Library_State(modules.mutex_factory());
		libstate->prealloc_size = prealloc * 1024;
		Botan::set_global_state(libstate);
		Botan::global_state().load(modules);
	}

	bool secmem = false;
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

	return secmem;
}

void botan_deinit()
{
	alloc = 0;
	Botan::set_global_state(0);
}

void *botan_secure_alloc(int bytes)
{
	return alloc->allocate((Botan::u32bit)bytes);
}

void botan_secure_free(void *p, int bytes)
{
	alloc->deallocate(p, (Botan::u32bit)bytes);
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

//----------------------------------------------------------------------------
// SecureArray
//----------------------------------------------------------------------------
class SecureArray::Private : public QSharedData
{
public:
	Botan::SecureVector<Botan::byte> *buf;
	int size;

	Private(uint _size)
	{
		size = _size;
		buf = new Botan::SecureVector<Botan::byte>((Botan::u32bit)size + 1);
		(*buf)[size] = 0;
	}

	Private(const QByteArray &from)
	{
		size = from.size();
		buf = new Botan::SecureVector<Botan::byte>((Botan::u32bit)size + 1);
		(*buf)[size] = 0;
		Botan::byte *p = (Botan::byte *)(*buf);
		memcpy(p, from.data(), from.size());
	}

	Private(const Private &from) : QSharedData(from)
	{
		buf = new Botan::SecureVector<Botan::byte>(*(from.buf));
		size = from.size;
	}

	~Private()
	{
		delete buf;
	}

	bool resize(int new_size)
	{
		Botan::SecureVector<Botan::byte> *new_buf = new Botan::SecureVector<Botan::byte>((Botan::u32bit)new_size + 1);
		Botan::byte *new_p = (Botan::byte *)(*new_buf);
		const Botan::byte *old_p = (const Botan::byte *)(*buf);
		memcpy(new_p, old_p, qMin(new_size, size));
		delete buf;
		buf = new_buf;
		size = new_size;
		(*buf)[size] = 0;
		return true;
	}
};

SecureArray::SecureArray()
{
	d = 0;
}

SecureArray::SecureArray(int size, char ch)
{
	if(size > 0)
	{
		d = new Private(size);

		// botan fills with zeros for us
		if(ch != 0)
			fill(ch, size);
	}
	else
		d = 0;
}

SecureArray::SecureArray(const char *str)
{
	QByteArray a = QByteArray::fromRawData(str, strlen(str));
	if(a.size() > 0)
		d = new Private(a);
	else
		d = 0;
}

SecureArray::SecureArray(const QByteArray &a)
{
	d = 0;
	*this = a;
}

SecureArray::SecureArray(const SecureArray &from)
{
	d = 0;
	*this = from;
}

SecureArray::~SecureArray()
{
}

SecureArray & SecureArray::operator=(const SecureArray &from)
{
	d = from.d;
	return *this;
}

SecureArray & SecureArray::operator=(const QByteArray &from)
{
	d = 0;
	if(!from.isEmpty())
		d = new Private(from);

	return *this;
}

void SecureArray::clear()
{
	d = 0;
}

bool SecureArray::resize(int size)
{
	int cur_size = (d ? d->size : 0);
	if(cur_size == size)
		return true;

	if(size > 0)
	{
		if(d)
		{
			if(!d->resize(size))
				return false;
		}
		else
			d = new Private(size);
	}
	else
	{
		d = 0;
	}
	return true;
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
	Botan::byte *bp = (Botan::byte *)(*d->buf) + index;
	char *cp = (char *)bp;
	return *cp;
}

const char & SecureArray::at(int index) const
{
	const Botan::byte *bp = (const Botan::byte *)(*d->buf) + index;
	const char *cp = (const char *)bp;
	return *cp;
}

char *SecureArray::data()
{
	if(!d)
		return 0;
	Botan::byte *p = (Botan::byte *)(*d->buf);
	return ((char *)p);
}

const char *SecureArray::data() const
{
	if(!d)
		return 0;
	const Botan::byte *p = (const Botan::byte *)(*d->buf);
	return ((const char *)p);
}

const char *SecureArray::constData() const
{
	return data();
}

int SecureArray::size() const
{
	return (d ? d->size : 0);
}

bool SecureArray::isEmpty() const
{
	return (size() == 0);
}

QByteArray SecureArray::toByteArray() const
{
	if(isEmpty())
		return QByteArray();

	QByteArray buf(size(), 0);
	memcpy(buf.data(), data(), size());
	return buf;
}

SecureArray & SecureArray::append(const SecureArray &a)
{
	int oldsize = size();
	resize(oldsize + a.size());
	memcpy(data() + oldsize, a.data(), a.size());
	return *this;
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

bool operator==(const SecureArray &a, const SecureArray &b)
{
	if(&a == &b)
		return true;
	if(a.size() == b.size() && memcmp(a.data(), b.data(), a.size()) == 0)
		return true;
	return false;
}

bool operator!=(const SecureArray &a, const SecureArray &b)
{
	return !(a == b);
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
