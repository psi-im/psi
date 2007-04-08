/*
 * qca_core.h - Qt Cryptographic Architecture
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
   \file qca_core.h

   Header file for core %QCA infrastructure

   \note You should not use this header directly from an
   application. You should just use <tt> \#include \<QtCrypto>
   </tt> instead.
*/

#ifndef QCA_CORE_H
#define QCA_CORE_H

/**
   The current version of %QCA

   This provides you a compile time check of the %QCA version.

   \sa qcaVersion for a runtime check.
*/
#define QCA_VERSION 0x020000

#include <QString>
#include <QStringList>
#include <QList>
#include <QSharedData>
#include <QSharedDataPointer>
#include "qca_export.h"
#include "qca_support.h"
#include "qca_tools.h"

/**
   The current version of %QCA.

   This is equivalent to QCA_VERSION, except it provides
   a runtime check of the version of %QCA that is being used.
*/
QCA_EXPORT int qcaVersion();

/** 
 * QCA - the Qt Cryptographic Architecture
 */
namespace QCA
{
	class Provider;
	class Random;
	class CertificateCollection;
	class Global;
	class KeyStore;
	class KeyStoreManager;
	class Logger;

	/**
	 * Convenience representation for the plugin providers
	 * 
	 * You can get a list of providers using the providers()
	 * function
	 *
	 * \sa ProviderListIterator
	 * \sa providers()
	 */
	typedef QList<Provider*> ProviderList;

	/**
	 * Mode settings for memory allocation
	 *
	 * QCA can use secure memory, however most operating systems
	 * restrict the amount of memory that can be pinned by user
	 * applications, to prevent a denial-of-service attack. 
	 *
	 * QCA supports two approaches to getting memory - the mlock
	 * method, which generally requires root (administrator) level
	 * privileges, and the mmap method which is not as secure, but
	 * which should be able to be used by any process.
	 * 
	 * \sa Initializer
	 */
	enum MemoryMode
	{
		Practical, ///< mlock and drop root if available, else mmap
		Locking, ///< mlock and drop root
		LockingKeepPrivileges ///< mlock, retaining root privileges
	};

	/**
	 * Direction settings for symmetric algorithms
	 *
	 * For some algorithms, it makes sense to have a "direction", such
	 * as Cipher algorithms which can be used to encrypt or decrypt.
	 */
	enum Direction
	{
		Encode, ///< Operate in the "forward" direction; for example, encrypting
		Decode  ///< Operate in the "reverse" direction; for example, decrypting
	};

	/**
	 * Initialise %QCA.
	 * This call is not normally required, because it is cleaner
	 * to use an Initializer.
	 */
	QCA_EXPORT void init();

	/**
	 * \overload
	 *
	 * \param m the MemoryMode to use
	 * \param prealloc the amount of memory in kilobytes to allocate
	 *                 for secure storage
	 */
	QCA_EXPORT void init(MemoryMode m, int prealloc);

	/**
	 * Clean up routine
	 *
	 * This routine cleans up %QCA, including memory allocations
	 * This call is not normally required, because it is cleaner
	 * to use an Initializer
	 */
	QCA_EXPORT void deinit();

	/**
	 * Test if secure storage memory is available
	 *
	 * \return true if secure storage memory is available
	 */ 
	QCA_EXPORT bool haveSecureMemory();

	/**
	 * Test if a capability (algorithm) is available.
	 *
	 * Since capabilities are made available at runtime, you
	 * should always check before using a capability the first
	 * time, as shown below.
	 * \code
	 * QCA::init();
         * if(!QCA::isSupported("sha1"))
         *     printf("SHA1 not supported!\n");
	 * else {
         *     QString result = QCA::SHA1::hashToString(myString);
         *     printf("sha1(\"%s\") = [%s]\n", myString.data(), result.latin1());
	 * }
	 * \endcode
	 * 
	 * \param features the name of the capability to test for
	 * \param provider if specified, only check for the capability in that 
	          specific provider. If not provided, or provided as an empty
		  string, then check for capabilities in all available providers
	 * \return true if the capability is available, otherwise false
	 *
	 * Note that you can test for a combination of capabilities,
	 * using a comma delimited list:
	 * \code
	 * QCA::isSupported("sha1,md5"):
	 * \endcode
	 * which will return true if all of the capabilities listed
	 * are present.
	 *
	 */
	QCA_EXPORT bool isSupported(const char *features, const QString &provider = QString());

	/**
	   \overload
	 
	   \param features a list of features to test for
	   \param provider if specified, only check for the capability in that 
	          specific provider. If not provided, or provided as an empty
		  string, then check for capabilities in all available providers
	*/
	QCA_EXPORT bool isSupported(const QStringList &features, const QString &provider = QString());

	/**
	 * Generate a list of all the supported features in plugins,
	 * and in built in capabilities
	 *
	 * \return a list containing the names of the features
	 *
	 * The following code writes a list of features to standard out
	 * \code 
	 * QStringList capabilities;
	 * capabilities = QCA::supportedFeatures();
	 * std::cout << "Supported:" << capabilities.join(",") << std::endl;
	 * \endcode
	 *
	 * \sa isSupported(const char *features)
	 * \sa isSupported(const QStringList &features)
	 * \sa defaultFeatures()
	 */
	QCA_EXPORT QStringList supportedFeatures();

	/**
	 * Generate a list of the built in features. This differs from
	 * supportedFeatures() in that it does not include features provided
	 * by plugins.
	 *
	 * \return a list containing the names of the features
	 *
	 * The following code writes a list of features to standard out
	 * \code 
	 * QStringList capabilities;
	 * capabilities = QCA::defaultFeatures();
	 * std::cout << "Default:" << capabilities.join(",") << std::endl;
	 * \endcode
	 *
	 * \sa isSupported(const char *features)
	 * \sa isSupported(const QStringList &features)
	 * \sa supportedFeatures()
	 */
	QCA_EXPORT QStringList defaultFeatures();

	/**
	 * Add a provider to the current list of providers
	 * 
	 * This function allows you to add a provider to the 
	 * current plugin providers at a specified priority. If
	 * a provider with the name already exists, this call fails.
	 *
	 * \param p a pointer to a Provider object, which must be
	 *        set up.
	 * \param priority the priority level to set the provider to
	 *
	 * \return true if the provider is added, and false if the 
	 *         provider is not added (failure)
	 *
	 * \sa setProviderPriority for a description of the provider priority system
	 */
	QCA_EXPORT bool insertProvider(Provider *p, int priority = 0);

	/**
	 * Change the priority of a specified provider
	 *
	 * QCA supports a number of providers, and if a number of providers
	 * support the same algorithm, it needs to choose between them. You
	 * can do this at object instantiation time (by specifying the name
	 * of the provider that should be used). Alternatively, you can provide a
	 * relative priority level at an application level, using this call.
	 * 
	 * Priority is used at object instantiation time. The provider is selected
	 * according to the following logic:
	 * - if a particular provider is nominated, and that provider supports
	 *   the required algorithm, then the nominated provider is used
	 * - if no provider is nominated, or it doesn't support the required
	 *   algorithm, then the provider with the lowest priority number will be used,
	 *   if that provider supports the algorithm.
	 * - if the provider with the lowest priority number doesn't support 
	 *   the required algorithm, the provider with the next lowest priority number
	 *   will be tried,and so on through to the provider with the largest priority number
	 * - if none of the plugin providers support the required algorithm, then
	 *   the default (built-in) provider will be tried.
	 * 
	 * \param name the name of the provider
	 * \param priority the new priority of the provider. As a special case, if
	 *        you pass in -1, then this provider gets the same priority as the
	 *        the last provider that was added or had its priority set using this
	 *        call.
	 *
	 * \sa providerPriority
	 */
	QCA_EXPORT void setProviderPriority(const QString &name, int priority);

	/**
	 * Return the priority of a specified provider
	 *
	 * The name of the provider (eg "qca-openssl") is used to look up the 
	 * current priority associated with that provider. If the provider
	 * is not found (or something else went wrong), -1 is returned.
	 *
	 * \param name the name of the provider
	 *
	 * \return the current priority level
	 *
	 * \sa setProviderPriority for a description of the provider priority system
	 */
	QCA_EXPORT int providerPriority(const QString &name);

	/**
	 * Return a list of the current providers
	 *
	 * The current plugin providers are provided as a list, which you
	 * can iterate over using ProviderListIterator.
	 *
	 * \sa ProviderList
	 * \sa ProviderListIterator
	 */
	QCA_EXPORT ProviderList providers();

	/**
	 * Return the named provider, or 0 if not found
	 */
	QCA_EXPORT Provider *findProvider(const QString &name);

	/**
	 * Return the default provider
	 */
	QCA_EXPORT Provider *defaultProvider();

	/**
	 * Scan for new plugins
	 */
	QCA_EXPORT void scanForPlugins();

	/**
	 * Unload the current plugins
	 */
	QCA_EXPORT void unloadAllPlugins();

	/**
	 * Retrieve plugin diagnostic text
	 */
	QCA_EXPORT QString pluginDiagnosticText();

	/**
	 * Clear plugin diagnostic text
	 */
	QCA_EXPORT void clearPluginDiagnosticText();

	/**
	 * Set a global property
	 */
	QCA_EXPORT void setProperty(const QString &name, const QVariant &value);

	/**
	 * Retrieve a global property
	 */
	QCA_EXPORT QVariant getProperty(const QString &name);

	/**
	 * Set provider configuration
	 *
	 * Allowed value types: QString, int, bool
	 */
	QCA_EXPORT void setProviderConfig(const QString &name, const QVariantMap &config);

	/**
	 * Retrieve provider configuration
	 */
	QCA_EXPORT QVariantMap getProviderConfig(const QString &name);

	/**
	 * Save provider configuration to persistent storage
	 */
	QCA_EXPORT void saveProviderConfig(const QString &name);

	/**
	 * Return the Random provider that is currently set to be the
	 * global random number generator.
	 *
	 * For example, to get the name of the provider that is currently
	 * providing the Random capability, you could use:
	 * \code
	 * QCA::Random rng = QCA::globalRNG();
         * std::cout << "Provider name: " << rng.provider()->name() << std::endl;
	 * \endcode
	 */
	QCA_EXPORT Random & globalRNG();

	/**
	 * Change the global random generation provider
	 *
	 * The Random capabilities of %QCA are provided as part of the
	 * built in capabilities, however the generator can be changed
	 * if required.
	 */
	QCA_EXPORT void setGlobalRNG(const QString &provider);

	/**
	   Return a reference to the %QCA Logger, which is used for diagnostics
	   and error recording.

	   The system Logger is automatically created for you on start. 
	*/
	QCA_EXPORT Logger *logger();

	/**
	   Log a text message. This is just a convenience function
	   to avoid having to call Logger::logTextMessage() on the
	   global Logger.

	   \param message the text to log
	   \param severity the type of information to log
	*/
	QCA_EXPORT void logText( const QString &message, Logger::Severity severity = Logger::Information );

	/**
	   Test if QCA can access the root CA certificates

	   If root certificates are available, this function returns true,
	   otherwise it returns false.
	  
	   \sa systemStore
	*/
	QCA_EXPORT bool haveSystemStore();

	/**
	   Get system-wide root CA certificates

	   \sa haveSystemStore
	*/
	QCA_EXPORT CertificateCollection systemStore();

	/**
	 * Get the application name that will be used by SASL server mode
	 *
	 * The application name is used by SASL in server mode, as some systems might
	 * have different security policies depending on the app.
	 * The default application name  is 'qca'
	 */
	QCA_EXPORT QString appName();

	/**
	 * Set the application name that will be used by SASL server mode
	 *
	 * The application name is used by SASL in server mode, as some systems might
	 * have different security policies depending on the app. This should be set 
	 * before using SASL objects, and it cannot be changed later.
	 *
	 * \param name the name string to use for SASL server mode
	 */
	QCA_EXPORT void setAppName(const QString &name);

	/**
	 * Convert a byte array to printable hexadecimal
	 * representation.
	 *
	 * This is a convenience function to convert an arbitrary
	 * QSecureArray to a printable representation.
	 *
	 * \code
	 * 	QSecureArray test(10);
	 *	test.fill('a');
	 * 	// 0x61 is 'a' in ASCII
	 *	if (QString("61616161616161616161") == QCA::arrayToHex(test) ) {
	 *		printf ("arrayToHex passed\n");
	 *	}
	 * \endcode
	 *
	 * \param array the array to be converted
	 * \return a printable representation
	 */
	QCA_EXPORT QString arrayToHex(const QSecureArray &array);

	/**
	 * Convert a QString containing a hexadecimal representation
	 * of a byte array into a QByteArray
	 *
	 * This is a convenience function to convert a printable
	 * representation into a QByteArray - effectively the inverse
	 * of QCA::arrayToHex.
	 *
	 * \code
	 * 	QCA::init();
	 * 	QByteArray test(10);
	 *
	 *	test.fill('b'); // 0x62 in hexadecimal
	 *	test[7] = 0x00; // can handle strings with nulls
	 *
	 *	if (QCA::hexToArray(QString("62626262626262006262") ) == test ) {
	 *		printf ("hexToArray passed\n");
	 *	}
	 * \endcode
	 *
	 * \param hexString the string containing a printable
	 * representation to be converted
	 * \return the equivalent QByteArray
	 *
	 */
	QCA_EXPORT QByteArray hexToArray(const QString &hexString);

	/**
	   \class Initializer qca_core.h QtCrypto
	   
	   Convenience method for initialising and cleaning up %QCA

	   To ensure that QCA is properly initialised and cleaned up,
	   it is convenient to create an Initializer object, and let it
	   go out of scope at the end of %QCA usage.
	 */
	class QCA_EXPORT Initializer
	{
	public:
		/**
		 * Standard constructor
		 *
		 * \param m the MemoryMode to use for secure memory
		 * \param prealloc the amount of secure memory to pre-allocate,
		 *        in units of 1024 bytes (1K).
		 */
		Initializer(MemoryMode m = Practical, int prealloc = 64);
		~Initializer();
	};

	/**
	   \class KeyLength qca_core.h QtCrypto

	   Simple container for acceptable key lengths

	   The KeyLength specifies the minimum and maximum byte sizes
	   allowed for a key, as well as a "multiple" which the key
	   size must evenly divide into.

	   As an example, if the key can be 4, 8 or 12 bytes, you can
	   express this as 
	   \code
	   KeyLength keyLen( 4, 12, 4 );
	   \endcode

	   If you want to express a KeyLength that takes any number
	   of bytes (including zero), you may want to use
	   \code
	   #include<limits>
	   KeyLength( 0, std::numeric_limits<int>::max(), 1 );
	   \endcode
	 */
	class QCA_EXPORT KeyLength
	{
	public:
		/**
		 * Construct a %KeyLength object
		 *
		 * \param min the minimum length of the key, in bytes
		 * \param max the maximum length of the key, in bytes
		 * \param multiple the number of bytes that the key must be a 
		 * multiple of.
		 */
		KeyLength(int min, int max, int multiple)
			: _min( min ), _max(max), _multiple( multiple )
		{ }
		/**
		 * Obtain the minimum length for the key, in bytes
		 */
		int minimum() const { return _min; }

		/**
		 * Obtain the maximum length for the key, in bytes
		 */
		int maximum() const { return _max; }

		/**
		 * Return the number of bytes that the key must be a multiple of
		 *
		 * If this is one, then anything between minimum and maximum (inclusive)
		 * is acceptable.
		 */
		int multiple() const { return _multiple; }

	private:
		int const _min, _max, _multiple;
	};

	/**
	   \class Provider qca_core.h QtCrypto

	   Algorithm provider

	   Provider represents a plugin provider (or as a special case, the
	   built-in provider). This is the class you need to inherit
	   from to create your own plugin. You don't normally need to 
	   worry about this class if you are just using existing 
	   QCA capabilities and plugins, however there is nothing stopping
	   you from using it to obtain information about specific plugins,
	   as shown in the example below.
	 */
	class QCA_EXPORT Provider
	{
	public:
		virtual ~Provider();

		class Context;

		/**
		 * Initialisation routine.
		 * 
		 * This routine will be called when your plugin
		 * is loaded, so this is a good place to do any
		 * one-off initialisation tasks. If you don't need
		 * any initialisation, just implement it as an empty
		 * routine.
		 */
		virtual void init();

		/**
		 * Target QCA version for the provider.
		 *
		 * This is used to verify compatibility between the
		 * provider and QCA.  For a provider to be used, it
		 * must have major and minor version numbers that are
		 * less-than or equal to the QCA version (the patch
		 * version number is ignored).  This means an older
		 * provider may be used with a newer QCA, but a newer
		 * provider cannot be used with an older QCA.
		 */
		virtual int version() const = 0;

		/**
		 * The name of the provider.
		 * 
		 * Typically you just return a string containing a 
		 * convenient name.
		 *
		 * \code
		 * QString name() const
		 * {
		 *        return "qca-myplugin";
		 * }
		 * \endcode
		 *
		 * \note  The name is used to tell if a provider is
		 * already loaded, so you need to make sure it is
		 * unique amongst the various plugins.
		 */
		virtual QString name() const = 0;

		/**
		 * The capabilities (algorithms) of the provider.
		 *
		 * Typically you just return a fixed QStringList:
		 * \code
		 * QStringList features() const
		 * {
		 *      QStringList list;
		 *      list += "sha1";
		 *      list += "sha256";
		 *      list += "hmac(md5)";
		 *      return list;
		 * }
		 * \endcode
		 */
		virtual QStringList features() const = 0;

		/**
		 * Optional credit text for the provider.
		 *
		 * You might display this information in a credits or
		 * "About" dialog.  Returns an empty string if the
		 * provider has no credit text.
		 */
		virtual QString credit() const;

		/**
		 * Routine to create a plugin context
		 *
		 * You need to return a pointer to an algorithm
		 * Context that corresponds with the algorithm
		 * name specified. 
		 *
		 * \param type the name of the algorithm required
		 *
		 * \code
		 * Context *createContext(const QString &type)
		 * {
		 * if ( type == "sha1" )
		 *     return new SHA1Context( this );
		 * else if ( type == "sha256" )
		 *     return new SHA0256Context( this );
		 * else if ( type == "hmac(sha1)" )
		 *     return new HMACSHA1Context( this );
		 * else
		 *     return 0;
		 * }
		 * \endcode
		 * 
		 * Naturally you also need to implement
		 * the specified Context subclasses as well.
		 */
		virtual Context *createContext(const QString &type) = 0;

		virtual QVariantMap defaultConfig() const;
		virtual void configChanged(const QVariantMap &config);
	};

	/**
	   \class Context qca_core.h QtCrypto

	   Internal context class used for the plugin

	   \internal
	*/
	class QCA_EXPORT Provider::Context : public QObject
	{
		Q_OBJECT
	public:
		/**
		   Standard constructor

		   \param parent the parent provider for this 
		   context
		   \param type the name of the provider context type
		*/
		Context(Provider *parent, const QString &type);
		Context(const Context &from);
		virtual ~Context();

		/**
		   The Provider associated with this Context
		*/
		Provider *provider() const;

		/**
		   The type of context, as passed to the constructor
		*/
		QString type() const;

		/**
		   Create a duplicate of this Context
		*/
		virtual Context *clone() const = 0;

		/**
		   Test if two Contexts have the same Provider

		   \param c pointer to the Context to compare to

		   \return true if the argument and this Context
		   have the same provider.
		*/
		bool sameProvider(const Context *c) const;

	private:
		Provider *_provider;
		QString _type;
	};

	class BasicContext : public Provider::Context
	{
		Q_OBJECT
	public:
		BasicContext(Provider *parent, const QString &type);
		BasicContext(const BasicContext &from);
		~BasicContext();
	};

	/**
	   \class BufferedComputation qca_core.h QtCrypto

	   General superclass for buffered computation algorithms

	   A buffered computation is characterised by having the
	   algorithm take data in an incremental way, then having
	   the results delivered at the end. Conceptually, the
	   algorithm has some internal state that is modified
	   when you call update() and returned when you call
	   final().
	*/
	class QCA_EXPORT BufferedComputation
	{
	public:
		virtual ~BufferedComputation();

		/**
		   Reset the internal state
		*/
		virtual void clear() = 0;

		/**
		   Update the internal state with a byte array

		   \param a the byte array of data that is to 
		   be used to update the internal state.
		*/
		virtual void update(const QSecureArray &a) = 0;

		/**
		   Complete the algorithm and return the internal state
		*/
		virtual QSecureArray final() = 0;

		/**
		   Perform an "all in one" update, returning
		   the result. This is appropriate if you
		   have all the data in one array - just
		   call process on that array, and you will
		   get back the results of the computation.

		   \note This will invalidate any previous
		   computation using this object.
		*/
		QSecureArray process(const QSecureArray &a);
	};

	/**
	   \class Filter qca_core.h QtCrypto

	   General superclass for filtering transformation algorithms

	   A filtering computation is characterised by having the
	   algorithm take input data in an incremental way, with results
	   delivered for each input, or block of input. Some internal
	   state may be managed, with the transformation completed
	   when final() is called.

	   If this seems a big vague, then you might try deriving
	   your class from a subclass with stronger semantics, or if your
	   update() function is always returning null results, and
	   everything comes out at final(), try BufferedComputation.
	*/
	class QCA_EXPORT Filter
	{
	public:
		virtual ~Filter();

		/**
		   Reset the internal state
		*/
		virtual void clear() = 0;

		/**
		   Process more data, returning the corresponding
		   filtered version of the data.

		   \param a the array containing data to process
		*/
		virtual QSecureArray update(const QSecureArray &a) = 0;

		/**
		   Complete the algorithm, returning any 
		   additional results.
		*/
		virtual QSecureArray final() = 0;

		/**
		 Test if an update() or final() call succeeded.
		 
		 \return true if the previous call succeeded
		*/
		virtual bool ok() const = 0;

		/**
		   Perform an "all in one" update, returning
		   the result. This is appropriate if you
		   have all the data in one array - just
		   call process on that array, and you will
		   get back the results of the computation.

		   \note This will invalidate any previous
		   computation using this object.
		*/
		QSecureArray process(const QSecureArray &a);
	};

	/**
	   \class Algorithm qca_core.h QtCrypto

	   General superclass for an algorithm. 

	   This is a fairly abstract class, mainly used for
	   implementing the backend "provider" interface.
	*/
	class QCA_EXPORT Algorithm
	{
	public:
		/**
		   Standard copy constructor

		   \param from the Algorithm to copy from
		*/
		Algorithm(const Algorithm &from);

		virtual ~Algorithm();

		/**
		   Assignment operator
		   
		   \param from the Algorithm to copy state from
		*/
		Algorithm & operator=(const Algorithm &from);

		/**
		   The name of the algorithm type.
		*/
		QString type() const;

		/**
		 The name of the provider

		 Each algorithm is implemented by a provider. This
		 allows you to figure out which provider is associated
		*/
		Provider *provider() const;

		// Note: The next five functions are not public!

		/**
		   \internal

		   The context associated with this algorithm
		*/
		Provider::Context *context();

		/**
		   \internal

		   The context associated with this algorithm
		*/
		const Provider::Context *context() const;

		/**
		   \internal

		   Set the Provider for this algorithm

		   \param c the context for the Provider to use
		*/
		void change(Provider::Context *c);

		/**
		   \internal

		   \overload

		   \param type the name of the algorithm to use
		   \param provider the name of the preferred provider
		*/
		void change(const QString &type, const QString &provider);

		/**
		   \internal

		   Take the Provider from this algorithm
		*/
		Provider::Context *takeContext();

	protected:
		/**
		   Constructor for empty algorithm
		*/
		Algorithm();

		/**
		   Constructor of a particular algorithm.

		   \param type the algorithm to construct
		   \param provider the name of a particular Provider
		*/
		Algorithm(const QString &type, const QString &provider);

	private:
		class Private;
		QSharedDataPointer<Private> d;
	};

	/**
	   \class SymmetricKey qca_core.h QtCrypto

	   Container for keys for symmetric encryption algorithms.
	 */
	class QCA_EXPORT SymmetricKey : public QSecureArray
	{
	public:
		/**
		 * Construct an empty (zero length) key
		 */
		SymmetricKey();

		/**
		 * Construct an key of specified size, with random contents
		 *
		 * This is intended to be used as a random session key.
		 *
		 * \param size the number of bytes for the key
		 *
		 */
		SymmetricKey(int size);

		/**
		 * Construct a key from a provided byte array
		 *
		 * \param a the byte array to copy
		 */
		SymmetricKey(const QSecureArray &a);

		/**
		 * Construct a key from a provided byte array
		 *
		 * \param a the byte array to copy
		 */
		SymmetricKey(const QByteArray &a);

		/**
		 * Test for weak DES keys
		 *
		 * \return true if the key is a weak key for DES
		 */
		bool isWeakDESKey();
	};

	/**
	   \class InitializationVector qca_core.h QtCrypto

	   Container for initialisation vectors and nonces
	 */
	class QCA_EXPORT InitializationVector : public QSecureArray
	{
	public:
		/** 
		    Construct an empty (zero length) initisation vector
		 */
		InitializationVector();

		/**
		   Construct an initialisation vector of the specified size

		   \param size the length of the initialisation vector, in bytes
		 */
		InitializationVector(int size);

		/**
		   Construct an initialisation vector from a provided byte array

		   \param a the byte array to copy
		 */
		InitializationVector(const QSecureArray &a);

		/**
		   Construct an initialisation vector from a provided byte array

		   \param a the byte array to copy
		 */
		InitializationVector(const QByteArray &a);
	};

	/**
	   An asynchronous event

	   Events are produced in response to the library's need for some user
	   intervention, such as entering a pin or password, or inserting a cryptographic
	   token.

	   Event is an abstraction, so you can handle this need in a way that makes sense
	   for your application.
	*/
	class QCA_EXPORT Event
	{
	public:
	        /** 
		    %Type of event

		    \sa type()
		*/
		enum Type
		{
			Password,   ///< Asking for a password, PIN or passphrase.
			Token       ///< Asking for a token
		};

	        /** 
		    %Source of the event

		    Events are associated with access to a KeyStore, or access to 
		    a file (or bytearray/stream or equivalent). This tells you the
		    type of source that caused the Event.

		    \sa source()
		    \sa fileName() for the name, if source is Event::Data
		    \sa keyStoreId() and keyStoreEntryId for the keystore and entry, if
		    the source is Event::KeyStore
		*/
		enum Source
		{
			KeyStore,   ///< KeyStore generated the event
			Data        ///< File or bytearray generated the event
		};

	        /** 
		    password variation

		    If the Type of Event is Password, PasswordStyle tells you whether 
		    it is a PIN, passphrase or password.

		    \sa passwordStyle()
		*/
		enum PasswordStyle
		{
			StylePassword,   ///< User should be prompted for a "Password"
			StylePassphrase, ///< User should be prompted for a "Passphrase"
			StylePIN         ///< User should be prompted for a "PIN"
		};

		/**
		   Constructor
		*/
		Event();

		/**
		   Copy constructor

		   \param from the Event to copy from
		*/
		Event(const Event &from);

		/**
		   Destructor
		*/
		~Event();

		/**
		   Assignment operator

		   \param from the Event to copy from
		*/
		Event & operator=(const Event &from);

		/**
		   test if this event has been setup correctly
		*/
		bool isNull() const;

		/**
		   the Type of this event
		*/
		Type type() const;

		/**
		   the Source of this event
		*/
		Source source() const;

		/**
		   the style of password required.

		   This is not meaningful unless the Type is Event::Password.

		   \sa PasswordStyle 
		*/
		PasswordStyle passwordStyle() const;

		/**
		   The id of the KeyStore associated with this event

		   This is not meaningful unless the Source is KeyStore.
		*/
		QString keyStoreId() const;

		/**
		   The id of the KeyStoreEntry associated with this event

		   This is not meaningful unless the Source is KeyStore.
		*/
		QString keyStoreEntryId() const;

		/**
		   Name or other identifier for the file or byte array
		   associated with this event.

		   This is not meaningful unless the Source is Data.
		*/
		QString fileName() const;

		/**
		   opaque data
		*/
		void *ptr() const;

		/**
		   Set the values for this Event

		   This creates a Password type event, for a keystore.

		   \param pstyle the style of information required (e.g. PIN, password or passphrase)
		   \param keyStoreId the keystore that the information is required for
		   \param keyStoreEntryId the entry in the keystore that the information is required for
		   \param ptr opaque data
		*/
		void setPasswordKeyStore(PasswordStyle pstyle, const QString &keyStoreId, const QString &keyStoreEntryId, void *ptr);

		/**
		   Set the values for this Event

		   This creates a Password type event, for a file.

		   \param pstyle the style of information required (e.g. PIN, password or passphrase)
		   \param fileName the name of the file (or other identifier) that the information is required for
		   \param ptr opaque data
		*/
		void setPasswordData(PasswordStyle pstyle, const QString &fileName, void *ptr);

		/**
		   Set the values for this Event

		   This creates a Token type event.
		   
		   \param keyStoreEntryId the entry in the keystore that the token is required for
		   \param ptr opaque data
		*/
		void setToken(const QString &keyStoreEntryId, void *ptr);

	private:
		class Private;
		QSharedDataPointer<Private> d;
	};

	class EventHandlerPrivate;
	class PasswordAsker;
	class PasswordAskerPrivate;
	class TokenAsker;
	class TokenAskerPrivate;
	class AskerItem;

	/**
	   Interface class for password / passphrase / PIN and token handlers

	   This class is used on client side applications to handle
	   the provision of passwords, passphrases and PINs by users, and
	   to indicate that tokens have been correctly inserted.

	   The concept behind this class is that the library can raise
	   events (typically using PasswordAsker or TokenAsker), which
	   may (or may not) be handled by the application using a
	   handler object (that has-a EventHandler, or possibly is-a
	   EventHandler) that is connected to the eventReady() signal.
	*/
	class QCA_EXPORT EventHandler : public QObject
	{
		Q_OBJECT
	public:
	        /**
		   Constructor

		   \param parent the parent object for this object
		*/
		EventHandler(QObject *parent = 0);
		~EventHandler();

		/**
		   mandatory function to call after connecting the
		   signal to a slot in your application specific password
		   / passphrase / PIN or token handler
		*/
		void start();

		/**
		   function to call to return the user provided
		   password, passphrase or PIN.

		   \param id the id corresponding to the password request
		   \param password the user-provided password, passphrase or PIN.

		   \note the id parameter is the same as that provided in the
		   eventReady() signal.
		*/
		void submitPassword(int id, const QSecureArray &password);

		/**
		   function to call to indicate that the token has been inserted
		   by the user.

		   \param id the id corresponding to the password request

		   \note the id parameter is the same as that provided in the
		   eventReady() signal.
		*/
		void tokenOkay(int id);

		/**
		   function to call to indicate that the user declined to 
		   provide a password, passphrase, PIN or token.

		   \param id the id corresponding to the password request

		   \note the id parameter is the same as that provided in the
		   eventReady() signal.
		*/
		void reject(int id);

	signals:
		/**
		   signal emitted when an Event requires attention.

		   You typically need to connect this signal to
		   a compatible slot in your callback handler
		*/
		void eventReady(int id, const QCA::Event &context);

	private:
		friend class EventHandlerPrivate;
		EventHandlerPrivate *d;

		friend class PasswordAsker;
		friend class PasswordAskerPrivate;
		friend class TokenAsker;
		friend class AskerItem;
	};

	/** 
	    User password / passphrase / PIN handler

	    This class is used to obtain a password from a user.
	*/
	class QCA_EXPORT PasswordAsker : public QObject
	{
		Q_OBJECT
	public:
	       /**
		  Construct a new asker

		  \param parent the parent object for this QObject
	       */
		PasswordAsker(QObject *parent = 0);
		~PasswordAsker();

		/**
		   queue a password / passphrase request associated with a key store
		   
		   \param pstyle the type of information required (e.g. PIN, passphrase or password)
		   \param keyStoreId the key store that the information is required for
		   \param keyStoreEntryId the item in the key store that the information is required for
		   \param ptr opaque data
		*/
		void ask(Event::PasswordStyle pstyle, const QString &keyStoreId, const QString &keyStoreEntryId, void *ptr);

		/**
		   queue a password / passphrase request associated with a file
		   
		   \param pstyle the type of information required (e.g. PIN, passphrase or password)
		   \param fileName the name of the file that the information is required for
		   \param ptr opaque data
		*/
		void ask(Event::PasswordStyle pstyle, const QString &fileName, void *ptr);

		/**
		   Cancel the pending password / passphrase request
		*/
		void cancel();

		/**
		   Block until the password / passphrase request is
		   completed

		   You can use the responseReady signal instead of
		   blocking, if appropriate.
		*/
		void waitForResponse();

		/**
		   Determine whether the password / passphrase was accepted or not

		   In this context, returning true is indicative of the user clicking "Ok"
		   or equivalent; and returning false indicates that either the user
		   clicked "Cancel" or equivalent, or that the cancel() function was
		   called, or that the request is still pending.
		*/
		bool accepted() const;

		/**
		   The password / passphrase / PIN provided by the user in response to
		   the asker request. This may be empty.
		*/
		QSecureArray password() const;

	signals:
		/**
		   Emitted when the asker process has been completed. 

		   You should check whether the user accepted() the response
		   prior to relying on the password().
		*/
		void responseReady();

	private:
		friend class PasswordAskerPrivate;
		PasswordAskerPrivate *d;

		friend class AskerItem;
	};

	/** 
	    User token handler

	    This class is used to request the user to insert a token.
	*/
	class QCA_EXPORT TokenAsker : public QObject
	{
		Q_OBJECT
	public:
	       /**
		  Construct a new asker

		  \param parent the parent object for this QObject
	       */
		TokenAsker(QObject *parent = 0);
		~TokenAsker();

		/**
		   queue a token request associated with a key store
		   
		   \param keyStoreEntryId the item in the key store that the information is required for
		   \param ptr opaque data
		*/
		void ask(const QString &keyStoreEntryId, void *ptr);

		/**
		   Cancel the pending password / passphrase request
		*/
		void cancel();

		/**
		   Block until the token request is completed

		   You can use the responseReady signal instead of
		   blocking, if appropriate.
		*/
		void waitForResponse();

		/**
		   Test if the token request was accepted or not.

		   \return true if the token request was accepted
		*/
		bool accepted() const;

	signals:
		/**
		   Emitted when the asker process has been completed. 

		   You should check whether the user accepted() the response
		   prior to relying on token being present.
		*/
		void responseReady();

	private:
		friend class TokenAskerPrivate;
		TokenAskerPrivate *d;

		friend class AskerItem;
	};
}

#endif
