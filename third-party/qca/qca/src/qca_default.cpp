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

#include "qca_core.h"

#include <QMutex>
#include <QHash>
#include "qca_textfilter.h"
#include "qca_cert.h"
#include "qcaprovider.h"

#ifndef QCA_NO_SYSTEMSTORE
# include "qca_systemstore.h"
#endif

#define FRIENDLY_NAMES

namespace QCA {

class DefaultShared
{
private:
	mutable QMutex m;
	bool _use_system;
	QString _roots_file;
	QStringList _skip_plugins;
	QStringList _plugin_priorities;

public:
	DefaultShared() : _use_system(true)
	{
	}

	bool use_system() const
	{
		QMutexLocker locker(&m);
		return _use_system;
	}

	QString roots_file() const
	{
		QMutexLocker locker(&m);
		return _roots_file;
	}

	QStringList skip_plugins() const
	{
		QMutexLocker locker(&m);
		return _skip_plugins;
	}

	QStringList plugin_priorities() const
	{
		QMutexLocker locker(&m);
		return _plugin_priorities;
	}

	void set(bool use_system, const QString &roots_file, const QStringList &skip_plugins, const QStringList &plugin_priorities)
	{
		QMutexLocker locker(&m);
		_use_system = use_system;
		_roots_file = roots_file;
		_skip_plugins = skip_plugins;
		_plugin_priorities = plugin_priorities;
	}
};

//----------------------------------------------------------------------------
// DefaultRandomContext
//----------------------------------------------------------------------------
class DefaultRandomContext : public RandomContext
{
public:
	DefaultRandomContext(Provider *p) : RandomContext(p) {}

	virtual Provider::Context *clone() const
	{
		return new DefaultRandomContext(provider());
	}

	virtual SecureArray nextBytes(int size)
	{
		SecureArray buf(size);
		for(int n = 0; n < (int)buf.size(); ++n)
			buf[n] = (char)qrand();
		return buf;
	}
};

//----------------------------------------------------------------------------
// DefaultMD5Context
//----------------------------------------------------------------------------

/* NOTE: the following code was modified to not need BYTE_ORDER -- Justin */

/*
  Copyright (C) 1999, 2000, 2002 Aladdin Enterprises.  All rights reserved.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  L. Peter Deutsch
  ghost@aladdin.com

 */
/* $Id: qca_default.cpp 808523 2008-05-16 20:41:50Z infiniti $ */
/*
  Independent implementation of MD5 (RFC 1321).

  This code implements the MD5 Algorithm defined in RFC 1321, whose
  text is available at
	http://www.ietf.org/rfc/rfc1321.txt
  The code is derived from the text of the RFC, including the test suite
  (section A.5) but excluding the rest of Appendix A.  It does not include
  any code or documentation that is identified in the RFC as being
  copyrighted.

  The original and principal author of md5.c is L. Peter Deutsch
  <ghost@aladdin.com>.  Other authors are noted in the change history
  that follows (in reverse chronological order):

  2002-04-13 lpd Clarified derivation from RFC 1321; now handles byte order
	either statically or dynamically; added missing #include <string.h>
	in library.
  2002-03-11 lpd Corrected argument list for main(), and added int return
	type, in test program and T value program.
  2002-02-21 lpd Added missing #include <stdio.h> in test program.
  2000-07-03 lpd Patched to eliminate warnings about "constant is
	unsigned in ANSI C, signed in traditional"; made test program
	self-checking.
  1999-11-04 lpd Edited comments slightly for automatic TOC extraction.
  1999-10-18 lpd Fixed typo in header comment (ansi2knr rather than md5).
  1999-05-03 lpd Original version.
 */

/*
 * This package supports both compile-time and run-time determination of CPU
 * byte order.  If ARCH_IS_BIG_ENDIAN is defined as 0, the code will be
 * compiled to run only on little-endian CPUs; if ARCH_IS_BIG_ENDIAN is
 * defined as non-zero, the code will be compiled to run only on big-endian
 * CPUs; if ARCH_IS_BIG_ENDIAN is not defined, the code will be compiled to
 * run on either big- or little-endian CPUs, but will run slightly less
 * efficiently on either one than if ARCH_IS_BIG_ENDIAN is defined.
 */

typedef quint8  md5_byte_t; /* 8-bit byte */
typedef quint32 md5_word_t; /* 32-bit word */

/* Define the state of the MD5 Algorithm. */
struct md5_state_t {
    SecureArray sbuf;
    md5_word_t *count; // 2   /* message length in bits, lsw first */
    md5_word_t *abcd;  // 4   /* digest buffer */
    md5_byte_t *buf;   // 64  /* accumulate block */

    md5_state_t()
    {
        sbuf.resize((6 * sizeof(md5_word_t)) + 64);
        setup();
    }

    md5_state_t(const md5_state_t &from)
    {
        *this = from;
    }

    md5_state_t & operator=(const md5_state_t &from)
    {
        sbuf = from.sbuf;
        setup();
        return *this;
    }

    inline void setup()
    {
        char *p = sbuf.data();
        count = (md5_word_t *)p;
        abcd = (md5_word_t *)(p + (2 * sizeof(md5_word_t)));
        buf = (md5_byte_t *)(p + (6 * sizeof(md5_word_t)));
    }
};

/* Initialize the algorithm. */
void md5_init(md5_state_t *pms);

/* Append a string to the message. */
void md5_append(md5_state_t *pms, const md5_byte_t *data, int nbytes);

/* Finish the message and return the digest. */
void md5_finish(md5_state_t *pms, md5_byte_t digest[16]);

#define T_MASK ((md5_word_t)~0)
#define T1 /* 0xd76aa478 */ (T_MASK ^ 0x28955b87)
#define T2 /* 0xe8c7b756 */ (T_MASK ^ 0x173848a9)
#define T3    0x242070db
#define T4 /* 0xc1bdceee */ (T_MASK ^ 0x3e423111)
#define T5 /* 0xf57c0faf */ (T_MASK ^ 0x0a83f050)
#define T6    0x4787c62a
#define T7 /* 0xa8304613 */ (T_MASK ^ 0x57cfb9ec)
#define T8 /* 0xfd469501 */ (T_MASK ^ 0x02b96afe)
#define T9    0x698098d8
#define T10 /* 0x8b44f7af */ (T_MASK ^ 0x74bb0850)
#define T11 /* 0xffff5bb1 */ (T_MASK ^ 0x0000a44e)
#define T12 /* 0x895cd7be */ (T_MASK ^ 0x76a32841)
#define T13    0x6b901122
#define T14 /* 0xfd987193 */ (T_MASK ^ 0x02678e6c)
#define T15 /* 0xa679438e */ (T_MASK ^ 0x5986bc71)
#define T16    0x49b40821
#define T17 /* 0xf61e2562 */ (T_MASK ^ 0x09e1da9d)
#define T18 /* 0xc040b340 */ (T_MASK ^ 0x3fbf4cbf)
#define T19    0x265e5a51
#define T20 /* 0xe9b6c7aa */ (T_MASK ^ 0x16493855)
#define T21 /* 0xd62f105d */ (T_MASK ^ 0x29d0efa2)
#define T22    0x02441453
#define T23 /* 0xd8a1e681 */ (T_MASK ^ 0x275e197e)
#define T24 /* 0xe7d3fbc8 */ (T_MASK ^ 0x182c0437)
#define T25    0x21e1cde6
#define T26 /* 0xc33707d6 */ (T_MASK ^ 0x3cc8f829)
#define T27 /* 0xf4d50d87 */ (T_MASK ^ 0x0b2af278)
#define T28    0x455a14ed
#define T29 /* 0xa9e3e905 */ (T_MASK ^ 0x561c16fa)
#define T30 /* 0xfcefa3f8 */ (T_MASK ^ 0x03105c07)
#define T31    0x676f02d9
#define T32 /* 0x8d2a4c8a */ (T_MASK ^ 0x72d5b375)
#define T33 /* 0xfffa3942 */ (T_MASK ^ 0x0005c6bd)
#define T34 /* 0x8771f681 */ (T_MASK ^ 0x788e097e)
#define T35    0x6d9d6122
#define T36 /* 0xfde5380c */ (T_MASK ^ 0x021ac7f3)
#define T37 /* 0xa4beea44 */ (T_MASK ^ 0x5b4115bb)
#define T38    0x4bdecfa9
#define T39 /* 0xf6bb4b60 */ (T_MASK ^ 0x0944b49f)
#define T40 /* 0xbebfbc70 */ (T_MASK ^ 0x4140438f)
#define T41    0x289b7ec6
#define T42 /* 0xeaa127fa */ (T_MASK ^ 0x155ed805)
#define T43 /* 0xd4ef3085 */ (T_MASK ^ 0x2b10cf7a)
#define T44    0x04881d05
#define T45 /* 0xd9d4d039 */ (T_MASK ^ 0x262b2fc6)
#define T46 /* 0xe6db99e5 */ (T_MASK ^ 0x1924661a)
#define T47    0x1fa27cf8
#define T48 /* 0xc4ac5665 */ (T_MASK ^ 0x3b53a99a)
#define T49 /* 0xf4292244 */ (T_MASK ^ 0x0bd6ddbb)
#define T50    0x432aff97
#define T51 /* 0xab9423a7 */ (T_MASK ^ 0x546bdc58)
#define T52 /* 0xfc93a039 */ (T_MASK ^ 0x036c5fc6)
#define T53    0x655b59c3
#define T54 /* 0x8f0ccc92 */ (T_MASK ^ 0x70f3336d)
#define T55 /* 0xffeff47d */ (T_MASK ^ 0x00100b82)
#define T56 /* 0x85845dd1 */ (T_MASK ^ 0x7a7ba22e)
#define T57    0x6fa87e4f
#define T58 /* 0xfe2ce6e0 */ (T_MASK ^ 0x01d3191f)
#define T59 /* 0xa3014314 */ (T_MASK ^ 0x5cfebceb)
#define T60    0x4e0811a1
#define T61 /* 0xf7537e82 */ (T_MASK ^ 0x08ac817d)
#define T62 /* 0xbd3af235 */ (T_MASK ^ 0x42c50dca)
#define T63    0x2ad7d2bb
#define T64 /* 0xeb86d391 */ (T_MASK ^ 0x14792c6e)


static void
md5_process(md5_state_t *pms, const md5_byte_t *data /*[64]*/)
{
	md5_word_t
	a = pms->abcd[0], b = pms->abcd[1],
	c = pms->abcd[2], d = pms->abcd[3];
	md5_word_t t;

	/* Define storage for little-endian or both types of CPUs. */
	// possible FIXME: does xbuf really need to be secured?
	SecureArray sxbuf(16 * sizeof(md5_word_t));
	md5_word_t *xbuf = (md5_word_t *)sxbuf.data();
	const md5_word_t *X;

	{
		if(QSysInfo::ByteOrder == QSysInfo::BigEndian)
		{
			/*
			* On big-endian machines, we must arrange the bytes in the
			* right order.
			*/
			const md5_byte_t *xp = data;
			int i;

			X = xbuf;		/* (dynamic only) */

			for (i = 0; i < 16; ++i, xp += 4)
				xbuf[i] = xp[0] + (xp[1] << 8) + (xp[2] << 16) + (xp[3] << 24);
		}
		else			/* dynamic big-endian */
		{
			/*
			* On little-endian machines, we can process properly aligned
			* data without copying it.
			*/
			if (!((data - (const md5_byte_t *)0) & 3)) {
				/* data are properly aligned */
				X = (const md5_word_t *)data;
			} else {
				/* not aligned */
				memcpy(xbuf, data, 64);
				X = xbuf;
			}
		}
	}

#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

    /* Round 1. */
    /* Let [abcd k s i] denote the operation
       a = b + ((a + F(b,c,d) + X[k] + T[i]) <<< s). */
#define F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + F(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
    /* Do the following 16 operations. */
    SET(a, b, c, d,  0,  7,  T1);
    SET(d, a, b, c,  1, 12,  T2);
    SET(c, d, a, b,  2, 17,  T3);
    SET(b, c, d, a,  3, 22,  T4);
    SET(a, b, c, d,  4,  7,  T5);
    SET(d, a, b, c,  5, 12,  T6);
    SET(c, d, a, b,  6, 17,  T7);
    SET(b, c, d, a,  7, 22,  T8);
    SET(a, b, c, d,  8,  7,  T9);
    SET(d, a, b, c,  9, 12, T10);
    SET(c, d, a, b, 10, 17, T11);
    SET(b, c, d, a, 11, 22, T12);
    SET(a, b, c, d, 12,  7, T13);
    SET(d, a, b, c, 13, 12, T14);
    SET(c, d, a, b, 14, 17, T15);
    SET(b, c, d, a, 15, 22, T16);
#undef SET

     /* Round 2. */
     /* Let [abcd k s i] denote the operation
          a = b + ((a + G(b,c,d) + X[k] + T[i]) <<< s). */
#define G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + G(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
     /* Do the following 16 operations. */
    SET(a, b, c, d,  1,  5, T17);
    SET(d, a, b, c,  6,  9, T18);
    SET(c, d, a, b, 11, 14, T19);
    SET(b, c, d, a,  0, 20, T20);
    SET(a, b, c, d,  5,  5, T21);
    SET(d, a, b, c, 10,  9, T22);
    SET(c, d, a, b, 15, 14, T23);
    SET(b, c, d, a,  4, 20, T24);
    SET(a, b, c, d,  9,  5, T25);
    SET(d, a, b, c, 14,  9, T26);
    SET(c, d, a, b,  3, 14, T27);
    SET(b, c, d, a,  8, 20, T28);
    SET(a, b, c, d, 13,  5, T29);
    SET(d, a, b, c,  2,  9, T30);
    SET(c, d, a, b,  7, 14, T31);
    SET(b, c, d, a, 12, 20, T32);
#undef SET

     /* Round 3. */
     /* Let [abcd k s t] denote the operation
          a = b + ((a + H(b,c,d) + X[k] + T[i]) <<< s). */
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + H(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
     /* Do the following 16 operations. */
    SET(a, b, c, d,  5,  4, T33);
    SET(d, a, b, c,  8, 11, T34);
    SET(c, d, a, b, 11, 16, T35);
    SET(b, c, d, a, 14, 23, T36);
    SET(a, b, c, d,  1,  4, T37);
    SET(d, a, b, c,  4, 11, T38);
    SET(c, d, a, b,  7, 16, T39);
    SET(b, c, d, a, 10, 23, T40);
    SET(a, b, c, d, 13,  4, T41);
    SET(d, a, b, c,  0, 11, T42);
    SET(c, d, a, b,  3, 16, T43);
    SET(b, c, d, a,  6, 23, T44);
    SET(a, b, c, d,  9,  4, T45);
    SET(d, a, b, c, 12, 11, T46);
    SET(c, d, a, b, 15, 16, T47);
    SET(b, c, d, a,  2, 23, T48);
#undef SET

     /* Round 4. */
     /* Let [abcd k s t] denote the operation
          a = b + ((a + I(b,c,d) + X[k] + T[i]) <<< s). */
#define I(x, y, z) ((y) ^ ((x) | ~(z)))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + I(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
     /* Do the following 16 operations. */
    SET(a, b, c, d,  0,  6, T49);
    SET(d, a, b, c,  7, 10, T50);
    SET(c, d, a, b, 14, 15, T51);
    SET(b, c, d, a,  5, 21, T52);
    SET(a, b, c, d, 12,  6, T53);
    SET(d, a, b, c,  3, 10, T54);
    SET(c, d, a, b, 10, 15, T55);
    SET(b, c, d, a,  1, 21, T56);
    SET(a, b, c, d,  8,  6, T57);
    SET(d, a, b, c, 15, 10, T58);
    SET(c, d, a, b,  6, 15, T59);
    SET(b, c, d, a, 13, 21, T60);
    SET(a, b, c, d,  4,  6, T61);
    SET(d, a, b, c, 11, 10, T62);
    SET(c, d, a, b,  2, 15, T63);
    SET(b, c, d, a,  9, 21, T64);
#undef SET

     /* Then perform the following additions. (That is increment each
        of the four registers by the value it had before this block
        was started.) */
    pms->abcd[0] += a;
    pms->abcd[1] += b;
    pms->abcd[2] += c;
    pms->abcd[3] += d;
}

void
md5_init(md5_state_t *pms)
{
    pms->count[0] = pms->count[1] = 0;
    pms->abcd[0] = 0x67452301;
    pms->abcd[1] = /*0xefcdab89*/ T_MASK ^ 0x10325476;
    pms->abcd[2] = /*0x98badcfe*/ T_MASK ^ 0x67452301;
    pms->abcd[3] = 0x10325476;
}

void
md5_append(md5_state_t *pms, const md5_byte_t *data, int nbytes)
{
    const md5_byte_t *p = data;
    int left = nbytes;
    int offset = (pms->count[0] >> 3) & 63;
    md5_word_t nbits = (md5_word_t)(nbytes << 3);

    if (nbytes <= 0)
	return;

    /* Update the message length. */
    pms->count[1] += nbytes >> 29;
    pms->count[0] += nbits;
    if (pms->count[0] < nbits)
	pms->count[1]++;

    /* Process an initial partial block. */
    if (offset) {
	int copy = (offset + nbytes > 64 ? 64 - offset : nbytes);

	memcpy(pms->buf + offset, p, copy);
	if (offset + copy < 64)
	    return;
	p += copy;
	left -= copy;
	md5_process(pms, pms->buf);
    }

    /* Process full blocks. */
    for (; left >= 64; p += 64, left -= 64)
	md5_process(pms, p);

    /* Process a final partial block. */
    if (left)
	memcpy(pms->buf, p, left);
}

void
md5_finish(md5_state_t *pms, md5_byte_t digest[16])
{
    static const md5_byte_t pad[64] = {
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    md5_byte_t data[8];
    int i;

    /* Save the length before padding. */
    for (i = 0; i < 8; ++i)
	data[i] = (md5_byte_t)(pms->count[i >> 2] >> ((i & 3) << 3));
    /* Pad to 56 bytes mod 64. */
    md5_append(pms, pad, ((55 - (pms->count[0] >> 3)) & 63) + 1);
    /* Append the length. */
    md5_append(pms, data, 8);
    for (i = 0; i < 16; ++i)
	digest[i] = (md5_byte_t)(pms->abcd[i >> 2] >> ((i & 3) << 3));
}

class DefaultMD5Context : public HashContext
{
public:
	DefaultMD5Context(Provider *p) : HashContext(p, "md5")
	{
		clear();
	}

	virtual Provider::Context *clone() const
	{
		return new DefaultMD5Context(*this);
	}

	virtual void clear()
	{
		secure = true;
		md5_init(&md5);
	}

	virtual void update(const MemoryRegion &in)
	{
		if(!in.isSecure())
			secure = false;
		md5_append(&md5, (const md5_byte_t *)in.data(), in.size());
	}

	virtual MemoryRegion final()
	{
		if(secure)
		{
			SecureArray b(16, 0);
			md5_finish(&md5, (md5_byte_t *)b.data());
			return b;
		}
		else
		{
			QByteArray b(16, 0);
			md5_finish(&md5, (md5_byte_t *)b.data());
			return b;
		}
	}

	bool secure;
	md5_state_t md5;
};

//----------------------------------------------------------------------------
// DefaultSHA1Context
//----------------------------------------------------------------------------

// SHA1 - from a public domain implementation by Steve Reid (steve@edmweb.com)

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))
#define blk(i) (block->l[i&15] = rol(block->l[(i+13)&15]^block->l[(i+8)&15]^block->l[(i+2)&15]^block->l[i&15],1))

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);

struct SHA1_CONTEXT
{
	SecureArray sbuf;
	quint32 *state; // 5
	quint32 *count; // 2
	unsigned char *buffer; // 64

	SHA1_CONTEXT()
	{
		sbuf.resize((7 * sizeof(quint32)) + 64);
		setup();
	}

	SHA1_CONTEXT(const SHA1_CONTEXT &from)
	{
		*this = from;
	}

	SHA1_CONTEXT & operator=(const SHA1_CONTEXT &from)
	{
		sbuf = from.sbuf;
		setup();
		return *this;
	}

	inline void setup()
	{
		char *p = sbuf.data();
		state = (quint32 *)p;
		count = (quint32 *)(p + (5 * sizeof(quint32)));
		buffer = (unsigned char *)(p + (7 * sizeof(quint32)));
	}
};

typedef union {
	unsigned char c[64];
	quint32 l[16];
} CHAR64LONG16;

class DefaultSHA1Context : public HashContext
{
public:
	SHA1_CONTEXT _context;
	CHAR64LONG16* block;
	bool secure;

	DefaultSHA1Context(Provider *p) : HashContext(p, "sha1")
	{
		clear();
	}

	virtual Provider::Context *clone() const
	{
		return new DefaultSHA1Context(*this);
	}

	virtual void clear()
	{
		secure = true;
		sha1_init(&_context);
	}

	virtual void update(const MemoryRegion &in)
	{
		if(!in.isSecure())
			secure = false;
		sha1_update(&_context, (unsigned char *)in.data(), (unsigned int)in.size());
	}

	virtual MemoryRegion final()
	{
		if(secure)
		{
			SecureArray b(20, 0);
			sha1_final((unsigned char *)b.data(), &_context);
			return b;
		}
		else
		{
			QByteArray b(20, 0);
			sha1_final((unsigned char *)b.data(), &_context);
			return b;
		}
	}

	inline unsigned long blk0(quint32 i)
	{
		if(QSysInfo::ByteOrder == QSysInfo::BigEndian)
			return block->l[i];
		else
			return (block->l[i] = (rol(block->l[i],24)&0xFF00FF00) | (rol(block->l[i],8)&0x00FF00FF));
	}

	// Hash a single 512-bit block. This is the core of the algorithm.
	void transform(quint32 state[5], unsigned char buffer[64])
	{
		quint32 a, b, c, d, e;

		block = (CHAR64LONG16*)buffer;

		// Copy context->state[] to working vars
		a = state[0];
		b = state[1];
		c = state[2];
		d = state[3];
		e = state[4];

		// 4 rounds of 20 operations each. Loop unrolled.
		R0(a,b,c,d,e, 0); R0(e,a,b,c,d, 1); R0(d,e,a,b,c, 2); R0(c,d,e,a,b, 3);
		R0(b,c,d,e,a, 4); R0(a,b,c,d,e, 5); R0(e,a,b,c,d, 6); R0(d,e,a,b,c, 7);
		R0(c,d,e,a,b, 8); R0(b,c,d,e,a, 9); R0(a,b,c,d,e,10); R0(e,a,b,c,d,11);
		R0(d,e,a,b,c,12); R0(c,d,e,a,b,13); R0(b,c,d,e,a,14); R0(a,b,c,d,e,15);
		R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);
		R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);
		R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);
		R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);
		R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);
		R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);
		R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);
		R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);
		R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);
		R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);
		R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);
		R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);
		R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);
		R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);
		R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);
		R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);

		// Add the working vars back into context.state[]
		state[0] += a;
		state[1] += b;
		state[2] += c;
		state[3] += d;
		state[4] += e;

		// Wipe variables
		a = b = c = d = e = 0;
	}

	// SHA1Init - Initialize new context
	void sha1_init(SHA1_CONTEXT* context)
	{
		// SHA1 initialization constants
		context->state[0] = 0x67452301;
		context->state[1] = 0xEFCDAB89;
		context->state[2] = 0x98BADCFE;
		context->state[3] = 0x10325476;
		context->state[4] = 0xC3D2E1F0;
		context->count[0] = context->count[1] = 0;
	}

	// Run your data through this
	void sha1_update(SHA1_CONTEXT* context, unsigned char* data, quint32 len)
	{
		quint32 i, j;

		j = (context->count[0] >> 3) & 63;
		if((context->count[0] += len << 3) < (len << 3))
			context->count[1]++;

		context->count[1] += (len >> 29);

		if((j + len) > 63) {
			memcpy(&context->buffer[j], data, (i = 64-j));
			transform(context->state, context->buffer);
			for ( ; i + 63 < len; i += 64) {
				transform(context->state, &data[i]);
			}
			j = 0;
		}
		else i = 0;
			memcpy(&context->buffer[j], &data[i], len - i);
	}

	// Add padding and return the message digest
	void sha1_final(unsigned char digest[20], SHA1_CONTEXT* context)
	{
		quint32 i;
		unsigned char finalcount[8];

		for (i = 0; i < 8; i++) {
			finalcount[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)]
			>> ((3-(i & 3)) * 8) ) & 255);  // Endian independent
		}
		sha1_update(context, (unsigned char *)"\200", 1);
		while ((context->count[0] & 504) != 448) {
			sha1_update(context, (unsigned char *)"\0", 1);
		}
		sha1_update(context, finalcount, 8);  // Should cause a transform()
		for (i = 0; i < 20; i++) {
			digest[i] = (unsigned char) ((context->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);
		}

		// Wipe variables
		i = 0;
		memset(context->buffer, 0, 64);
		memset(context->state, 0, 20);
		memset(context->count, 0, 8);
		memset(&finalcount, 0, 8);
	}
};

//----------------------------------------------------------------------------
// DefaultKeyStoreEntry
//----------------------------------------------------------------------------

// this escapes colons, commas, and newlines.  colons and commas so that they
//   are available as delimiters, and newlines so that our output can be a
//   single line of text.
static QString escape_string(const QString &in)
{
	QString out;
	for(int n = 0; n < in.length(); ++n)
	{
		if(in[n] == '\\')
			out += "\\\\";
		else if(in[n] == ':')
			out += "\\c";
		else if(in[n] == ',')
			out += "\\o";
		else if(in[n] == '\n')
			out += "\\n";
		else
			out += in[n];
	}
	return out;
}

static bool unescape_string(const QString &in, QString *_out)
{
	QString out;
	for(int n = 0; n < in.length(); ++n)
	{
		if(in[n] == '\\')
		{
			if(n + 1 >= in.length())
				return false;

			if(in[n + 1] == '\\')
				out += '\\';
			else if(in[n + 1] == 'c')
				out += ':';
			else if(in[n + 1] == 'o')
				out += ',';
			else if(in[n + 1] == 'n')
				out += '\n';
			else
				return false;
			++n;
		}
		else
			out += in[n];
	}
	*_out = out;
	return true;
}

static QString escape_stringlist(const QStringList &in)
{
	QStringList list;
	for(int n = 0; n < in.count(); ++n)
		list += escape_string(in[n]);
	return list.join(":");
}

static bool unescape_stringlist(const QString &in, QStringList *_out)
{
	QStringList out;
	QStringList list = in.split(':');
	for(int n = 0; n < list.count(); ++n)
	{
		QString str;
		if(!unescape_string(list[n], &str))
			return false;
		out += str;
	}
	*_out = out;
	return true;
}

// serialization format is a colon separated list of 7 escaped strings
//  0 - "qca_def_1" (header)
//  1 - store id
//  2 - store name
//  3 - entry id
//  4 - entry name
//  5 - entry type (e.g. "cert")
//  6 - string encoding of object (e.g. DER encoded in Base64)
static QString entry_serialize(const QString &storeId, const QString &storeName, const QString &entryId, const QString &entryName, const QString &entryType, const QString &data)
{
	QStringList out;
	out += "qca_def";
	out += storeId;
	out += storeName;
	out += entryId;
	out += entryName;
	out += entryType;
	out += data;
	return escape_stringlist(out);
}

static bool entry_deserialize(const QString &in, QString *storeId, QString *storeName, QString *entryId, QString *entryName, QString *entryType, QString *data)
{
	QStringList list;
	if(!unescape_stringlist(in, &list))
		return false;
	if(list.count() != 7)
		return false;
	if(list[0] != "qca_def")
		return false;
	*storeId   = list[1];
	*storeName = list[2];
	*entryId   = list[3];
	*entryName = list[4];
	*entryType = list[5];
	*data      = list[6];
	return true;
}

class DefaultKeyStoreEntry : public KeyStoreEntryContext
{
public:
	KeyStoreEntry::Type _type;
	QString _id, _name, _storeId, _storeName;
	Certificate _cert;
	CRL _crl;
	mutable QString _serialized;

	DefaultKeyStoreEntry(const Certificate &cert, const QString &storeId, const QString &storeName, Provider *p) : KeyStoreEntryContext(p)
	{
		_type = KeyStoreEntry::TypeCertificate;
		_storeId = storeId;
		_storeName = storeName;
		_cert = cert;
	}

	DefaultKeyStoreEntry(const CRL &crl, const QString &storeId, const QString &storeName, Provider *p) : KeyStoreEntryContext(p)
	{
		_type = KeyStoreEntry::TypeCRL;
		_storeId = storeId;
		_storeName = storeName;
		_crl = crl;
	}

	virtual Provider::Context *clone() const
	{
		return new DefaultKeyStoreEntry(*this);
	}

	virtual KeyStoreEntry::Type type() const
	{
		return _type;
	}

	virtual QString id() const
	{
		return _id;
	}

	virtual QString name() const
	{
		return _name;
	}

	virtual QString storeId() const
	{
		return _storeId;
	}

	virtual QString storeName() const
	{
		return _storeName;
	}

	virtual Certificate certificate() const
	{
		return _cert;
	}

	virtual CRL crl() const
	{
		return _crl;
	}

	virtual QString serialize() const
	{
		if(_serialized.isEmpty())
		{
			QString typestr;
			QString datastr;

			if(_type == KeyStoreEntry::TypeCertificate)
			{
				typestr = "cert";
				datastr = Base64().arrayToString(_cert.toDER());
			}
			else
			{
				typestr = "crl";
				datastr = Base64().arrayToString(_crl.toDER());
			}

			_serialized = entry_serialize(_storeId, _storeName, _id, _name, typestr, datastr);
		}

		return _serialized;
	}

	static DefaultKeyStoreEntry *deserialize(const QString &in, Provider *provider)
	{
		QString storeId, storeName, id, name, typestr, datastr;

		if(entry_deserialize(in, &storeId, &storeName, &id, &name, &typestr, &datastr))
		{
			QByteArray data = Base64().stringToArray(datastr).toByteArray();
			DefaultKeyStoreEntry *c;

			if(typestr == "cert")
			{
				Certificate cert = Certificate::fromDER(data);
				if(cert.isNull())
					return 0;
				c = new DefaultKeyStoreEntry(cert, storeId, storeName, provider);
			}
			else if(typestr == "crl")
			{
				CRL crl = CRL::fromDER(data);
				if(crl.isNull())
					return 0;
				c = new DefaultKeyStoreEntry(crl, storeId, storeName, provider);
			}
			else
				return 0;

			c->_id = id;
			c->_name = name;
			c->_serialized = in;
			return c;
		}
		return 0;
	}

	QString simpleId() const
	{
		if(_type == KeyStoreEntry::TypeCertificate)
			return QString::number(qHash(_cert.toDER()));
		else
			return QString::number(qHash(_crl.toDER()));
	}

	QString simpleName() const
	{
		// use the common name, else orgname
		if(_type == KeyStoreEntry::TypeCertificate)
		{
			QString str = _cert.commonName();
			if(str.isEmpty())
				str = _cert.subjectInfo().value(Organization);
			return str;
		}
		else
			return _crl.issuerInfo().value(CommonName);
	}
};

//----------------------------------------------------------------------------
// DefaultKeyStoreList
//----------------------------------------------------------------------------
class DefaultKeyStoreList : public KeyStoreListContext
{
	Q_OBJECT
public:
	bool x509_supported;
	DefaultShared *shared;

	DefaultKeyStoreList(Provider *p, DefaultShared *_shared) : KeyStoreListContext(p), shared(_shared)
	{
	}

	~DefaultKeyStoreList()
	{
	}

	virtual Provider::Context *clone() const
	{
		return 0;
	}

	virtual void start()
	{
		x509_supported = false;

		QMetaObject::invokeMethod(this, "busyEnd", Qt::QueuedConnection);
	}

	virtual QList<int> keyStores()
	{
		if(!x509_supported)
		{
			if(isSupported("cert") && isSupported("crl"))
				x509_supported = true;
		}

		bool have_systemstore = false;
#ifndef QCA_NO_SYSTEMSTORE
		if(shared->use_system())
			have_systemstore = qca_have_systemstore();
#endif

		QList<int> list;

		// system store only shows up if the OS store is available or
		//   there is a configured store file
		if(x509_supported && (have_systemstore || !shared->roots_file().isEmpty()))
			list += 0;

		return list;
	}

	virtual KeyStore::Type type(int) const
	{
		return KeyStore::System;
	}

	virtual QString storeId(int) const
	{
		return "qca-default-systemstore";
	}

	virtual QString name(int) const
	{
		return "System Trusted Certificates";
	}

	virtual QList<KeyStoreEntry::Type> entryTypes(int) const
	{
		QList<KeyStoreEntry::Type> list;
		list += KeyStoreEntry::TypeCertificate;
		list += KeyStoreEntry::TypeCRL;
		return list;
	}

	virtual QList<KeyStoreEntryContext*> entryList(int)
	{
		QList<KeyStoreEntryContext*> out;

		QList<Certificate> certs;
		QList<CRL> crls;

		if(shared->use_system())
		{
			CertificateCollection col;
#ifndef QCA_NO_SYSTEMSTORE
			col = qca_get_systemstore(QString());
#endif
			certs += col.certificates();
			crls += col.crls();
		}

		QString roots = shared->roots_file();
		if(!roots.isEmpty())
		{
			CertificateCollection col = CertificateCollection::fromFlatTextFile(roots);
			certs += col.certificates();
			crls += col.crls();
		}

#ifdef FRIENDLY_NAMES
		QStringList names = makeFriendlyNames(certs);
#endif
		for(int n = 0; n < certs.count(); ++n)
		{
			DefaultKeyStoreEntry *c = new DefaultKeyStoreEntry(certs[n], storeId(0), name(0), provider());
			c->_id = c->simpleId();
#ifdef FRIENDLY_NAMES
			c->_name = names[n];
#else
			c->_name = c->simpleName();
#endif
			out.append(c);
		}

		for(int n = 0; n < crls.count(); ++n)
		{
			DefaultKeyStoreEntry *c = new DefaultKeyStoreEntry(crls[n], storeId(0), name(0), provider());
			c->_id = c->simpleId();
			c->_name = c->simpleName();
			out.append(c);
		}

		return out;
	}

	virtual KeyStoreEntryContext *entryPassive(const QString &serialized)
	{
		return DefaultKeyStoreEntry::deserialize(serialized, provider());
	}
};

//----------------------------------------------------------------------------
// DefaultProvider
//----------------------------------------------------------------------------
static bool unescape_config_stringlist(const QString &in, QStringList *_out)
{
	QStringList out;
	QStringList list = in.split(',');
	for(int n = 0; n < list.count(); ++n)
	{
		QString str;
		if(!unescape_string(list[n], &str))
			return false;
		out += str.trimmed();
	}
	*_out = out;
	return true;
}

class DefaultProvider : public Provider
{
public:
	DefaultShared shared;

	virtual void init()
	{
		QDateTime now = QDateTime::currentDateTime();

		uint t = now.toTime_t();
	        if(now.time().msec() > 0)
			t /= now.time().msec();
		qsrand(t);
	}

	virtual int version() const
	{
		return QCA_VERSION;
	}

	virtual int qcaVersion() const
	{
		return QCA_VERSION;
	}

	virtual QString name() const
	{
		return "default";
	}

	virtual QStringList features() const
	{
		QStringList list;
		list += "random";
		list += "md5";
		list += "sha1";
		list += "keystorelist";
		return list;
	}

	virtual Provider::Context *createContext(const QString &type)
	{
		if(type == "random")
			return new DefaultRandomContext(this);
		else if(type == "md5")
			return new DefaultMD5Context(this);
		else if(type == "sha1")
			return new DefaultSHA1Context(this);
		else if(type == "keystorelist")
			return new DefaultKeyStoreList(this, &shared);
		else
			return 0;
	}

	virtual QVariantMap defaultConfig() const
	{
		QVariantMap config;
		config["formtype"] = "http://affinix.com/qca/forms/default#1.0";
		config["use_system"] = true;
		config["roots_file"] = QString();
		config["skip_plugins"] = QString();
		config["plugin_priorities"] = QString();
		return config;
	}

	virtual void configChanged(const QVariantMap &config)
	{
		bool use_system = config["use_system"].toBool();
		QString roots_file = config["roots_file"].toString();
		QString skip_plugins_str = config["skip_plugins"].toString();
		QString plugin_priorities_str = config["plugin_priorities"].toString();

		QStringList tmp;

		QStringList skip_plugins;
		if(unescape_config_stringlist(skip_plugins_str, &tmp))
			skip_plugins = tmp;

		QStringList plugin_priorities;
		if(unescape_config_stringlist(plugin_priorities_str, &tmp))
			plugin_priorities = tmp;

		for(int n = 0; n < plugin_priorities.count(); ++n)
		{
			QString &s = plugin_priorities[n];

			// make sure the entry ends with ":number"
			int x = s.indexOf(':');
			bool ok = false;
			if(x != -1)
				s.mid(x + 1).toInt(&ok);
			if(!ok)
			{
				plugin_priorities.removeAt(n);
				--n;
			}
		}

		shared.set(use_system, roots_file, skip_plugins, plugin_priorities);
	}
};

Provider *create_default_provider()
{
	return new DefaultProvider;
}

QStringList skip_plugins(Provider *defaultProvider)
{
	DefaultProvider *that = (DefaultProvider *)defaultProvider;
	return that->shared.skip_plugins();
}

QStringList plugin_priorities(Provider *defaultProvider)
{
	DefaultProvider *that = (DefaultProvider *)defaultProvider;
	return that->shared.plugin_priorities();
}

#include "qca_default.moc"

}
