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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "qca_tools.h"

#include <QtCore>

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

static void add_mmap()
{
#ifdef Q_OS_UNIX
	Botan::add_allocator_type("mmap", new Botan::MemoryMapping_Allocator);
	Botan::set_default_allocator("mmap");
#endif
}

// Botan shouldn't throw any exceptions in our init/deinit.

static const Botan::SecureAllocator *alloc = 0;

bool botan_init(int prealloc, bool mmap)
{
	// 64k minimum
	if(prealloc < 64)
		prealloc = 64;

	Botan::botan_memory_chunk = 64 * 1024;
	Botan::botan_prealloc = prealloc / 64;
	if(prealloc % 64 != 0)
		++Botan::botan_prealloc;

	Botan::Init::set_mutex_type(new Botan::Qt_Mutex);
	Botan::Init::startup_memory_subsystem();

	bool secmem = false;
	if(can_lock())
	{
		Botan::set_default_allocator("locking");
		secmem = true;
	}
	else if(mmap)
	{
		add_mmap();
		secmem = true;
	}
	alloc = Botan::get_allocator("default");

	return secmem;
}

void botan_deinit()
{
	alloc = 0;
	Botan::Init::shutdown_memory_subsystem();
	Botan::Init::set_mutex_type(0);
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

using namespace QCA;

//----------------------------------------------------------------------------
// QSecureArray
//----------------------------------------------------------------------------
class QSecureArray::Private : public QSharedData
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

QSecureArray::QSecureArray()
{
	d = 0;
}

QSecureArray::QSecureArray(int size, char ch)
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

QSecureArray::QSecureArray(const char *str)
{
	QByteArray a = QByteArray::fromRawData(str, strlen(str));
	if(a.size() > 0)
		d = new Private(a);
	else
		d = 0;
}

QSecureArray::QSecureArray(const QByteArray &a)
{
	d = 0;
	*this = a;
}

QSecureArray::QSecureArray(const QSecureArray &from)
{
	d = 0;
	*this = from;
}

QSecureArray::~QSecureArray()
{
}

QSecureArray & QSecureArray::operator=(const QSecureArray &from)
{
	d = from.d;
	return *this;
}

QSecureArray & QSecureArray::operator=(const QByteArray &from)
{
	d = 0;
	if(!from.isEmpty())
		d = new Private(from);

	return *this;
}

void QSecureArray::clear()
{
	d = 0;
}

bool QSecureArray::resize(int size)
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

char & QSecureArray::operator[](int index)
{
	return at(index);
}

const char & QSecureArray::operator[](int index) const
{
	return at(index);
}

char & QSecureArray::at(int index)
{
	Botan::byte *bp = (Botan::byte *)(*d->buf) + index;
	char *cp = (char *)bp;
	return *cp;
}

const char & QSecureArray::at(int index) const
{
	const Botan::byte *bp = (const Botan::byte *)(*d->buf) + index;
	const char *cp = (const char *)bp;
	return *cp;
}

char *QSecureArray::data()
{
	if(!d)
		return 0;
	Botan::byte *p = (Botan::byte *)(*d->buf);
	return ((char *)p);
}

const char *QSecureArray::data() const
{
	if(!d)
		return 0;
	const Botan::byte *p = (const Botan::byte *)(*d->buf);
	return ((const char *)p);
}

const char *QSecureArray::constData() const
{
	return data();
}

int QSecureArray::size() const
{
	return (d ? d->size : 0);
}

bool QSecureArray::isEmpty() const
{
	return (size() == 0);
}

QByteArray QSecureArray::toByteArray() const
{
	if(isEmpty())
		return QByteArray();

	QByteArray buf(size(), 0);
	memcpy(buf.data(), data(), size());
	return buf;
}

QSecureArray & QSecureArray::append(const QSecureArray &a)
{
	int oldsize = size();
	resize(oldsize + a.size());
	memcpy(data() + oldsize, a.data(), a.size());
	return *this;
}

QSecureArray & QSecureArray::operator+=(const QSecureArray &a)
{
	return append(a);
}

void QSecureArray::fill(char fillChar, int fillToPosition)
{
	int len = (fillToPosition == -1) ? size() : qMin(fillToPosition, size());
	if(len > 0)
		memset(data(), (int)fillChar, len);
}

void QSecureArray::set(const QSecureArray &from)
{
	*this = from;
}

void QSecureArray::set(const QByteArray &from)
{
	*this = from;
}

bool operator==(const QSecureArray &a, const QSecureArray &b)
{
	if(&a == &b)
		return true;
	if(a.size() == b.size() && memcmp(a.data(), b.data(), a.size()) == 0)
		return true;
	return false;
}

bool operator!=(const QSecureArray &a, const QSecureArray &b)
{
	return !(a == b);
}

const QSecureArray operator+(const QSecureArray &a, const QSecureArray &b)
{
	QSecureArray c = a;
	return c.append(b);
}

//----------------------------------------------------------------------------
// QBigInteger
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

class QBigInteger::Private : public QSharedData
{
public:
	Botan::BigInt n;
};

QBigInteger::QBigInteger()
{
	d = new Private;
}

QBigInteger::QBigInteger(int i)
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

QBigInteger::QBigInteger(const char *c)
{
	d = new Private;
	fromString(QString(c));
}

QBigInteger::QBigInteger(const QString &s)
{
	d = new Private;
	fromString(s);
}

QBigInteger::QBigInteger(const QSecureArray &a)
{
	d = new Private;
	fromArray(a);
}

QBigInteger::QBigInteger(const QBigInteger &from)
{
	*this = from;
}

QBigInteger::~QBigInteger()
{
}

QBigInteger & QBigInteger::operator=(const QBigInteger &from)
{
	d = from.d;
	return *this;
}

QBigInteger & QBigInteger::operator+=(const QBigInteger &i)
{
	d->n += i.d->n;
	return *this;
}

QBigInteger & QBigInteger::operator-=(const QBigInteger &i)
{
	d->n -= i.d->n;
	return *this;
}

QBigInteger & QBigInteger::operator=(const QString &s)
{
	fromString(s);
	return *this;
}

int QBigInteger::compare(const QBigInteger &n) const
{
	return ( (d->n).cmp( n.d->n, true) );
}

QTextStream &operator<<(QTextStream &stream, const QBigInteger& b)
{
	stream << b.toString();
	return stream;
}

QSecureArray QBigInteger::toArray() const
{
	int size = d->n.encoded_size(Botan::BigInt::Binary);

	// return at least 8 bits
	if(size == 0)
	{
		QSecureArray a(1);
		a[0] = 0;
		return a;
	}

	int offset = 0;
	QSecureArray a;

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

void QBigInteger::fromArray(const QSecureArray &_a)
{
	if(_a.isEmpty())
	{
		d->n = Botan::BigInt(0);
		return;
	}
	QSecureArray a = _a;

	Botan::BigInt::Sign sign = Botan::BigInt::Positive;
	if(a[0] & 0x80)
		sign = Botan::BigInt::Negative;

	if(sign == Botan::BigInt::Negative)
		negate_binary(a.data(), a.size());

	d->n = Botan::BigInt::decode((const Botan::byte *)a.data(), a.size(), Botan::BigInt::Binary);
	d->n.set_sign(sign);
}

QString QBigInteger::toString() const
{
	QByteArray cs;
	try
	{
		cs.resize(d->n.encoded_size(Botan::BigInt::Decimal));
		Botan::BigInt::encode((Botan::byte *)cs.data(), d->n, Botan::BigInt::Decimal);
	}
	catch(std::exception &)
	{
		return QString::null;
	}

	QString str;
	if(d->n.is_negative())
		str += '-';
	str += QString::fromLatin1(cs);
	return str;
}

bool QBigInteger::fromString(const QString &s)
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
