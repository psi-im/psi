/*
 * filesharingroxy.cpp - http proxy for shared files downloads
 * Copyright (C) 2019  Sergey Ilinykh
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

#include "filesharingproxy.h"
#include "filecache.h"
#include "filesharingitem.h"
#include "filesharingmanager.h"
#include "psiaccount.h"
#include "psicon.h"
#include "qhttpserverconnection.hpp"
#include "qhttpserverrequest.hpp"
#include "qhttpserverresponse.hpp"

#include <QTcpSocket>
#include <tuple>

#define HTTP_CHUNK (512 * 1024 * 1024)

FileSharingProxy::FileSharingProxy(PsiAccount *acc, const QString &sourceIdHex, qhttp::server::QHttpRequest *req,
                                   qhttp::server::QHttpResponse *res) :
    QObject(res),
    item(acc->psi()->fileSharingManager()->item(XMPP::Hash::from(QStringRef(&sourceIdHex)))), acc(acc), request(req),
    response(res)
{
    qDebug("proxy %s %s %s range: %s", qPrintable(sourceIdHex), qPrintable(req->methodString()),
           qPrintable(req->url().toString()), qPrintable(req->headers().value("range")));

    if (!item) {
        res->setStatusCode(qhttp::ESTATUS_NOT_FOUND);
        req->end();
        return;
    }

    auto status = qhttp::TStatusCode(parseHttpRangeRequest());
    if (status != qhttp::ESTATUS_OK) {
        res->setStatusCode(status);
        qWarning("http range parse failed: %d", status);
        req->end();
        return; // handled with error
    }

    if (isRanged && item->isSizeKnown()) {
        if (requestedStart == 0 && requestedSize == item->fileSize())
            isRanged = false;
        else if (requestedStart + requestedSize > item->fileSize())
            requestedSize = item->fileSize() - requestedStart; // don't request more than declared in share
    }

    auto cache = item->cache();
    if (cache) {
        proxyCache(cache);
        return; // handled with success
    }

    downloader = item->download(isRanged, requestedStart, requestedSize);
    Q_ASSERT(downloader);

    connect(downloader, &FileShareDownloader::metaDataChanged, this, &FileSharingProxy::onMetadataChanged);
    connect(downloader, &FileShareDownloader::finished, this, [this]() {
        if (!response->property("headers").toBool()) {
            response->setStatusCode(qhttp::ESTATUS_BAD_GATEWAY); // something finnished with errors quite early
            response->end();
        }
    });

    downloader->open();
}

// returns <parsed,list of start/size>
int FileSharingProxy::parseHttpRangeRequest()
{
    QByteArray rangesBa = request->headers().value("range");
    if (!rangesBa.size())
        return qhttp::ESTATUS_OK;

    if ((item->isSizeKnown() && !item->fileSize()) || !rangesBa.startsWith("bytes=")) {
        return qhttp::ESTATUS_REQUESTED_RANGE_NOT_SATISFIABLE;
    }

    if (rangesBa.indexOf(',') != -1) {
        return qhttp::ESTATUS_NOT_IMPLEMENTED;
    }

    auto ba = QByteArray::fromRawData(rangesBa.data() + sizeof("bytes"), rangesBa.size() - int(sizeof("bytes")));

    auto trab = ba.trimmed();
    if (!trab.size()) {
        return qhttp::ESTATUS_BAD_REQUEST;
    }
    bool   ok;
    qint64 start;
    qint64 end;
    if (trab[0] == '-') {
        return qhttp::ESTATUS_NOT_IMPLEMENTED;
    }

    auto l = trab.split('-');
    if (l.size() != 2) {
        return qhttp::ESTATUS_BAD_REQUEST;
    }

    start = l[0].toLongLong(&ok);
    if (l[1].size()) {
        if (ok && l[1].size())
            end = l[1].toLongLong(&ok);
        if (!ok || start > end) {
            return qhttp::ESTATUS_BAD_REQUEST;
        }

        if (!item->isSizeKnown() || start < item->fileSize()) {
            isRanged       = true;
            requestedStart = start;
            requestedSize  = end - start + 1;
        }
    } else {
        if (!item->isSizeKnown() || start < item->fileSize()) {
            isRanged       = true;
            requestedStart = start;
            requestedSize  = 0;
        }
    }

    if (item->isSizeKnown() && !isRanged) { // isRanged is not set. So it doesn't fit
        response->addHeader("Content-Range", QByteArray("bytes */") + QByteArray::number(item->fileSize()));
        return qhttp::ESTATUS_REQUESTED_RANGE_NOT_SATISFIABLE;
    }

    return qhttp::ESTATUS_OK;
}

void FileSharingProxy::setupHeaders(qint64 fileSize, QString contentType, QDateTime lastModified, bool isRanged,
                                    qint64 rangeStart, qint64 rangeSize)
{
    if (lastModified.isValid())
        response->addHeader("Last-Modified", lastModified.toString(Qt::RFC2822Date).toLatin1());
    if (contentType.count())
        response->addHeader("Content-Type", contentType.toLatin1());

    response->addHeader("Accept-Ranges", "bytes");
    response->addHeader("connection", "keep-alive");
    if (isRanged) {
        auto range = QString(QLatin1String("bytes %1-%2/%3"))
                         .arg(rangeStart)
                         .arg(rangeStart + rangeSize - 1)
                         .arg(fileSize == -1 ? QString('*') : QString::number(fileSize));
        response->addHeader("Content-Range", range.toLatin1());
        response->setStatusCode(qhttp::ESTATUS_PARTIAL_CONTENT);
    } else {
        if (fileSize != -1)
            response->addHeader("Content-Length", QByteArray::number(fileSize));
        response->setStatusCode(qhttp::ESTATUS_OK);
    }
}

void FileSharingProxy::proxyCache(FileCacheItem *cache)
{
    QFile *   file = new QFile(acc->psi()->fileSharingManager()->cacheDir() + "/" + cache->fileName(), response);
    QFileInfo fi(*file);
    auto      status = qhttp::TStatusCode(parseHttpRangeRequest());
    if (status != qhttp::ESTATUS_OK) {
        response->setStatusCode(status);
        qWarning("http range parse failed: %d", status);
        request->end();
        return; // handled with error
    }
    file->open(QIODevice::ReadOnly);
    qint64 size = fi.size();
    if (isRanged) {
        if (requestedSize)
            size = (requestedStart + requestedSize) > fi.size() ? fi.size() - requestedStart : requestedSize;
        else
            size = fi.size() - requestedStart;
        file->seek(requestedStart);
    }
    // TODO If-Modified-Since
    setupHeaders(fi.size(), item->mimeType(), fi.lastModified(), isRanged, requestedStart, size);
    connect(response, &qhttp::server::QHttpResponse::allBytesWritten, file, [this, file, size]() {
        qint64 toWrite = requestedStart + size - file->pos();
        if (!toWrite) {
            return;
        }
        if (toWrite > HTTP_CHUNK)
            response->write(file->read(HTTP_CHUNK));
        else
            response->end(file->read(toWrite));
    });

    if (size < HTTP_CHUNK) {
        response->end(file->read(size));
    }
}

void FileSharingProxy::onMetadataChanged()
{
    qint64 start;
    qint64 size;
    std::tie(start, size) = downloader->range();
    auto const file       = downloader->jingleFile();

    if (downloader->isRanged())
        qDebug("FSM metaDataChanged: rangeStart=%lld rangeSize=%lld", start, size);
    else if (downloader->jingleFile().hasSize())
        qDebug("FSM metaDataChanged: size=%lld", downloader->jingleFile().size());
    else
        qDebug("FSM metaDataChanged: unknown size or range");

    setupHeaders(file.hasSize() ? qint64(file.size()) : -1, file.mediaType(), file.date(), downloader->isRanged(),
                 start, size);
    response->setProperty("headers", true);

    // TODO below connections mess has to be fixed or at least documeted

    connect(downloader, &FileShareDownloader::readyRead, this, [this]() {
        if (response->connection()->tcpSocket()->bytesToWrite() < HTTP_CHUNK) {
            qDebug("FSM readyRead available=%lld transfer them", downloader->bytesAvailable());
            response->write(downloader->read(downloader->bytesAvailable() > HTTP_CHUNK ? HTTP_CHUNK
                                                                                       : downloader->bytesAvailable()));
        } else {
            qDebug("FSM readyRead available=%lld wait till previous chunk is wrtten", downloader->bytesAvailable());
        }
    });

    connect(response, &qhttp::server::QHttpResponse::allBytesWritten, downloader, [this]() {
        auto bytesAvail = downloader->bytesAvailable();
        if (!bytesAvail) {
            if (upstreamDisconnected) {
                qDebug("FSM bytesWritten all data transferred. closing");
                response->end();
            } else
                qDebug("FSM bytesWritten downloader doesn't have more data yet. waiting..");
            return; // let's wait readyRead
        }
        auto toTransfer = bytesAvail > HTTP_CHUNK && !upstreamDisconnected ? HTTP_CHUNK : bytesAvail;
        qDebug("FSM bytesWritten transferring %lld bytes", toTransfer);
        response->write(downloader->read(toTransfer));
    });

    // for keep alive connections we should detach from the socket as soon as response is destroyed (or earlier?)
    connect(response, &qhttp::server::QHttpResponse::done, downloader,
            [this](bool) { response->connection()->tcpSocket()->disconnect(downloader); });

    connect(downloader, &FileShareDownloader::disconnected, this, [this]() {
        upstreamDisconnected = true;
        qDebug("FSM disconnected.");
    });

    connect(downloader, &FileShareDownloader::finished, this, [this]() {
        response->end();
        downloader->deleteLater();
    });
}
