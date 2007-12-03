/*
 * srvresolver.h - class to simplify SRV lookups
 * Copyright (C) 2003  Justin Karneges
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

#ifndef CS_SRVRESOLVER_H
#define CS_SRVRESOLVER_H

#include <QList>
#include <q3dns.h>

// CS_NAMESPACE_BEGIN

class SrvResolver : public QObject
{
	Q_OBJECT
public:
	SrvResolver(QObject *parent=0);
	~SrvResolver();

	void resolve(const QString &server, const QString &type, const QString &proto);
	void resolveSrvOnly(const QString &server, const QString &type, const QString &proto);
	void next();
	void stop();
	bool isBusy() const;

	QList<Q3Dns::Server> servers() const;

	bool failed() const;
	QHostAddress resultAddress() const;
	Q_UINT16 resultPort() const;

signals:
	void resultsReady();

private slots:
	void qdns_done();
	void ndns_done();
	void t_timeout();

private:
	class Private;
	Private *d;

	void tryNext();
	void resolve(const QString &server, const QString &type, const QString &proto, bool srvOnly);
};

// CS_NAMESPACE_END

#endif
