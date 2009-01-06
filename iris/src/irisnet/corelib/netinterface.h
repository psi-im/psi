/*
 * Copyright (C) 2006  Justin Karneges
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

#ifndef NETINTERFACE_H
#define NETINTERFACE_H

#include "irisnetglobal.h"

namespace XMPP {

class NetInterfaceManager;
class NetInterfacePrivate;
class NetInterfaceManagerPrivate;

/**
   \brief Provides information about a network interface

   NetInterface provides information about a particular network interface.  Construct it by passing the interface id of interest (e.g. "eth0") and a NetInterfaceManager parent object.  Interface ids can be obtained from NetInterfaceManager.

   To test if a NetInterface is valid, call isValid().  Use name() to return a display-friendly name of the interface.  The addresses() function returns a list of IP addresses for this interface.  There may be a gateway IP address associated with this interface, which can be fetched with gateway().

   Here's an example of how to print the IP addresses of eth0:
   \code
NetInterface iface("eth0");
if(iface.isValid())
{
	QList<QHostAddress> addrs = iface.addresses();
	for(int n = 0; n < addrs.count(); ++n)
		printf("%s\n", qPrintable(addrs[n].toString()));
}
   \endcode

   If the interface goes away, the unavailable() signal is emitted and the NetInterface becomes invalid.

   \sa NetInterfaceManager
*/
class IRISNET_EXPORT NetInterface : public QObject
{
	Q_OBJECT
public:
	/**
	   \brief Constructs a new interface object with the given \a id and \a manager

	   If \a id is not a valid interface id, then the object will not be valid (isValid() will return false).  Normally it is not necessary to check for validity, since interface ids obtained from NetInterfaceManager are guaranteed to be valid until the event loop resumes.

	   \sa isValid
	*/
	NetInterface(const QString &id, NetInterfaceManager *manager);

	/**
	   \brief Destroys the interface object
	*/
	~NetInterface();

	/**
	   \brief Returns true if the interface is valid, otherwise returns false

	   \sa unavailable
	*/
	bool isValid() const;

	/**
	   \brief Returns the id of this interface

	   This is the id that was passed in the constructor.
	*/
	QString id() const;

	/**
	   \brief Returns a display-friendly name of this interface

	   The name may be the same as the id.

	   \sa id
	*/
	QString name() const;

	/**
	   \brief Returns the addresses of this interface

	   There will always be at least one address.  In some cases there might be multiple, such as on Unix where it is possible for the same interface to have both an IPv4 and an IPv6 address.
	*/
	QList<QHostAddress> addresses() const;

	/**
	   \brief Returns the gateway of this interface

	   If there is no gateway associated with this interface, a null QHostAddress is returned.
	*/
	QHostAddress gateway() const; // optional

signals:
	/**
	   \brief Notifies when the interface becomes unavailable

	   Once this signal is emitted, the NetInterface object becomes invalid and is no longer very useful.  A new NetInterface object must be created if a valid object with current information is desired.

	   \note If the interface information changes, the interface is considered to have become unavailable.

	   \sa isValid
	*/
	void unavailable();

private:
	friend class NetInterfacePrivate;
	NetInterfacePrivate *d;

	friend class NetInterfaceManagerPrivate;
};

/**
   \brief Manages network interface information

   NetInterfaceManager keeps track of all available network interfaces.

   An interface is considered available if it exists, is "Up", has at least one IP address, and is non-Loopback.

   The interfaces() function returns a list of available interface ids.  These ids can be used with NetInterface to get information about the interfaces.  For example, here is how you could print the names of the available interfaces:

   \code
NetInterfaceManager netman;
QStringList id_list = netman.interfaces();
for(int n = 0; n < id_list.count(); ++n)
{
	NetInterface iface(id_list[n], &netman);
	printf("name: [%s]\n", qPrintable(iface.name()));
}
   \endcode

   When a new network interface is available, the interfaceAvailable() signal will be emitted.  Note that interface unavailability is not notified by NetInterfaceManager.  Instead, use NetInterface to monitor a specific network interface for unavailability.

   Interface ids obtained through NetInterfaceManager are guaranteed to be valid until the event loop resumes, or until the next call to interfaces() or interfaceForAddress().

   \sa NetInterface
*/
class IRISNET_EXPORT NetInterfaceManager : public QObject
{
	Q_OBJECT
public:
	/**
	   \brief Constructs a new manager object with the given \a parent
	*/
	NetInterfaceManager(QObject *parent = 0);

	/**
	   \brief Destroys the manager object
	*/
	~NetInterfaceManager();

	/**
	   \brief Returns the list of available interface ids

	   \sa interfaceAvailable
	   \sa interfaceForAddress
	*/
	QStringList interfaces() const;

	/**
	   \brief Looks up an interface id by IP address

	   This function looks for an interface that has the address \a a.  If there is no such interface, a null string is returned.

	   This is useful for determing the network interface associated with an outgoing QTcpSocket:

	   \code
QString iface = NetInterfaceManager::interfaceForAddress(tcpSocket->localAddress());
	   \endcode

	   \sa interfaces
	*/
	static QString interfaceForAddress(const QHostAddress &a);

signals:
	/**
	   \brief Notifies when an interface becomes available

	   The \a id parameter is the interface id, ready to use with NetInterface.
	*/
	void interfaceAvailable(const QString &id);

private:
	friend class NetInterfaceManagerPrivate;
	NetInterfaceManagerPrivate *d;

	friend class NetInterface;
	friend class NetInterfacePrivate;

	void *reg(const QString &id, NetInterface *i);
	void unreg(NetInterface *i);
};

}

#endif
