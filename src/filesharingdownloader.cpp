/*
 * filesharingdownloader.cpp - file sharing downloader
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

#include "filesharingdownloader.h"
#include "filesharingitem.h"
#include "filesharingmanager.h"
#include "fileutil.h"
#include "httputil.h"
#include "jidutil.h"
#include "jingle-session.h"
#include "networkaccessmanager.h"
#include "psiaccount.h"
#include "psicon.h"
#include "userlist.h"
#include "xmpp_client.h"
#include "xmpp_hash.h"
#include "xmpp_jid.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QTimer>
#include <QUrlQuery>
#include <QVariant>

class AbstractFileShareDownloader : public QObject {
    Q_OBJECT
protected:
    QString     _lastError;
    qint64      rangeStart = 0;
    qint64      rangeSize  = 0; // 0 - all the remaining
    PsiAccount *acc;
    QString     sourceUri;

    void downloadError(const QString &err)
    {
        if (!err.isEmpty()) {
            _lastError = err;
            qDebug("Jingle failed: %s", qPrintable(err));
        }
        QTimer::singleShot(0, this, &AbstractFileShareDownloader::failed);
    }

    Jid selectOnlineJid(const QList<Jid> &jids) const
    {
        for (auto const &j : jids) {
            if (j == acc->client()->jid()) // skip self
                continue;
            for (UserListItem *u : acc->findRelevant(j)) {
                UserResourceList::Iterator rit = u->userResourceList().find(j.resource());
                if (rit != u->userResourceList().end())
                    return j;
            }
            if (acc->findGCContact(j))
                return j;
        }
        return Jid();
    }

public:
    AbstractFileShareDownloader(PsiAccount *acc, const QString &uri, QObject *parent) :
        QObject(parent), acc(acc), sourceUri(uri)
    {
    }

    virtual void   start()                                                          = 0;
    virtual qint64 bytesAvailable() const                                           = 0;
    virtual qint64 read(char *data, qint64 maxSize)                                 = 0;
    virtual void   abort(bool isFailure = false, const QString &reason = QString()) = 0;
    virtual bool   isConnected() const                                              = 0;
    virtual bool   hasFileSize() const                                              = 0;
    virtual qint64 fileSize() const                                                 = 0;
    virtual void   close() {}

    inline const QString &lastError() const { return _lastError; }
    void                  setRange(qint64 offset, qint64 length)
    {
        rangeStart = offset;
        rangeSize  = length;
    }
    inline bool                isRanged() const { return rangeSize || rangeStart; }
    std::tuple<qint64, qint64> range() const { return std::tuple<qint64, qint64>(rangeStart, rangeSize); }

signals:
    void metaDataChanged();
    void readyRead();
    void disconnected();
    void failed();
    void success();
};

class JingleFileShareDownloader : public AbstractFileShareDownloader {
    Q_OBJECT

    XMPP::Jingle::Connection::Ptr      connection;
    Jingle::FileTransfer::Application *app = nullptr;
    XMPP::Jingle::FileTransfer::File   file;
    QList<Jid>                         jids;

public:
    JingleFileShareDownloader(PsiAccount *acc_, const QString &uri, const XMPP::Jingle::FileTransfer::File &file,
                              const QList<Jid> &jids, QObject *parent) :
        AbstractFileShareDownloader(acc_, uri, parent),
        file(file), jids(jids)
    {
    }

    void start()
    {
        QUrl    uriToOpen(sourceUri);
        QString path = uriToOpen.path();
        if (path.startsWith('/')) { // this happens when authority part is present
            path = path.mid(1);
        }
        Jid  entity     = JIDUtil::fromString(path);
        auto sourceJids = jids;
        if (entity.isValid() && !entity.node().isEmpty())
            sourceJids.prepend(entity);
        Jid dataSource = selectOnlineJid(sourceJids);
        if (!dataSource.isValid()) {
            downloadError(tr("Jingle data source is offline"));
            return;
        }

        // query
        QUrlQuery uri;
        uri.setQueryDelimiters('=', ';');
        uri.setQuery(uriToOpen.query(QUrl::FullyEncoded));

        QString querytype = uri.queryItems().value(0).first;

        if (querytype != QStringLiteral("jingle-ft")) {
            downloadError(tr("Invalid Jingle-FT URI"));
            return;
        }

        auto *session = acc->client()->jingleManager()->newSession(dataSource);
        app           = static_cast<Jingle::FileTransfer::Application *>(
            session->newContent(Jingle::FileTransfer::NS, Jingle::Origin::Responder));
        if (!app) {
            downloadError(QString::fromLatin1("Jingle file transfer is disabled"));
            return;
        }
        if (isRanged())
            file.setRange(XMPP::Jingle::FileTransfer::Range(rangeStart, rangeSize));
        app->setFile(file);
        app->setStreamingMode(true);
        session->addContent(app);

        connect(session, &Jingle::Session::newContentReceived, this, [session, this]() {
            // we don't expect any new content on the session
            _lastError = tr("Unexpected incoming content");
            session->terminate(Jingle::Reason::Condition::Decline, _lastError);
        });

        connect(app, &Jingle::FileTransfer::Application::connectionReady, this, [this]() {
            auto r     = app->acceptFile().range();
            rangeStart = qint64(r.offset);
            rangeSize  = qint64(r.length);
            connection = app->connection();
            connect(connection.data(), &XMPP::Jingle::Connection::readyRead, this,
                    &JingleFileShareDownloader::readyRead);
            emit metaDataChanged();
            if (connection->bytesAvailable())
                emit readyRead();
        });

        connect(app, &Jingle::Application::stateChanged, this, [this](Jingle::State state) {
            if (state != Jingle::State::Finished)
                return;
            auto r = app->lastReason();
            app    = nullptr;
            if (r.isValid() && r.condition() == Jingle::Reason::Success) {
                emit disconnected();
                return;
            }
            downloadError(tr("Jingle download failed"));
        });

        session->initiate();
    }

    qint64 bytesAvailable() const { return connection ? connection->bytesAvailable() : 0; }

    qint64 read(char *data, qint64 maxSize) { return connection ? connection->read(data, maxSize) : 0; }

    void abort(bool isFailure, const QString &reason)
    {
        if (app) {
            if (connection) {
                connection->disconnect(this);
                connection.reset();
            }
            app->pad()->session()->disconnect(this);
            app->remove(isFailure ? XMPP::Jingle::Reason::FailedApplication : XMPP::Jingle::Reason::Decline, reason);
            app = nullptr;
        }
    }

    void close()
    {
        if (app) {
            if (connection) {
                connection->disconnect(this);
                connection.reset();
            }
            app->pad()->session()->disconnect(this);
            app->remove(XMPP::Jingle::Reason::Success);
            app = nullptr;
        }
    }

    bool isConnected() const { return connection && app && app->state() == XMPP::Jingle::State::Active; }

    bool   hasFileSize() const { return app && app->acceptFile().hasSize(); }
    qint64 fileSize() const { return app ? qint64(app->acceptFile().size()) : 0; }
};

class NAMFileShareDownloader : public AbstractFileShareDownloader {
    Q_OBJECT

    QNetworkReply *reply = nullptr;

    void namFailed(const QString &err = QString())
    {
        reply->disconnect(this);
        reply->deleteLater();
        downloadError(err);
    }

public:
    using AbstractFileShareDownloader::AbstractFileShareDownloader;
    void start()
    {
        QNetworkRequest req = QNetworkRequest(QUrl(sourceUri));
        if (isRanged()) {
            QString range = QString("bytes=%1-%2")
                                .arg(QString::number(rangeStart),
                                     rangeSize ? QString::number(rangeStart + rangeSize - 1) : QString());
            req.setRawHeader("Range", range.toLatin1());
        }
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#elif QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
        req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
        reply = acc->psi()->networkAccessManager()->get(req);
        connect(reply, &QNetworkReply::metaDataChanged, this, [this]() {
            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == 206) { // partial content
                QByteArray ba = reply->rawHeader("Content-Range");
                bool       parsed;
                std::tie(parsed, rangeStart, rangeSize) = Http::parseContentRangeHeader(ba);
                if (ba.size() && !parsed) {
                    namFailed(QLatin1String("Invalid HTTP response range"));
                    return;
                }
            } else if (status != 200 && status != 203) {
                rangeStart = 0;
                rangeSize  = 0; // make it not-ranged
                namFailed(tr("Unexpected HTTP status") + QString(": %1").arg(status));
                return;
            }

            emit metaDataChanged();
        });

        connect(reply, &QNetworkReply::readyRead, this, &NAMFileShareDownloader::readyRead);
        connect(reply, static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error),
                [=](QNetworkReply::NetworkError code) { qDebug("reply errored %d", code); });
        connect(reply, &QNetworkReply::finished, this, [this]() {
            qDebug("reply is finished. error code=%d. bytes available=%lld", reply->error(), reply->bytesAvailable());
            if (reply->error() == QNetworkReply::NoError)
                emit disconnected();
            else
                emit failed();
        });
    }

    qint64 bytesAvailable() const { return reply ? reply->bytesAvailable() : 0; }

    qint64 read(char *data, qint64 maxSize) { return reply ? reply->read(data, maxSize) : 0; }

    void abort(bool isFailure, const QString &reason)
    {
        Q_UNUSED(isFailure)
        Q_UNUSED(reason)
        if (reply) {
            reply->disconnect(this);
            reply->deleteLater();
            reply = nullptr;
        }
    }

    bool isConnected() const { return reply && reply->isRunning(); }

    bool   hasFileSize() const { return reply && reply->header(QNetworkRequest::ContentLengthHeader).isValid(); }
    qint64 fileSize() const { return reply ? reply->header(QNetworkRequest::ContentLengthHeader).toLongLong() : 0; }
};

class BOBFileShareDownloader : public AbstractFileShareDownloader {
    Q_OBJECT

    QList<Jid> jids;
    QByteArray receivedData;
    bool       destroyed = false;
    bool       connected = false;

public:
    BOBFileShareDownloader(PsiAccount *acc_, const QString &uri, const QList<Jid> &jids, QObject *parent) :
        AbstractFileShareDownloader(acc_, uri, parent), jids(jids)
    {
    }

    void start()
    {
        Jid j = selectOnlineJid(jids);
        if (!j.isValid()) {
            downloadError(tr("\"Bits Of Binary\" data source is offline"));
            return;
        }
        // skip cid: from uri and request it
        acc->loadBob(jids.first(), sourceUri.mid(4), this,
                     [this](bool success, const QByteArray &data, const QByteArray & /* media type */) {
                         if (destroyed)
                             return; // should not happen but who knows.
                         connected = true;
                         if (!success) {
                             downloadError(tr("Download using \"Bits Of Binary\" failed"));
                             return;
                         }
                         receivedData = data;
                         if (isRanged()) { // there is not such a thing like ranged bob
                             rangeStart = 0;
                             rangeSize  = 0; // make it not-ranged
                         }
                         emit metaDataChanged();
                         emit readyRead();
                         connected = false;
                         emit disconnected();
                     });
    }

    qint64 bytesAvailable() const { return receivedData.size(); }

    qint64 read(char *data, qint64 maxSize)
    {
        if (maxSize >= receivedData.count()) {
            qint64 ret = receivedData.count();
            memcpy(data, receivedData.data(), size_t(ret));
            receivedData.clear();
            return ret;
        }

        memcpy(data, receivedData.data(), size_t(maxSize));
        receivedData = receivedData.mid(int(maxSize));
        return maxSize;
    }

    void abort(bool isFailure, const QString &reason)
    {
        Q_UNUSED(isFailure)
        Q_UNUSED(reason)
        destroyed = true;
    }

    bool isConnected() const { return connected; }

    bool   hasFileSize() const { return !receivedData.isNull(); }
    qint64 fileSize() const { return receivedData.isNull() ? 0 : receivedData.size(); }
};

class FileShareDownloader::Private : public QObject {
    Q_OBJECT
public:
    FileShareDownloader *        q   = nullptr;
    PsiAccount *                 acc = nullptr;
    QList<XMPP::Hash>            sums;
    Jingle::FileTransfer::File   file;
    QList<Jid>                   jids;
    QStringList                  uris; // sorted from low priority to high.
    QScopedPointer<QFile>        tmpFile;
    QString                      dstFileName;
    QString                      lastError;
    qint64                       rangeStart  = 0;
    qint64                       rangeSize   = 0; // 0 - all the remaining
    qint64                       bytesLeft   = -1;
    AbstractFileShareDownloader *downloader  = nullptr;
    bool                         metaReady   = false;
    bool                         finished    = false;
    bool                         success     = false;
    bool                         selfDelete  = false;
    FileSharingItem::SourceType  currentType = FileSharingItem::SourceType::None;

    void finishWithError(const QString &errStr)
    {
        Q_ASSERT(!finished);
        finished  = true;
        success   = false;
        lastError = errStr;
        if (tmpFile) {
            tmpFile->close();
            tmpFile->remove();

            tmpFile.reset();
        }
        dstFileName.clear();
        if (selfDelete) {
            q->deleteLater();
        }
        emit q->failed();
    }

    void checkCacheReady()
    {
        if (!bytesLeft) {
            if (tmpFile) {
                tmpFile->close();
                tmpFile.reset();
                emit q->cacheReady();
            }
            finished = true;

            if (selfDelete) {
                qDebug("all data downloaded");
                downloader->close();
                q->deleteLater();
            }
        }
    }

    void startNextDownloader()
    {
        if (downloader) {
            lastError = downloader->lastError();
            delete downloader;
            downloader = nullptr;
        }

        if (uris.isEmpty()) {
            finishWithError(lastError.isEmpty() ? tr("Download sources are not given") : lastError);
            return;
        }
        QString uri  = uris.takeLast();
        auto    type = FileSharingItem::sourceType(uri);
        switch (type) {
        case FileSharingItem::SourceType::HTTP:
        case FileSharingItem::SourceType::FTP:
            downloader = new NAMFileShareDownloader(acc, uri, q);
            break;
        case FileSharingItem::SourceType::BOB:
            downloader = new BOBFileShareDownloader(acc, uri, jids, q);
            break;
        case FileSharingItem::SourceType::Jingle:
            downloader = new JingleFileShareDownloader(acc, uri, file, jids, q);
            break;
        default:
            finishWithError("Unhandled downloader");
            return;
        }

        downloader->setRange(rangeStart, rangeSize);

        connect(downloader, &AbstractFileShareDownloader::failed, q, [this]() {
            success = false;
            if (metaReady) {
                finishWithError(downloader->lastError());
                return;
            }
            startNextDownloader();
        });

        connect(downloader, &AbstractFileShareDownloader::metaDataChanged, q, [this]() {
            metaReady = true;

            std::tie(rangeStart, rangeSize) = downloader->range();
            if (rangeSize) {
                bytesLeft = rangeSize;
            } else if (!rangeStart && downloader->hasFileSize()) { // definitely not ranged and full size is known
                bytesLeft = downloader->fileSize();

                // then we are going to cache it as well. TODO: review caching when size is unknown
                auto partDir = QDir(acc->psi()->fileSharingManager()->cacheDir() + "/partial");
                partDir.mkpath(".");
                dstFileName = partDir.absoluteFilePath(QString::fromLatin1(sums.value(0).data().toHex()));

                tmpFile.reset(new QFile(dstFileName));
                if (!tmpFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) { // TODO complete unfinished downloads
                    downloader->abort();
                    finishWithError(tmpFile->errorString());
                    return;
                }
            }
            emit q->metaDataChanged();
        });

        connect(downloader, &AbstractFileShareDownloader::readyRead, q, &FileShareDownloader::readyRead);
        connect(downloader, &AbstractFileShareDownloader::disconnected, q, [this]() {
            if (!finished)
                checkCacheReady(); // TODO avoid double emit
            emit q->disconnected();
        });

        downloader->start();
    }
};

FileShareDownloader::FileShareDownloader(PsiAccount *acc, const QList<XMPP::Hash> &sums,
                                         const Jingle::FileTransfer::File &file, const QList<Jid> &jids,
                                         const QStringList &uris, QObject *parent) :
    QIODevice(parent),
    d(new Private)
{
    d->q    = this;
    d->acc  = acc;
    d->sums = sums;
    d->file = file;
    d->jids = jids;
    d->uris = FileSharingItem::sortSourcesByPriority(uris);
}

FileShareDownloader::~FileShareDownloader()
{
    abort();
    qDebug("downloader deleted");
}

bool FileShareDownloader::isSuccess() const { return d->success; }

bool FileShareDownloader::isConnected() const { return d->downloader ? d->downloader->isConnected() : false; }

bool FileShareDownloader::open(QIODevice::OpenMode mode)
{
    if (!d->uris.size() && !(mode & QIODevice::ReadOnly)) {
        d->success = false;
        return false;
    }
    if (isOpen())
        return true;

    QIODevice::open(mode);
    d->startNextDownloader();

    return true;
}

void FileShareDownloader::abort()
{
    if (d->downloader) {
        d->downloader->abort();
    }
}

void FileShareDownloader::setRange(qint64 start, qint64 size)
{
    d->rangeStart = start;
    d->rangeSize  = size;
}

bool FileShareDownloader::isRanged() const { return d->rangeStart > 0 || d->rangeSize > 0; }

std::tuple<qint64, qint64> FileShareDownloader::range() const
{
    return std::tuple<qint64, qint64>(d->rangeStart, d->rangeSize);
}

QString FileShareDownloader::takeFile() const
{
    if (d->tmpFile) {
        d->tmpFile.reset(); // flush/close/clear ptr
    }
    auto f = d->dstFileName;
    d->dstFileName.clear();
    return f;
}

const Jingle::FileTransfer::File &FileShareDownloader::jingleFile() const { return d->file; }

qint64 FileShareDownloader::readData(char *data, qint64 maxSize)
{
    if (!maxSize || !d->downloader) // wtf?
        return 0;

    qint64 bytesRead = d->downloader->read(data, maxSize);
    if (d->tmpFile && d->tmpFile->write(data, bytesRead) != bytesRead) {
        // file engine tries to write everything unless it's a system error
        d->downloader->abort();
        d->lastError = d->tmpFile->errorString();
        return 0;
    }

    if (d->bytesLeft != -1) {
        d->bytesLeft -= bytesRead;
    }

    d->checkCacheReady();

    return bytesRead;
}

qint64 FileShareDownloader::writeData(const char *, qint64)
{
    return 0; // not supported
}

bool FileShareDownloader::isSequential() const { return true; }

qint64 FileShareDownloader::bytesAvailable() const { return d->downloader ? d->downloader->bytesAvailable() : 0; }

void FileShareDownloader::setSelfDelete(bool enable) { d->selfDelete = enable; }

#include "filesharingdownloader.moc"
