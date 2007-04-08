/*
 * qca_basic.h - Qt Cryptographic Architecture
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

/**
   \file qca_basic.h

   Header file for classes for cryptographic primitives (basic operations)

   \note You should not use this header directly from an
   application. You should just use <tt> \#include \<QtCrypto>
   </tt> instead.
*/

#ifndef QCA_BASIC_H
#define QCA_BASIC_H

#include "qca_core.h"

namespace QCA
{
	/**
	   \class Random qca_basic.h QtCrypto

	   Source of random numbers

	   QCA provides a built in source of random numbers, which
	   can be accessed through this class. You can also use
	   an alternative random number source, by implementing
	   another provider.
	 
	   You can select the "quality" of the random numbers. For 
	   best results, you should use Nonce or PublicValue for values
	   that are likely to become public, and SessionKey or LongTermKey
	   for those values that are more critical. All that said, please
	   note that this is only a hint to the provider - it may make
	   no difference at all.
	 
	   The normal use of this class is expected to be through the
	   static members - randomChar(), randomInt() and randomArray().
	 */
	class QCA_EXPORT Random : public Algorithm
	{
	public:
		/**
		 * How much entropy to use for the random numbers that
		 * are required.
		 */
		enum Quality
		{
			Nonce,      ///< Low quality, will become public
			PublicValue,///< Will become public
			SessionKey, ///< Intended to remain private
			LongTermKey ///< Best available quality
		};

		/**
		 * Standard Constructor
		 *
		 * \param provider the provider library for the random
		 *                 number generation
		 */ 
		Random(const QString &provider = QString());

		/**
		 * Provide a random byte.
		 *
		 * This method isn't normally required - you should use
		 * the static randomChar() method instead.
		 * 
		 * \param q the quality of the random byte that is required
		 *
		 * \sa randomChar
		 */
		uchar nextByte(Quality q = SessionKey);

		/**
		 * Provide a specified number of random bytes
		 *
		 * This method isn't normally required - you should use
		 * the static randomArray() method instead.
		 *
		 * \param size the number of bytes to provide
		 * \param q the quality of the random bytes that are required
		 *
		 * \sa randomArray
		 */
		QSecureArray nextBytes(int size, Quality q = SessionKey);

		/**
		 * Provide a random character (byte)
		 *
		 * This is the normal way of obtaining a single random char
		 * (ie. 8 bit byte), of the default quality, as shown below:
		 * \code
		 * myRandomChar = QCA::Random::randomChar();
		 * \endcode
		 * 
		 * \param q the quality of the random character that is required
		 *
		 * If you need a number of bytes, perhaps randomArray() may be of use
		 */
		static uchar randomChar(Quality q = SessionKey);

		/**
		 * Provide a random integer
		 *
		 * This is the normal way of obtaining a single random integer,
		 * as shown below:
		 * \code
		 * // default quality
		 * myRandomInt = QCA::Random::randomInt();
		 * // cheap integer
		 * myCheapInt = QCA::Random::randomInt( QCA::Random::Nonce );
		 * \endcode
		 *
		 * \param q the quality of the random integer that is required
		 */
		static uint randomInt(Quality q = SessionKey);

		/**
		 * Provide a specified number of random bytes
		 * 
		 * \code
		 * // build a 30 byte secure array.
		 * QSecureArray arry = QCA::Random::randomArray(30);
		 * // take 20 bytes, as high a quality as we can get
		 * QSecureArray newKey = QCA::Random::randomArray(20, QCA::Random::LongTermKey);
		 * \endcode
		 *
		 * \param size the number of bytes to provide
		 * \param q the quality of the random bytes that are required
		 */
		static QSecureArray randomArray(int size, Quality q = SessionKey);
	};

	/**
	   \class Hash qca_basic.h QtCrypto

	   General superclass for hashing algorithms.

	   Hash is a superclass for the various hashing algorithms
	   within %QCA. You should not need to use it directly unless you are
	   adding another hashing capability to %QCA - you should be
	   using a sub-class. SHA1 or RIPEMD160 are recommended for
	   new applications, although MD2, MD4, MD5 or SHA0 may be
	   applicable (for interoperability reasons) for some
	   applications. 

	   To perform a hash, you create an object (of one of the
	   sub-classes of %Hash), call update() with the data that
	   needs to be hashed, and then call final(), which returns
	   a QByteArray of the hash result. An example (using the SHA1
	   hash, with 1000 updates of a 1000 byte string) is shown below:
	   \code
	   if(!QCA::isSupported("sha1"))
	       printf("SHA1 not supported!\n");
	   else {
	       QByteArray fillerString;
	       fillerString.fill('a', 1000);

	       QCA::SHA1 shaHash;
	       for (int i=0; i<1000; i++)
	           shaHash.update(fillerString);
	       QByteArray hashResult = shaHash.final();
	       if ( "34aa973cd4c4daa4f61eeb2bdbad27316534016f" == QCA::arrayToHex(hashResult) ) {
	           printf("big SHA1 is OK\n");
	       } else {
                   printf("big SHA1 failed\n");
	       }
	   }
	   \endcode

	   If you only have a simple hash requirement - a single
	   string that is fully available in memory at one time - 
	   then you may be better off with one of the convenience
	   methods. So, for example, instead of creating a QCA::SHA1
	   or QCA::MD5 object, then doing a single update() and the final()
	   call; you simply call QCA::SHA1().hash() or
	   QCA::MD5().hash() with the data that you would otherwise
	   have provided to the update() call.
	 */
	class QCA_EXPORT Hash : public Algorithm, public BufferedComputation
	{
	public:
		/**
		 * Reset a hash, dumping all previous parts of the
		 * message.
		 *
		 * This method clears (or resets) the hash algorithm,
		 * effectively undoing any previous update()
		 * calls. You should use this call if you are re-using
		 * a Hash sub-class object to calculate additional
		 * hashes.
		 */
		virtual void clear();

		/**
		 * Update a hash, adding more of the message contents
		 * to the digest. The whole message needs to be added
		 * using this method before you call final(). 
		 *
		 * If you find yourself only calling update() once,
		 * you may be better off using a convenience method
		 * such as hash() or hashToString() instead.
		 *
 		 * \param a the byte array to add to the hash 
		 */
		virtual void update(const QSecureArray &a);

		/**
		 * \overload
		 *
		 * \param a the QByteArray to add to the hash 
		 */
		virtual void update(const QByteArray &a);

		/**
		 * \overload
		 *
		 * This method is provided to assist with code that
		 * already exists, and is being ported to %QCA. You are
		 * better off passing a QSecureArray (as shown above)
		 * if you are writing new code.
		 *
		 * \param data pointer to a char array
		 * \param len the length of the array. If not specified
		 * (or specified as a negative number), the length will be
		 * determined with strlen(), which may not be what you want
		 * if the array contains a null (0x00) character.
		 */
		virtual void update(const char *data, int len = -1);

		/**
		 * \overload
		 *
		 * This allows you to read from a file or other
		 * I/O device. Note that the device must be already
		 * open for reading
		 *
		 * \param file an I/O device
		 *
		 * If you are trying to calculate the hash of
		 * a whole file (and it isn't already open), you
		 * might want to use code like this:
		 * \code
		 * QFile f( "file.dat" );
		 * if ( f1.open( IO_ReadOnly ) ) {
		 *     QCA::SHA1 hashObj;
		 *     hashObj.update( f1 );
		 *     QString output = hashObj.final() ) ),
		 * }
		 * \endcode
		 */
		virtual void update(QIODevice &file);

		/**
		 * Finalises input and returns the hash result
		 *
		 * After calling update() with the required data, the
		 * hash results are finalised and produced.
		 *
		 * Note that it is not possible to add further data (with
		 * update()) after calling final(), because of the way
		 * the hashing works - null bytes are inserted to pad
		 * the results up to a fixed size. If you want to
		 * reuse the Hash object, you should call clear() and
		 * start to update() again.
		 */
		virtual QSecureArray final();

		/**
		 * %Hash a byte array, returning it as another
		 * byte array.
		 * 
		 * This is a convenience method that returns the
		 * hash of a QSecureArray.
		 * 
		 * \code
		 * QSecureArray sampleArray(3);
		 * sampleArray.fill('a');
		 * QSecureArray outputArray = QCA::MD2::hash(sampleArray);
		 * \endcode
		 * 
		 * \param array the QByteArray to hash
		 *
		 * If you need more flexibility (e.g. you are constructing
		 * a large byte array object just to pass it to hash(), then
		 * consider creating an Hash sub-class object, and calling
		 * update() and final().
		 */
		QSecureArray hash(const QSecureArray &array);

		/**
		 * %Hash a byte array, returning it as a printable
		 * string
		 * 
		 * This is a convenience method that returns the
		 * hash of a QSeecureArray as a hexadecimal
		 * representation encoded in a QString.
		 * 
		 * \param array the QByteArray to hash
		 *
		 * If you need more flexibility, you can create a Hash
		 * sub-class object, call Hash::update() as
		 * required, then call Hash::final(), before using
		 * the static arrayToHex() method.
		 */
		QString hashToString(const QSecureArray &array);

	protected:
		/**
		 *  Constructor to override in sub-classes.
		 *
		 * \param type label for the type of hash to be
		 * implemented (eg "sha1" or "md2")
		 * \param provider the name of the provider plugin
		 * for the subclass (eg "qca-openssl")
		 */
		Hash(const QString &type, const QString &provider);
	};

        /** \page padding Padding

	For those Cipher sub-classes that are block based, there are modes
	that require a full block on encryption and decryption - %Cipher Block
	Chaining mode and Electronic Code Book modes are good examples. 
	
	Since real world messages are not always a convenient multiple of a
	block size, we have to adding <i>padding</i>. There are a number of
	padding modes that %QCA supports, including not doing any padding
	at all.
	
	If you are not going to use padding, then you can pass 
	QCA::Cipher::NoPadding as the pad argument to the Cipher sub-class, however
	it is then your responsibility to pass in appropriate data for
	the mode that you are using.

	The most common padding scheme is known as PKCS#7 (also PKCS#1), and
	it specifies that the pad bytes are all equal to the length of the 
	padding ( for example, if you need three pad bytes to complete the block, then
	the padding is 0x03 0x03 0x03 ).

	On encryption, for algorithm / mode combinations that require
	padding, you will get a block of ciphertext when the input plain text block
	is complete. When you call final(), you will get out the ciphertext that
	corresponds to the last bit of plain text, plus any padding. If you had
	provided plaintext that matched up with a block size, then the cipher
	text block is generated from pure padding - you always get at least some
	padding, to ensure that the padding can be safely removed on decryption.
	
	On decryption, for algorithm / mode combinations that use padding,
	you will get back a block of plaintext when the input ciphertext block
	is complete. When you call final(), you will a block that has been stripped
	of ciphertext.
	*/

	/**
	   \class Cipher qca_basic.h QtCrypto

	   General superclass for cipher (encryption / decryption) algorithms.

	   Cipher is a superclass for the various algorithms that perform
	   low level encryption and decryption within %QCA. You should
	   not need to use it directly unless you are
	   adding another capability to %QCA - you should be
	   using a sub-class. AES128, AES192 and AES256 are recommended for
	   new applications.
	 */
	class QCA_EXPORT Cipher : public Algorithm, public Filter
	{
	public:
		/**
		 * Mode settings for cipher algorithms
		 */
		enum Mode
		{
			CBC, ///< operate in %Cipher Block Chaining mode
			CFB, ///< operate in %Cipher FeedBack mode
			ECB, ///< operate in Electronic Code Book mode
			OFB  ///< operate in Output FeedBack Mode
		};

		/**
		 * Padding variations for cipher algorithms
		 */
		enum Padding
		{
			DefaultPadding, ///< Default for cipher-mode
			NoPadding,      ///< Do not use padding
			PKCS7           ///< Pad using the scheme in PKCS#7
		};

		/** 
		 * Standard copy constructor
		 */
		Cipher(const Cipher &from);
		~Cipher();

		/**
		   Assignment operator
		   
		   \param from the Cipher to copy state from
		*/
		Cipher & operator=(const Cipher &from);

		/**
		 * Return acceptable key lengths
		 */
		KeyLength keyLength() const;

		/**
		 * Test if a key length is valid for the cipher algorithm
		 *
		 * \param n the key length in bytes
		 * \return true if the key would be valid for the current algorithm
		 */
		bool validKeyLength(int n) const;

		/**
		 * return the block size for the cipher object
		 */
		uint blockSize() const;

		/**
		 * reset the cipher object, to allow re-use
		 */
		virtual void clear();

		/** 
		 * pass in a byte array of data, which will be encrypted or decrypted
		 * (according to the Direction that was set in the constructor or in
		 * setup() ) and returned.
		 *
		 * \param a the array of data to encrypt / decrypt
		 */
		virtual QSecureArray update(const QSecureArray &a);

		/**
		 * complete the block of data, padding as required, and returning
		 * the completed block
		 */
		virtual QSecureArray final();

		/**

		 Test if an update() or final() call succeeded.
		 
		 \return true if the previous call succeeded
		*/
		virtual bool ok() const;

		/**
		   Reset / reconfigure the Cipher

		   You can use this to re-use an existing Cipher, rather than creating a new object
		   with a slightly different configuration.

		   \param dir the Direction that this Cipher should use (Encode for encryption, Decode for decryption)
		   \param key the SymmetricKey array that is the key
		   \param iv the InitializationVector to use
		*/
		void setup(Direction dir, const SymmetricKey &key, const InitializationVector &iv = InitializationVector());
		/**
		   Construct a Cipher type string

		   \param cipherType the name of the algorithm (eg AES128, DES)
		   \param modeType the mode to operate the cipher in (eg QCA::CBC, QCA::CFB)
		   \param paddingType the padding required (eg QCA::NoPadding, QCA::PCKS7)
		*/
		static QString withAlgorithms(const QString &cipherType, Mode modeType, Padding paddingType);

	protected:
		/**
		   Standard constructor

		   \param type the name of the cipher specialisation to use
		   \param dir the Direction that this Cipher should use (Encode for encryption, Decode for decryption)
		   \param key the SymmetricKey array that is the key
		   \param iv the InitializationVector to use
		   \param provider the name of the Provider to use

		   \note Padding only applies to CBC and ECB modes.  CFB and OFB ciphertext is always
		   the length of the plaintext.
		*/
		Cipher(const QString &type, Direction dir, const SymmetricKey &key, const InitializationVector &iv, const QString &provider);

	private:
		class Private;
		Private *d;
	};

	/**
	   \class MessageAuthenticationCode  qca_basic.h QtCrypto

	   General superclass for message authentication code (MAC) algorithms.

	   MessageAuthenticationCode is a superclass for the various 
	   message authentication code algorithms within %QCA. You should
	   not need to use it directly unless you are
	   adding another message authentication code capability to %QCA - you should be
	   using a sub-class. HMAC using SHA1 is recommended for new applications.
	 */
	class QCA_EXPORT MessageAuthenticationCode : public Algorithm, public BufferedComputation
	{
	public:
		/**
		 * Standard copy constructor
		 */
		MessageAuthenticationCode(const MessageAuthenticationCode &from);

		~MessageAuthenticationCode();

		/**
		 * Assignment operator.
		 *
		 * Copies the state (including key) from one MessageAuthenticationCode
		 * to another
		 */
		MessageAuthenticationCode & operator=(const MessageAuthenticationCode &from);

		/**
		 * Return acceptable key lengths
		 */
		KeyLength keyLength() const;

		/**
		 * Test if a key length is valid for the MAC algorithm
		 *
		 * \param n the key length in bytes
		 * \return true if the key would be valid for the current algorithm
		 */
		bool validKeyLength(int n) const;
		
		/**
		 * Reset a MessageAuthenticationCode, dumping all
		 * previous parts of the message.
		 *
		 * This method clears (or resets) the algorithm,
		 * effectively undoing any previous update()
		 * calls. You should use this call if you are re-using
		 * a %MessageAuthenticationCode sub-class object
		 * to calculate additional MACs. Note that if the key
		 * doesn't need to be changed, you don't need to call
		 * setup() again, since the key can just be reused.
		 */
		virtual void clear();

		/**
		 * Update the MAC, adding more of the message contents
		 * to the digest. The whole message needs to be added
		 * using this method before you call final(). 
		 *
		 * \param array the message contents
		 */
		virtual void update(const QSecureArray &array);

		/**
		 * Finalises input and returns the MAC result
		 *
		 * After calling update() with the required data, the
		 * hash results are finalised and produced.
		 *
		 * Note that it is not possible to add further data (with
		 * update()) after calling final(). If you want to
		 * reuse the %MessageAuthenticationCode object, you
		 * should call clear() and start to update() again.
		 */
		virtual QSecureArray final();

		/**
		 * Initialise the MAC algorithm.
		 *
		 * \param key the key to use for the algorithm
		 */
		void setup(const SymmetricKey &key);

		/**
		 * Construct the name of the algorithm
		 *
		 * You can use this to build a standard name string.
		 * You probably only need this method if you are 
		 * creating a new subclass.
		 */
		static QString withAlgorithm(const QString &macType, const QString &algType);

	protected:
		/**
		 * Special constructor for subclass initialisation
		 *
		 * To create HMAC with a default algorithm of "sha1", you would use something like:
		 * \code
		 * HMAC(const QString &hash = "sha1", const SymmetricKey &key = SymmetricKey(), const QString &provider = QString())
		 * : MessageAuthenticationCode(withAlgorithm("hmac", hash), key, provider)
		 * {
		 * }
		 * \endcode
		 *
		 * \note The HMAC subclass is already provided in QCA - you don't need to create
		 * your own.
		 */
		MessageAuthenticationCode(const QString &type, const SymmetricKey &key, const QString &provider);

	private:
		class Private;
		Private *d;
	};


	/**
	 * \class SHA0  qca_basic.h QtCrypto
	 *
	 * SHA-0 cryptographic message digest hash algorithm.
	 *
	 * %SHA0 is a 160 bit hashing function, no longer recommended
	 * for new applications because of known (partial) attacks
	 * against it.
	 *
	 * You can use this class in two basic ways - standard member
	 * methods, and convenience methods. Both are shown in
	 * the example below.
	 *
	 * \code
	 *        if(!QCA::isSupported("sha0"))
	 *                printf("SHA0 not supported!\n");
	 *        else {
	 *                QCString actualResult;
	 *                actualResult = QCA::SHA0().hashToString(message);
	 *
	 *                // normal methods - update() and final()
	 *                QByteArray fillerString;
	 *                fillerString.fill('a', 1000);
	 *                QCA::SHA0 shaHash;
	 *                for (int i=0; i<1000; i++)
	 *                        shaHash.update(fillerString);
	 *                QByteArray hashResult = shaHash.final();
	 *        }
	 * \endcode
	 *
	 */
	class QCA_EXPORT SHA0 : public Hash
	{
	public:
		/**
		 * Standard constructor
		 *
		 * This is the normal way of creating a SHA-0 hash,
		 * although if you have the whole message in memory at
		 * one time, you may be better off using QCA::SHA0().hash()
		 *
		 * \param provider specify a particular provider 
		 * to use. For example if you wanted the SHA0 implementation
		 * from qca-openssl, you would use SHA0("qca-openssl")
		 */
		SHA0(const QString &provider = QString()) : Hash("sha0", provider) {}
	};

	/**
	 * \class SHA1  qca_basic.h QtCrypto
	 *
	 * SHA-1 cryptographic message digest hash algorithm.
	 *
	 * This algorithm takes an arbitrary data stream, known as the
	 * message (up to \f$2^{64}\f$ bits in length) and outputs a
	 * condensed 160 bit (20 byte) representation of that data
	 * stream, known as the message digest.
	 *
	 * This algorithm is considered secure in that it is considered
	 * computationally infeasible to find the message that
	 * produced the message digest.
	 *
	 * You can use this class in two basic ways - standard member
	 * methods, and convenience methods. Both are shown in
	 * the example below.
	 *
	 * \code
	 *        if(!QCA::isSupported("sha1"))
	 *                printf("SHA1 not supported!\n");
	 *        else {
	 *                QCString actualResult;
	 *                actualResult = QCA::SHA1().hashToString(message);
	 *
	 *                // normal methods - update() and final()
	 *                QByteArray fillerString;
	 *                fillerString.fill('a', 1000);
	 *                QCA::SHA1 shaHash;
	 *                for (int i=0; i<1000; i++)
	 *                        shaHash.update(fillerString);
	 *                QByteArray hashResult = shaHash.final();
	 *        }
	 * \endcode
	 *
	 * For more information, see Federal Information Processing
	 * Standard Publication 180-2 "Specifications for the Secure
	 * %Hash Standard", available from http://csrc.nist.gov/publications/
	 */
	class QCA_EXPORT SHA1 : public Hash
	{
	public:
		/**
		 * Standard constructor
		 *
		 * This is the normal way of creating a SHA-1 hash,
		 * although if you have the whole message in memory at
		 * one time, you may be better off using QCA::SHA1().hash()
		 *
		 * \param provider specify a particular provider 
		 * to use. For example if you wanted the SHA1 implementation
		 * from qca-openssl, you would use SHA1("qca-openssl")
		 */
		SHA1(const QString &provider = QString()) : Hash("sha1", provider) {}
	};

	/**
	   \class SHA224  qca_basic.h QtCrypto
	   
	   SHA-224 cryptographic message digest hash algorithm.

	   This algorithm takes an arbitrary data stream, known as the
	   message (up to \f$2^{64}\f$ bits in length) and outputs a
	   condensed 224 bit (28 byte) representation of that data
	   stream, known as the message digest.
	   
	   This algorithm is considered secure in that it is considered
	   computationally infeasible to find the message that
	   produced the message digest.

	   For more information, see Federal Information Processing
	   Standard Publication 180-2 "Specifications for the Secure
	   %Hash Standard", with change notice 1
	   available from http://csrc.nist.gov/publications/
	 */
	class QCA_EXPORT SHA224 : public Hash
	{
	public:
		/**
		   Standard constructor

		   This is the normal way of creating a SHA224 hash,
		   although if you have the whole message in memory at
		   one time, you may be better off using
		   QCA::SHA224().hash()

		   \param provider specify a particular provider 
		   to use. For example if you wanted the SHA224 implementation
		   from qca-gcrypt, you would use SHA224("qca-gcrypt")
		 */
		SHA224(const QString &provider = QString()) : Hash("sha224", provider) {}
	};

	/**
	 * \class SHA256  qca_basic.h QtCrypto
	 *
	 * SHA-256 cryptographic message digest hash algorithm.
	 *
	 * This algorithm takes an arbitrary data stream, known as the
	 * message (up to \f$2^{64}\f$ bits in length) and outputs a
	 * condensed 256 bit (32 byte) representation of that data
	 * stream, known as the message digest.
	 *
	 * This algorithm is considered secure in that it is considered
	 * computationally infeasible to find the message that
	 * produced the message digest.
	 *
	 * For more information, see Federal Information Processing
	 * Standard Publication 180-2 "Specifications for the Secure
	 * %Hash Standard", available from http://csrc.nist.gov/publications/
	 */
	class QCA_EXPORT SHA256 : public Hash
	{
	public:
		/**
		 * Standard constructor
		 *
		 * This is the normal way of creating a SHA256 hash,
		 * although if you have the whole message in memory at
		 * one time, you may be better off using
		 * QCA::SHA256().hash()
		 *
		 * \param provider specify a particular provider 
		 * to use. For example if you wanted the SHA256 implementation
		 * from qca-gcrypt, you would use SHA256("qca-gcrypt")
		 */
		SHA256(const QString &provider = QString()) : Hash("sha256", provider) {}
	};

	/**
	 * \class SHA384  qca_basic.h QtCrypto
	 *
	 * SHA-384 cryptographic message digest hash algorithm.
	 *
	 * This algorithm takes an arbitrary data stream, known as the
	 * message (up to \f$2^{128}\f$ bits in length) and outputs a
	 * condensed 384 bit (48 byte) representation of that data
	 * stream, known as the message digest.
	 *
	 * This algorithm is considered secure in that it is considered
	 * computationally infeasible to find the message that
	 * produced the message digest.
	 *
	 * For more information, see Federal Information Processing
	 * Standard Publication 180-2 "Specifications for the Secure
	 * %Hash Standard", available from http://csrc.nist.gov/publications/
	 */
	class QCA_EXPORT SHA384 : public Hash
	{
	public:
		/**
		 * Standard constructor
		 *
		 * This is the normal way of creating a SHA384 hash,
		 * although if you have the whole message in memory at
		 * one time, you may be better off using
		 * QCA::SHA384().hash()
		 *
		 * \param provider specify a particular provider 
		 * to use. For example if you wanted the SHA384 implementation
		 * from qca-gcrypt, you would use SHA384("qca-gcrypt")
		 */
		SHA384(const QString &provider = QString()) : Hash("sha384", provider) {}
	};

	/**
	 * \class SHA512  qca_basic.h QtCrypto
	 *
	 * SHA-512 cryptographic message digest hash algorithm.
	 *
	 * This algorithm takes an arbitrary data stream, known as the
	 * message (up to \f$2^{128}\f$ bits in length) and outputs a
	 * condensed 512 bit (64 byte) representation of that data
	 * stream, known as the message digest.
	 *
	 * This algorithm is considered secure in that it is considered
	 * computationally infeasible to find the message that
	 * produced the message digest.
	 *
	 * For more information, see Federal Information Processing
	 * Standard Publication 180-2 "Specifications for the Secure
	 * %Hash Standard", available from http://csrc.nist.gov/publications/
	 */
	class QCA_EXPORT SHA512 : public Hash
	{
	public:
		/**
		 * Standard constructor
		 *
		 * This is the normal way of creating a SHA512 hash,
		 * although if you have the whole message in memory at
		 * one time, you may be better off using
		 * QCA::SHA512().hash()
		 *
		 * \param provider specify a particular provider 
		 * to use. For example if you wanted the SHA512 implementation
		 * from qca-gcrypt, you would use SHA512("qca-gcrypt")
		 */
		SHA512(const QString &provider = QString()) : Hash("sha512", provider) {}
	};

	/**
	 * \class MD2  qca_basic.h QtCrypto
	 *
	 * %MD2 cryptographic message digest hash algorithm.
	 *
	 * This algorithm takes an arbitrary data stream, known as the
	 * message and outputs a
	 * condensed 128 bit (16 byte) representation of that data
	 * stream, known as the message digest.
	 *
	 * This algorithm is considered slightly more secure than MD5,
	 * but is more expensive to compute. Unless backward
	 * compatibility or interoperability are considerations, you
	 * are better off using the SHA1 or RIPEMD160 hashing algorithms.
	 *
	 * For more information, see B. Kalinski RFC1319 "The %MD2
	 * Message-Digest Algorithm".
	 */
	class QCA_EXPORT MD2 : public Hash
	{
	public:
		/**
		 * Standard constructor
		 *
		 * This is the normal way of creating an MD-2 hash,
		 * although if you have the whole message in memory at
		 * one time, you may be better off using QCA::MD2().hash()
		 *
		 * \param provider specify a particular provider 
		 * to use. For example if you wanted the MD2 implementation
		 * from qca-openssl, you would use MD2("qca-openssl")
		 */
		MD2(const QString &provider = QString()) : Hash("md2", provider) {}
	};

	/**
	 * \class MD4  qca_basic.h QtCrypto
	 *
	 * %MD4 cryptographic message digest hash algorithm.
	 *
	 * This algorithm takes an arbitrary data stream, known as the
	 * message and outputs a
	 * condensed 128 bit (16 byte) representation of that data
	 * stream, known as the message digest.
	 *
	 * This algorithm is not considered to be secure, based on
	 * known attacks. It should only be used for
	 * applications where collision attacks are not a
	 * consideration (for example, as used in the rsync algorithm
	 * for fingerprinting blocks of data). If a secure hash is
	 * required, you are better off using the SHA1 or RIPEMD160
	 * hashing algorithms. MD2 and MD5 are both stronger 128 bit
	 * hashes.
	 *
	 * For more information, see R. Rivest RFC1320 "The %MD4
	 * Message-Digest Algorithm".
	 */
	class QCA_EXPORT MD4 : public Hash
	{
	public:
		/**
		 * Standard constructor
		 *
		 * This is the normal way of creating an MD-4 hash,
		 * although if you have the whole message in memory at
		 * one time, you may be better off using QCA::MD4().hash()
		 *
		 * \param provider specify a particular provider 
		 * to use. For example if you wanted the MD4 implementation
		 * from qca-openssl, you would use MD4("qca-openssl")
		 */
		MD4(const QString &provider = QString()) : Hash("md4", provider) {}
	};

	/**
	 * \class MD5  qca_basic.h QtCrypto
	 *
	 * %MD5 cryptographic message digest hash algorithm.
	 *
	 * This algorithm takes an arbitrary data stream, known as the
	 * message and outputs a
	 * condensed 128 bit (16 byte) representation of that data
	 * stream, known as the message digest.
	 *
	 * This algorithm is not considered to be secure, based on
	 * known attacks. It should only be used for
	 * applications where collision attacks are not a
	 * consideration. If a secure hash is
	 * required, you are better off using the SHA1 or RIPEMD160
	 * hashing algorithms.
	 *
	 * For more information, see R. Rivest RFC1321 "The %MD5
	 * Message-Digest Algorithm".
	 */
	class QCA_EXPORT MD5 : public Hash
	{
	public:
		/**
		 * Standard constructor
		 *
		 * This is the normal way of creating an MD-5 hash,
		 * although if you have the whole message in memory at
		 * one time, you may be better off using QCA::MD5().hash()
		 *
		 * \param provider specify a particular provider 
		 * to use. For example if you wanted the MD5 implementation
		 * from qca-openssl, you would use MD5("qca-openssl")
		 */
		MD5(const QString &provider = QString()) : Hash("md5", provider) {}
	};

	/**
	 * \class RIPEMD160  qca_basic.h QtCrypto
	 *
	 * %RIPEMD160 cryptographic message digest hash algorithm.
	 *
	 * This algorithm takes an arbitrary data stream, known as the
	 * message (up to \f$2^{64}\f$ bits in length) and outputs a
	 * condensed 160 bit (20 byte) representation of that data
	 * stream, known as the message digest.
	 *
	 * This algorithm is considered secure in that it is considered
	 * computationally infeasible to find the message that
	 * produced the message digest.
	 *
	 * You can use this class in two basic ways - standard member
	 * methods, and convenience methods. Both are shown in
	 * the example below.
	 *
	 * \code
	 *        if(!QCA::isSupported("ripemd160")
	 *                printf("RIPEMD-160 not supported!\n");
	 *        else {
	 *                QCString actualResult;
	 *                actualResult = QCA::RIPEMD160().hashToString(message);
	 *
	 *                // normal methods - update() and final()
	 *                QByteArray fillerString;
	 *                fillerString.fill('a', 1000);
	 *                QCA::RIPEMD160 ripemdHash;
	 *                for (int i=0; i<1000; i++)
	 *                        ripemdHash.update(fillerString);
	 *                QByteArray hashResult = ripemdHash.final();
	 *        }
	 * \endcode
	 *
	 */
	class QCA_EXPORT RIPEMD160 : public Hash
	{
	public:
		/**
		 * Standard constructor
		 *
		 * This is the normal way of creating a RIPEMD160 hash,
		 * although if you have the whole message in memory at
		 * one time, you may be better off using
		 * QCA::RIPEMD160().hash()
		 *
		 * \param provider specify a particular provider 
		 * to use. For example if you wanted the RIPEMD160
		 * implementation from qca-openssl, you would use 
		 * RIPEMD160("qca-openssl")
		 */
		RIPEMD160(const QString &provider = QString()) : Hash("ripemd160", provider) {}
	};

	/**
	 * \class BlowFish qca_basic.h QtCrypto
	 *
	 * Bruce Schneier's %BlowFish %Cipher
	 *
	 */
	class QCA_EXPORT BlowFish : public Cipher
	{
	public:
		/**
		 * Standard constructor
		 *
		 * This is the normal way of creating a %BlowFish encryption or decryption object.
		 *
		 * \param m the Mode to operate in
		 * \param dir whether this object should encrypt (QCA::Encode) or decypt (QCA::Decode)
		 * \param key the key to use. 
		 * \param iv the initialisation vector to use. Ignored for ECB mode.
		 * \param pad the type of padding to apply (or remove, for decryption). Ignored if the 
		 *        Mode does not require padding
		 * \param provider the provider to use (eg "qca-gcrypt" )
		 *
		 */
		BlowFish(Mode m, Padding pad = DefaultPadding, Direction dir = Encode, const SymmetricKey &key = SymmetricKey(), const InitializationVector &iv = InitializationVector(), const QString &provider = QString())
		:Cipher(withAlgorithms("blowfish", m, pad), dir, key, iv, provider) {}
	};

	/**
	 * \class TripleDES qca_basic.h QtCrypto
	 *
	 * Triple %DES %Cipher
	 *
	 */
	class QCA_EXPORT TripleDES : public Cipher
	{
		/**
		 * Standard constructor
		 *
		 * This is the normal way of creating a triple %DES encryption or decryption object.
		 *
		 * \param m the Mode to operate in
		 * \param dir whether this object should encrypt (QCA::Encode) or decypt (QCA::Decode)
		 * \param key the key to use. Note that triple %DES requires a 24 byte (192 bit) key,
		 * even though the effective key length is 168 bits.
		 * \param iv the initialisation vector to use. Ignored for ECB mode.
		 * \param pad the type of padding to apply (or remove, for decryption). Ignored if the 
		 *        Mode does not require padding
		 * \param provider the provider to use (eg "qca-gcrypt" )
		 *
		 */
	public:
		TripleDES(Mode m, Padding pad = DefaultPadding, Direction dir = Encode, const SymmetricKey &key = SymmetricKey(), const InitializationVector &iv = InitializationVector(), const QString &provider = QString())
		:Cipher(withAlgorithms("tripledes", m, pad), dir, key, iv, provider) {}
	};

	/**
	 * \class DES  qca_basic.h QtCrypto
	 *
	 * %DES %Cipher
	 *
	 */
	class QCA_EXPORT DES : public Cipher
	{
		/**
		 * Standard constructor
		 *
		 * This is the normal way of creating a %DES encryption or decryption object.
		 *
		 * \param m the Mode to operate in
		 * \param dir whether this object should encrypt (QCA::Encode) or decypt (QCA::Decode)
		 * \param key the key to use. Note that %DES requires a 8 byte (64 bit) key,
		 * even though the effective key length is 56 bits.
		 * \param iv the initialisation vector to use. Ignored for ECB mode.
		 * \param pad the type of padding to apply (or remove, for decryption). Ignored if the 
		 *        Mode does not require padding
		 * \param provider the provider to use (eg "qca-gcrypt" )
		 *
		 */
	public:
		DES(Mode m, Padding pad = DefaultPadding, Direction dir = Encode, const SymmetricKey &key = SymmetricKey(), const InitializationVector &iv = InitializationVector(), const QString &provider = QString())
		:Cipher(withAlgorithms("des", m, pad), dir, key, iv, provider) {}
	};

	/**
	 * \class AES128 qca_basic.h QtCrypto
	 *
	 * Advanced Encryption Standard %Cipher - 128 bits
	 *
	 */
	class QCA_EXPORT AES128 : public Cipher
	{
	public:
		/**
		 * Standard constructor
		 *
		 * This is the normal way of creating a 128 bit 
		 * AES encryption or decryption object.
		 *
		 * \param m the Mode to operate in
		 * \param dir whether this object should encrypt (QCA::Encode) or decypt (QCA::Decode)
		 * \param key the key to use. Note that AES128 requires a 16 byte (128 bit) key.
		 * \param iv the initialisation vector to use. Ignored for ECB mode.
		 * \param pad the type of padding to apply (or remove, for decryption). Ignored if the 
		 *        Mode does not require padding
		 * \param provider the provider to use (eg "qca-gcrypt" )
		 *
		 */
		AES128(Mode m, Padding pad = DefaultPadding, Direction dir = Encode, const SymmetricKey &key = SymmetricKey(), const InitializationVector &iv = InitializationVector(), const QString &provider = QString())
		:Cipher(withAlgorithms("aes128", m, pad), dir, key, iv, provider) {}
	};

	/**
	 * \class AES192 qca_basic.h QtCrypto
	 *
	 * Advanced Encryption Standard %Cipher - 192 bits
	 *
	 */
	class QCA_EXPORT AES192 : public Cipher
	{
	public:
		/**
		 * Standard constructor
		 *
		 * This is the normal way of creating a 192 bit 
		 * AES encryption or decryption object.
		 *
		 * \param m the Mode to operate in
		 * \param dir whether this object should encrypt (QCA::Encode) or decypt (QCA::Decode)
		 * \param key the key to use. Note that AES192 requires a 24 byte (192 bit) key.
		 * \param iv the initialisation vector to use. Ignored for ECB mode.
		 * \param pad the type of padding to apply (or remove, for decryption). Ignored if the 
		 *        Mode does not require padding
		 * \param provider the provider to use (eg "qca-gcrypt" )
		 *
		 */
		AES192(Mode m, Padding pad = DefaultPadding, Direction dir = Encode, const SymmetricKey &key = SymmetricKey(), const InitializationVector &iv = InitializationVector(), const QString &provider = QString())
		:Cipher(withAlgorithms("aes192", m, pad), dir, key, iv, provider) {}
	};

	/**
	 * \class AES256 qca_basic.h QtCrypto
	 *
	 * Advanced Encryption Standard %Cipher - 256 bits
	 *
	 */
	class QCA_EXPORT AES256 : public Cipher
	{
	public:
		/**
		 * Standard constructor
		 *
		 * This is the normal way of creating a 256 bit 
		 * AES encryption or decryption object.
		 *
		 * \param m the Mode to operate in
		 * \param dir whether this object should encrypt (QCA::Encode) or decypt (QCA::Decode)
		 * \param key the key to use. Note that AES256 requires a 32 byte (256 bit) key.
		 * \param iv the initialisation vector to use. Ignored for ECB mode.
		 * \param pad the type of padding to apply (or remove, for decryption). Ignored if the 
		 *        Mode does not require padding
		 * \param provider the provider to use (eg "qca-gcrypt" )
		 *
		 */
		AES256(Mode m, Padding pad = DefaultPadding, Direction dir = Encode, const SymmetricKey &key = SymmetricKey(), const InitializationVector &iv = InitializationVector(), const QString &provider = QString())
		:Cipher(withAlgorithms("aes256", m, pad), dir, key, iv, provider) {}
	};

	/**
	 * \class HMAC  qca_basic.h QtCrypto
	 *
	 * Keyed %Hash message authentication codes
	 *
	 * This algorithm takes an arbitrary data stream, known as the
	 * message and outputs an authentication code for that message.
	 * The authentication code is generated using a secret key in
	 * such a way that the authentication code shows that the 
	 * message has not be altered.
	 *
	 * As an example, to create a MAC using HMAC with SHA1, you
	 * could do the following:
	 * \code
	 * if( QCA::isSupported( "hmac(sha1)" ) ) {
	 *      QCA::HMAC hmacObj; // don't need to specify, "sha1" is default
	 *	hmacObj.setup( key ); // key is a QCA::SymmetricKey, set elsewhere
	 *                            // could also be done in constructor
	 *    	hmacObj.update( dataArray ); // dataArray is a QSecureArray, set elsewhere
	 *	output = hmacObj.final();
	 * }
	 * \endcode
	 *
	 * Note that if your application is potentially susceptable to "replay attacks"
	 * where the message is sent more than once, you should include a counter in
	 * the message that is covered by the MAC, and check that the counter is always
	 * incremented every time you recieve a message and MAC.
	 *
	 * For more information, see H. Krawczyk et al. RFC2104 
	 * "HMAC: Keyed-Hashing for Message Authentication"
	 */
	class QCA_EXPORT HMAC : public MessageAuthenticationCode
	{
	public:
		/**
		 * %HMAC constructor
		 *
		 * To create a simple HMAC object
		 * \param hash the type of the hash (eg "sha1", "md5" or "ripemd160" )
		 * \param key the key to use for the HMAC algorithm.
		 * \param provider the name of the provider to use (eg "qca-openssl")
		 *
		 * To construct a keyed-hash message authentication code object, you
		 * can do one of the following variations.
		 * \code
		 * QCA::HMAC sha1HMAC; // defaults to SHA1
		 * QCA::HMAC sha1HMAC( "sha1" ); // explicitly SHA1, but same as above
		 * QCA::HMAC md5HMAC( "md5" );  // MD5 algorithm
		 * QCA::HMAC sha1HMAC( "sha1", key ); // key is a QCA::SymmetricKey
		 * // next line uses RIPEMD160, empty key, implementation from qca-openssl provider
		 * QCA::HMAC ripemd160HMAC( "ripemd160", QCA::SymmetricKey(), "qca-openssl" );
		 * \endcode
		 */
		HMAC(const QString &hash = "sha1", const SymmetricKey &key = SymmetricKey(), const QString &provider = QString()) : MessageAuthenticationCode(withAlgorithm("hmac", hash), key, provider) {}
	};

	/**
	 * \class KeyDerivationFunction  qca_basic.h QtCrypto
	 *
	 * General superclass for key derivation algorithms.
	 *
	 * %KeyDerivationFunction is a superclass for the various 
	 * key derivation function algorithms within %QCA. You should
	 * not need to use it directly unless you are
	 * adding another key derivation capability to %QCA - you should be
	 * using a sub-class. PBKDF2 using SHA1 is recommended for new applications.
	 */
	class QCA_EXPORT KeyDerivationFunction : public Algorithm
	{
	public:
		/**
		 * Standard copy constructor
		 */
		KeyDerivationFunction(const KeyDerivationFunction &from);
		~KeyDerivationFunction();

		/**
		   Assignment operator.

		   Copies the state (including key) from one KeyDerivationFunction
		   to another
		 */
		KeyDerivationFunction & operator=(const KeyDerivationFunction &from);

		/**
		 * Generate the key from a specified secret and salt value.
		 * 
		 * \note key length is ignored for some functions
		 *
		 * \param secret the secret (password or passphrase)
		 * \param salt the salt to use
		 * \param keyLength the length of key to return
		 * \param iterationCount the number of iterations to perform
		 *
		 * \return the derived key
		 */
		SymmetricKey makeKey(const QSecureArray &secret, const InitializationVector &salt, unsigned int keyLength, unsigned int iterationCount);

		/**
		 * Construct the name of the algorithm
		 *
		 * You can use this to build a standard name string.
		 * You probably only need this method if you are 
		 * creating a new subclass.
		 */
		static QString withAlgorithm(const QString &kdfType, const QString &algType);

	protected:
		/**
		 * Special constructor for subclass initialisation
 		 */
		KeyDerivationFunction(const QString &type, const QString &provider);

	private:
		class Private;
		Private *d;
	};

	/**
	   \class PBKDF1 qca_basic.h QtCrypto

	   Password based key derivation function version 1

	   This class implements Password Based Key Derivation Function version 1,
	   as specified in RFC2898, and also in PKCS#5.
	 */
	class QCA_EXPORT PBKDF1 : public KeyDerivationFunction
	{
	public:
		/**
		   Standard constructor
		   
		   \param algorithm the name of the hashing algorithm to use
		   \param provider the name of the provider to use, if available
		*/
		PBKDF1(const QString &algorithm = "sha1", const QString &provider = QString()) : KeyDerivationFunction(withAlgorithm("pbkdf1", algorithm), provider) {}
	};

	/**
	   \class PBKDF2 qca_basic.h QtCrypto

	   Password based key derivation function version 2

	   This class implements Password Based Key Derivation Function version 2,
	   as specified in RFC2898, and also in PKCS#5.
	 */
	class QCA_EXPORT PBKDF2 : public KeyDerivationFunction
	{
	public:
		/**
		   Standard constructor
		   
		   \param algorithm the name of the hashing algorithm to use
		   \param provider the name of the provider to use, if available
		*/
		PBKDF2(const QString &algorithm = "sha1", const QString &provider = QString()) : KeyDerivationFunction(withAlgorithm("pbkdf2", algorithm), provider) {}
	};
}

#endif
