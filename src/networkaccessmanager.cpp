/*
 * networkaccessmanager.cpp - Network Manager for WebView able to process
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

#include "networkaccessmanager.h"

#include "bytearrayreply.h"
#include <QCoreApplication>

NetworkAccessManager::NetworkAccessManager(QObject *parent) :
    QNetworkAccessManager(parent)
{
}

QNetworkReply *NetworkAccessManager::createRequest(Operation op, const QNetworkRequest & req,
                                                   QIODevice * outgoingData = 0)
{
	if (req.url().host() != QLatin1String("psi")) {
		return QNetworkAccessManager::createRequest(op, req, outgoingData);
    }

	QNetworkReply *reply = nullptr;
	QByteArray data;
	QByteArray mime;

	for (auto &handler : _pathHandlers) {
		if (handler->data(req, data, mime)) {
			reply = new ByteArrayReply(req, data, mime, this);
			break;
		}
	}

	if (!reply) {
#ifdef HAVE_QT5
		QString ua = req.header(QNetworkRequest::UserAgentHeader).toString();
#else
		QString ua = QString(req.rawHeader("User-Agent"));
#endif
		auto handler = _sessionHandlers.value(ua);
		if (handler && handler->data(req, data, mime)) {
			reply = new ByteArrayReply(req, data, mime, this);
		}
	}

	if (!reply) {
		reply = new ByteArrayReply(req); //finishes with error
	}
    connect(reply, SIGNAL(finished()), SLOT(callFinished()));

    return reply;
}

void NetworkAccessManager::callFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());

    if (reply) {
        emit finished(reply);
    }
}

QString NetworkAccessManager::registerSessionHandler(const QSharedPointer<NAMDataHandler> &handler)
{
	QString s;
	s.sprintf("t%x", _handlerSeed);
	_handlerSeed += 0x10;

	_sessionHandlers.insert(s, handler);

	return s;
}

void NetworkAccessManager::unregisterSessionHandler(const QString &id)
{
	_sessionHandlers.remove(id);
}
