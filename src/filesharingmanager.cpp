/*
 * filesharingmanager.cpp - file sharing manager
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

#include "filesharingmanager.h"

#include "applicationinfo.h"
#include "filecache.h"
#include "fileutil.h"
#include "httpfileupload.h"
#include "jidutil.h"
#include "networkaccessmanager.h"
#include "psiaccount.h"
#include "psicon.h"
#ifndef WEBKIT
#    include "qiteaudio.h"
#endif
#include "userlist.h"
#include "xmpp_bitsofbinary.h"
#include "xmpp_client.h"
#include "xmpp_hash.h"
#include "xmpp_jid.h"
#include "xmpp_message.h"
#include "xmpp_reference.h"
#include "xmpp_vcard.h"
#ifdef HAVE_WEBSERVER
#    include "qhttpserverconnection.hpp"
#    include "webserver.h"
#endif

#include <QBuffer>
#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QImageReader>
#include <QMimeData>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QPainter>
#include <QPixmap>
#include <QTemporaryFile>
#include <QUrl>
#include <QUrlQuery>
#include <cmath>
#include <cstdlib>

#define FILE_TTL (365 * 24 * 3600)
#define TEMP_TTL (7 * 24 * 3600)
#define HTTP_CHUNK (512 * 1024 * 1024)

static std::tuple<bool,qint64,qint64> parseHttpRangeResponse(const QByteArray &value)
{
    auto arr = value.split(' ');
    if (arr.size() != 2 || arr[0] != "bytes" || (arr = arr[1].split('-')).size() != 2)
        return std::tuple<bool,qint64,qint64>(false, 0, 0);
    qint64 start, size;
    bool ok;
    start = arr[0].toLongLong(&ok);
    if (ok) {
        arr = arr[1].split('/');
        size = arr[0].toLongLong(&ok) - start + 1;
    }
    if (!ok || size < 0)
        return std::tuple<bool,qint64,qint64>(false, 0, 0);
    return std::tuple<bool,qint64,qint64>(true, start, size);
}

class AbstractFileShareDownloader : public QObject
{
    Q_OBJECT
protected:
    QString _lastError;
    qint64  rangeStart = 0;
    qint64  rangeSize = 0;
    PsiAccount *acc;
    QString sourceUri;

    void downloadError(const QString &err)
    {
        if (!err.isEmpty())
            _lastError = err;
        QTimer::singleShot(0, this, &AbstractFileShareDownloader::failed);
    }

    Jid selectOnlineJid(const QList<Jid> &jids) const
    {
        for (auto const &j: jids) {
            for (UserListItem *u: acc->findRelevant(j)) {
                UserResourceList::Iterator rit = u->userResourceList().find(j.resource());
                if (rit != u->userResourceList().end())
                    return j;
            }
        }
        return Jid();
    }
public:
    AbstractFileShareDownloader(PsiAccount *acc, const QString &uri, QObject *parent) :
        QObject(parent),
        acc(acc),
        sourceUri(uri)
    {

    }

    virtual void start() = 0;
    virtual qint64 bytesAvailable() const = 0;
    virtual qint64 read(char *data, qint64 maxSize) = 0;
    virtual void abort(bool isFailure = false, const QString &reason = QString()) = 0;
    virtual bool isConnected() const = 0;

    inline const QString &lastError() const { return _lastError; }
    void setRange(qint64 offset, qint64 length)
    {
        rangeStart = offset;
        rangeSize = length;
    }
    inline bool isRanged() const { return rangeSize || rangeStart; }
    std::tuple<qint64, qint64> range() const
    {
        return std::tuple<qint64,qint64>(rangeStart, rangeSize);
    }

signals:
    void metaDataChanged();
    void readyRead();
    void disconnected();
    void failed();
    void success();
};

class JingleFileShareDownloader : public AbstractFileShareDownloader
{
    Q_OBJECT

    XMPP::Jingle::Connection::Ptr connection;
    Jingle::FileTransfer::Application *app = nullptr;
    XMPP::Jingle::FileTransfer::File file;
    QList<Jid> jids;

public:
    JingleFileShareDownloader(PsiAccount *acc, const QString &uri,
                              const XMPP::Jingle::FileTransfer::File &file,
                              const QList<Jid> &jids, QObject *parent) :
        AbstractFileShareDownloader(acc, uri, parent),
        file(file),
        jids(jids)
    {

    }

    void start()
    {
        QUrl uriToOpen(sourceUri);
        QString path = uriToOpen.path();
        if (path.startsWith('/')) {    // this happens when authority part is present
            path = path.mid(1);
        }
        Jid entity = JIDUtil::fromString(path);
        auto myJids = jids;
        if (entity.isValid() && !entity.node().isEmpty())
            myJids.prepend(entity);
        Jid dataSource = selectOnlineJid(myJids);
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

        Jingle::Session *session = acc->client()->jingleManager()->newSession(dataSource);
        app = static_cast<Jingle::FileTransfer::Application*>(session->newContent(Jingle::FileTransfer::NS, Jingle::Origin::Responder));
        if (!app) {
            downloadError(QString::fromLatin1("Jingle file transfer is disabled"));
            return;
        }
        if (isRanged())
            file.setRange(XMPP::Jingle::FileTransfer::Range(quint64(rangeStart), quint64(rangeSize)));
        app->setFile(file);
        app->setStreamingMode(true);
        session->addContent(app);

        connect(session, &Jingle::Session::newContentReceived, this, [session, this](){
            // we don't expect any new content on the session
            _lastError = tr("Unexpected incoming content");
            session->terminate(Jingle::Reason::Condition::Decline, _lastError);
        });

        connect(app, &Jingle::FileTransfer::Application::connectionReady, this, [this](){
            auto r = app->acceptFile().range();
            rangeStart = qint64(r.offset);
            rangeSize  = qint64(r.length);
            connection = app->connection();
            connect(connection.data(), &XMPP::Jingle::Connection::readyRead, this, &JingleFileShareDownloader::readyRead);
            emit metaDataChanged();
        });

        connect(session, &Jingle::Session::terminated, this, [this](){
            auto r = app->terminationReason();
            app = nullptr;
            if (r.isValid() && r.condition() == Jingle::Reason::Success) {
                emit disconnected();
                return;
            }
            downloadError(tr("Jingle download failed"));
        });
    }

    qint64 bytesAvailable() const
    {
        return connection? connection->bytesAvailable() : 0;
    }

    qint64 read(char *data, qint64 maxSize)
    {
        return connection? connection->read(data, maxSize) : 0;
    }

    void abort(bool isFailure, const QString &reason)
    {
        if (app) {
            if (connection) {
                connection->disconnect(this);
                connection.reset();
            }
            app->pad()->session()->disconnect(this);
            app->remove(isFailure? XMPP::Jingle::Reason::FailedApplication : XMPP::Jingle::Reason::Decline, reason);
            app = nullptr;
        }
    }

    bool isConnected() const
    {
        return connection && app && app->state() == XMPP::Jingle::State::Active;
    }
};

class NAMFileShareDownloader : public AbstractFileShareDownloader
{
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
            QString range = QString("bytes=%1-%2").arg(QString::number(rangeStart), rangeSize?
                                                           QString::number(rangeStart + rangeSize - 1) :
                                                           QString());
            req.setRawHeader("Range", range.toLatin1());
        }
#if QT_VERSION >= QT_VERSION_CHECK(5,9,0)
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#elif QT_VERSION >= QT_VERSION_CHECK(5,6,0)
        req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
        reply = acc->psi()->networkAccessManager()->get(req);
        connect(reply, &QNetworkReply::metaDataChanged, this, [this](){
            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == 206) { // partial content
                QByteArray ba = reply->rawHeader("Content-Range");
                bool parsed;
                std::tie(parsed, rangeStart, rangeSize) = parseHttpRangeResponse(ba);
                if (ba.size() && !parsed) {
                    namFailed(QLatin1String("Invalid HTTP response range"));
                    return;
                }
            } else if (status != 200 && status != 203) {
                rangeStart = 0;
                rangeSize = 0; // make it not-ranged
                namFailed(tr("Unexpected HTTP status") + QString(": %1").arg(status));
                return;
            }

            emit metaDataChanged();
        });

        connect(reply, &QNetworkReply::readyRead, this, &NAMFileShareDownloader::readyRead);
        connect(reply, static_cast<void(QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error),
              [=](QNetworkReply::NetworkError code){ qDebug("reply errored %d", code); });
        connect(reply, &QNetworkReply::finished, this, [this](){
            qDebug("reply is finished. error code=%d. bytes available=%lld", reply->error(), reply->bytesAvailable());
            if (reply->error() == QNetworkReply::NoError)
                emit disconnected();
            else
                emit failed();
        });
    }

    qint64 bytesAvailable() const
    {
        return reply? reply->bytesAvailable() : 0;
    }

    qint64 read(char *data, qint64 maxSize)
    {
        return reply? reply->read(data, maxSize) : 0;
    }

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

    bool isConnected() const
    {
        return reply && reply->isRunning();
    }
};

class BOBFileShareDownloader : public AbstractFileShareDownloader
{
    Q_OBJECT

    PsiAccount *acc;
    QList<Jid> jids;
    QByteArray receivedData;
    bool destroyed = false;
    bool connected = false;

public:
    BOBFileShareDownloader(PsiAccount *acc, const QString &uri,
                              const QList<Jid> &jids, QObject *parent) :
        AbstractFileShareDownloader(acc, uri, parent),
        jids(jids)
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
        acc->loadBob(jids.first(), sourceUri.mid(4), this, [this](bool success, const QByteArray &data, const QByteArray &/* media type */) {
            if (destroyed) return; // should not happen but who knows.
            connected = true;
            if (!success) {
                downloadError(tr("Download using \"Bits Of Binary\" failed"));
                return;
            }
            receivedData = data;
            if (isRanged()) { // there is not such a thing like ranged bob
                rangeStart = 0;
                rangeSize = 0; // make it not-ranged
            }
            emit metaDataChanged();
            connected = false;
            emit disconnected();
        });
    }

    qint64 bytesAvailable() const
    {
        return receivedData.size();
    }

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

    bool isConnected() const
    {
        return connected;
    }
};

class FileShareDownloader::Private : public QObject
{
    Q_OBJECT
public:
    FileShareDownloader *q = nullptr;
    PsiAccount *acc = nullptr;
    QString sourceId;
    Jingle::FileTransfer::File file;
    QList<Jid> jids;
    QStringList uris; // sorted from low priority to high.
    FileSharingManager *manager = nullptr;
    FileSharingManager::SourceType currentType = FileSharingManager::SourceType::None;
    QScopedPointer<QFile> tmpFile;
    QString dstFileName;
    QString lastError;
    qint64 rangeStart = 0;
    qint64 rangeSize = 0;
    AbstractFileShareDownloader *downloader = nullptr;
    bool metaReady = false;
    bool success = false;

    void startNextDownloader()
    {
        if (downloader) {
            lastError = downloader->lastError();
            delete downloader;
            downloader = nullptr;
        }

        if (uris.isEmpty()) {
            success = false;
            if (lastError.isEmpty())
                lastError = tr("Download sources are not given");
            emit q->finished();
            return;
        }
        QString uri = uris.takeLast();
        auto type = FileSharingManager::sourceType(uri);
        switch (type) {
        case FileSharingManager::SourceType::HTTP:
        case FileSharingManager::SourceType::FTP:
            downloader = new NAMFileShareDownloader(acc, uri, q);
            break;
        case FileSharingManager::SourceType::BOB:
            downloader = new BOBFileShareDownloader(acc, uri, jids, q);
            break;
        case FileSharingManager::SourceType::Jingle:
            downloader = new JingleFileShareDownloader(acc, uri, file, jids, q);
            break;
        default:
            lastError = "Unhandled downloader";
            success = false;
            emit q->finished();
            return;
        }

        downloader->setRange(rangeStart, rangeSize);

        connect(downloader, &AbstractFileShareDownloader::failed, q, [this](){
            success = false;
            if (metaReady) {
                lastError = downloader->lastError();
                emit q->finished();
                return;
            }
            startNextDownloader();
        });

        connect(downloader, &AbstractFileShareDownloader::metaDataChanged, q, [this](){
            metaReady = true;
            QFileInfo fi(dstFileName);
            QString tmpFN = QString::fromLatin1("dl-") + fi.fileName();
            tmpFN = QString("%1/%2").arg(fi.path(), tmpFN);
            tmpFile.reset(new QFile(tmpFN));
            if (!tmpFile->open(QIODevice::ReadWrite | QIODevice::Truncate)) { // TODO complete unfinished downloads
                lastError = tmpFile->errorString();
                tmpFile.reset();
                downloader->abort();
                success = false;
                emit q->finished();
                return;
            }
            std::tie(rangeStart, rangeSize) = downloader->range();
            emit q->metaDataChanged();
        });

        connect(downloader, &AbstractFileShareDownloader::readyRead, q, &FileShareDownloader::readyRead);
        connect(downloader, &AbstractFileShareDownloader::disconnected, q, &FileShareDownloader::disconnected);

        downloader->start();
    }

};

FileShareDownloader::FileShareDownloader(PsiAccount *acc, const QString &sourceId, const Jingle::FileTransfer::File &file,
                                         const QList<Jid> &jids, const QStringList &uris, FileSharingManager *manager) :
    QIODevice(manager),
    d(new Private)
{
    d->q        = this;
    d->acc      = acc;
    d->sourceId = sourceId;
    d->file     = file;
    d->jids     = jids;
    d->manager  = manager;
    d->uris     = FileSharingManager::sortSourcesByPriority(uris);
}

FileShareDownloader::~FileShareDownloader()
{
    abort();
    qDebug("downloader deleted");
}

bool FileShareDownloader::isSuccess() const
{
    return d->success;
}

bool FileShareDownloader::open(QIODevice::OpenMode mode)
{
    if (!d->uris.size() && !(mode & QIODevice::ReadOnly)) {
        d->success = false;
        return false;
    }
    if (isOpen())
        return true;

    auto docLoc = ApplicationInfo::documentsDir();
    QDir docDir(docLoc);
    QFileInfo fi(d->file.name());
    QString fn = FileUtil::cleanFileName(fi.fileName());
    if (fn.isEmpty()) {
        fn = d->sourceId;
    }
    fi = QFileInfo(fn);
    QString baseName = fi.completeBaseName();
    QString suffix = fi.suffix();

    int index = 1;
    while (docDir.exists(fn)) {
        if (suffix.size()) {
            fn = QString("%1-%2.%3").arg(baseName, QString::number(index), suffix);
        } else {
            fn = QString("%1-%2").arg(baseName, QString::number(index));
        }
    }

    d->dstFileName = docDir.absoluteFilePath(fn);
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
    d->rangeSize = size;
}

bool FileShareDownloader::isRanged() const
{
    return d->rangeStart > 0 || d->rangeSize > 0;
}

std::tuple<qint64, qint64> FileShareDownloader::range() const
{
    return std::tuple<qint64,qint64>(d->rangeStart, d->rangeSize);
}

QString FileShareDownloader::fileName() const
{
    if (d->success) {
        return d->dstFileName;
    }
    return QString();
}

const Jingle::FileTransfer::File &FileShareDownloader::jingleFile() const
{
    return d->file;
}

qint64 FileShareDownloader::readData(char *data, qint64 maxSize)
{
    if (!d->tmpFile || !d->downloader) return 0; // wtf?

    qint64 bytesRead = d->downloader->read(data, maxSize);
    if (d->tmpFile->write(data, bytesRead) != bytesRead) { // file engine tries to write everything until it's a system error
        d->downloader->abort();
        d->lastError = d->tmpFile->errorString();
        return 0;
    }

    if (!d->downloader->isConnected() && !d->downloader->bytesAvailable()) {
        // seems like last piece of data was written. we are ready to finish
        d->success = true;
        emit finished();
    }

    return bytesRead;
}

qint64 FileShareDownloader::writeData(const char *, qint64)
{
    return 0; // not supported
}

bool FileShareDownloader::isSequential() const
{
    return true;
}

qint64 FileShareDownloader::bytesAvailable() const
{
    return d->downloader? d->downloader->bytesAvailable() : 0;
}

// ======================================================================
// FileSharingItem
// ======================================================================
FileSharingItem::FileSharingItem(FileCacheItem *cache, PsiAccount *acc, FileSharingManager *manager) :
    QObject(manager),
    cache(cache),
    acc(acc),
    manager(manager),
    isTempFile(false)
{
    initFromCache();
}

FileSharingItem::FileSharingItem(const QImage &image, PsiAccount *acc, FileSharingManager *manager) :
    QObject(manager),
    acc(acc),
    manager(manager),
    isImage(true),
    isTempFile(false)
{
    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG", 0);
    sha1hash = QCryptographicHash::hash(ba, QCryptographicHash::Sha1);
    mimeType = QString::fromLatin1("image/png");
    _fileSize = size_t(ba.size());

    cache = manager->getCacheItem(sha1hash, true);
    if (cache) {
        _fileName = cache->fileName();
    } else {
        QTemporaryFile file(QDir::tempPath() + QString::fromLatin1("/psishare-XXXXXX.png"));
        file.open();
        file.write(ba);
        file.setAutoRemove(false);
        _fileName = file.fileName();
        isTempFile = true;
        file.close();
    }

}

FileSharingItem::FileSharingItem(const QString &fileName, PsiAccount *acc, FileSharingManager *manager) :
    QObject(manager),
    acc(acc),
    manager(manager),
    isImage(false),
    isTempFile(false),
    _fileName(fileName)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    QFile file(fileName);
    _fileSize = size_t(file.size());
    file.open(QIODevice::ReadOnly);
    hash.addData(&file);
    sha1hash = hash.result();
    cache = manager->getCacheItem(sha1hash, true);

    if (cache) {
        initFromCache();
    } else {
        file.seek(0);
        mimeType = QMimeDatabase().mimeTypeForFileNameAndData(fileName, &file).name();
        isImage = QImageReader::supportedMimeTypes().contains(mimeType.toLatin1());
    }
}

FileSharingItem::FileSharingItem(const QString &mime, const QByteArray &data, const QVariantMap &metaData,
                                 PsiAccount *acc, FileSharingManager *manager) :
    QObject(manager),
    acc(acc),
    manager(manager),
    isImage(false),
    isTempFile(false),
    metaData(metaData)
{
    sha1hash = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
    mimeType = mime;
    _fileSize = size_t(data.size());

    cache = manager->getCacheItem(sha1hash, true);
    if (cache) {
        _fileName = cache->fileName();
    } else {
        QMimeDatabase db;
        QString fileExt = db.mimeTypeForData(data).suffixes().value(0);
        QTemporaryFile file(QDir::tempPath() + QString::fromLatin1("/psi-XXXXXX") +
                            (fileExt.isEmpty()? fileExt : QString('.')+fileExt));
        file.open();
        file.write(data);
        file.setAutoRemove(false);
        _fileName = file.fileName();
        isTempFile = true;
        file.close();
    }
}

FileSharingItem::~FileSharingItem()
{
    if (!cache && isTempFile && !_fileName.isEmpty()) {
        QFile f(_fileName);
        if (f.exists())
            f.remove();
    }
}

void FileSharingItem::initFromCache()
{
    auto md = cache->metadata();
    mimeType = md.value(QString::fromLatin1("type")).toString();
    isImage = QImageReader::supportedMimeTypes().contains(mimeType.toLatin1());
    QString link = md.value(QString::fromLatin1("link")).toString();
    if (link.isEmpty()) {
        _fileName = cache->fileName();
        _fileSize = cache->size();
    }
    else {
        _fileName = link;
        _fileSize = size_t(QFileInfo(_fileName).size());
    }

    sha1hash = QByteArray::fromBase64(cache->id().toLatin1());
    readyUris = md.value(QString::fromLatin1("uris")).toStringList();

    QString httpScheme(QString::fromLatin1("http"));
    QString xmppScheme(QString::fromLatin1("xmpp")); // jingle ?
    for (const auto &u: readyUris) {
        QUrl url(u);
        auto scheme = url.scheme();
        if (scheme.startsWith(httpScheme)) {
            httpFinished = true;
        } else if (scheme == xmppScheme) {
            jingleFinished = true;
        }
    }
}

void FileSharingItem::checkFinished()
{
    // if we didn't emit yet finished signal and everything is finished
    if (!finishNotified && httpFinished && jingleFinished) {
        QVariantMap meta = metaData;
        meta["type"] = mimeType;
        if (readyUris.count()) // if ever published something on external service like http
            meta["uris"] = readyUris;
        if (isTempFile) {
            QFile f(_fileName);
            f.open(QIODevice::ReadOnly);
            auto d = f.readAll();
            f.close();
            cache = manager->saveToCache(sha1hash, d, meta, TEMP_TTL);
            _fileName = cache->fileName();
            f.remove();
        } else {
            meta["link"] = _fileName;
            cache = manager->saveToCache(sha1hash, QByteArray(), meta, FILE_TTL);
        }
        finishNotified = true;
        emit publishFinished();
    }
}

Reference FileSharingItem::toReference() const
{
    QStringList uris(readyUris);

    UserListItem *u = acc->find(acc->jid());
    if (u->userResourceList().isEmpty())
        return Reference();
    Jid selfJid = u->jid().withResource(u->userResourceList().first().name());

    uris.append(QString::fromLatin1("xmpp:%1?jingle-ft").arg(selfJid.full()));
    uris = FileSharingManager::sortSourcesByPriority(uris);
    std::reverse(uris.begin(), uris.end());

    Hash hash(Hash::Sha1);
    hash.setData(sha1hash);

    Jingle::FileTransfer::File jfile;
    QFileInfo fi(_fileName);
    jfile.setDate(fi.lastModified());
    jfile.addHash(hash);
    jfile.setName(fi.fileName());
    jfile.setSize(quint64(fi.size()));
    jfile.setMediaType(mimeType);
    jfile.setDescription(_description);

    QSize thumbSize(64,64);
    auto thumbPix = thumbnail(thumbSize).pixmap(thumbSize);
    if (!thumbPix.isNull()) {
        QByteArray pixData;
        QBuffer buf(&pixData);
        thumbPix.save(&buf, "PNG");
        QString png(QString::fromLatin1("image/png"));
        auto bob = acc->client()->bobManager()->append(pixData, png, isTempFile? TEMP_TTL: FILE_TTL);
        Thumbnail thumb(QByteArray(), png, quint32(thumbSize.width()), quint32(thumbSize.height()));
        thumb.uri = QLatin1String("cid:") + bob.cid();
        jfile.setThumbnail(thumb);
    }

    auto bhg = metaData.value(QLatin1String("histogram")).toByteArray();
    if (bhg.size()) {
        XMPP::Jingle::FileTransfer::File::Histogram hg;
        hg.coding = XMPP::Jingle::FileTransfer::File::Histogram::Coding::U8;
        std::transform(bhg.begin(), bhg.end(), std::back_inserter(hg.bars), [](auto v){ return quint32(v); });
        jfile.setAudioHistogram(hg);
    }

    Reference r(Reference::Data, uris.first());
    MediaSharing ms;
    ms.file = jfile;
    ms.sources = uris;

    r.setMediaSharing(ms);

    return r;
}

QIcon FileSharingItem::thumbnail(const QSize &size) const
{
    QImage image;
    if (isImage && image.load(_fileName)) {
        auto img = image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QImage back(64,64,QImage::Format_ARGB32_Premultiplied);
        back.fill(Qt::transparent);
        QPainter painter(&back);
        auto imgRect = img.rect();
        imgRect.moveCenter(back.rect().center());
        painter.drawImage(imgRect, img);
        return QIcon(QPixmap::fromImage(std::move(back)));
    }
    return QFileIconProvider().icon(_fileName);
}

QImage FileSharingItem::preview(const QSize &maxSize) const
{
    QImage image;
    if (image.load(_fileName)) {
        auto s = image.size().boundedTo(maxSize);
        return image.scaled(s, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

QString FileSharingItem::displayName() const
{
    if (_fileName.isEmpty()) {
        auto ext = FileUtil::mimeToFileExt(mimeType);
        return QString("psi-%1.%2").arg(QString::fromLatin1(sha1hash.toHex()), ext).replace("/", "");
    }
    return QFileInfo(_fileName).fileName();
}

QString FileSharingItem::fileName() const
{
    return _fileName;
}

void FileSharingItem::publish()
{
    if (!httpFinished) {
        auto hm = acc->client()->httpFileUploadManager();
        if (hm->discoveryStatus() == HttpFileUploadManager::DiscoNotFound) {
            httpFinished = true;
            checkFinished();
        } else {
            auto hfu = hm->upload(_fileName, displayName(), mimeType);
            hfu->setParent(this);
            connect(hfu, &HttpFileUpload::progress, this, [this](qint64 bytesReceived, qint64 bytesTotal){
                Q_UNUSED(bytesTotal)
                emit publishProgress(size_t(bytesReceived));
            });
            connect(hfu, &HttpFileUpload::finished, this, [hfu, this]() {
                httpFinished = true;
                if (hfu->success()) {
                    _log.append(tr("Published on HttpUpload service"));
                    readyUris.append(hfu->getHttpSlot().get.url);
                } else {
                    _log.append(QString("%1: %2")
                               .arg(tr("Failed to publish on HttpUpload service"),
                                    hfu->statusString()));
                }
                emit logChanged();
                checkFinished();
            });
        }
    }
    if (!jingleFinished) {
        // FIXME we have to add muc jids here if shared with muc
        //readyUris.append(QString::fromLatin1("xmpp:%1?jingle").arg(acc->jid().full()));
        jingleFinished = true;
        checkFinished();
    }
}

// ======================================================================
// FileSharingManager
// ======================================================================
class FileSharingManager::Private
{
public:
    class Source
    {
    public:
        Source(const XMPP::Jingle::FileTransfer::File &file, const QList<XMPP::Jid> &jids, const QStringList &uris) :
            file(file), jids(jids), uris(uris) {}
        XMPP::Jingle::FileTransfer::File file;
        QList<XMPP::Jid> jids;
        QStringList uris;
        FileShareDownloader *downloader = nullptr; // download request for full file
    };

    FileCache *cache;
    QHash<QByteArray,Source> sources;
};

FileSharingManager::FileSharingManager(QObject *parent) : QObject(parent),
    d(new Private)
{
    d->cache = new FileCache(getCacheDir(), this);
}

FileSharingManager::~FileSharingManager()
{

}

QString FileSharingManager::getCacheDir()
{
    QDir shares(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation) + "/shares");
    if (!shares.exists()) {
        QDir home(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation));
        home.mkdir("shares");
    }
    return shares.path();
}

FileCacheItem *FileSharingManager::getCacheItem(const QByteArray &id, bool reborn, QString *fileName_out)
{
    QString idStr = QString::fromLatin1(id.toBase64());
    auto item = d->cache->get(idStr, reborn);
    if (!item)
        return nullptr;

    QString link = item->metadata().value(QLatin1String("link")).toString();
    QString fileName = link.size()? link : d->cache->cacheDir() + "/" + item->fileName();
    if (fileName.isEmpty() || !QFileInfo(fileName).isReadable()) {
        d->cache->remove(idStr);
        return nullptr;
    }
    if (fileName_out)
        *fileName_out = fileName;
    return item;
}

FileCacheItem * FileSharingManager::saveToCache(const QByteArray &id, const QByteArray &data,
                                                const QVariantMap &metadata, unsigned int maxAge)
{
    auto item = d->cache->append(QString::fromLatin1(id.toBase64()), data, metadata, maxAge);
    if (item && data.size())
        d->cache->sync();
    return item;
}

/*
FileSharingItem *FileSharingManager::fromReference(const Reference &ref, PsiAccount *acc)
{
    auto h = ref.mediaSharing().file.hash(Hash::Sha1);
    if (!h.isValid())
        return nullptr;
    auto c = cache->get(QString::fromLatin1(h.data().toHex()));
    if (c)
        return new FileSharingItem(c, acc, this);

    return nullptr;
}
*/

QList<FileSharingItem*> FileSharingManager::fromMimeData(const QMimeData *data, PsiAccount *acc)
{
    QList<FileSharingItem*> ret;
    QStringList files;

    QString voiceMsgMime(QLatin1String("audio/ogg"));
    QString voiceHistogramMime(QLatin1String("application/x-psi-histogram"));

    bool hasVoice = data->hasFormat(voiceMsgMime) && data->hasFormat(voiceHistogramMime);
    if (!data->hasImage() && !hasVoice && data->hasUrls()) {
        for(auto const &url: data->urls()) {
            if (!url.isLocalFile()) {
                continue;
            }
            QFileInfo fi(url.toLocalFile());
            if (fi.isFile() && fi.isReadable()) {
                files.append(fi.filePath());
            }
        }
    }
    if (files.isEmpty() && !data->hasImage() && !hasVoice) {
        return ret;
    }

    if (files.isEmpty()) { // so we have an image
        FileSharingItem *item;
        if (hasVoice) {
            QByteArray ba = data->data(voiceMsgMime);
            QByteArray histogram = data->data(voiceHistogramMime);
            QVariantMap vm;
            vm.insert("histogram", histogram);
            item = new FileSharingItem(voiceMsgMime, ba, vm, acc, this);
        } else {
            QImage img = data->imageData().value<QImage>();
            item = new FileSharingItem(img, acc, this);
        }
        ret.append(item);
    } else {
        for (auto const &f: files) {
            auto item = new FileSharingItem(f, acc, this);
            ret.append(item);
        }
    }

    return ret;
}

QList<FileSharingItem*> FileSharingManager::fromFilesList(const QStringList &fileList, PsiAccount *acc)
{
    QList<FileSharingItem*> ret;
    if(fileList.isEmpty())
        return ret;
    foreach (const QString &file, fileList) {
        QFileInfo fi(file);
        if (fi.isFile() && fi.isReadable()) {
            auto item = new FileSharingItem(file, acc, this);
            ret << item;
        }
    }
    return ret;
}

QByteArray FileSharingManager::registerSource(const Jingle::FileTransfer::File &file, const Jid &source, const QStringList &uris)
{
    Hash h = file.hash(Hash::Sha1);
    if (!h.isValid()) {
        h = file.hash();
    }
    QByteArray shareId = h.data();
    if (shareId.isEmpty())
        return QByteArray();

    auto it = d->sources.find(shareId);
    if (it == d->sources.end())
        it = d->sources.insert(shareId, Private::Source(file, QList<Jid>()<<source, uris));
    else {
        if (it.value().jids.indexOf(source) == -1)
            it.value().jids.append(source);
        if (!it.value().file.merge(file)) { // failed to merge files. replace it
            it = d->sources.insert(shareId, Private::Source(file, QList<Jid>()<<source, uris));
        } else {
            for (auto const &uri : uris) {
                if (it->uris.indexOf(uri) == -1)
                    it->uris.append(uri);
            }
        }
    }
    return shareId;
}

QString FileSharingManager::downloadThumbnail(const QString &sourceId)
{
    Q_UNUSED(sourceId);
    return QString(); //fixme
}

// try take http or ftp source to be passed directly to media backend
QUrl FileSharingManager::simpleSource(const QByteArray &sourceId) const
{
    auto it = d->sources.find(sourceId);
    if (it == d->sources.end())
        return QUrl();
    Private::Source &src = *it;
    auto sorted = sortSourcesByPriority(src.uris);
    QString srcUrl = sorted.last();
    auto t = sourceType(srcUrl);
    if (t == SourceType::HTTP || t == SourceType::FTP) {
        return QUrl(srcUrl);
    }
    return QUrl();
}

Jingle::FileTransfer::File FileSharingManager::registeredSourceFile(const QByteArray &sourceId)
{
    auto it = d->sources.find(sourceId);
    if (it == d->sources.end())
        return Jingle::FileTransfer::File();
    Private::Source &src = *it;
    return src.file;
}

FileShareDownloader* FileSharingManager::downloadShare(PsiAccount *acc, const QByteArray &sourceId,
                                                       bool isRanged, qint64 start, qint64 size)
{
    auto it = d->sources.find(sourceId);
    if (it == d->sources.end())
        return nullptr;

    Private::Source &src = *it;
    if (isRanged && src.file.hasSize() && start == 0 && size == qint64(src.file.size()))
        isRanged = false;

    if (!isRanged && src.downloader) { // let's wait till first one is finished
        qWarning("%s download is in progress already", qPrintable(src.file.name()));
        return nullptr;
    }

    FileShareDownloader *downloader = new FileShareDownloader(acc, sourceId, src.file, src.jids, src.uris, this);
    if (isRanged) {
        downloader->setRange(start, size);
        return downloader;
    }

    src.downloader = downloader;
    connect(downloader, &FileShareDownloader::finished, this, [this,sourceId, downloader](){
        if (!downloader->isSuccess())
            return;

        auto it = d->sources.find(sourceId);
        if (it == d->sources.end())
            return;
        Private::Source &src = *it;
        src.downloader->disconnect(this);
        src.downloader->deleteLater();

        QFile tmpFile(src.downloader->fileName());
        tmpFile.open(QIODevice::ReadOnly);
        auto h = src.downloader->jingleFile().hash(Hash::Sha1);
        QByteArray sha1hash;
        if (!h.isValid() || h.data().isEmpty()) {
            // no sha1 hash
            Hash h(Hash::Sha1);
            if (h.computeFromDevice(&tmpFile)) {
                sha1hash = h.data();
            }
        } else {
            sha1hash = h.data();
        }
        tmpFile.close();

        QVariantMap vm;
        vm.insert(QString::fromLatin1("type"), src.file.mediaType());
        vm.insert(QString::fromLatin1("uris"), src.uris);
        vm.insert(QString::fromLatin1("link"), QFileInfo(src.downloader->fileName()).absoluteFilePath());
        auto thumb = src.file.thumbnail();
        if (thumb.isValid())
            vm.insert(QString::fromLatin1("thumbnail"), thumb.uri);
        if (src.file.audioHistogram().bars.count()) {
            auto s = src.file.audioHistogram();
            QStringList sl;
            sl.reserve(s.bars.count());
            for (auto const &v: s.bars) sl.append(QString::number(v));
            vm.insert(QString::fromLatin1("spectrum"), sl.join(','));
            vm.insert(QString::fromLatin1("spectrum_coding"), s.coding);
        }

        saveToCache(sha1hash, QByteArray(), vm, FILE_TTL);
        d->sources.erase(it); // now it's in cache so the source is not needed anymore
    });

    connect(downloader, &FileShareDownloader::destroyed, this, [this,sourceId](){
        auto it = d->sources.find(sourceId);
        if (it == d->sources.end())
            return;
        it->downloader = nullptr;
    });

    return downloader;
}

bool FileSharingManager::jingleAutoAcceptIncomingDownloadRequest(Jingle::Session *session)
{
    QList<QPair<Jingle::FileTransfer::Application*,FileCacheItem*>> toAccept;
    // check if we can accept the session immediately w/o user interaction
    for (auto const &a: session->contentList()) {
        auto ft = static_cast<Jingle::FileTransfer::Application*>(a);
        if (a->senders() == Jingle::Origin::Initiator)
            return false;

        Hash h = ft->file().hash(Hash::Sha1);
        if (!h.isValid()) {
            h = ft->file().hash();
        }
        if (!h.isValid() || h.data().isEmpty())
            return false;

        FileCacheItem *item = getCacheItem(h.data(), true);
        if (!item)
            return false;

        toAccept.append(qMakePair(ft,item));
    }

    for (auto const &p: toAccept) {
        auto ft = p.first;
        auto item = p.second;

        connect(ft, &Jingle::FileTransfer::Application::deviceRequested, this, [ft,item](quint64 offset, quint64 /*size*/){
            auto vm = item->metadata();
            QString fileName = vm.value(QString::fromLatin1("link")).toString();
            if (fileName.isEmpty()) {
                fileName = item->fileName();
            }
            auto f = new QFile(fileName, ft);
            if (!f->open(QIODevice::ReadOnly)) {
                ft->setDevice(nullptr);
                return;
            }
            f->seek(qint64(offset));
            ft->setDevice(f);
        });
    }
    session->accept();

    return true;
}

FileSharingManager::SourceType FileSharingManager::sourceType(const QString &uri)
{
    if (uri.startsWith(QLatin1String("http"))) {
        return FileSharingManager::SourceType::HTTP;
    }
    else if (uri.startsWith(QLatin1String("xmpp"))) {
        return FileSharingManager::SourceType::Jingle;
    }
    else if (uri.startsWith(QLatin1String("ftp"))) {
        return FileSharingManager::SourceType::FTP;
    }
    else if (uri.startsWith(QLatin1String("cid"))) {
        return FileSharingManager::SourceType::BOB;
    }
    return FileSharingManager::SourceType::None;
}

QStringList FileSharingManager::sortSourcesByPriority(const QStringList &uris)
{
    // sort uris by priority first
    QMultiMap<int,QString> sorted;
    for (auto const &u: uris) {
        auto type = FileSharingManager::sourceType(u);
        if (type != FileSharingManager::SourceType::None)
            sorted.insert(int(type), u);
    }

    return sorted.values();
}

#ifdef HAVE_WEBSERVER
// returns <parsed,list of start/size>
static std::tuple<bool,QList<QPair<qint64,qint64>>> parseHttpRangeRequest(qhttp::server::QHttpRequest* req,
                                                                          qhttp::server::QHttpResponse *res,
                                                                          bool fullSizeKnown = false,
                                                                          qint64 fullSize = 0)
{
    QList<QPair<qint64,qint64>> ret;
    QByteArray rangesBa = req->headers().value("range");
    if (!rangesBa.size())
        return std::make_tuple(true, ret);

    if ((fullSizeKnown && !fullSize) || !rangesBa.startsWith("bytes=")) {
        res->setStatusCode(qhttp::ESTATUS_REQUESTED_RANGE_NOT_SATISFIABLE);
        return std::make_tuple(false, ret);
    }

    QList<QByteArray> arr = QByteArray::fromRawData(rangesBa.data() + sizeof("bytes"),
                                                    rangesBa.size() - int(sizeof("bytes"))).split(',');

    for (const auto &ba: arr) {
        auto trab = ba.trimmed();
        if (!trab.size()) {
            res->setStatusCode(qhttp::ESTATUS_BAD_REQUEST);
            return std::make_tuple(false, ret);
        }
        bool ok;
        qint64 start;
        qint64 end;
        if (trab[0] == '-') {
            res->setStatusCode(qhttp::ESTATUS_NOT_IMPLEMENTED);
            return std::make_tuple(false, ret);
        }

        auto l = trab.split('-');
        if (l.size() != 2) {
            res->setStatusCode(qhttp::ESTATUS_BAD_REQUEST);
            return std::make_tuple(false, ret);
        }

        start = l[0].toLongLong(&ok);
        if (l[1].size()) {
            if (ok && l[1].size())
                end = l[1].toLongLong(&ok);
            if (!ok || start > end) {
                res->setStatusCode(qhttp::ESTATUS_BAD_REQUEST);
                return std::make_tuple(false, ret);
            }

            if (!fullSizeKnown || start < fullSize) {
                ret.append(qMakePair(start, end - start + 1));
            }
        } else {
            if (!fullSizeKnown || start < fullSize)
                ret.append(qMakePair(start, 0));
        }
    }

    if (fullSizeKnown && !ret.size()) {
        res->setStatusCode(qhttp::ESTATUS_REQUESTED_RANGE_NOT_SATISFIABLE);
        res->addHeader("Content-Range", QByteArray("bytes */") + QByteArray::number(fullSize));
        return std::make_tuple(false, ret);
    }

    return std::make_tuple(true, ret);
}

// returns true if request handled. false if we need to find another hander
bool FileSharingManager::downloadHttpRequest(PsiAccount *acc, const QString &sourceIdHex,
                                             qhttp::server::QHttpRequest* req, qhttp::server::QHttpResponse *res)
{
    qint64 requestedStart = 0;
    qint64 requestedSize = 0;
    bool isRanged = false;
    QByteArray sourceId = QByteArray::fromHex(sourceIdHex.toLatin1());

    auto handleRequestedRange = [&](qint64 fileSize = -1)
    {
        QList<QPair<qint64,qint64>> ranges;
        bool parsed;
        std::tie(parsed,ranges) = parseHttpRangeRequest(req, res, fileSize != -1, fileSize);
        if (!parsed) {
            res->end();
            return false;
        }
        if (ranges.count() > 1) { // maybe we can support this in the future..
            res->setStatusCode(qhttp::ESTATUS_NOT_IMPLEMENTED);
            res->end();
            return false;
        }

        if (ranges.count()) {
            requestedStart = ranges[0].first;
            requestedSize = ranges[0].second;
            isRanged = true;
        }
        return true;
    };

    auto setupHeaders = [res](
            qint64 fileSize,
            QString contentType,
            QDateTime lastModified,
            bool isRanged,
            qint64 start,
            qint64 size
    )
    {

        if (lastModified.isValid())
            res->addHeader("Last-Modified", lastModified.toString(Qt::RFC2822Date).toLatin1());
        if (contentType.count())
            res->addHeader("Content-Type", contentType.toLatin1());

        res->addHeader("Accept-Ranges", "bytes");
        res->addHeader("connection", "keep-alive");
        if (isRanged) {
            auto range = QString(QLatin1String("bytes %1-%2/%3")).arg(start).arg(start+size-1)
                    .arg(fileSize == -1? QString('*'): QString::number(fileSize));
            res->addHeader("Content-Range", range.toLatin1());
            res->setStatusCode(qhttp::ESTATUS_PARTIAL_CONTENT);
        } else {
            if (fileSize != -1)
                res->addHeader("Content-Length", QByteArray::number(fileSize));
            res->setStatusCode(qhttp::ESTATUS_OK);
        }
    };

    QString fileName;
    FileCacheItem *item = acc->psi()->fileSharingManager()->getCacheItem(sourceId, true, &fileName);
    if (item) {
        QFile *file = new QFile(fileName, res);
        QFileInfo fi(*file);
        if (!handleRequestedRange(fi.size())) {
            return true; // handled but with error
        }
        file->open(QIODevice::ReadOnly);
        qint64 size = fi.size();
        if (isRanged) {
            size = (requestedStart + requestedSize) > fi.size()? fi.size() - requestedStart : requestedSize;
            file->seek(requestedStart);
        }
        // TODO If-Modified-Since
        setupHeaders(fi.size(), item->metadata().value("type").toString(), fi.lastModified(), isRanged, requestedStart, size);
        connect(res, &qhttp::server::QHttpResponse::allBytesWritten, file, [res,file,requestedStart,size](){
            qint64 toWrite = requestedStart + size - file->pos();
            if (!toWrite) {
                return;
            }
            if (toWrite > HTTP_CHUNK)
                res->write(file->read(HTTP_CHUNK));
            else
                res->end(file->read(toWrite));
        });

        if (size < HTTP_CHUNK) {
            res->end(file->read(size));
        }
        return true; // handled with success
    }

    // So the sources is not cached. Try to download it.

    auto it = d->sources.find(sourceId);
    if (it == d->sources.end())
        return false; // really? maybe better 404 ?
    Private::Source &src = *it;

    if (!handleRequestedRange(src.file.hasSize()? qint64(src.file.size()) : -1)) {
        return true; // handled with error
    }

    if (isRanged && src.file.hasSize()) {
        if (requestedStart == 0 && requestedSize == qint64(src.file.size()))
            isRanged = false;
        else if (requestedStart + requestedSize > qint64(src.file.size()))
            requestedSize = qint64(src.file.size()) - requestedStart; // don't request more than declared in share
    }

    FileShareDownloader *downloader = downloadShare(acc, sourceId, isRanged, requestedStart, requestedSize);
    if (!downloader)
        return false; // REVIEW probably 404 would be better

    downloader->setParent(res);
    connect(downloader, &FileShareDownloader::metaDataChanged, this, [this, downloader, setupHeaders, res](){
        qint64 start;
        qint64 size;
        std::tie(start, size) = downloader->range();
        auto const file = downloader->jingleFile();
        setupHeaders(file.hasSize()? qint64(file.size()) : -1, file.mediaType(),
                     file.date(), downloader->isRanged(), start, size);

        bool *disconnected = new bool(false);
        connect(downloader, &FileShareDownloader::readyRead, res, [downloader, res]() {
            if (res->connection()->tcpSocket()->bytesToWrite() < HTTP_CHUNK)
                res->write(downloader->read(downloader->bytesAvailable() > HTTP_CHUNK? HTTP_CHUNK : downloader->bytesAvailable()));
        });

        connect(res->connection()->tcpSocket(), &QTcpSocket::bytesWritten, downloader, [downloader,res, disconnected](){
            if (res->connection()->tcpSocket()->bytesToWrite() >= HTTP_CHUNK)
                return; // we will return here when the buffer will be less occupied
            auto bytesAvail = downloader->bytesAvailable();
            if (!bytesAvail) return; // let's wait readyRead
            if (*disconnected && bytesAvail <= HTTP_CHUNK) {
                res->end(downloader->read(bytesAvail));
                delete disconnected;
            } else
                res->write(downloader->read(bytesAvail > HTTP_CHUNK? HTTP_CHUNK : bytesAvail));
        });

        connect(downloader, &FileShareDownloader::disconnected, res, [downloader, res, disconnected](){
            *disconnected = true;
            if (!res->connection()->tcpSocket()->bytesToWrite() && !downloader->bytesAvailable()) {
                res->disconnect(downloader);
                res->end();
                delete disconnected;
            }
        });
    });

    downloader->open();

    return true;
}
#endif

#ifndef WEBKIT
QByteArray FileSharingDeviceOpener::urlToSourceId(const QUrl &url)
{
    if (url.scheme() != QLatin1String("share"))
        return QByteArray();

    QString sourceId = url.path();
    if (sourceId.startsWith('/'))
        sourceId = sourceId.mid(1);
    return QByteArray::fromHex(sourceId.toLatin1());
}

QIODevice *FileSharingDeviceOpener::open(QUrl &url)
{
    QByteArray sourceId = urlToSourceId(url);
    if (sourceId.isEmpty())
        return nullptr;

    QString fileName;
    FileCacheItem *item = acc->psi()->fileSharingManager()->getCacheItem(sourceId, true, &fileName);
    if (item) {
        url = QUrl::fromLocalFile(fileName);
        return nullptr;
    }

#ifdef HAVE_WEBSERVER
    QUrl localServerUrl = acc->psi()->webServer()->serverUrl();
    QString path = localServerUrl.path();
    path.reserve(128);
    if (!path.endsWith('/'))
        path += '/';
    path += QString("psi/account/%1/sharedfile/%2").arg(acc->id(), QString::fromLatin1(sourceId.toHex()));
    localServerUrl.setPath(path);
    url = localServerUrl;
    return nullptr;
#else
# ifndef Q_OS_WIN
    QUrl simplelUrl = acc->psi()->fileSharingManager()->simpleSource(sourceId);
    if (simplelUrl.isValid()) {
        url = simplelUrl;
        return nullptr;
    }
# endif
    //return acc->psi()->networkAccessManager()->get(QNetworkRequest(QUrl("https://jabber.ru/upload/98354d3264f6584ef9520cc98641462d6906288f/mW6JnUCmCwOXPch1M3YeqSQUMzqjH9NjmeYuNIzz/file_example_OOG_1MG.ogg")));
    FileShareDownloader *downloader = acc->psi()->fileSharingManager()->downloadShare(acc, sourceId);
    if (!downloader)
        return nullptr;

    downloader->open(QIODevice::ReadOnly);
    return downloader;
#endif
}

void FileSharingDeviceOpener::close(QIODevice *dev)
{
    dev->close();
    dev->deleteLater();
}

QVariant FileSharingDeviceOpener::metadata(const QUrl &url)
{
    QByteArray sourceId = urlToSourceId(url);
    if (sourceId.isEmpty())
        return QVariant();

    FileCacheItem *item = acc->psi()->fileSharingManager()->getCacheItem(sourceId, true, nullptr);
    if (item) {
        QByteArray hg = item->metadata().value("histogram").toByteArray();
        if (hg.isEmpty())
            return QVariant();

        // so it looks like a voice message
        ITEAudioController::Histogram iteHg;
        iteHg.reserve(hg.size());
        for (quint8 b: hg) {
            iteHg.append(float(b) / 255.0);
        }

        QVariantMap vm;
        vm.insert(QString::fromLatin1("histogram"),
                  QVariant::fromValue<ITEAudioController::Histogram>(iteHg));
        return vm;
    }

    auto file = acc->psi()->fileSharingManager()->registeredSourceFile(sourceId);
    if (file.isValid() && file.audioHistogram().bars.count()) {
        auto as = file.audioHistogram();
        std::function<float(quint32)> normalizer;
        switch (as.coding) {
        case Jingle::FileTransfer::File::Histogram::U8:  normalizer = [](quint32 v){ return quint8(v) / 255.0f;                      }; break;
        case Jingle::FileTransfer::File::Histogram::S8:  normalizer = [](quint32 v){ return std::fabs(qint8(v) / 128.0f);            }; break;
        case Jingle::FileTransfer::File::Histogram::U16: normalizer = [](quint32 v){ return quint16(v) / float(1 << 16);             }; break;
        case Jingle::FileTransfer::File::Histogram::S16: normalizer = [](quint32 v){ return std::fabs(qint16(v) / float(1 << 15));   }; break;
        case Jingle::FileTransfer::File::Histogram::U32: normalizer = [](quint32 v){ return quint32(v) / float(quint64(1) << 32);           }; break;
        case Jingle::FileTransfer::File::Histogram::S32: normalizer = [](quint32 v){ return std::fabs(qint32(v) / float(quint64(1) << 31)); }; break;
        }
        if (normalizer) {
            ITEAudioController::Histogram ret;
            std::transform(as.bars.begin(), as.bars.end(), std::back_inserter(ret), normalizer);
            QVariantMap vm;
            vm.insert(QString::fromLatin1("histogram"),
                      QVariant::fromValue<ITEAudioController::Histogram>(ret));
            return vm;
        }
    }

    return QVariant();
}
#endif

#include "filesharingmanager.moc"
