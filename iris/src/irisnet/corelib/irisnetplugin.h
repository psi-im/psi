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

#ifndef IRISNETPLUGIN_H
#define IRISNETPLUGIN_H

#include "irisnetglobal.h"
#include "netinterface.h"
#include "netavailability.h"
#include "netnames.h"

namespace XMPP {

class NetInterfaceProvider;
class NetAvailabilityProvider;
class NameProvider;
class ServiceProvider;

class IRISNET_EXPORT IrisNetProvider : public QObject
{
	Q_OBJECT

public:
	virtual NetInterfaceProvider *createNetInterfaceProvider();
	virtual NetAvailabilityProvider *createNetAvailabilityProvider();
	virtual NameProvider *createNameProviderInternet();
	virtual NameProvider *createNameProviderLocal();
	virtual ServiceProvider *createServiceProvider();
};

class IRISNET_EXPORT NetInterfaceProvider : public QObject
{
	Q_OBJECT

public:
	class Info
	{
	public:
		QString id, name;
		bool isLoopback;
		QList<QHostAddress> addresses;
		QHostAddress gateway;
	};

	NetInterfaceProvider(QObject *parent = 0) :
		QObject(parent)
	{
	}

	// calling start should populate an initial list that can be
	//   immediately fetched.  do not signal updated() for this.
	virtual void start() = 0;
	virtual QList<Info> interfaces() const = 0;

signals:
	void updated();
};

class IRISNET_EXPORT NetAvailabilityProvider : public QObject
{
	Q_OBJECT

public:
	NetAvailabilityProvider(QObject *parent = 0) :
		QObject(parent)
	{
	}

	// calling start should populate an initial value that can be
	//   immediately fetched.  do not signal updated() for this.
	virtual void start() = 0;
	virtual bool isAvailable() const = 0;

signals:
	void updated();
};

class IRISNET_EXPORT NameProvider : public QObject
{
	Q_OBJECT

public:
	NameProvider(QObject *parent = 0) :
		QObject(parent)
	{
	}

	virtual bool supportsSingle() const;
	virtual bool supportsLongLived() const;
	virtual bool supportsRecordType(int type) const;

	virtual int resolve_start(const QByteArray &name, int qType, bool longLived) = 0;
	virtual void resolve_stop(int id) = 0;

	// transfer from local back to internet
	virtual void resolve_localResultsReady(int id, const QList<XMPP::NameRecord> &results);
	virtual void resolve_localError(int id, XMPP::NameResolver::Error e);

signals:
	void resolve_resultsReady(int id, const QList<XMPP::NameRecord> &results);
	void resolve_error(int id, XMPP::NameResolver::Error e);

	// transfer from internet to local provider
	void resolve_useLocal(int id, const QByteArray &name);
};

class IRISNET_EXPORT ServiceProvider : public QObject
{
	Q_OBJECT

public:
	class ResolveResult
	{
	public:
		QMap<QString,QByteArray> attributes;
		QHostAddress address;
		int port;
		QByteArray hostName; // optional
	};

	ServiceProvider(QObject *parent = 0) :
		QObject(parent)
	{
	}

	virtual int browse_start(const QString &type, const QString &domain) = 0;
	virtual void browse_stop(int id) = 0;
	virtual int resolve_start(const QByteArray &name) = 0;
	virtual void resolve_stop(int id) = 0;
	virtual int publish_start(const QString &instance, const QString &type, int port, const QMap<QString,QByteArray> &attributes) = 0;
	virtual void publish_update(int id, const QMap<QString,QByteArray> &attributes) = 0;
	virtual void publish_stop(int id) = 0;
	virtual int publish_extra_start(int pub_id, const NameRecord &name) = 0;
	virtual void publish_extra_update(int id, const NameRecord &name) = 0;
	virtual void publish_extra_stop(int id) = 0;

signals:
	void browse_instanceAvailable(int id, const XMPP::ServiceInstance &instance);
	void browse_instanceUnavailable(int id, const XMPP::ServiceInstance &instance);
	void browse_error(int id, XMPP::ServiceBrowser::Error e);
	void resolve_resultsReady(int id, const QList<XMPP::ServiceProvider::ResolveResult> &results);
	void resolve_error(int id, XMPP::ServiceResolver::Error e);

	// update does not cause published() signal to be emitted again
	void publish_published(int id);
	void publish_error(int id, XMPP::ServiceLocalPublisher::Error e);

	// update does not cause published() signal to be emitted again
	void publish_extra_published(int id);
	void publish_extra_error(int id, XMPP::ServiceLocalPublisher::Error e);
};

}

Q_DECLARE_INTERFACE(XMPP::IrisNetProvider,      "com.affinix.irisnet.IrisNetProvider/1.0")
Q_DECLARE_INTERFACE(XMPP::NetInterfaceProvider, "com.affinix.irisnet.NetInterfaceProvider/1.0")
Q_DECLARE_INTERFACE(XMPP::NameProvider,         "com.affinix.irisnet.NameProvider/1.0")
Q_DECLARE_INTERFACE(XMPP::ServiceProvider,      "com.affinix.irisnet.ServiceProvider/1.0")

#endif
