/*
 * qca_tools.h - Qt Cryptographic Architecture
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

/**
   \file qca_tools.h

   Header file for "tool" classes used in %QCA

   These classes differ from those in qca_support.h, in that they have
   some cryptographic relationship, and require secure memory.

   \Note You should not use this header directly from an
   application. You should just use <tt> \#include \<QtCrypto>
   </tt> instead.
*/

#ifndef QCA_TOOLS_H
#define QCA_TOOLS_H

#include <QSharedData>
#include <QSharedDataPointer>
#include <QMetaType>
#include "qca_export.h"

class QString;
class QByteArray;
class QTextStream;

/**
   Allocate a block of memory from the secure memory pool.

   This is intended to be used when working with C libraries.

   \param bytes the number of bytes to allocate
*/
QCA_EXPORT void *qca_secure_alloc(int bytes);

/**
   Free (de-allocate) a block of memory that has been previously
   allocated from the secure memory pool.

   This is intended to be used when working with C libraries.

   \param p pointer to the block of memory to be free'd
*/
QCA_EXPORT void qca_secure_free(void *p);

/**
   Resize (re-allocate) a block of memory that has been previously
   allocated from the secure memory pool.

   \param p pointer to the block of memory to be resized.
   \param bytes the new size that is required.
*/
QCA_EXPORT void *qca_secure_realloc(void *p, int bytes);

/**
 \class QSecureArray qca_tools.h QtCrypto

 Secure array of bytes

 The %QSecureArray provides an array of memory from a pool that is,
 at least partly, secure. In this sense, secure means that the contents
 of the memory should not be made available to other applications. By
 comparison, a QMemArray (or subclass such as QCString or QByteArray) may
 be held in pages that might be swapped to disk or free'd without being
 cleared first.
 
 Note that this class is implicitly shared (that is, copy on write).
 **/
class QCA_EXPORT QSecureArray
{
public:
	/**
	 * Construct a secure byte array, zero length
	 */
	QSecureArray();

	/**
	 * Construct a secure byte array of the specified length
	 *
	 * \param size the number of bytes in the array
	 * \param ch the value every byte should be set to
	 */
	QSecureArray(int size, char ch = 0);

	/**
	 * Construct a secure byte array from a string
	 *
	 * Note that this copies, rather than references the source array
	 */
	QSecureArray(const char *str);

	/**
	 * Construct a secure byte array from a QByteArray
	 *
	 * Note that this copies, rather than references the source array
	 *
	 * \sa operator=()
	 */
	QSecureArray(const QByteArray &a);

	/**
	 * Construct a (shallow) copy of another secure byte array
	 *
	 * \param from the source of the data and length.
	 */
	QSecureArray(const QSecureArray &from);

	~QSecureArray();

	/** 
	 * Creates a reference, rather than a deep copy.
	 * if you want a deep copy then you should use copy()
	 * instead, or use operator=(), then call detach() when required.
	 */
	QSecureArray & operator=(const QSecureArray &from);

	/**
	 * Creates a copy, rather than references
	 *
	 * \param a the array to copy from
	 */
	QSecureArray & operator=(const QByteArray &a);

	/**
	 * Clears the contents of the array and makes it empty
	 */
	void clear();

	/**
	 * Returns a reference to the byte at the index position
	 *
	 * \param index the zero-based offset to obtain
	 */
	char & operator[](int index);

	/**
	 * Returns a reference to the byte at the index position
	 *
	 * \param index the zero-based offset to obtain
	 */
	const char & operator[](int index) const;

	/**
	 * Pointer to the data in the secure array
	 * 
	 * You can use this for memcpy and similar functions. If you are trying
	 * to obtain data at a particular offset, you might be better off using
	 * at() or operator[]
	 *
	 */
	char *data();

	/**
	 * Pointer to the data in the secure array
	 * 
	 * You can use this for memcpy and similar functions. If you are trying
	 * to obtain data at a particular offset, you might be better off using
	 * at() or operator[]
	 *
	 */
	const char *data() const;

	/**
	 * Pointer to the data in the secure array
	 * 
	 * You can use this for memcpy and similar functions. If you are trying
	 * to obtain data at a particular offset, you might be better off using
	 * at() or operator[]
	 *
	 */
	const char *constData() const;

	/**
	 * Returns a reference to the byte at the index position
	 *
	 * \param index the zero-based offset to obtain
	 */
	char & at(int index);

	/**
	 * Returns a reference to the byte at the index position
	 *
	 * \param index the zero-based offset to obtain
	 */
	const char & at(int index) const;

	/**
	 * Returns the number of bytes in the array
	 */
	int size() const;

	/**
	 * Test if the array contains any bytes.
	 * 
	 * This is equivalent to testing (size() != 0). Note that if
	 * the array is allocated, isEmpty() is false (even if no data
	 * has been added)
	 *
	 * \return true if the array has zero length, otherwise false
	 */
	bool isEmpty() const;

	/**
	 * Change the length of this array
	 * If the new length is less than the old length, the extra information
	 * is (safely) discarded. If the new length is equal to or greater than
	 * the old length, the existing data is copied into the array.
	 *
	 * \param size the new length
	 */
	bool resize(int size);

        /**
         * Fill the data array with a specified character
	 *
	 * \param fillChar the character to use as the fill
	 * \param fillToPosition the number of characters to fill
	 *        to. If not specified (or -1), fills array to
	 *        current length.
	 *
	 * \note This function does not extend the array - if
	 * you ask for fill beyond the current length, only
	 * the current length will be used.
	 * \note The number of characters is 1 based, so if
	 * you ask for fill('x', 10), it will fill from
	 * 
         */
        void fill(char fillChar, int fillToPosition = -1);

	/**
	  * Copy the contents of the secure array out to a 
	  * standard QByteArray. Note that this performs a deep copy
	  * of the data.
	  */
	QByteArray toByteArray() const;

	/**
	 * Append a secure byte array to the end of this array
	 */
	QSecureArray & append(const QSecureArray &a);

	/**
	 * Append a secure byte array to the end of this array
	 */
	QSecureArray & operator+=(const QSecureArray &a);

protected:
	/**
	 * Assign the contents of a provided byte array to this
	 * object.
	 *
	 * \param from the byte array to copy
	 */
	void set(const QSecureArray &from);

	/**
	 * Assign the contents of a provided byte array to this
	 * object.
	 *
	 * \param from the byte array to copy
	 */
	void set(const QByteArray &from);

private:
	class Private;
	QSharedDataPointer<Private> d;
};

Q_DECLARE_METATYPE(QSecureArray)

/**
 * Equality operator. Returns true if the two QSecureArray
 * arguments have the same data (and the same length, of course).
 *
 * \relates QSecureArray
 **/
QCA_EXPORT bool operator==(const QSecureArray &a, const QSecureArray &b);

/**
 * Inequality operator. Returns true if the two QSecureArray
 * arguments have different length, or the same length but
 * different data
 *
 * \relates QSecureArray
 **/
QCA_EXPORT bool operator!=(const QSecureArray &a, const QSecureArray &b);

/**
 * Returns an array that is the result of concatenating a and b
 *
 * \relates QSecureArray
 **/
QCA_EXPORT const QSecureArray operator+(const QSecureArray &a, const QSecureArray &b);

/**
   \class QBigInteger qca_tools.h QtCrypto

   Arbitrary precision integer

   QBigInteger provides arbitrary precision integers.
   \code
   if ( QBigInteger("3499543804349") == 
       QBigInteger("38493290803248") + QBigInteger( 343 ) )
   {
       // do something
   }
   \endcode
 **/
class QCA_EXPORT QBigInteger
{
public:
	/**
	 * Constructor. Creates a new QBigInteger, initialised to zero.
	 */
	QBigInteger();

	/**
	 * \overload
	 *
	 * \param n an alternative integer initialisation value.
	 */
	QBigInteger(int n);

	/**
	 * \overload
	 *
	 * \param c an alternative initialisation value, encoded as a character array
	 *
	 * \code
	 * QBigInteger b ( "9890343" );
	 * \endcode
	 */
	QBigInteger(const char *c);

	/**
	 * \overload
	 *
	 * \param s an alternative initialisation value, encoded as a string
	 */
	QBigInteger(const QString &s);

	/**
	 * \overload
	 *
	 * \param a an alternative initialisation value, encoded as QSecureArray
	 */
	QBigInteger(const QSecureArray &a);

	/**
	 * \overload
	 *
	 * \param from an alternative initialisation value, encoded as a %QBigInteger
	 */
	QBigInteger(const QBigInteger &from);

	~QBigInteger();

	/**
	 * Assignment operator
	 * 
	 * \param from the QBigInteger to copy from
	 *
	 * \code
	 * QBigInteger a; // a is zero
	 * QBigInteger b( 500 );
	 * a = b; // a is now 500
	 * \endcode
	 */
	QBigInteger & operator=(const QBigInteger &from);

	/**
	 * \overload
	 *
	 * \param s the QString containing an integer representation
	 *
	 * \sa bool fromString(const QString &s)
	 *
	 * \note it is the application's responsibility to make sure
	 * that the QString represents a valid integer (ie it only
	 * contains numbers and an optional minus sign at the start)
	 * 
	 **/
	QBigInteger & operator=(const QString &s);

	/**
	 * Increment in place operator
	 *
	 * \param b the amount to increment by
	 *
	 * \code
	 * QBigInteger a; // a is zero
	 * QBigInteger b( 500 );
	 * a += b; // a is now 500
	 * a += b; // a is now 1000
	 * \endcode
	 **/
	QBigInteger & operator+=(const QBigInteger &b);

	/**
	 * Decrement in place operator
	 *
	 * \param b the amount to decrement by
	 *
	 * \code
	 * QBigInteger a; // a is zero
	 * QBigInteger b( 500 );
	 * a -= b; // a is now -500
	 * a -= b; // a is now -1000
	 * \endcode
	 **/
	QBigInteger & operator-=(const QBigInteger &b);

	/** 
	 * Output %QBigInteger as a byte array, useful for storage or
	 * transmission.  The format is a binary integer in sign-extended
	 * network-byte-order.
	 *
	 * \sa void fromArray(const QSecureArray &a);
	 */
	QSecureArray toArray() const;

	/**
	 * Assign from an array.  The input is expected to be a binary integer
	 * in sign-extended network-byte-order.
	 *
	 * \param a a QSecureArray that represents an integer
	 *
	 * \sa QBigInteger(const QSecureArray &a);
	 * \sa QSecureArray toArray() const;
	 */
	void fromArray(const QSecureArray &a);

	/** 
	 * Convert %QBigInteger to a QString
	 *
	 * \code
	 * QString aString;
	 * QBigInteger aBiggishInteger( 5878990 );
	 * aString = aBiggishInteger.toString(); // aString is now "5878990"
	 * \endcode
	 */
	QString toString() const;

	/**
	 * Assign from a QString
	 *
	 * \param s a QString that represents an integer
	 *
	 * \note it is the application's responsibility to make sure
	 * that the QString represents a valid integer (ie it only
	 * contains numbers and an optional minus sign at the start)
	 * 
	 * \sa QBigInteger(const QString &s)
	 * \sa QBigInteger & operator=(const QString &s)
	 */
	bool fromString(const QString &s);

	/** 
	 * Compare this value with another %QBigInteger
	 *
	 * Normally it is more readable to use one of the operator overloads,
	 * so you don't need to use this method directly.
	 *
	 * \param n the QBigInteger to compare with
	 *
	 * \return zero if the values are the same, negative if the argument
	 * is less than the value of this QBigInteger, and positive if the argument
	 * value is greater than this QBigInteger
	 *
	 * \code
	 * QBigInteger a( "400" );
	 * QBigInteger b( "-400" );
	 * QBigInteger c( " 200 " );
	 * int result;
	 * result = a.compare( b );        // return positive 400 > -400
	 * result = a.compare( c );        // return positive,  400 > 200
	 * result = b.compare( c );        // return negative, -400 < 200
	 * \endcode
	 **/
	int compare(const QBigInteger &n) const;

private:
	class Private;
	QSharedDataPointer<Private> d;
};

Q_DECLARE_METATYPE(QBigInteger)

/**
 * Equality operator. Returns true if the two QBigInteger values
 * are the same, including having the same sign. 
 *
 * \relates QBigInteger
 **/
inline bool operator==(const QBigInteger &a, const QBigInteger &b)
{
	return (0 == a.compare( b ) );
}

/**
 * Inequality operator. Returns true if the two QBigInteger values
 * are different in magnitude, sign or both  
 *
 * \relates QBigInteger
 **/
inline bool operator!=(const QBigInteger &a, const QBigInteger &b)
{
	return (0 != a.compare( b ) );
}

/**
 * Less than or equal operator. Returns true if the QBigInteger value
 * on the left hand side is equal to or less than the QBigInteger value
 * on the right hand side.
 *
 * \relates QBigInteger
 **/
inline bool operator<=(const QBigInteger &a, const QBigInteger &b)
{
	return (a.compare( b ) <= 0 );
}

/**
 * Greater than or equal operator. Returns true if the QBigInteger value
 * on the left hand side is equal to or greater than the QBigInteger value
 * on the right hand side.
 *
 * \relates QBigInteger
 **/
inline bool operator>=(const QBigInteger &a, const QBigInteger &b)
{
	return (a.compare( b ) >= 0 );
}

/**
 * Less than operator. Returns true if the QBigInteger value
 * on the left hand side is less than the QBigInteger value
 * on the right hand side.
 *
 * \relates QBigInteger
 **/
inline bool operator<(const QBigInteger &a, const QBigInteger &b)
{
	return (a.compare( b ) < 0 );
}

/**
 * Greater than operator. Returns true if the QBigInteger value
 * on the left hand side is greater than the QBigInteger value
 * on the right hand side.
 *
 * \relates QBigInteger
 **/
inline bool operator>(const QBigInteger &a, const QBigInteger &b)
{
	return (a.compare( b ) > 0 );
}

/**
 * Stream operator.
 *
 * \relates QBigInteger
 **/
QCA_EXPORT QTextStream &operator<<(QTextStream &stream, const QBigInteger &b);

#endif
