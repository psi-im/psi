/*
 * qca_keystore.h - Qt Cryptographic Architecture
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
   \file qca_keystore.h

   Header file for classes that provide and manage keys

   \note You should not use this header directly from an
   application. You should just use <tt> \#include \<QtCrypto>
   </tt> instead.
*/

#ifndef QCA_KEYSTORE_H
#define QCA_KEYSTORE_H

#include "qca_core.h"
#include "qca_cert.h"

namespace QCA
{
	class KeyStoreTracker;
	class KeyStoreManagerPrivate;
	class KeyStorePrivate;

	/**
	   \class KeyStoreEntry qca_keystore.h QtCrypto

	   Single entry in a KeyStore

	   This is a container for any kind of object in a KeyStore
	*/
	class QCA_EXPORT KeyStoreEntry : public Algorithm
	{
	public:
		/**
		   The type of entry in the KeyStore
		*/
		enum Type
		{
			TypeKeyBundle,
			TypeCertificate,
			TypeCRL,
			TypePGPSecretKey,
			TypePGPPublicKey
		};

		/**
		   Create an empty KeyStoreEntry
		*/
		KeyStoreEntry();

		/**
		   Create a passive KeyStoreEntry based on known entry
		*/
		KeyStoreEntry(const QString &id);

		/**
		   Standard copy constructor

		   \param from the source entry
		*/
		KeyStoreEntry(const KeyStoreEntry &from);

		~KeyStoreEntry();

		/**
		   Standard assignment operator

		   \param from the source entry
		*/
		KeyStoreEntry & operator=(const KeyStoreEntry &from);

		/**
		   Test if this key is empty (null)
		*/
		bool isNull() const;

		bool isAvailable() const;
		bool isAccessible() const;

		/**
		   Determine the type of key stored in this object 
		*/
		Type type() const;

		/**
		   The name associated with the key stored in this object
		*/
		QString name() const;

		/**
		   The ID associated with the key stored in this object.

		   The ID is unique across all stores, and may be very long.
		*/
		QString id() const;

		QString storeName() const;
		QString storeId() const;

		/**
		   If a KeyBundle is stored in this object, return that
		   bundle.
		*/
		KeyBundle keyBundle() const;

		/**
		   If a Certificate is stored in this object, return that
		   certificate.
		*/
		Certificate certificate() const;

		/**
		   If a CRL is stored in this object, return the value
		   of the CRL
		*/
		CRL crl() const;

		/**
		   If the key stored in this object is a private
		   PGP key, return the contents of that key.
		*/
		PGPKey pgpSecretKey() const;

		/**
		   If the key stored in this object is either an 
		   public or private PGP key, extract the public key
		   part of that PGP key.
		*/
		PGPKey pgpPublicKey() const;

		/**
		   Returns true if the entry is available, otherwise false.
		   Available means that the retrieval functions (like
		   keyBundle(), certificate(), pgpPublicKey(), etc) will
		   return non-null objects.  Entries retrieved from a
		   KeyStore are always available, and therefore it is not
		   necessary to call this function.  Calling this function
		   on an already available entry may cause the entry to
		   be refreshed.

		   \note This function is blocking.
		*/
		bool ensureAvailable();

		// like ensureAvailable, but also login to the token if needed
		bool ensureAccess();

	private:
		class Private;
		Private *d;

		friend class KeyStoreTracker;
	};

	class QCA_EXPORT KeyStoreEntryWatcher : public QObject
	{
		Q_OBJECT
	public:
		KeyStoreEntryWatcher(const KeyStoreEntry &e, QObject *parent = 0);
		~KeyStoreEntryWatcher();

		KeyStoreEntry entry() const;

	signals:
		void available();
		void unavailable();

	private:
		class Private;
		friend class Private;
		Private *d;
	};

	/**
	   \class KeyStore qca_keystore.h QtCrypto

	   General purpose key storage object

	   Examples of use of this are:
	    -  systemstore:          System TrustedCertificates
	    -  accepted self-signed: Application TrustedCertificates
	    -  apple keychain:       User Identities
	    -  smartcard:            SmartCard Identities
	    -  gnupg:                PGPKeyring Identities,PGPPublicKeys

	    \note
	    - there can be multiple KeyStore objects referring to the same id
	    - when a KeyStore is constructed, it refers to a given id (deviceId)
	    and internal contextId.  if the context goes away, the KeyStore
	    becomes invalid (isValid() == false), and unavailable() is emitted.
	    even if the device later reappears, the KeyStore remains invalid.
	    a new KeyStore will have to be created to use the device again.
	*/
	class QCA_EXPORT KeyStore : public QObject, public Algorithm
	{
		Q_OBJECT
	public:
		/**
		   The type of keystore
		*/
		enum Type
		{
			System,      ///< objects such as root certificates
			User,        ///< objects such as Apple Keychain, KDE Wallet
			Application, ///< for caching accepted self-signed certificates
			SmartCard,   ///< for smartcards
			PGPKeyring   ///< for a PGP keyring
		};

		/**
		   Obtain a specific KeyStore

		   \param id the identification for the key store
		   \param keyStoreManager the parent manager for this keystore
		*/
		KeyStore(const QString &id, KeyStoreManager *keyStoreManager);

		~KeyStore();

		/**
		   Check if this KeyStore is valid

		   \return true if the KeyStore is valid
		*/
		bool isValid() const;

		/**
		   The KeyStore Type
		*/
		Type type() const;

		/**
		   The name associated with the KeyStore
		*/
		QString name() const;

		/**
		   The ID associated with the KeyStore
		*/
		QString id() const;

		/**
		   Test if the KeyStore is writeable or not

		   \return true if the KeyStore is read-only
		*/
		bool isReadOnly() const;

		/**
		   A list of the KeyStoreEntry objects in this store
		*/
		QList<KeyStoreEntry> entryList() const;

		/**
		   test if the KeyStore holds trusted certificates (and CRLs)
		*/
		bool holdsTrustedCertificates() const;

		/**
		   test if the KeyStore holds identities (eg KeyBundle or PGPSecretKey)
		*/
		bool holdsIdentities() const;

		/**
		   test if the KeyStore holds PGPPublicKey objects
		*/
		bool holdsPGPPublicKeys() const;

		/**
		   Add a entry to the KeyStore

		   \param kb the KeyBundle to add to the KeyStore
		*/
		bool writeEntry(const KeyBundle &kb);

		/**
		   \overload

		   \param cert the Certificate to add to the KeyStore
		*/
		bool writeEntry(const Certificate &cert);

		/**
		   \overload

		   \param crl the CRL to add to the KeyStore
		*/
		bool writeEntry(const CRL &crl);

		/**
		   \overload

		   \param key the PGPKey to add to the KeyStore

		   \return a ref to the key in the keyring
		*/
		PGPKey writeEntry(const PGPKey &key);

		/**
		   Delete the a specified KeyStoreEntry from this KeyStore

		   \param id the ID for the entry to be deleted
		*/
		bool removeEntry(const QString &id);

	signals:
		/**
		   Emitted when the KeyStore is changed
		*/
		void updated();

		/**
		   Emitted when the KeyStore becomes unavailable
		*/
		void unavailable();

	private:
		friend class KeyStorePrivate;
		KeyStorePrivate *d;

		friend class KeyStoreManagerPrivate;
	};

	/**
	   \class KeyStoreManager qca_keystore.h QtCrypto

	   Access keystores, and monitor keystores for changes.

	   If you are looking to use this class, you probably want to
	   take a reference to the global KeyStoreManager, using the
	   QCA::keyStoreManager() function. You then need to start()
	   the KeyStoreManager, and either wait for the busyFinished()
	   signal, or block using waitForBusyFinished().
	*/
	class QCA_EXPORT KeyStoreManager : public QObject
	{
		Q_OBJECT
	public:
		KeyStoreManager(QObject *parent = 0);
		~KeyStoreManager();

		/**
		   Initialize all key store providers
		*/
		static void start();

		/**
		   Initialize a specific key store provider
		*/
		static void start(const QString &provider);

		/**
		   Indicates if the manager is busy looking for key stores
		*/
		bool isBusy() const;

		/**
		   Blocks until the manager is done looking for key stores
		*/
		void waitForBusyFinished();

		/**
		   A list of all the key stores
		*/
		QStringList keyStores() const;

		/**
		   The diagnostic result of key store operations, such as
		   warnings and errors
		*/
		static QString diagnosticText();

		/**
		   Clears the diagnostic result log
		*/
		static void clearDiagnosticText();

		/**
		   If you are not using the eventloop, call this to update
		   the object state to the present
		*/
		void sync();

	signals:
		/**
		   emitted when the manager has started looking for key stores
		*/
		void busyStarted();

		/**
		   emitted when the manager has finished looking for key stores
		*/
		void busyFinished();

		/**
		   emitted when a new key store becomes available
		*/
		void keyStoreAvailable(const QString &id);

	private:
		friend class KeyStoreManagerPrivate;
		KeyStoreManagerPrivate *d;

		friend class Global;
		friend class KeyStorePrivate;

		static void scan();
		static void shutdown();
	};
}

Q_DECLARE_METATYPE(QCA::KeyStoreEntry)
Q_DECLARE_METATYPE(QList<QCA::KeyStoreEntry>)
Q_DECLARE_METATYPE(QList<QCA::KeyStoreEntry::Type>)

#endif
