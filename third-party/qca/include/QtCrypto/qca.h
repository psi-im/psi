/*
 * qca.h - Qt Cryptographic Architecture
 * Copyright (C) 2003-2005  Justin Karneges <justin@affinix.com>
 * Copyright (C) 2004-2006  Brad Hards <bradh@frogmouth.net>
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
   \file qca.h

   Summary header file for %QCA.

   \note You should not use this header directly from an
   application. You should just use <tt> \#include \<QtCrypto>
   </tt> instead.
*/

#ifndef QCA_H
#define QCA_H

#include "qca_core.h"
#include "qca_textfilter.h"
#include "qca_basic.h"
#include "qca_publickey.h"
#include "qca_cert.h"
#include "qca_keystore.h"
#include "qca_securelayer.h"
#include "qca_securemessage.h"
#include "qcaprovider.h"
#include "qpipe.h"

/**
   \mainpage Qt Cryptographic Architecture

   Taking a hint from the similarly-named
   <a href="http://java.sun.com/j2se/1.5.0/docs/guide/security/CryptoSpec.html">Java
   Cryptography Architecture</a>, %QCA aims to provide a
   straightforward and cross-platform cryptographic API, using Qt
   datatypes and conventions.  %QCA separates the API from the
   implementation, using plugins known as Providers.  The advantage
   of this model is to allow applications to avoid linking to or
   explicitly depending on any particular cryptographic library.
   This allows one to easily change or upgrade Provider
   implementations without even needing to recompile the
   application!

   %QCA should work everywhere %Qt does, including Windows/Unix/MacOSX. This
   version of %QCA is for Qt4, and requires no Qt3 compatibility code.

   \section features Features

   This library provides an easy API for the following features:
     - Secure byte arrays (QSecureArray)
     - Arbitrary precision integers (QBigInteger)
     - Random number generation (QCA::Random)
     - SSL/TLS (QCA::TLS)
     - X509 certificates (QCA::Certificate and QCA::CertificateCollection)
     - X509 certificate revocation lists (QCA::CRL)
     - Built-in support for operating system certificate root storage (QCA::systemStore)
     - Simple Authentication and Security Layer (SASL) (QCA::SASL)
     - Cryptographic Message Syntax (e.g., for S/MIME) (QCA::CMS)
     - PGP messages (QCA::OpenPGP)
     - Unified PGP/CMS API (QCA::SecureMessage)
     - Subsystem for managing Smart Cards and PGP keyrings (QCA::KeyStore)
     - Simple but flexible logging system (QCA::Logger)
     - RSA (QCA::RSAPrivateKey and QCA::RSAPublicKey)
     - DSA (QCA::DSAPrivateKey and QCA::DSAPublicKey)
     - Diffie-Hellman (QCA::DHPrivateKey and QCA::DHPublicKey)
     - Hashing (QCA::Hash) with
         - SHA-0
         - SHA-1
         - MD2
         - MD4
         - MD5
         - RIPEMD160
         - SHA-224
         - SHA-256
         - SHA-384
         - SHA-512
     - Ciphers (QCA::Cipher) using
         - BlowFish
         - Triple DES
         - DES
         - AES (128, 192 and 256 bit)
     - Message Authentication Code (QCA::MAC), using
         - HMAC with SHA-1
         - HMAC with MD5
         - HMAC with RIPEMD160
	 - HMAC with SHA-224
	 - HMAC with SHA-256
	 - HMAC with SHA-384
	 - HMAC with SHA-512
     - Encoding and decoding of hexadecimal (QCA::Hex) and 
     Base64 (QCA::Base64)
  
   Functionality is supplied via plugins.  This is useful for avoiding
   dependence on a particular crypto library and makes upgrading easier,
   as there is no need to recompile your application when adding or
   upgrading a crypto plugin.  Also, by pushing crypto functionality into
   plugins, your application is free of legal issues, such as export
   regulation.
 
   And of course, you get a very simple crypto API for Qt, where you can
   do things like:
   \code
   QString hash = QCA::Hash("sha1").hashToString(blockOfData);
   \endcode

   \section using Using QCA

   The application simply includes &lt;QtCrypto> and links to
   libqca, which provides the 'wrapper API' and plugin loader.  Crypto
   functionality is determined during runtime, and plugins are loaded
   from the 'crypto' subfolder of the %Qt library paths. There are <a
   href="examples.html">additional examples available</a>.

   \section availability Availability

   \subsection qca2code Current development

   The latest version of the code is available from the KDE Subversion
   server (there is no formal release of the current version at this time). See
   <a href="http://developer.kde.org/source/anonsvn.html">
   http://developer.kde.org/source/anonsvn.html
   </a> for general instructions. You do <i>not</i> need kdelibs or
   arts modules for %QCA - just pull down kdesupport/qca. The plugins
   are in the same tree. Naturally you will need %Qt properly set up
   and configured in order to build and use %QCA.

   The Subversion code can also be browsed
   <a href="http://websvn.kde.org/trunk/kdesupport/qca/">
   via the web</a>

   \subsection qca1code Previous versions
   
   A previous version of %QCA (sometimes referred to as QCA1) which
   works with Qt3, is still available.
   You will need to get the main library 
   (<a href="src/qca1/qca-1.0.tar.bz2">qca-1.0.tar.bz2</a>) and one or
   more providers
   (<a href="src/qca1/qca-tls-1.0.tar.bz2">qca-tls-1.0.tar.bz2</a> for
   the OpenSSL based provider, or
   <a href="src/qca1/qca-sasl-1.0.tar.bz2">qca-sasl-1.0.tar.bz2</a> for
   the SASL based provider). Note that development of QCA1 has basically
   stopped.

 */

/** \page architecture Architecture

\note You don't need to understand any of this to use %QCA. It is
documented for those who are curious, and for anyone planning to
extend or modify %QCA.

The design of %QCA is based on the Bridge design pattern. The intent of
the Bridge pattern is to "Decouple an abstraction from its
implementation so that the two can vary independently." [Gamma et.al,
pg 151].

To understand how this decoupling works in the case of %QCA, is is
easiest to look at an example - a cryptographic Hash. The API is
pretty simple (although I've left out some parts that aren't required
for this example):

\code
class QCA_EXPORT Hash : public Algorithm, public BufferedComputation
{
public:
    Hash(const QString &type, const QString &provider);
    virtual void clear();
    virtual void update(const QSecureArray &a);
    virtual QSecureArray final();
}
\endcode

The implementation for the Hash class is almost as simple:

\code
Hash::Hash(const QString &type, const QString &provider)
:Algorithm(type, provider)
{
}

void Hash::clear()
{
        static_cast<HashContext *>(context())->clear();
}

void Hash::update(const QSecureArray &a)
{
        static_cast<HashContext *>(context())->update(a);
}

QSecureArray Hash::final()
{
        return static_cast<HashContext *>(context())->final();
}
\endcode

The reason why it looks so simple is that the various methods in Hash
just call out to equivalent routines in the context() object. The
context comes from a call (getContext()) that is made as part of the
Algorithm constructor. That getContext() call causes %QCA to work
through the list of providers (generally plugins) that it knows about,
looking for a provider that can produce the right kind of context (in
this case, a HashContext).

  The code for a HashContext doesn't need to be linked into %QCA - it can
be varied in its implementation, including being changed at run-time. The
application doesn't need to know how HashContext is implemented, because
  it just has to deal with the Hash class interface. In fact, HashContext
  may not be implemented, so the application should check (using 
  QCA::isSupported()) before trying to use features that are implemented
with plugins.

  The code for one implementation (in this case, calling OpenSSL) is shown
below. 
\code
class opensslHashContext : public HashContext
{
public:
    opensslHashContext(const EVP_MD *algorithm, Provider *p, const QString &type) : HashContext(p, type)
    {
        m_algorithm = algorithm;
        EVP_DigestInit( &m_context, m_algorithm );
    };

    ~opensslHashContext()
    {
        EVP_MD_CTX_cleanup(&m_context);
    }

    void clear()
    {
        EVP_MD_CTX_cleanup(&m_context);
        EVP_DigestInit( &m_context, m_algorithm );
    }

    void update(const QSecureArray &a)
    {
        EVP_DigestUpdate( &m_context, (unsigned char*)a.data(), a.size() );
    }

    QSecureArray final()
    {
        QSecureArray a( EVP_MD_size( m_algorithm ) );
        EVP_DigestFinal( &m_context, (unsigned char*)a.data(), 0 );
        return a;
    }

    Provider::Context *clone() const
    {
        return new opensslHashContext(*this);
    }

protected:
    const EVP_MD *m_algorithm;
    EVP_MD_CTX m_context;
};
\endcode

This approach (using an Adapter pattern) is very common in %QCA backends,
because the plugins are often based on existing libraries.

  In addition to the various Context objects, each provider also has
  a parameterised Factory class that has a createContext() method, as
  shown below:
\code
        Context *createContext(const QString &type)
        {
                //OpenSSL_add_all_digests();
                if ( type == "sha1" )
                        return new opensslHashContext( EVP_sha1(), this, type);
                else if ( type == "sha0" )
                        return new opensslHashContext( EVP_sha(), this, type);
                else if ( type == "md5" )
                        return new opensslHashContext( EVP_md5(), this, type);
                else if ( type == "aes128-cfb" )
                        return new opensslCipherContext( EVP_aes_128_cfb(), 0, this, type);
                else if ( type == "aes128-cbc" )
                        return new opensslCipherContext( EVP_aes_128_cbc(), 0, this, type);
		else
		        return 0;
\endcode

The resulting effect is that %QCA can ask the provider to provide an appropriate
Context object without worrying about how it is implemented.

*/

/** \page providers Providers

%QCA works on the concept of a "provider". There is a limited
internal provider (named "default"), but most of the work is
done in plugin modules.

The logic to selection of a provider is fairly simple. The user can 
specify a provider name - if that name exists, and the provider supports
the requested feature, then the named provider is used. If that
didn't work, then the available plugins are searched (based on a
priority order) for the requested feature. If that doesn't work,
then the default provider is searched for the requested feature.

So the only way to get the default provider is to either have no other support
whatsoever, or to specify the default provider directly (this goes for the
algorithm constructors as well as setGlobalRNG()).

You can add your own provider in two ways - as a shared object plugin,
and as a part of the client code.

The shared object plugin needs to be able to be found using the
built-in scan logic - this normally means you need to install it into
the plugins/crypto subdirectory within the directory that Qt is
installed to. This will make it available for all applications.

If you have a limited application domain (such as a specialist
algorithm, or a need to be bug-compatible), you may find it easier to
create a client-side provider, and add it using the
QCA::insertProvider call. There is an example of this - see
<a href="aes-cmac_8cpp-example.html">the AES-CMAC example</a>.
*/


#endif
