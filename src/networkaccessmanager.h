/*
 * networkaccessmanager.h - Network Manager for WebView able to process
 * custom url schemas
 * Copyright (C) 2010 senu, Rion
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
#include <QStringList>
#include <QSharedPointer>
#include <QHash> //for qt-4.4
#include <QMutex>

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QIODevice>

class NetworkAccessManager;

class NAMSchemeHandler {
public:
	virtual ~NAMSchemeHandler() {}
	virtual QByteArray data(const QUrl &) const = 0;
};

/** Blocks internet connections and allows to use icon:// URLs in webkit-based ChatViews*/
class NetworkAccessManager : public QNetworkAccessManager {

	Q_OBJECT
public:
	/**
	 * Constructor.
	 *
	 * \param iconServer will be used to serve icon:// urls
	 */
	NetworkAccessManager(QObject *parent = 0);
	~NetworkAccessManager();

	static NetworkAccessManager* instance();
	QSharedPointer<NAMSchemeHandler> schemeHandler(const QString &);
	void setSchemeHandler(const QString &, NAMSchemeHandler *);

private slots:

	/**
	 * Called by QNetworkReply::finish().
	 *
	 * Emitts finish(reply)
	 */
	void callFinished();

protected:
	QNetworkReply* createRequest(Operation op, const QNetworkRequest & req, QIODevice * outgoingData);

	/*
	 * List of whitelisted URLs.
	 *
	 * Access to whitelisted URLs is not denied.
	 */
	QStringList whiteList;

	/**
	 * Mutal exclusion for whitList.
	 *
	 * WhiteList can be accessed by Webkit (createRequest())
	 * and Psi (addUrlToWhiteList()) simultaneously)
	 */
	QMutex whiteListMutex;

private:
	static NetworkAccessManager* instance_;
	QHash<QString, QSharedPointer<NAMSchemeHandler> > schemeHandlers_;
};

#endif
