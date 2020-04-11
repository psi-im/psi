/*
 * networkaccessmanager.cpp - Network Manager for WebView able to process
 * custom url schemas
 * Copyright (C) 2010-2017  senu, Sergey Ilinykh
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "networkaccessmanager.h"

#include "bytearrayreply.h"

#include <QCoreApplication>
#include <QTimer>

NetworkAccessManager::NetworkAccessManager(QObject *parent) : QNetworkAccessManager(parent), _handlerSeed(0) {}

QNetworkReply *NetworkAccessManager::createRequest(Operation op, const QNetworkRequest &req,
                                                   QIODevice *outgoingData = nullptr)
{
    if (req.url().host() != QLatin1String("psi")) {
        return QNetworkAccessManager::createRequest(op, req, outgoingData);
    }

    QNetworkReply *reply = nullptr;
    QByteArray     data;
    QByteArray     mime;

    QString path = req.url().path();
    for (auto &h : _streamHandlers) {
        if (path.startsWith(h.first)) {
            reply = h.second(req);
            if (reply) {
                reply->setParent(this);
                return reply;
            } else {
                return new NAMNotFoundReply(this);
            }
        }
    }

    for (auto &handler : _pathHandlers) {
        if (handler(req, data, mime)) {
            reply = new ByteArrayReply(req, data, mime, this);
            break;
        }
    }

    if (!reply) {
        QString ua      = req.header(QNetworkRequest::UserAgentHeader).toString();
        auto    handler = _sessionHandlers.value(ua);
        if (handler && handler(req, data, mime)) {
            reply = new ByteArrayReply(req, data, mime, this);
        }
    }

    if (!reply) {
        reply = new ByteArrayReply(req); // finishes with error
    }
    connect(reply, SIGNAL(finished()), SLOT(callFinished()));

    return reply;
}

void NetworkAccessManager::callFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());

    if (reply) {
        emit finished(reply);
    }
}

QString NetworkAccessManager::registerSessionHandler(const Handler &&handler)
{
    QString s = QString::asprintf("t%x", _handlerSeed);
    _handlerSeed += 0x10;

    _sessionHandlers.insert(s, std::move(handler));
    return s;
}

void NetworkAccessManager::unregisterSessionHandler(const QString &id) { _sessionHandlers.remove(id); }

void NetworkAccessManager::route(const QString &path, const NetworkAccessManager::StreamHandler &handler)
{
    auto it = _streamHandlers.begin();
    while (it != _streamHandlers.end()) {
        if (path.startsWith(it->first))
            break;
    }
    _streamHandlers.insert(it, std::make_pair(path, handler));
}

NAMNotFoundReply::NAMNotFoundReply(QObject *parent) : QNetworkReply(parent)
{
    setError(QNetworkReply::ContentNotFoundError, "Not found");
    QTimer::singleShot(0, this, [this]() { emit error(QNetworkReply::ContentNotFoundError); });
    QTimer::singleShot(0, this, &NAMNotFoundReply::finished);
}

qint64 NAMNotFoundReply::readData(char *, qint64) { return 0; }

void NAMNotFoundReply::abort() {}
