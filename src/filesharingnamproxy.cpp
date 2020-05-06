/*
 * filesharingnamproxy.cpp - proxy network access reply for shared files
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

#include "filesharingnamproxy.h"
#include "filecache.h"
#include "filesharingitem.h"
#include "filesharingmanager.h"
#include "httputil.h"
#include "psiaccount.h"
#include "psicon.h"

#include <tuple>

FileSharingNAMReply::FileSharingNAMReply(PsiAccount *acc, const QString &sourceIdHex, const QNetworkRequest &req) :
    item(acc->psi()->fileSharingManager()->item(XMPP::Hash::from(QStringRef(&sourceIdHex)))), acc(acc)
{
    setRequest(req);
    setOperation(QNetworkAccessManager::GetOperation); // TODO rewrite when we have others
    QTimer::singleShot(0, this, &FileSharingNAMReply::init);
}

FileSharingNAMReply::~FileSharingNAMReply() { qDebug("FS-NAM destroy reply"); }

void FileSharingNAMReply::init()
{
    qDebug() << "New FS-NAM" << request().url().toString();
    const auto headers = request().rawHeaderList();
    for (auto const &h : headers)
        qDebug("  %s: %s", h.data(), request().rawHeader(h).data());

    auto rangesBa = request().rawHeader(QByteArray::fromRawData("Range", 5));

    if (rangesBa.size()) {
        Http::ParseResult result;
        qint64            rangeStart, rangeSize;
        std::tie(result, rangeStart, rangeSize)
            = Http::parseRangeHeader(rangesBa, item->isSizeKnown() ? item->fileSize() : -1);
        switch (result) {
        case Http::Parsed:
            isRanged       = rangeStart != 0 || rangeSize != 0;
            requestedStart = rangeStart;
            requestedSize  = rangeSize;
            break;
        case Http::Unparsed:
            finishWithError(QNetworkReply::ProtocolFailure, 400, "Bad request");
            return;
        case Http::NotImplementedRangeType:
            finishWithError(QNetworkReply::UnknownContentError, 416, "Expected bytes range");
            return;
        case Http::NotImplementedTailLoad:
            finishWithError(QNetworkReply::UnknownContentError, 501, "tail load is not supported");
            return;
        case Http::NotImplementedMultirange:
            finishWithError(QNetworkReply::UnknownContentError, 501, "multi range is not supported");
            return;
        case Http::OutOfRange:
            setRawHeader("Content-Range", QByteArray("bytes */") + QByteArray::number(item->fileSize()));
            finishWithError(QNetworkReply::UnknownContentError, 416, "Requested range not satisfiable");
            return;
        }
    }

    if (isRanged && item->isSizeKnown()) {
        if (requestedStart == 0 && requestedSize == item->fileSize())
            isRanged = false;
        else if (requestedStart + requestedSize > item->fileSize())
            requestedSize = item->fileSize() - requestedStart; // don't request more than declared in share
    }

    auto cache = item->cache();
    if (cache) {
        cachedFile = new QFile(item->fileName(), this);
        QFileInfo fi(*cachedFile);

        cachedFile->open(QIODevice::ReadOnly);
        qint64 size = fi.size();
        if (isRanged) {
            if (requestedSize)
                size = (requestedStart + requestedSize) > fi.size() ? fi.size() - requestedStart : requestedSize;
            else // remaining part
                size = fi.size() - requestedStart;
            cachedFile->seek(requestedStart);
        }
        // TODO If-Modified-Since
        setupHeaders(fi.size(), item->mimeType(), fi.lastModified(), isRanged, requestedStart, size);
        if (cachedFile->size())
            emit readyRead();
        return; // handled with success
    }

    downloader = item->download(isRanged, requestedStart, requestedSize);
    Q_ASSERT(downloader);
    downloader->setParent(this);

    connect(downloader, &FileShareDownloader::metaDataChanged, this, &FileSharingNAMReply::onMetadataChanged);
    connect(downloader, &FileShareDownloader::readyRead, this, &FileSharingNAMReply::readyRead);
    connect(downloader, &FileShareDownloader::failed, this, [this]() {
        if (!headersSent) {
            finishWithError(QNetworkReply::ServiceUnavailableError, 502, "Upstream failed");
        }
    });

    downloader->open();
}

void FileSharingNAMReply::finishWithError(QNetworkReply::NetworkError networkError, int httpCode,
                                          const char *httpReason)
{
    qDebug() << "FS-NAM failed" << networkError << httpCode << httpReason;
    QString reason = QString::fromLatin1(httpReason);
    if (httpCode) {
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, httpCode);
        if (httpReason) {
            setAttribute(QNetworkRequest::HttpReasonPhraseAttribute, reason);
        }
        emit metaDataChanged();
    }
    if (httpCode) {
        setError(networkError, reason);
        emit error(networkError);
    }
    if (downloader) {
        downloader->disconnect(this);
        downloader->deleteLater();
    }
    setFinished(true);
    emit finished();
}

void FileSharingNAMReply::setupHeaders(qint64 fileSize, QString contentType, QDateTime lastModified, bool isRanged,
                                       qint64 rangeStart, qint64 rangeSize)
{
    if (lastModified.isValid())
        setRawHeader("Last-Modified", lastModified.toString(Qt::RFC2822Date).toLatin1());
    if (contentType.count())
        setRawHeader("Content-Type", contentType.toLatin1());

    bool keepAlive = true;
    setRawHeader("Accept-Ranges", "bytes");
    if (isRanged) {
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 206); // partial content
        auto range = QString(QLatin1String("bytes %1-%2/%3"))
                         .arg(rangeStart)
                         .arg(rangeStart + rangeSize - 1)
                         .arg(fileSize == -1 ? QString('*') : QString::number(fileSize));
        setRawHeader("Content-Range", range.toLatin1());
        setHeader(QNetworkRequest::ContentLengthHeader, rangeSize);
    } else {
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        if (fileSize == -1)
            keepAlive = false;
        else
            setHeader(QNetworkRequest::ContentLengthHeader, fileSize);
    }
    if (keepAlive) {
        setRawHeader("Connection", "keep-alive");
    }

    qDebug("FS-NAM set open mode to readonly");
    setOpenMode(QIODevice::ReadOnly);

    qDebug() << "FS-NAM headers sent";
    for (auto const &h : rawHeaderList())
        qDebug("  %s: %s", h.data(), rawHeader(h).data());

    emit metaDataChanged();
}

void FileSharingNAMReply::onMetadataChanged()
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
        finishWithError(QNetworkReply::ServiceUnavailableError, 502, "Upstream failed");
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
    if (downloader->bytesAvailable())
        emit readyRead();
}

void FileSharingNAMReply::abort()
{
    if (downloader)
        downloader->abort();
    setError(QNetworkReply::OperationCanceledError, "aborted");
    setFinished(true);
    emit finished();
}

qint64 FileSharingNAMReply::bytesAvailable() const
{
    if (cachedFile)
        return (cachedFile->isOpen() ? cachedFile->size() - cachedFile->pos() : 0) + QNetworkReply::bytesAvailable();
    return (downloader ? downloader->bytesAvailable() : 0) + QNetworkReply::bytesAvailable();
}

qint64 FileSharingNAMReply::readData(char *buf, qint64 maxlen)
{
    // qDebug() << "reading" << maxlen << "bytes";
    if (cachedFile) {
        if (!cachedFile->isOpen())
            return 0;
        auto len = cachedFile->read(buf, maxlen);
        if (!bytesAvailable())
            QTimer::singleShot(0, this, [this]() {
                setFinished(true);
                emit finished();
            });
        return len;
    }

    if (!downloader->isOpen())
        return 0;

    return downloader->read(buf, maxlen);
}
