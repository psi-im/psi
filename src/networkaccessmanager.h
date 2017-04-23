/*
 * networkaccessmanager.h - Network Manager for WebView able to process
 * custom url schemas
 * Copyright (C) 2010-2017 senu, Sergey Ilinykh
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef _NETWORKACCESSMANAGER_H
#define _NETWORKACCESSMANAGER_H

#include <QNetworkAccessManager>
#include <QSharedPointer>
#include <QHash>

class QByteArray;
class QNetworkRequest;
class QNetworkReply;

class NAMDataHandler {
public:
	virtual ~NAMDataHandler() {}
	virtual bool data(const QNetworkRequest &req, QByteArray &data, QByteArray &mime) const = 0;
};

class NetworkAccessManager : public QNetworkAccessManager {

	Q_OBJECT
public:

	NetworkAccessManager(QObject *parent = 0);

	inline void registerPathHandler(const QSharedPointer<NAMDataHandler> &handler)
	{ _pathHandlers.append(handler); }

	QString registerSessionHandler(const QSharedPointer<NAMDataHandler> &handler);
	void unregisterSessionHandler(const QString &id);

	void releaseHandlers() { _pathHandlers.clear(); _sessionHandlers.clear(); }

private slots:

	/**
	 * Called by QNetworkReply::finish().
	 *
	 * Emitts finish(reply)
	 */
	void callFinished();

protected:
	QNetworkReply* createRequest(Operation op, const QNetworkRequest & req, QIODevice * outgoingData);

private:

	int _handlerSeed;
	QList<QSharedPointer<NAMDataHandler> > _pathHandlers;
	QHash<QString,QSharedPointer<NAMDataHandler> > _sessionHandlers;
};

#endif
