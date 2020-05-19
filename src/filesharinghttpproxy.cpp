/*
 * filesharinghttpproxy.cpp - http proxy for shared files downloads
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

#include "filesharinghttpproxy.h"
#include "filecache.h"
#include "filesharingitem.h"
#include "filesharingmanager.h"
#include "psiaccount.h"
#include "psicon.h"
#include "qhttpserverconnection.hpp"
#include "qhttpserverrequest.hpp"
#include "qhttpserverresponse.hpp"
#include "webserver.h"

#include <QTcpSocket>
#include <tuple>

#define HTTP_CHUNK (512 * 1024 * 1024)

FileSharingHttpProxy::FileSharingHttpProxy(PsiAccount *acc, const QString &sourceIdHex,
                                           qhttp::server::QHttpRequest *req, qhttp::server::QHttpResponse *res) :
    QObject(res),
    item(acc->psi()->fileSharingManager()->item(XMPP::Hash::from(QStringRef(&sourceIdHex)))), acc(acc), request(req),
    response(res)
{
    auto baseUrl = acc->psi()->webServer()->serverUrl().toString();
    qDebug("FSP %s %s%s range: %s", qPrintable(req->methodString()), qPrintable(baseUrl),
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
        proxyCache();
        return; // handled with success
    }

    downloader = item->download(isRanged, requestedStart, requestedSize);
    Q_ASSERT(downloader);
    downloader->setParent(this);

    connect(downloader, &FileShareDownloader::metaDataChanged, this, &FileSharingHttpProxy::onMetadataChanged);
    connect(downloader, &FileShareDownloader::failed, this, [this]() {
        if (!headersSent) {
            response->setStatusCode(qhttp::ESTATUS_BAD_GATEWAY); // something finnished with errors quite early
            response->end();
        }
    });

    downloader->open();
}

FileSharingHttpProxy::~FileSharingHttpProxy() { qDebug("FSP deleted"); }

// returns <parsed,list of start/size>
int FileSharingHttpProxy::parseHttpRangeRequest()
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

    auto l = ba.trimmed().split('-');
    if (l.size() != 2) {
        return qhttp::ESTATUS_BAD_REQUEST;
    }

    bool   ok;
    qint64 start;
    qint64 end;

    if (!l[0].size()) { // bytes from the end are requested. Jingle-ft doesn't support this
        return qhttp::ESTATUS_NOT_IMPLEMENTED;
    }

    start = l[0].toLongLong(&ok);
    if (!ok) {
        return qhttp::ESTATUS_BAD_REQUEST;
    }
    if (l[1].size()) {              // if we have end
        end = l[1].toLongLong(&ok); // then parse it
        if (!ok || start > end) {   // if something not parsed or range is invalid
            return qhttp::ESTATUS_BAD_REQUEST;
        }

        if (!item->isSizeKnown() || start < item->fileSize()) {
            isRanged       = true;
            requestedStart = start;
            requestedSize  = end - start + 1;
        }
    } else { // no end. all the remaining
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

void FileSharingHttpProxy::setupHeaders(qint64 fileSize, QString contentType, QDateTime lastModified, bool isRanged,
                                        qint64 rangeStart, qint64 rangeSize)
{
    if (lastModified.isValid())
        response->addHeader("Last-Modified", lastModified.toString(Qt::RFC2822Date).toLatin1());
    if (contentType.count())
        response->addHeader("Content-Type", contentType.toLatin1());

    bool keepAlive = true;
    response->addHeader("Accept-Ranges", "bytes");
    if (isRanged) {
        response->setStatusCode(qhttp::ESTATUS_PARTIAL_CONTENT);
        auto range = QString(QLatin1String("bytes %1-%2/%3"))
                         .arg(rangeStart)
                         .arg(rangeStart + rangeSize - 1)
                         .arg(fileSize == -1 ? QString('*') : QString::number(fileSize));
        response->addHeader("Content-Range", range.toLatin1());
        response->addHeader("Content-Length", QByteArray::number(rangeSize));
    } else {
        response->setStatusCode(qhttp::ESTATUS_OK);
        if (fileSize == -1)
            keepAlive = false;
        else
            response->addHeader("Content-Length", QByteArray::number(fileSize));
    }
    if (keepAlive) {
        response->addHeader("Connection", "keep-alive");
    }
}

void FileSharingHttpProxy::proxyCache()
{
    auto status = qhttp::TStatusCode(parseHttpRangeRequest());
    if (status != qhttp::ESTATUS_OK) {
        response->setStatusCode(status);
        qWarning("http range parse failed: %d", status);
        request->end();
        return; // handled with error
    }
    QFile *   file = new QFile(item->fileName(), response);
    QFileInfo fi(*file);
    if (!file->open(QIODevice::ReadOnly)) {
        response->setStatusCode(qhttp::ESTATUS_NOT_FOUND);
        qWarning("FSP failed to open cached file: %s", qPrintable(file->errorString()));
        request->end();
        return; // handled with error
    }
    qint64 size = fi.size();
    if (isRanged) {
        if (requestedSize)
            size = (requestedStart + requestedSize) > fi.size() ? fi.size() - requestedStart : requestedSize;
        else // remaining part
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

void FileSharingHttpProxy::onMetadataChanged()
{
    qint64 start;
    qint64 size;
    std::tie(start, size) = downloader->range();
    auto const file       = downloader->jingleFile();

    if (downloader->isRanged())
        qDebug("FSP metaDataChanged: rangeStart=%lld rangeSize=%lld", start, size);
    else if (downloader->jingleFile().hasSize())
        qDebug("FSP metaDataChanged: size=%lld", downloader->jingleFile().size());
    else
        qDebug("FSP metaDataChanged: unknown size or range");

    // check range satisfaction
    if (isRanged && downloader->isRanged() && !file.hasSize() && !size) { // size unknown for ranged response.
        qWarning("Unknown size for ranged response");
        downloader->disconnect(this);
        downloader->deleteLater();
        response->setStatusCode(qhttp::ESTATUS_BAD_GATEWAY);
        response->end();
        return;
    }

    if (isRanged && !downloader->isRanged()) {
        qWarning("FSP: remote doesn't support ranged. transfer everything");
        isRanged = false;
        start    = 0;
        size     = file.size();
    }

    if (isRanged && !size) {
        size = file.size() - start;
    }
    bytesLeft = isRanged ? size : (file.hasSize() ? file.size() : -1);

    setupHeaders(file.hasSize() ? qint64(file.size()) : -1, file.mediaType(), file.date(), downloader->isRanged(),
                 start, size);
    headersSent = true;

    connect(downloader, &FileShareDownloader::readyRead, this, &FileSharingHttpProxy::transfer);
    connect(downloader, &FileShareDownloader::disconnected, this, &FileSharingHttpProxy::transfer);
    connect(response, &qhttp::server::QHttpResponse::allBytesWritten, this, &FileSharingHttpProxy::transfer);
}

void FileSharingHttpProxy::transfer()
{
    qint64 bytesAvail  = downloader ? downloader->bytesAvailable() : 0;
    bool   isConnected = downloader ? downloader->isConnected() : false;
    if (response->connection()->tcpSocket()->bytesToWrite() >= HTTP_CHUNK) {
        qDebug("FSP available=%lld wait till previous chunk is wrtten", bytesAvail);
        return;
    }

    if (isConnected) {
        if (bytesAvail) {
            qint64 toTransfer = bytesAvail > HTTP_CHUNK ? HTTP_CHUNK : bytesAvail;
            auto   data       = downloader->read(toTransfer);
            if (bytesLeft != -1) {
                if (data.size() < bytesLeft)
                    response->write(data);
                else {
                    // if (data.size() > bytesLeft)
                    //    data.resize(bytesLeft);
                    response->end(data);
                }
                bytesLeft -= data.size();
            } else {
                response->write(data);
            }
            qDebug("FSP transferred %d bytes of %lld bytes", data.size(), toTransfer);
        } else {
            qDebug("FSP we have to wait for readyRead or disconnected");
        }
        return;
    }

    // so we are not connected
    if (bytesAvail) {
        response->end(downloader->read(bytesAvail));
        qDebug("FSP transferred final %lld bytes", bytesAvail);
    } else {
        response->end();
        qDebug("FSP ended with no additional data");
    }
}
