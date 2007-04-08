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

#ifndef IRISNETPLUGIN_H
#define IRISNETPLUGIN_H

#include "irisnetglobal.h"
#include "netinterface.h"
#include "netnames.h"

namespace XMPP {

class NetInterfaceProvider;
class NameProvider;
class ServiceProvider;

class IRISNET_EXPORT IrisNetProvider : public QObject
{
	Q_OBJECT
public:
	virtual ~IrisNetProvider();

	virtual NetInterfaceProvider *createNetInterfaceProvider();
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

	NetInterfaceProvider(QObject *parent = 0) : QObject(parent) {}

	// calling start should populate an initial list that can be
	//   immediately fetched.  do not signal updated() for this.
	virtual void start() = 0;
	virtual QList<Info> interfaces() const = 0;

signals:
	void updated();
};

class IRISNET_EXPORT NameProvider : public QObject
{
	Q_OBJECT
public:
	NameProvider(QObject *parent = 0) : QObject(parent) {}

	virtual int resolve_start(const QByteArray &name, int qType, bool longLived) = 0;
	virtual void resolve_stop(int id) = 0;

	// transfer from local back to internet
	virtual void resolve_localResultsReady(int id, const QList<XMPP::NameRecord> &results) = 0;
	virtual void resolve_localError(int id, XMPP::NameResolver::Error e) = 0;

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
	ServiceProvider(QObject *parent = 0) : QObject(parent) {}

	virtual int browse_start(const QString &type, const QString &domain) = 0;
	virtual void browse_stop(int id) = 0;
	virtual int resolve_start(const QByteArray &name) = 0;
	virtual void resolve_stop(int id) = 0;
	virtual int publish_start(const QString &instance, const QString &type, int port, const QMap<QString,QByteArray> &attributes) = 0;
	virtual int publish_update(const QMap<QString,QByteArray> &attributes) = 0;
	virtual void publish_cancel(int id) = 0;
	virtual int publish_extra_start(int pub_id, const NameRecord &name) = 0;
	virtual int publish_extra_update(int id, const NameRecord &name) = 0;
	virtual void publish_extra_cancel(int id) = 0;

signals:
	void browse_instanceAvailable(int id, const XMPP::ServiceInstance &instance);
	void browse_instanceUnavailable(int id, const XMPP::ServiceInstance &instance);
	void browse_error(int id);
	void resolve_resultsReady(int id, const QHostAddress &address, int port);
	void resolve_error(int id);
	void publish_published(int id);
	void publish_error(int id, XMPP::ServiceLocalPublisher::Error e);
	void publish_extra_published(int id);
	void publish_extra_error(int id, XMPP::ServiceLocalPublisher::Error e);
};

}

Q_DECLARE_INTERFACE(XMPP::IrisNetProvider,      "com.affinix.irisnet.IrisNetProvider/1.0")
Q_DECLARE_INTERFACE(XMPP::NetInterfaceProvider, "com.affinix.irisnet.NetInterfaceProvider/1.0")
Q_DECLARE_INTERFACE(XMPP::NameProvider,         "com.affinix.irisnet.NameProvider/1.0")
Q_DECLARE_INTERFACE(XMPP::ServiceProvider,      "com.affinix.irisnet.ServiceProvider/1.0")

#endif
