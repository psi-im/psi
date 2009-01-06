/*
 * ndns.h - native DNS resolution
 * Copyright (C) 2001, 2002  Justin Karneges
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

#ifndef CS_NDNS_H
#define CS_NDNS_H

#include <QtCore>
#include <QtNetwork>
#include "netnames.h"

// CS_NAMESPACE_BEGIN

class NDns : public QObject
{
	Q_OBJECT
public:
	NDns(QObject *parent=0);
	~NDns();

	void resolve(const QString &);
	void stop();
	bool isBusy() const;

	QHostAddress result() const;
	QString resultString() const;

signals:
	void resultsReady();

private slots:
	void dns_resultsReady(const QList<XMPP::NameRecord> &);
	void dns_error(XMPP::NameResolver::Error);

private:
	XMPP::NameResolver dns;
	bool busy;
	QHostAddress addr;
};

// CS_NAMESPACE_END

#endif
