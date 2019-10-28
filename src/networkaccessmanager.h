/*
 * networkaccessmanager.h - Network Manager for WebView able to process
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

#ifndef NETWORKACCESSMANAGER_H
#define NETWORKACCESSMANAGER_H

#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSharedPointer>
#include <functional>

class QByteArray;
class QNetworkRequest;
class QNetworkReply;

class NAMNotFoundReply : public QNetworkReply {
    Q_OBJECT
public:
    NAMNotFoundReply(QObject *parent);
    qint64 readData(char *data, qint64 maxlen) override;
    void   abort() override;
};

class NetworkAccessManager : public QNetworkAccessManager {

    Q_OBJECT
public:
    using Handler       = std::function<bool(const QNetworkRequest &req, QByteArray &data, QByteArray &mime)>;
    using StreamHandler = std::function<QNetworkReply *(const QNetworkRequest &req)>;

    NetworkAccessManager(QObject *parent = nullptr);

    inline void registerPathHandler(const Handler &&handler) { _pathHandlers.append(std::move(handler)); }

    QString registerSessionHandler(const Handler &&handler);
    void    unregisterSessionHandler(const QString &id);

    void route(const QString &path, const StreamHandler &handler);

    void releaseHandlers()
    {
        _pathHandlers.clear();
        _sessionHandlers.clear();
    }

private slots:

    /**
     * Called by QNetworkReply::finish().
     *
     * Emitts finish(reply)
     */
    void callFinished();

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &req, QIODevice *outgoingData);

private:
    int                                      _handlerSeed;
    QList<std::pair<QString, StreamHandler>> _streamHandlers;
    QList<Handler>                           _pathHandlers;
    QHash<QString, Handler>                  _sessionHandlers;
};

#endif // NETWORKACCESSMANAGER_H
