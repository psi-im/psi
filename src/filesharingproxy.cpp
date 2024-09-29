/*
 * filesharingproxy.cpp - proxy network access reply for shared files
 * Copyright (C) 2024  Sergey Ilinykh
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

#define QHTTP_MEMORY_LOG 1

#include "filesharingdownloader.h"
#include "filesharingitem.h"
#include "filesharingmanager.h"
#include "httputil.h"
#include "psiaccount.h"
#include "psicon.h"
#ifdef HAVE_WEBSERVER
#include "qhttpfwd.hpp"
#include "qhttpserverresponse.hpp"
#include "webserver.h"
#endif

#include <QFileInfo>
#include <QNetworkReply>
#include <QPointer>

#include <cstring>

#define HTTP_CHUNK (512 * 1024)

template <typename Impl> class ControlBase : public QObject {

public:
    enum class StatusCode {
        Ok,
        PartialContent,     // 206
        BadRequest,         // 400
        NotFound,           // 404
        RangeNotSatisfied,  // 416
        NotImplemented,     // 501
        BadGateway,         // 502
        ServiceUnavailable, // 503
    };

    PsiAccount                           *acc;
    FileSharingItem                      *item = nullptr;
    QPointer<FileShareDownloader>         downloader;
    std::optional<FileSharingItem::Range> requestedRange;
    std::optional<quint64>                bytesLeft; // if not set - unknown
    qint64                                totalTranferred = 0;
    bool                                  headersSent     = false;
    bool                                  dataHungry      = true;
    bool                                  finished        = false;

    ControlBase(PsiAccount *acc, const QString &sourceIdHex, QObject *parent) :
        QObject(parent), acc(acc), item(acc->psi()->fileSharingManager()->item(XMPP::Hash::from(sourceIdHex)))
    {
    }

    ~ControlBase() { qDebug("FSP destroyed. Total transferred bytes: %lld", totalTranferred); }

    void process()
    {
        auto self = static_cast<Impl *>(this);
        if (!item) {
            _finishWithMetadataError(StatusCode::NotFound);
            return;
        }

        QByteArray rangeHeaderValue = self->requestHeader("range");
        if (rangeHeaderValue.size()) {
            auto status = parseHttpRangeRequest(rangeHeaderValue);
            if (status != StatusCode::Ok) {
                qWarning("http range parse failed: %d", int(status));
                _finishWithMetadataError(status);
                return; // handled with error
            }
        }

        auto cache = item->cache();
        if (cache) {
            proxyCache();
            return; // handled with success
        }

        downloader = item->download(requestedRange);
        Q_ASSERT(downloader);
        downloader->setParent(this);

        connect(downloader, &FileShareDownloader::metaDataChanged, this, &ControlBase::onMetadataChanged);
        connect(downloader, &FileShareDownloader::failed, this, [this]() {
            if (!headersSent) {
                _finishWithMetadataError(StatusCode::BadGateway);
            }
        });

        downloader->open();
    }

    // returns <parsed,list of start/size>
    StatusCode parseHttpRangeRequest(const QByteArray &rangeValue)
    {
        auto const [parseResult, start, size] = Http::parseRangeHeader(rangeValue, item->fileSize());

        switch (parseResult) {
        case Http::Parsed:
            requestedRange = FileSharingItem::Range { start, size };
            return StatusCode::Ok;
        case Http::Unparsed:
            return StatusCode::BadRequest;
        case Http::NotImplementedRangeType:
        case Http::NotImplementedTailLoad:
        case Http::NotImplementedMultirange:
            return StatusCode::NotImplemented;
        case Http::OutOfRange:
            static_cast<Impl *>(this)->setResponseHeader(
                "Content-Range", QByteArray("bytes */") + QByteArray::number(quint64(*item->fileSize())));
            return StatusCode::RangeNotSatisfied;
        }
        return StatusCode::NotImplemented;
    }

    void proxyCache()
    {
        // auto status = qhttp::TStatusCode(parseHttpRangeRequest());
        //  if (status != qhttp::ESTATUS_OK) {
        //      response->setStatusCode(status);
        //      qWarning("http range parse failed: %d", status);
        //      emit request->end();
        //      return; // handled with error
        //  }
        auto      self = static_cast<Impl *>(this);
        QFile    *file = new QFile(item->fileName(), this);
        QFileInfo fi(*file);
        if (!file->open(QIODevice::ReadOnly)) {
            qWarning("FSP failed to open cached file: %s", qPrintable(file->errorString()));
            _finishWithMetadataError(StatusCode::NotFound);
            return; // handled with error
        }
        auto size        = quint64(fi.size());
        auto actualRange = requestedRange;
        if (requestedRange) {
            if (requestedRange->start >= size) {
                self->setResponseHeader("Content-Range", QByteArray("bytes */") + QByteArray::number(size));
                self->setResponseHeader("Content-Length", "0");
                _finishWithMetadataError(StatusCode::RangeNotSatisfied);
                return;
            }
            if (requestedRange->size)
                actualRange->size = qint64(requestedRange->start + requestedRange->size) > fi.size()
                    ? quint64(fi.size()) - requestedRange->start
                    : requestedRange->size;
            else // remaining part
                actualRange->size = quint64(fi.size()) - requestedRange->start;
            file->seek(requestedRange->start);
        }
        // TODO If-Modified-Since
        setupHeaders(fi.size(), item->mimeType(), fi.lastModified(), actualRange);
        self->connectReadyWrite(file, [this, file, size]() {
            qint64 toWrite = (requestedRange ? requestedRange->start : 0) + size - file->pos();
            if (!toWrite) {
                return;
            }
            if (toWrite > HTTP_CHUNK)
                _write(file->read(HTTP_CHUNK));
            else {
                _write(file->read(toWrite));
                _finish();
            }
        });

        if (size < HTTP_CHUNK) {
            _write(file->read(size));
            _finish();
        }
    }

    void onMetadataChanged()
    {
        auto const &responseRange = downloader->responseRange();
        auto const  file          = downloader->jingleFile();

        if (responseRange)
            qDebug("FSP metaDataChanged: rangeStart=%lld rangeSize=%lld", responseRange->start, responseRange->size);
        else if (file.size())
            qDebug("FSP metaDataChanged: size=%lu", *file.size());
        else
            qDebug("FSP metaDataChanged: unknown size or range");

        // check range satisfaction
        if (requestedRange && responseRange && !file.size().has_value()
            && !requestedRange->size) { // size unknown for ranged response.
            qWarning("Unknown size for ranged response");
            _finishWithMetadataError(StatusCode::NotImplemented);
            return;
        }

        auto reqsponseRange = requestedRange;
        if (requestedRange && !responseRange) {
            qWarning("FSP: remote doesn't support ranged. transfer everything");
            reqsponseRange = {};
        }

        if (reqsponseRange && !reqsponseRange->size) {
            reqsponseRange->size = *file.size() - reqsponseRange->start;
        }
        if (reqsponseRange || file.size()) {
            bytesLeft = reqsponseRange ? reqsponseRange->size : file.size();
        }

        setupHeaders(file.size(), file.mediaType(), file.date(), reqsponseRange);
        headersSent = true;

        connect(downloader, &FileShareDownloader::readyRead, this, [this]() {
            if (dataHungry) {
                transfer();
            }
        });
        connect(downloader, &FileShareDownloader::disconnected, this, &ControlBase::transfer);
        static_cast<Impl *>(this)->connectReadyWrite(this, [this]() {
            dataHungry = true;
            transfer();
        });
    }

    void setupHeaders(std::optional<quint64> fileSize, QString contentType, QDateTime lastModified,
                      const std::optional<FileSharingItem::Range> &range)
    {
        auto self = static_cast<Impl *>(this);
        if (contentType == QLatin1String("audio/x-vorbis+ogg")) {
            contentType = QLatin1String("audio/ogg");
        }
        if (lastModified.isValid())
            self->setResponseHeader("Last-Modified", lastModified.toString(Qt::RFC2822Date).toLatin1());
        if (contentType.size())
            self->setResponseHeader("Content-Type", contentType.toLatin1());

        bool keepAlive = true;
        self->setResponseHeader("Accept-Ranges", "bytes");
        if (range) {
            self->setResponseStatusCode(StatusCode::PartialContent);
            auto rangeStr = QString(QLatin1String("bytes %1-%2/%3"))
                                .arg(range->start)
                                .arg(range->start + range->size - 1)
                                .arg(fileSize ? QString::number(*fileSize) : QString('*'));
            self->setResponseHeader("Content-Range", rangeStr.toLatin1());
            self->setResponseHeader("Content-Length", QByteArray::number(range->size));
        } else {
            self->setResponseStatusCode(StatusCode::Ok);
            if (!fileSize)
                keepAlive = false;
            else
                self->setResponseHeader("Content-Length", QByteArray::number(*fileSize));
        }
        if (keepAlive) {
            self->setResponseHeader("Connection", "keep-alive");
        }
    }

    void transfer()
    {
        if (finished) {
            return;
        }
        qint64 bytesAvail  = downloader ? downloader->bytesAvailable() : 0;
        bool   isConnected = downloader ? downloader->isConnected() : false;

        if (isConnected) {
            if (bytesAvail) {
                qint64 toTransfer = bytesAvail > HTTP_CHUNK ? HTTP_CHUNK : bytesAvail;
                auto   data       = downloader->read(toTransfer);
                // qDebug("read %lld bytes from available %lld", qint64(data.size()), bytesAvail);
                if (bytesLeft.has_value()) {
                    if (data.size() < bytesLeft)
                        _write(data);
                    else {
                        // if (data.size() > bytesLeft)
                        //    data.resize(bytesLeft);
                        _write(data);
                        _finish();
                    }
                    *bytesLeft -= data.size();
                } else {
                    _write(data);
                }
                // qDebug("FSP transferred %lld bytes of %lld bytes", qint64(data.size()), toTransfer);
            } else {
                // qDebug("FSP we have to wait for readyRead or disconnected");
            }
            return;
        }

        // so we are not connected
        if (bytesAvail) {
            _write(downloader->read(bytesAvail));
            _finish();
            qDebug("FSP transferred final %lld bytes", bytesAvail);
        } else {
            _finish();
            qDebug("FSP ended with no additional data");
        }
    }

    void ensureUpstreamStopped()
    {
        if (downloader) {
            downloader->disconnect(this);
            downloader->deleteLater();
        }
    }

    void _write(const QByteArray &data)
    {
        // qDebug("FSP: write %lld bytes", qint64(data.size()));
        static_cast<Impl *>(this)->write(data);
        totalTranferred += data.size();
        dataHungry = false;
    }

    void _finish()
    {
        if (finished) {
            return;
        }
        finished = true;
        static_cast<Impl *>(this)->finish();
    }

    void _finishWithMetadataError(StatusCode code, QByteArray httpReason = {})
    {
        if (finished) {
            return;
        }
        qDebug() << "FS finishWithMetadataError " << int(code) << httpReason;
        auto self = static_cast<Impl *>(this);
        ensureUpstreamStopped();
        if (downloader) { // TODO we need download manager instead
            downloader->disconnect(this);
            downloader->deleteLater();
        }
        finished = true;
        self->finishWithMetadataError(code, httpReason);
    }
};

namespace {
class FileSharingNAMReply : public QNetworkReply {
    Q_OBJECT

    QByteArray buffer;
    bool       metadataSignalled = false;
    bool       finished_         = false;

public:
    FileSharingNAMReply(const QNetworkRequest &request)
    {
        setRequest(request);
        setOpenMode(QIODevice::ReadOnly);
    }

    qint64 bytesAvailable() const { return buffer.size() + QNetworkReply::bytesAvailable(); }

    inline void setRawHeader(const char *headerName, const QByteArray &value)
    {
        qDebug("FSP reply set header %s=%s", headerName, value.data());
        QNetworkReply::setRawHeader(headerName, value);
    }

    inline void setAttribute(QNetworkRequest::Attribute attr, QVariant value)
    {
        QNetworkReply::setAttribute(attr, std::move(value));
    }

    void appendData(const QByteArray &data)
    {
        // qDebug("FSP append %lld bytes for reading", qint64(data.size()));
        if (!metadataSignalled) {
            qDebug("FSP reply signalling metadataChanged");
            QTimer::singleShot(0, this, SIGNAL(metaDataChanged()));
            metadataSignalled = true;
        }
        buffer += data;
        QTimer::singleShot(0, this, SIGNAL(readyRead()));
    }

    void finishWithError(QNetworkReply::NetworkError networkError, int httpCode, const QByteArray &reason)
    {
        if (finished_) {
            return;
        }
        qDebug("FSP reply finishWithError");
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, httpCode);
        if (!reason.isEmpty()) {
            setAttribute(QNetworkRequest::HttpReasonPhraseAttribute, reason);
        }
        if (!metadataSignalled) {
            emit metaDataChanged();
            metadataSignalled = true;
        }
        setError(networkError, reason);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        emit errorOccurred(networkError);
#else
        emit error(networkError);
#endif
        finish();
    }

    void finish()
    {
        if (finished_) {
            return;
        }
        finished_ = true;
        qDebug("FSP reply finished");
        setFinished(true);
        QTimer::singleShot(0, this, [this]() {
            setFinished(true);
            emit finished();
        });
        // emit finished();
        //  we should not delete it here, because it may still have data buffered
        //  but we need to stop downloader if it's still running
    }

protected:
    qint64 readData(char *buf, qint64 maxlen)
    {
        auto sz = std::min(maxlen, qint64(buffer.size()));
        // qDebug("FSP read %lld bytes", sz);
        if (sz) {
            std::memcpy(buf, buffer.data(), sz);
            buffer.remove(0, sz);
        }
        return sz;
    }

    // QNetworkReply interface
public slots:
    void abort()
    {
        qDebug("FSP reply aborted");
        setError(QNetworkReply::OperationCanceledError, "aborted");
        finish();
    }
};

class NAMProxy : public ControlBase<NAMProxy> {
    Q_OBJECT

    QNetworkRequest request;

public:
    FileSharingNAMReply *reply;

    NAMProxy(PsiAccount *acc, const QString &sourceIdHex, const QNetworkRequest &req, FileSharingNAMReply *reply) :
        ControlBase<NAMProxy>(acc, sourceIdHex, reply), request(req), reply(reply)
    {
        qDebug("FSP GET %s range: %s", qUtf8Printable(req.url().toString()), qPrintable(request.rawHeader("range")));
        connect(reply, &QNetworkReply::finished, this, &ControlBase<NAMProxy>::ensureUpstreamStopped);
    }

    static std::pair<QNetworkReply::NetworkError, int> mapStatusCode(ControlBase::StatusCode code)
    {
        switch (code) {
        case StatusCode::Ok:
            return { QNetworkReply::NoError, 200 };
        case StatusCode::PartialContent:
            return { QNetworkReply::NoError, 206 };
        case StatusCode::BadRequest:
            return { QNetworkReply::ProtocolFailure, 400 };
        case StatusCode::NotFound:
            return { QNetworkReply::ContentNotFoundError, 404 };
        case StatusCode::RangeNotSatisfied:
            return { QNetworkReply::UnknownContentError, 416 };
        case StatusCode::NotImplemented:
            return { QNetworkReply::OperationNotImplementedError, 501 };
        case StatusCode::BadGateway:
            return { QNetworkReply::ServiceUnavailableError, 502 };
        case StatusCode::ServiceUnavailable:
            return { QNetworkReply::ServiceUnavailableError, 503 };
        }
        return { QNetworkReply::OperationNotImplementedError, 501 };
    }

    void finishWithMetadataError(ControlBase::StatusCode code, QByteArray httpReason = {})
    {
        auto const [replyError, httpCode] = mapStatusCode(code);
        reply->finishWithError(replyError, httpCode, httpReason);
    }
    void              finish() { reply->finish(); }
    inline QByteArray requestHeader(const char *headerName) { return request.rawHeader(headerName); }
    inline void setResponseHeader(const char *headerName, QByteArray value) { reply->setRawHeader(headerName, value); }
    inline void setResponseStatusCode(StatusCode code)
    {
        reply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, std::get<1>(mapStatusCode(code)));
    }
    void connectReadyWrite(QObject *ctx, std::function<void()> &&callback)
    {
        connect(reply, &QNetworkReply::bytesWritten, ctx, [this, cb = std::move(callback)](qint64 bytes) {
            Q_UNUSED(bytes);
            if (reply->bytesToWrite() < HTTP_CHUNK) {
                cb();
            }
        });
    }
    void write(const QByteArray &data) { return reply->appendData(data); }
};
#ifdef HAVE_WEBSERVER
class HTTPProxy : public ControlBase<HTTPProxy> {
    Q_OBJECT

    qhttp::server::QHttpRequest  *request;
    qhttp::server::QHttpResponse *response;

public:
    HTTPProxy(PsiAccount *acc, const QString &sourceIdHex, qhttp::server::QHttpRequest *request,
              qhttp::server::QHttpResponse *response) :
        ControlBase<HTTPProxy>(acc, sourceIdHex, response), request(request), response(response)
    {
        auto baseUrl = acc->psi()->webServer()->serverUrl().toString();
        qDebug("FSP %s %s%s range: %s", qPrintable(request->methodString()), qPrintable(baseUrl),
               qPrintable(request->url().toString()), qPrintable(request->headers().value("range")));
    }

    static qhttp::TStatusCode mapStatusCode(ControlBase::StatusCode code)
    {
        switch (code) {
        case StatusCode::Ok:
            return qhttp::ESTATUS_OK;
        case StatusCode::PartialContent:
            return qhttp::ESTATUS_PARTIAL_CONTENT;
        case StatusCode::BadRequest:
            return qhttp::ESTATUS_BAD_REQUEST;
        case StatusCode::NotFound:
            return qhttp::ESTATUS_NOT_FOUND;
        case StatusCode::RangeNotSatisfied:
            return qhttp::ESTATUS_REQUESTED_RANGE_NOT_SATISFIABLE;
        case StatusCode::NotImplemented:
            return qhttp::ESTATUS_NOT_IMPLEMENTED;
        case StatusCode::BadGateway:
            return qhttp::ESTATUS_BAD_GATEWAY;
        case StatusCode::ServiceUnavailable:
            return qhttp::ESTATUS_SERVICE_UNAVAILABLE;
        }
        return qhttp::ESTATUS_NOT_IMPLEMENTED;
    }

    void finishWithMetadataError(ControlBase::StatusCode code, QByteArray httpReason = {})
    {
        Q_UNUSED(httpReason);
        response->setStatusCode(mapStatusCode(code));
        response->end();
    }
    void              finish() { response->end(); }
    inline QByteArray requestHeader(const char *headerName) { return request->headers().value(headerName); }
    inline void setResponseHeader(const char *headerName, QByteArray value) { response->addHeader(headerName, value); }
    inline void setResponseStatusCode(StatusCode code) { response->setStatusCode(mapStatusCode(code)); }
    void        connectReadyWrite(QObject *ctx, std::function<void()> &&callback)
    {
        connect(response, &qhttp::server::QHttpResponse::allBytesWritten, ctx, std::move(callback));
    }
    void write(const QByteArray &data) { response->write(data); }
};
#endif
}

namespace FileSharingProxy {
#ifdef HAVE_WEBSERVER
void proxify(PsiAccount *acc, const QString &sourceIdHex, qhttp::server::QHttpRequest *request,
             qhttp::server::QHttpResponse *response)
{
    (new HTTPProxy(acc, sourceIdHex, request, response))->process();
}
#endif
QNetworkReply *proxify(PsiAccount *acc, const QString &sourceIdHex, const QNetworkRequest &req)
{
    FileSharingNAMReply *reply = new FileSharingNAMReply(req);
    (new NAMProxy(acc, sourceIdHex, req, reply))->process();
    return reply;
}
}

#include "filesharingproxy.moc"
