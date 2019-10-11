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
#include "filesharingdownloader.h"
#include "fileutil.h"
#include "httpfileupload.h"
#include "networkaccessmanager.h"
#include "psiaccount.h"
#include "psicon.h"
#ifndef WEBKIT
#include "qiteaudio.h"
#endif

#include "xmpp_bitsofbinary.h"
#include "xmpp_client.h"
#include "xmpp_hash.h"
#include "xmpp_jid.h"
#include "xmpp_message.h"
#include "xmpp_reference.h"
#include "xmpp_vcard.h"
#ifdef HAVE_WEBSERVER
#include "qhttpserverconnection.hpp"
#include "webserver.h"
#endif
#include "messageview.h"
#include "textutil.h"

#include <QBuffer>
#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QImageReader>
#include <QMimeData>
#include <QMimeDatabase>
#include <QPainter>
#include <QPixmap>
#include <QTemporaryFile>
#include <QUrl>
#include <QUrlQuery>
#include <cmath>
#include <cstdlib>

#define HTTP_CHUNK (512 * 1024 * 1024)

// ======================================================================
// FileSharingManager
// ======================================================================
class FileSharingManager::Private {
public:
    FileCache *                          cache;
    QHash<XMPP::Hash, FileSharingItem *> items;

    void rememberItem(FileSharingItem *item)
    {
        Q_ASSERT(item->sums().size());
        for (auto const &v : item->sums())
            items.insert(v, item); // TODO ensure we don't overwrite
    }
};

FileSharingManager::FileSharingManager(QObject *parent) :
    QObject(parent),
    d(new Private)
{
    d->cache = new FileCache(cacheDir(), this);
}

FileSharingManager::~FileSharingManager()
{
}

QString FileSharingManager::cacheDir()
{
    QDir shares(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation) + "/shares");
    if (!shares.exists()) {
        QDir home(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation));
        home.mkdir("shares");
    }
    return shares.path();
}

FileCacheItem *FileSharingManager::cacheItem(const XMPP::Hash &id, bool reborn, QString *fileName_out)
{
    auto item = d->cache->get(id, reborn);
    if (!item)
        return nullptr;

    QString link     = item->metadata().value(QLatin1String("link")).toString();
    QString fileName = link.size() ? link : d->cache->cacheDir() + "/" + item->fileName();
    if (fileName.isEmpty() || !QFileInfo(fileName).isReadable()) {
        d->cache->remove(id);
        return nullptr;
    }
    if (fileName_out)
        *fileName_out = fileName;
    return item;
}

FileCacheItem *FileSharingManager::saveToCache(const XMPP::Hash &id, const QByteArray &data,
                                               const QVariantMap &metadata, unsigned int maxAge)
{
    return d->cache->append(id, data, metadata, maxAge);
}

FileCacheItem *FileSharingManager::moveToCache(const XMPP::Hash &id, const QFileInfo &file,
                                               const QVariantMap &metadata, unsigned int maxAge)
{
    return d->cache->moveToCache(id, file, metadata, maxAge);
}

FileSharingItem *FileSharingManager::item(const Hash &id)
{
    return d->items.value(id);
}

QList<FileSharingItem *> FileSharingManager::fromMimeData(const QMimeData *data, PsiAccount *acc)
{
    QList<FileSharingItem *> ret;
    QStringList              files;

    QString voiceMsgMime(QLatin1String("audio/ogg"));
    QString voiceAmplitudesMime(QLatin1String("application/x-psi-amplitudes"));

    bool hasVoice = data->hasFormat(voiceMsgMime) && data->hasFormat(voiceAmplitudesMime);
    if (!data->hasImage() && !hasVoice && data->hasUrls()) {
        for (auto const &url : data->urls()) {
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
            QByteArray  ba         = data->data(voiceMsgMime);
            QByteArray  amplitudes = data->data(voiceAmplitudesMime);
            QVariantMap vm;
            vm.insert("amplitudes", amplitudes);
            item = new FileSharingItem(voiceMsgMime, ba, vm, acc, this);
        } else {
            QImage img = data->imageData().value<QImage>();
            item       = new FileSharingItem(img, acc, this);
        }
        d->rememberItem(item);
        ret.append(item);
    } else {
        for (auto const &f : files) {
            auto item = new FileSharingItem(f, acc, this);
            d->rememberItem(item);
            ret.append(item);
        }
    }

    return ret;
}

QList<FileSharingItem *> FileSharingManager::fromFilesList(const QStringList &fileList, PsiAccount *acc)
{
    QList<FileSharingItem *> ret;
    if (fileList.isEmpty())
        return ret;
    foreach (const QString &file, fileList) {
        QFileInfo fi(file);
        if (fi.isFile() && fi.isReadable()) {
            auto item = new FileSharingItem(file, acc, this);
            d->rememberItem(item);
            ret << item;
        }
    }
    return ret;
}

void FileSharingManager::fillMessageView(MessageView &mv, const Message &m, PsiAccount *acc)
{
    auto refs = m.references();
    if (refs.count()) {
        QString tailReferences;
        QString desc = m.body();
        QString htmlDesc;
        htmlDesc.reserve(desc.size() + refs.count() * 64);

        std::sort(refs.begin(), refs.end(), [](auto &a, auto &b) { return a.begin() < b.begin(); });

        int lastEnd = 0;
        for (auto const &r : m.references()) {
            MediaSharing ms = r.mediaSharing();
            if (!ms.isValid() || !ms.file.mediaType().startsWith(QLatin1String("audio")) || !ms.file.hash().isValid() || !ms.file.hasSize()) { // only audio is supported for now
                continue;
            }
            auto item = new FileSharingItem(ms, m.from(), acc, this);
            d->rememberItem(item);
            mv.addReference(item);

            QString shareStr(QString::fromLatin1("<share id=\"%1\"/>").arg(item->sums().cbegin().value().toString()));
            if (r.begin() != -1 && r.begin() >= lastEnd && QUrl(desc.mid(r.begin(), r.end() - r.begin() + 1).trimmed()).isValid()) {
                htmlDesc += TextUtil::linkify(TextUtil::plain2rich(desc.mid(lastEnd, r.begin() - lastEnd))); // something before link
                htmlDesc += shareStr;                                                                        // something instead of link
                lastEnd = r.end() + 1;
            } else {
                tailReferences += shareStr;
            }
        }
        if (lastEnd < desc.size()) {
            htmlDesc += TextUtil::linkify(TextUtil::plain2rich(desc.mid(lastEnd, desc.size() - lastEnd)));
        }
        htmlDesc += tailReferences;
        mv.setHtml(htmlDesc);
    }
}

bool FileSharingManager::jingleAutoAcceptIncomingDownloadRequest(Jingle::Session *session)
{
    QList<QPair<Jingle::FileTransfer::Application *, FileCacheItem *>> toAccept;
    // check if we can accept the session immediately w/o user interaction
    for (auto const &a : session->contentList()) {
        auto ft = static_cast<Jingle::FileTransfer::Application *>(a);
        if (a->senders() == Jingle::Origin::Initiator)
            return false;

        Hash h = ft->file().hash(Hash::Sha1);
        if (!h.isValid()) {
            h = ft->file().hash();
        }
        if (!h.isValid() || h.data().isEmpty())
            return false;

        FileCacheItem *item = cacheItem(h, true);
        if (!item)
            return false;

        toAccept.append(qMakePair(ft, item));
    }

    for (auto const &p : toAccept) {
        auto ft   = p.first;
        auto item = p.second;

        connect(ft, &Jingle::FileTransfer::Application::deviceRequested, this, [ft, item](quint64 offset, quint64 /*size*/) {
            auto    vm       = item->metadata();
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

#ifdef HAVE_WEBSERVER
// returns <parsed,list of start/size>
static std::tuple<bool, QList<QPair<qint64, qint64>>> parseHttpRangeRequest(qhttp::server::QHttpRequest * req,
                                                                            qhttp::server::QHttpResponse *res,
                                                                            bool                          fullSizeKnown = false,
                                                                            qint64                        fullSize      = 0)
{
    QList<QPair<qint64, qint64>> ret;
    QByteArray                   rangesBa = req->headers().value("range");
    if (!rangesBa.size())
        return std::make_tuple(true, ret);

    if ((fullSizeKnown && !fullSize) || !rangesBa.startsWith("bytes=")) {
        res->setStatusCode(qhttp::ESTATUS_REQUESTED_RANGE_NOT_SATISFIABLE);
        return std::make_tuple(false, ret);
    }

    QList<QByteArray> arr = QByteArray::fromRawData(rangesBa.data() + sizeof("bytes"),
                                                    rangesBa.size() - int(sizeof("bytes")))
                                .split(',');

    for (const auto &ba : arr) {
        auto trab = ba.trimmed();
        if (!trab.size()) {
            res->setStatusCode(qhttp::ESTATUS_BAD_REQUEST);
            return std::make_tuple(false, ret);
        }
        bool   ok;
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
                                             qhttp::server::QHttpRequest *req, qhttp::server::QHttpResponse *res)
{
    qint64     requestedStart = 0;
    qint64     requestedSize  = 0;
    bool       isRanged       = false;
    XMPP::Hash sourceId       = XMPP::Hash::from(QStringRef(&sourceIdHex));

    auto handleRequestedRange = [&](qint64 fileSize = -1) {
        QList<QPair<qint64, qint64>> ranges;
        bool                         parsed;
        std::tie(parsed, ranges) = parseHttpRangeRequest(req, res, fileSize != -1, fileSize);
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
            requestedSize  = ranges[0].second;
            isRanged       = true;
        }
        return true;
    };

    auto setupHeaders = [res](
                            qint64    fileSize,
                            QString   contentType,
                            QDateTime lastModified,
                            bool      isRanged,
                            qint64    start,
                            qint64    size) {
        if (lastModified.isValid())
            res->addHeader("Last-Modified", lastModified.toString(Qt::RFC2822Date).toLatin1());
        if (contentType.count())
            res->addHeader("Content-Type", contentType.toLatin1());

        res->addHeader("Accept-Ranges", "bytes");
        res->addHeader("connection", "keep-alive");
        if (isRanged) {
            auto range = QString(QLatin1String("bytes %1-%2/%3")).arg(start).arg(start + size - 1).arg(fileSize == -1 ? QString('*') : QString::number(fileSize));
            res->addHeader("Content-Range", range.toLatin1());
            res->setStatusCode(qhttp::ESTATUS_PARTIAL_CONTENT);
        } else {
            if (fileSize != -1)
                res->addHeader("Content-Length", QByteArray::number(fileSize));
            res->setStatusCode(qhttp::ESTATUS_OK);
        }
    };

    auto item = acc->psi()->fileSharingManager()->item(sourceId);
    //FileCacheItem *item = acc->psi()->fileSharingManager()->getCacheItem(sourceId, true, &fileName);
    if (item->isCached()) {
        QFile *   file = new QFile(acc->psi()->fileSharingManager()->cacheDir() + "/" + item->fileName(), res);
        QFileInfo fi(*file);
        if (!handleRequestedRange(fi.size())) {
            return true; // handled but with error
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
        connect(res, &qhttp::server::QHttpResponse::allBytesWritten, file, [res, file, requestedStart, size]() {
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

    if (!handleRequestedRange(item->isSizeKnown() ? item->fileSize() : -1)) {
        return true; // handled with error
    }

    if (isRanged && item->isSizeKnown()) {
        if (requestedStart == 0 && requestedSize == item->fileSize())
            isRanged = false;
        else if (requestedStart + requestedSize > item->fileSize())
            requestedSize = item->fileSize() - requestedStart; // don't request more than declared in share
    }

    FileShareDownloader *downloader = item->download(isRanged, requestedStart, requestedSize);
    if (!downloader)
        return false; // REVIEW probably 404 would be better

    downloader->setParent(res);
    connect(downloader, &FileShareDownloader::metaDataChanged, this, [this, downloader, setupHeaders, res]() {
        qint64 start;
        qint64 size;
        std::tie(start, size) = downloader->range();
        auto const file       = downloader->jingleFile();
        setupHeaders(file.hasSize() ? qint64(file.size()) : -1, file.mediaType(),
                     file.date(), downloader->isRanged(), start, size);
        res->setProperty("headers", true);

        bool *disconnected = new bool(false);
        connect(downloader, &FileShareDownloader::readyRead, res, [downloader, res]() {
            if (res->connection()->tcpSocket()->bytesToWrite() < HTTP_CHUNK)
                res->write(downloader->read(downloader->bytesAvailable() > HTTP_CHUNK ? HTTP_CHUNK : downloader->bytesAvailable()));
        });

        connect(res->connection()->tcpSocket(), &QTcpSocket::bytesWritten, downloader, [downloader, res, disconnected]() {
            if (res->connection()->tcpSocket()->bytesToWrite() >= HTTP_CHUNK)
                return; // we will return here when the buffer will be less occupied
            auto bytesAvail = downloader->bytesAvailable();
            if (!bytesAvail)
                return; // let's wait readyRead
            if (*disconnected && bytesAvail <= HTTP_CHUNK) {
                res->end(downloader->read(bytesAvail));
                delete disconnected;
            } else
                res->write(downloader->read(bytesAvail > HTTP_CHUNK ? HTTP_CHUNK : bytesAvail));
        });

        connect(downloader, &FileShareDownloader::disconnected, res, [downloader, res, disconnected]() {
            *disconnected = true;
            if (!res->connection()->tcpSocket()->bytesToWrite() && !downloader->bytesAvailable()) {
                res->disconnect(downloader);
                res->end();
                delete disconnected;
            }
        });
    });

    connect(downloader, &FileShareDownloader::finished, this, [this, downloader, setupHeaders, res]() {
        if (!res->property("headers").toBool()) {
            res->setStatusCode(qhttp::ESTATUS_BAD_GATEWAY); // something finnished with errors quite early
            res->end();
        }
    });

    downloader->open();

    return true;
}
#endif

XMPP::Hash FileSharingDeviceOpener::urlToSourceId(const QUrl &url)
{
    if (url.scheme() != QLatin1String("share"))
        return XMPP::Hash();

    QString    path = url.path();
    QStringRef sourceId(&path);
    if (sourceId.startsWith('/'))
        sourceId = sourceId.mid(1);
    return XMPP::Hash::from(sourceId);
}

QIODevice *FileSharingDeviceOpener::open(QUrl &url)
{
    auto sourceId = urlToSourceId(url);
    if (!sourceId.isValid())
        return nullptr;

    auto             fsm  = acc->psi()->fileSharingManager();
    FileSharingItem *item = fsm->item(sourceId);
    if (!item)
        return nullptr;

    if (item->isCached()) {
        url = QUrl::fromLocalFile(fsm->cacheDir() + "/" + item->cache()->fileName());
        return nullptr;
    }

#ifdef HAVE_WEBSERVER
    QUrl    localServerUrl = acc->psi()->webServer()->serverUrl();
    QString path           = localServerUrl.path();
    path.reserve(128);
    if (!path.endsWith('/'))
        path += '/';
    path += QString("psi/account/%1/sharedfile/%2").arg(acc->id(), sourceId.toString());
    localServerUrl.setPath(path);
    url = localServerUrl;
    return nullptr;
#else
#ifndef Q_OS_WIN
    QUrl simplelUrl = item->simpleSource();
    if (simplelUrl.isValid()) {
        url = simplelUrl;
        return nullptr;
    }
#endif
    //return acc->psi()->networkAccessManager()->get(QNetworkRequest(QUrl("https://jabber.ru/upload/98354d3264f6584ef9520cc98641462d6906288f/mW6JnUCmCwOXPch1M3YeqSQUMzqjH9NjmeYuNIzz/file_example_OOG_1MG.ogg")));
    FileShareDownloader *downloader = item->download();
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
    auto sourceId = urlToSourceId(url);
    auto item     = acc->psi()->fileSharingManager()->item(sourceId);
    if (item)
        return item->metaData();
    return QVariant();
}

FileSharingItem *FileSharingDeviceOpener::sharedItem(const QString &id)
{
    return acc->psi()->fileSharingManager()->item(XMPP::Hash::from(QStringRef(&id)));
}
