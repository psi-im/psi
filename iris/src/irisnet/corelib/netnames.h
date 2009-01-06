/*
 * Copyright (C) 2006,2008  Justin Karneges
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

#ifndef NETNAMES_H
#define NETNAMES_H

#include <QtCore>
#include <QtNetwork>
#include "irisnetglobal.h"

namespace XMPP {

class NameManager;

class IRISNET_EXPORT NetNames
{
public:
	// free any shared data, shutdown internal dns sessions if necessary.
	static void cleanup();

	// return current diagnostic text, clear the buffer.
	static QString diagnosticText();

	// convert idn names
	static QByteArray idnaFromString(const QString &in);
	static QString idnaToString(const QByteArray &in);

	// dns escaping
	static QByteArray escapeDomain(const QByteArray &in);
	static QByteArray unescapeDomain(const QByteArray &in);

private:
	NetNames();
};

/**
   \brief Provides a DNS record

   NameRecord provides a DNS (Domain Name System) record, which is information assicated with a domain name.  For most purposes, the information is an IP address.  However, DNS records are capable of holding a variety of data types, such as named pointers to other domain names and even arbitrary text strings.  The results of a NameResolver operation are a list of NameRecords.

   The most common type is the address record, "A", which contains an IPv4 address.  Here is an example of how to get the IP address out of an address record:

\code
NameRecord record = ... // obtain a record from somewhere
if(record.type() == NameRecord::A)
{
	QHostAddress ip = record.address(); // get the IP
	...
}
\endcode

   Getting the data out of a NameRecord involves calling the right retrieval functions, depending on the type.  Many types share retrieval functions.  For example, the "AAAA" type holds an IPv6 address, which is accessed the same way as the "A" type, by calling address().  See the NameRecord::Type enum for further information about which retrieval functions should be called for each type.

   To create a NameRecord, use setOwner() and setTtl() as necessary, and then call one of the set<em>X</em> functions (where <em>X</em> is the desired type).  For example, to set an A or AAAA record, use setAddress() like this:

\code
// make example.com the owner, with 1 hour TTL
NameRecord record("example.com", 3600);
record.setAddress(QHostAddress("1.2.3.4"));
\endcode

   Note that in the case of setAddress(), the record type need not be specified.  NameRecord will determine the type to use based on the given QHostAddress.

   \sa NameResolver
*/
class IRISNET_EXPORT NameRecord
{
public:
	/**
		\brief The type of DNS record

		The retrieval functions are shown for each type.
	*/
	enum Type
	{
		A,     ///< IPv4 address.  Use address().
		Aaaa,  ///< IPv6 address.  Use address().
		Mx,    ///< Mail server.  Use name() and priority().
		Srv,   ///< Generic server.  Use name(), port(), priority(), and weight().
		Cname, ///< Canonical name.  Use name().
		Ptr,   ///< Pointer.  Use name().
		Txt,   ///< List of text strings.  Use texts().
		Hinfo, ///< Host information.  Use cpu() and os().
		Ns,    ///< Name server.  Use name().
		Null,  ///< Null type.  Use rawData().
		Any    ///< "Any record", for use with NameResolver::start() only.  A NameRecord object will never be of this type.
	};

	/**
	   \brief Constructs a null record object

	   \sa isNull
	*/
	NameRecord();

	/**
	   \brief Constructs a partially initialized record object, with the given \a owner and \a ttl

	   For the record to be usable, call an appropriate set<em>X</em> function (where <em>X</em> is the desired type) afterwards.
	*/
	NameRecord(const QByteArray &owner, int ttl);

	/**
	   \brief Constructs a copy of \a from
	*/
	NameRecord(const NameRecord &from);

	/**
	   \brief Destroys the record object
	*/
	~NameRecord();

	/**
	   \brief Assigns \a from to this object and returns a reference to this object
	*/
	NameRecord & operator=(const NameRecord &from);

	/**
	   \brief Returns true if this record object is null, otherwise returns false

	   Be sure not to confuse a null object with the NULL type (NameRecord::Null).  Don't ask why DNS has a type called NULL that contains valid data.
	*/
	bool isNull() const; // don't confuse with Null type

	/**
	   \brief Returns the owner of this record

	   The owner is usually not a useful attribute, since it will be the same as the name searched for with NameResolver.  For example, if the A record of "example.com" is looked up, then the resulting records will all have "example.com" as the owner.

	   \sa setOwner
	*/
	QByteArray owner() const;

	/**
	   \brief Returns the TTL (time-to-live) of this record

	   This is the number of seconds the record should be considered valid, which is useful information when performing caching.

	   As a special exception, a TTL of 0 when performing a long-lived lookup indicates that a record is no longer available.

	   \sa setTtl
	*/
	int ttl() const;

	/**
	   \brief Returns the type of this record
	*/
	Type type() const;

	/**
	   \brief Returns the IP address

	   For NameRecord::A and NameRecord::Aaaa types.
	*/
	QHostAddress address() const;

	/**
	   \brief Returns the domain name

	   For NameRecord::Mx, NameRecord::Srv, NameRecord::Cname, NameRecord::Ptr, and NameRecord::Ns types.
	*/
	QByteArray name() const;

	/**
	   \brief Returns the priority

	   For NameRecord::Mx and NameRecord::Srv types.
	*/
	int priority() const;

	/**
	   \brief Returns the weight

	   For the NameRecord::Srv type.
	*/
	int weight() const;

	/**
	   \brief Returns the port

	   For the NameRecord::Srv type.
	*/
	int port() const;

	/**
	   \brief Returns the list of text strings

	   For the NameRecord::Txt type.
	*/
	QList<QByteArray> texts() const;

	/**
	   \brief Returns the architecture identifier string

	   For the NameRecord::Hinfo type.
	*/
	QByteArray cpu() const;

	/**
	   \brief Returns the operating system identifier string

	   For the NameRecord::Hinfo type.
	*/
	QByteArray os() const;

	/**
	   \brief Returns the raw data

	   For the NameRecord::Null type.
	*/
	QByteArray rawData() const;

	/**
	   \brief Sets the owner of this record to \a name

	   \sa owner
	*/
	void setOwner(const QByteArray &name);

	/**
	   \brief Sets the TTL (time-to-live) of this record to \a ttl seconds

	   \sa ttl
	*/
	void setTtl(int seconds);

	/**
	   \brief Set as A or AAAA record, with data \a a

	   The protocol of \a a determines whether the type will be NameRecord::A or NameRecord::Aaaa.
	*/
	void setAddress(const QHostAddress &a);

	/**
	   \brief Set as MX record, with data \a name and \a priority
	*/
	void setMx(const QByteArray &name, int priority);

	/**
	   \brief Set as SRV record, with data \a name, \a port, \a priority, and \a weight
	*/
	void setSrv(const QByteArray &name, int port, int priority, int weight);

	/**
	   \brief Set as CNAME record, with data \a name
	*/
	void setCname(const QByteArray &name);

	/**
	   \brief Set as PTR record, with data \a name
	*/
	void setPtr(const QByteArray &name);

	/**
	   \brief Set as TXT record, with data \a texts
	*/
	void setTxt(const QList<QByteArray> &texts);

	/**
	   \brief Set as HINFO record, with data \a cpu and \a os
	*/
	void setHinfo(const QByteArray &cpu, const QByteArray &os);

	/**
	   \brief Set as NS record, with data \a name
	*/
	void setNs(const QByteArray &name);

	/**
	   \brief Set as NULL record, with data \a rawData
	*/
	void setNull(const QByteArray &rawData);

private:
	class Private;
	QSharedDataPointer<Private> d;
};

class IRISNET_EXPORT ServiceInstance
{
public:
	ServiceInstance();
	ServiceInstance(const QString &instance, const QString &type, const QString &domain, const QMap<QString,QByteArray> &attributes);
	ServiceInstance(const ServiceInstance &from);
	~ServiceInstance();
	ServiceInstance & operator=(const ServiceInstance &from);

	QString instance() const;
	QString type() const;
	QString domain() const;
	QMap<QString,QByteArray> attributes() const;
	QByteArray name() const; // full dns label

private:
	class Private;
	QSharedDataPointer<Private> d;

	friend class NameManager;
};

/**
   \brief Performs a DNS lookup

   NameResolver performs an asynchronous DNS lookup for a given domain name and record type.  Call start() to begin.  The resultsReady() signal is emitted on success, otherwise error() is emitted.  To cancel a lookup, call stop().

   For example, here is how to obtain the IPv4 addresses of a domain name:
\code
NameResolver *resolver;

void do_lookup()
{
	resolver = new NameResolver;
	connect(resolver, SIGNAL(resultsReady(const QList<XMPP::NameRecord> &)),
		SLOT(dns_resultsReady(const QList<XMPP::NameRecord> &)));
	connect(resolver, SIGNAL(error(XMPP::NameResolver::Error)),
		SLOT(dns_error(XMPP::NameResolver::Error)));

	// look up affinix.com
	resolver->start("affinix.com");
}

void dns_resultsReady(const QList<XMPP::NameRecord> &results)
{
	// print IP addresses
	foreach(NameRecord i, results)
		printf("%s\n", qPrintable(i.address().toString()));
}

void dns_error(XMPP::NameResolver::Error error)
{
	// handle error
	...
}
\endcode

   Yes, a domain name can have multiple IP addresses.  Many applications ignore this fact, and use only one of the answers.  A proper network application should try connecting to each IP address until one succeeds.

   To lookup other types, pass the desired type to start().  For example, suppose you want to look up the MX record of a domain name:

\code
// look up the MX record for affinix.com
resolver->start("affinix.com", NameRecord::Mx);
\endcode

   It is also possible to perform long-lived queries.  This is generally useful for DNS Service Discovery.  Long-lived queries are continuous, and resultsReady() may be emitted multiple times.  Unlike a normal lookup, which stops once the results are returned, a long-lived query will keep going until stop() is called.

   For example, suppose you want to scan the local network for SSH services.  According to the DNS-SD protocol, this is done by querying for the name "_ssh._tcp.local." of type PTR.

\code
// monitor for SSH services on the local network
resolver->start("_ssh._tcp.local.", NameRecord::Ptr, NameResolver::LongLived);
\endcode

   Don't be alarmed by the trailing dot (".") character in this last example.  It is not well known, but all valid DNS domain names end with a dot.  However, NameResolver, like most DNS programming interfaces, allows the dot to be left out.  What this means is that if a trailing dot is missing in the input to start(), NameResolver will internally append one before performing the query.

   \sa NameRecord
*/
class IRISNET_EXPORT NameResolver : public QObject
{
	Q_OBJECT
public:
	/**
	   \brief Resolve mode
	*/
	enum Mode
	{
		Single,       ///< A normal DNS query with a single result set.
		LongLived     ///< An endless query, with multiple result sets allowed.
	};

	/**
	   \brief Resolve error
	*/
	enum Error
	{
		ErrorGeneric,     ///< General failure during lookup, no further details.
		ErrorNoName,      ///< Name does not exist.
		ErrorTimeout,     ///< The operation timed out.
		ErrorNoLocal,     ///< The query is to the local network, but no mechanism for Multicast DNS is available.
		ErrorNoLongLived  ///< The query requires long-lived capability, but no mechanism for doing so is available.
	};

	/**
	   \brief Constructs a new resolver object with the given \a parent
	*/
	NameResolver(QObject *parent = 0);

	/**
	   \brief Destroys the resolver object

	   The lookup is, of course, stopped.
	*/
	~NameResolver();

	/**
	   \brief Starts a lookup

	   A lookup for \a name of \a type is started.  For normal queries, \a mode should be NameResolver::Single (this is the default).  For long-lived queries, use NameResolver::LongLived.

	   \sa stop
	*/
	void start(const QByteArray &name, NameRecord::Type type = NameRecord::A, Mode mode = Single);

	/**
	   \brief Stops a lookup

	   Use this function if you want to stop the current lookup, such that the resolver object may be reused again later.  If you don't plan to reuse the object, then destroying the object is enough.

	   \sa start
	*/
	void stop();

signals:
	/**
	   \brief Notification of result records

	   This signal is emitted when results of the lookup operation have arrived.  The \a results parameter is a list of NameRecords.  All records will be of the type queried for with start(), unless the NameRecord::Any type was specified, in which case the records may be of any type

	   When using the NameResolver::Single mode, the lookup is stopped once results are ready.  However, with the NameResolver::LongLived mode, the lookup stays active, and in that case this signal may be emitted multiple times.
	*/
	void resultsReady(const QList<XMPP::NameRecord> &results);

	/**
	   \brief Notification of error

	   This signal is emitted if an error has occurred while performing a lookup.  The reason for error can be found in \a e.  Regardless of the mode used, the lookup is stopped when an error occurs.
	*/
	void error(XMPP::NameResolver::Error e);

private:
	class Private;
	friend class Private;
	Private *d;

	friend class NameManager;
};

class IRISNET_EXPORT ServiceBrowser : public QObject
{
	Q_OBJECT
public:
	enum Error
	{
		ErrorGeneric,
		ErrorNoLocal,
		ErrorNoWide
	};

	ServiceBrowser(QObject *parent = 0);
	~ServiceBrowser();

	void start(const QString &type, const QString &domain = "local");
	void stop();

signals:
	void instanceAvailable(const XMPP::ServiceInstance &instance);
	void instanceUnavailable(const XMPP::ServiceInstance &instance);
	void error();

private:
	class Private;
	friend class Private;
	Private *d;

	friend class NameManager;
};

class IRISNET_EXPORT ServiceResolver : public QObject
{
	Q_OBJECT
public:
	enum Error
	{
		ErrorGeneric,
		ErrorTimeout,
		ErrorNoLocal
	};

	ServiceResolver(QObject *parent = 0);
	~ServiceResolver();

	void startFromInstance(const QByteArray &name);
	void startFromDomain(const QString &domain, const QString &type);
	void startFromPlain(const QString &host, int port); // non-SRV
	void tryNext();
	void stop();

signals:
	void resultsReady(const QHostAddress &address, int port);
	void finished();
	void error(); // SRV lookup failed

private:
	class Private;
	friend class Private;
	Private *d;

	friend class NameManager;
};

class IRISNET_EXPORT ServiceLocalPublisher : public QObject
{
	Q_OBJECT
public:
	enum Error
	{
		ErrorGeneric,  // generic error
		ErrorConflict, // name in use
		ErrorNoLocal   // unable to setup multicast dns
	};

	ServiceLocalPublisher(QObject *parent = 0);
	~ServiceLocalPublisher();

	void publish(const QString &instance, const QString &type, int port, const QMap<QString,QByteArray> &attributes);
	void updateAttributes(const QMap<QString,QByteArray> &attributes);
	void addRecord(const NameRecord &rec);
	void cancel();

signals:
	void published();
	void error(XMPP::ServiceLocalPublisher::Error e);

private:
	class Private;
	friend class Private;
	Private *d;

	friend class NameManager;
};

}

#endif
