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

// JDnsShared is an object-based abstraction layer on top of QJDns for
// handling requests.  Now instead of tracking integer handles, the program
// can just use JDnsSharedRequest.
//
// In addition, JDnsShared can track multiple QJDns instances.  For unicast,
// requests are sent on all interfaces and the first reply is used.  For
// multicast, the interfaces are intended to be merged.  However, for now
// only one multicast interface is allowed.
//
// To simplify collection of debug feedback, there is JDnsSharedDebug, which
// can report debug information for multiple JDnsShared instances.
//
// note: this API is internal, and doesn't use private classes

#ifndef JDNSSHARED_H
#define JDNSSHARED_H

#include "jdns/qjdns.h"

namespace XMPP {

class JDnsShared;

class JDnsSharedDebug : public QObject
{
	Q_OBJECT
public:
	JDnsSharedDebug(QObject *parent = 0);
	~JDnsSharedDebug();

signals:
	void debug(const QStringList &lines);

private:
	friend class JDnsShared;
	QMutex m;
	QStringList _lines;
	void addDebug(const QString &name, const QStringList &lines);

private slots:
	void doUpdate();
};

class JDnsSharedRequest : public QObject
{
	Q_OBJECT
public:
	enum Type { Query, Publish };
	enum Error { ErrorNoNet, ErrorGeneric, ErrorNXDomain, ErrorTimeout, ErrorConflict };

	JDnsSharedRequest(JDnsShared *owner, QObject *parent = 0);
	~JDnsSharedRequest();

	Type type();
	void query(const QByteArray &name, int type);
	void publish(QJDns::PublishMode m, const QJDns::Record &record);
	void publishUpdate(const QJDns::Record &record);
	void cancel();

	bool success() const;
	Error error() const;
	QList<QJDns::Record> results() const;

signals:
	void resultsReady();

private:
	friend class JDnsShared;

	class Handle
	{
	public:
		QJDns *jdns;
		int id;
	};

	JDnsShared *_owner;
	Type _type;
	QList<Handle> _handles;
	bool _success;
	Error _error;
	QList<QJDns::Record> _results;
};

class JDnsShared : public QObject
{
	Q_OBJECT
public:
	enum Mode { UnicastInternet, UnicastLocal, Multicast };

	JDnsShared(Mode mode, JDnsSharedDebug *db, const QString &dbname, QObject *parent = 0);
	~JDnsShared();

	bool addInterface(const QHostAddress &addr);
	void removeInterface(const QHostAddress &addr);
	void setNameServers(const QList<QJDns::NameServer> &nameServers);
	void shutdown();

signals:
	void shutdownFinished();
	void debug(const QStringList &lines);

private:
	class Instance
	{
	public:
		QJDns *jdns;
		QHostAddress addr;
		bool shutdown_done;

		Instance() : jdns(0), shutdown_done(false) {}
	};

	Mode mode;
	bool shutting_down;
	JDnsSharedDebug *db;
	QString dbname;
	QList<Instance*> instances;
	QList<QJDns::NameServer> nameServers;
	QList<JDnsSharedRequest*> requests;

	void jdns_link(QJDns *jdns);
	int indexByJDns(const QJDns *jdns);
	void applyNameServers(Instance *i);
	void doDebug(QJDns *jdns, int index);
	JDnsSharedRequest *findRequest(QJDns *jdns, int id);

	friend class JDnsSharedRequest;
	void queryStart(JDnsSharedRequest *obj, const QByteArray &name, int qType);
	void queryCancel(JDnsSharedRequest *obj);
	void publishStart(JDnsSharedRequest *obj, QJDns::PublishMode m, const QJDns::Record &record);
	void publishUpdate(JDnsSharedRequest *obj, const QJDns::Record &record);
	void publishCancel(JDnsSharedRequest *obj);

private slots:
	void jdns_resultsReady(int id, const QJDns::Response &results);
	void jdns_published(int id);
	void jdns_error(int id, QJDns::Error e);
	void jdns_shutdownFinished();
	void jdns_debugLinesReady();
};

}

#endif
