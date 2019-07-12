/*
 * filesharingmanager.cpp - file sharing manager
 * Copyright (C) 2019 Sergey Ilinykh
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

#include "applicationinfo.h"
#include "filesharingmanager.h"
#include "psiaccount.h"
#include "xmpp_vcard.h"
#include "xmpp_message.h"
#include "xmpp_client.h"
#include "xmpp_reference.h"
#include "xmpp_hash.h"
#include "xmpp_jid.h"
#include "httpfileupload.h"
#include "filecache.h"
#include "fileutil.h"
#include "psicon.h"
#include "networkaccessmanager.h"
#include "xmpp_bitsofbinary.h"
#include "jidutil.h"
#include "userlist.h"
#ifdef HAVE_WEBSERVER
# include "webserver.h"
# include "qhttpserverconnection.hpp"
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
        size = arr[1].toLongLong(&ok) - start + 1;
    }
    if (!ok || size < 0)
        return std::tuple<bool,qint64,qint64>(false, 0, 0);
    return std::tuple<bool,qint64,qint64>(true, start, size);
}

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
    QString lastError;
    QString dstFileName;
    qint64 rangeStart = 0;
    qint64 rangeSize = 0;
    bool metaReady = false;
    bool success = false;

    void startNextDownloader()
    {
        if (uris.isEmpty()) {
            success = false;
            if (lastError.isEmpty())
                lastError = tr("Download sources are not given");
            emit q->finished();
        }
        QString uri = uris.takeLast();
        auto type = FileSharingManager::sourceType(uri);
        switch (type) {
        case FileSharingManager::SourceType::HTTP:
        case FileSharingManager::SourceType::FTP:
            downloadNetworkManager(uri);
            break;
        case FileSharingManager::SourceType::BOB:
            downloadBOB(uri);
            break;
        case FileSharingManager::SourceType::Jingle:
            downloadJingle(uri);
            break;
        default:
            break;
        }
    }

    void setupTempFile()
    {
        if (!tmpFile) {
            QString tmpFN = QString::fromLatin1("dl-") + dstFileName;
            tmpFN = QString("%1/%2").arg(ApplicationInfo::documentsDir(), tmpFN);
            tmpFile.reset(new QFile(tmpFN));
            if (!tmpFile->open(QIODevice::ReadWrite | QIODevice::Truncate)) { // TODO complete unfinished downloads
                lastError = tmpFile->errorString();
                tmpFile.reset();
            }
        }
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

    void downloadNetworkManager(const QString &uri)
    {
        QNetworkRequest req = QNetworkRequest(QUrl(uri));
        if (q->isRanged())
            req.setRawHeader("Range", QString("bytes=%1-%2").arg(QString::number(rangeStart), rangeSize?
                                                                 QString::number(rangeStart + rangeSize - 1) :
                                                                     QString()).toLatin1());
#if QT_VERSION >= QT_VERSION_CHECK(5,9,0)
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#elif QT_VERSION >= QT_VERSION_CHECK(5,6,0)
        req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
        QNetworkReply *reply = acc->psi()->networkAccessManager()->get(req);
        connect(reply, &QNetworkReply::metaDataChanged, this, [this,reply](){
            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == 206) {
                QByteArray ba = reply->rawHeader("Content-Range");
                bool parsed;
                std::tie(parsed, rangeStart, rangeSize) = parseHttpRangeResponse(ba);
                if (ba.size() && !parsed) {
                    lastError = QLatin1String("Invalid HTTP response range");
                    reply->disconnect(this);
                    reply->deleteLater();
                    startNextDownloader();
                    return;
                }
            } else if (status != 200 && status != 203) {
                rangeStart = 0;
                rangeSize = 0; // make it not-ranged
                lastError = tr("Unexpected HTTP status") + QString(": %1").arg(status);
                reply->disconnect(this);
                reply->deleteLater();
                startNextDownloader();
                return;
            }
            metaReady = true;
            emit q->metaDataChanged();
        });

        connect(reply, &QNetworkReply::readyRead, this, [this,reply](){
            setupTempFile();
            if (!tmpFile) {
                reply->disconnect(this);
                reply->deleteLater();
                startNextDownloader();
                return;
            }
            QByteArray ba = reply->read(reply->bytesAvailable());
            if (tmpFile->write(ba) != ba.size()) { // file engine tries to write everything until it's system error
                lastError = tmpFile->errorString();
                tmpFile.reset();
                reply->disconnect(this);
                reply->deleteLater();
                startNextDownloader();
                return;
            }
            if (reply->isFinished()) {
                // seems like last piece of data was written. we are ready to finish
                reply->disconnect(this);
                reply->deleteLater();
                downloadFinished();
            }
        });

        connect(reply, &QNetworkReply::downloadProgress, this, [this,reply](qint64 bytesReceived, qint64 bytesTotal){
            emit q->progress(bytesReceived, bytesTotal);
        });

        connect(reply, &QNetworkReply::finished, this, [this,reply](){
            if (reply->error() != QNetworkReply::NoError) {
                lastError = reply->errorString();
                tmpFile.reset();
                reply->deleteLater();
                if (metaReady)
                    emit q->finished();
                else
                    startNextDownloader();
                return;
            }
        });
    }

    void downloadBOB(const QString &uri)
    {
        Jid j = selectOnlineJid(jids);
        if (!j.isValid()) {
            lastError = tr("\"Bits Of Binary\" data source is offline");
            startNextDownloader();
            return;
        }
        acc->loadBob(jids.first(), uri.mid(4), this, [this](bool success, const QByteArray &data, const QByteArray &/* media type */) {
            if (!success) {
                lastError = tr("Download using \"Bits Of Binary\" failed"); // make translatable?
                startNextDownloader();
                return;
            }
            if (q->isRanged()) { // there is not such a thing like ranged bob
                rangeStart = 0;
                rangeSize = 0; // make it not-ranged
            }
            metaReady = true;
            emit q->metaDataChanged();
            emit q->progress(data.size(), file.size() > size_t(data.size())? file.size() : data.size());
            setupTempFile();
            if (!tmpFile) {
                startNextDownloader();
                return;
            }
            tmpFile->write(data);
            downloadFinished();
        });
    }

    void downloadJingle(const QString &sourceUri)
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
            lastError = tr("Jingle data source is offline");
            startNextDownloader();
            return;
        }

        // query
        QUrlQuery uri;
        uri.setQueryDelimiters('=', ';');
        uri.setQuery(uriToOpen.query(QUrl::FullyEncoded));

        QString querytype = uri.queryItems().value(0).first;

        if (querytype != QStringLiteral("jingle-ft")) {
            lastError = tr("Invalid Jingle-FT URI");
            startNextDownloader();
            return;
        }

        Jingle::Session *session = acc->client()->jingleManager()->newSession(dataSource);
        auto app = static_cast<Jingle::FileTransfer::Application*>(session->newContent(Jingle::FileTransfer::NS, Jingle::Origin::Responder));
        if (!app) {
            lastError = QString::fromLatin1("Jingle file transfer is disabled");
            startNextDownloader();
            return;
        }
        if (q->isRanged())
            file.setRange(XMPP::Jingle::FileTransfer::Range(quint64(rangeStart), quint64(rangeSize)));
        app->setFile(file);
        session->addContent(app);

        connect(session, &Jingle::Session::newContentReceived, this, [session, this](){
            // we don't expect any new content on the session
            lastError = tr("Unexpected incoming content");
            session->terminate(Jingle::Reason::Condition::Decline, lastError);
        });

        connect(app, &Jingle::FileTransfer::Application::deviceRequested, this, [app, this](quint64 offset, quint64 size){
            rangeStart = offset;
            rangeSize = size;
            setupTempFile();
            if (!tmpFile) {
                app->setDevice(nullptr);
                return;
            }
            app->setDevice(tmpFile.data());
            metaReady = true;
            emit q->metaDataChanged();
        });

        connect(session, &Jingle::Session::terminated, this, [this, app](){
            auto r = app->terminationReason();
            if (r.isValid() && r.condition() == Jingle::Reason::Success) {
                downloadFinished();
                return;
            }
            lastError = tr("Jingle download failed");
            if (metaReady)
                emit q->finished();
            else
                startNextDownloader();
        });

        connect(app, &Jingle::FileTransfer::Application::progress, this, [this](quint64 offset){
            emit q->progress(offset, file.size());
        });

    }

    void downloadFinished()
    {
        lastError.clear();
        tmpFile->flush();
        tmpFile->seek(0);
        tmpFile->rename(QString("%1/%2").arg(ApplicationInfo::documentsDir(),dstFileName));
        success = true;
        emit q->finished();
    }
};

FileShareDownloader::FileShareDownloader(PsiAccount *acc, const QString &sourceId, const Jingle::FileTransfer::File &file,
                                         const QList<Jid> &jids, const QStringList &uris, FileSharingManager *manager) :
    QObject(manager),
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

}

bool FileShareDownloader::isSuccess() const
{
    return d->success;
}

void FileShareDownloader::start()
{
    if (!d->uris.size()) {
        d->success = false;
        emit finished();
        return;
    }
    auto docLoc = ApplicationInfo::documentsDir();
    QDir docDir(docLoc);
    QString fn = FileUtil::cleanFileName(d->file.name()).split('/').last();
    if (fn.isEmpty()) {
        fn = d->sourceId;
    }
    int index = 1;
    while (docDir.exists(fn)) {
        QFileInfo fi(fn);
        fn = fi.completeBaseName();
        auto suf = fi.completeSuffix();
        if (suf.size()) {
            fn = QString("%1-%2.%3").arg(fn, QString::number(index), suf);
        }
    }

    d->dstFileName = fn;
    d->startNextDownloader();
}

void FileShareDownloader::abort()
{
    // TODO
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
    if (d->success && d->tmpFile) {
        return d->tmpFile->fileName();
    }
    return QString();
}

const Jingle::FileTransfer::File &FileShareDownloader::jingleFile() const
{
    return d->file;
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
    sha1hash = QString::fromLatin1(QCryptographicHash::hash(ba, QCryptographicHash::Sha1).toHex());
    mimeType = QString::fromLatin1("image/png");
    _fileSize = ba.size();

    cache = manager->getCacheItem(sha1hash, true);
    if (cache) {
        _fileName = cache->fileName();
    } else {
        QTemporaryFile file(QDir::tempPath() + QString::fromLatin1("/psishare"));
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
    _fileSize = file.size();
    file.open(QIODevice::ReadOnly);
    hash.addData(&file);
    sha1hash = QString::fromLatin1(hash.result().toHex());
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
    sha1hash = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex());
    mimeType = mime;
    _fileSize = data.size();

    cache = manager->getCacheItem(sha1hash, true);
    if (cache) {
        _fileName = cache->fileName();
    } else {
        QTemporaryFile file(QDir::tempPath() + QString::fromLatin1("/psishare"));
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
        _fileSize = QFileInfo(_fileName).size();
    }

    sha1hash = cache->id();
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
    if (!finishNotified && httpFinished && jingleFinished) {
        QVariantMap meta = metaData;
        meta["type"] = mimeType;
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

    QSize thumbSize(64,64);
    auto thumbPix = thumbnail(thumbSize).pixmap(thumbSize);
    QByteArray pixData;
    QBuffer buf(&pixData);
    thumbPix.save(&buf, "PNG");
    QString png(QString::fromLatin1("image/png"));
    auto bob = acc->client()->bobManager()->append(pixData, png, isTempFile? TEMP_TTL: FILE_TTL);
    Thumbnail thumb(QByteArray(), png, thumbSize.width(), thumbSize.height());
    thumb.uri = QLatin1String("cid:") + bob.cid();

    QFile file(_fileName);
    file.open(QIODevice::ReadOnly);
    Hash hash(Hash::Sha1);
    hash.computeFromDevice(&file);
    file.close();

    Jingle::FileTransfer::File jfile;
    QFileInfo fi(_fileName);
    jfile.setDate(fi.lastModified());
    jfile.addHash(hash);
    jfile.setName(fi.fileName());
    jfile.setSize(fi.size());
    jfile.setMediaType(mimeType);
    jfile.setThumbnail(thumb);
    jfile.setDescription(_description);

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
    if (isTempFile) {
        auto ext = FileUtil::mimeToFileExt(mimeType);
        return QString("psi-%1.%2").arg(sha1hash, ext);
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
        auto hfu = hm->upload(_fileName, displayName(), mimeType);
        hfu->setParent(this);
        connect(hfu, &HttpFileUpload::progress, this, [this](qint64 bytesReceived, qint64 bytesTotal){
            Q_UNUSED(bytesTotal)
            emit publishProgress(bytesReceived);
        });
        connect(hfu, &HttpFileUpload::finished, this, [hfu, this]() {
            httpFinished = true;
            if (hfu->success()) {
                readyUris.append(hfu->getHttpSlot().get.url);
            }
            checkFinished();
        });
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
    QHash<QString,Source> sources;
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

FileCacheItem *FileSharingManager::getCacheItem(const QString &id, bool reborn, QString *fileName_out)
{
    auto item = d->cache->get(id, reborn);
    if (!item)
        return nullptr;

    QString link = item->metadata().value(QLatin1String("link")).toString();
    QString fileName = link.size()? link : item->fileName();
    if (fileName.isEmpty() || !QFile(fileName).isReadable()) {
        d->cache->remove(id);
        return nullptr;
    }
    if (fileName_out)
        *fileName_out = fileName;
    return item;
}

FileCacheItem * FileSharingManager::saveToCache(const QString &id, const QByteArray &data,
                                                const QVariantMap &metadata, unsigned int maxAge)
{
    return d->cache->append(id, data, metadata, maxAge);
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

QString FileSharingManager::registerSource(const Jingle::FileTransfer::File &file, const Jid &source, const QStringList &uris)
{
    Hash h = file.hash(Hash::Sha1);
    if (!h.isValid()) {
        h = file.hash();
    }
    QString shareId = QString::fromLatin1(h.data().toHex());
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
QUrl FileSharingManager::simpleSource(const QString &sourceId) const
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


FileShareDownloader* FileSharingManager::downloadShare(PsiAccount *acc, const QString &sourceId,
                                                       bool isRanged, qint64 start, qint64 size)
{
    auto it = d->sources.find(sourceId);
    if (it == d->sources.end())
        return nullptr;

    Private::Source &src = *it;
    if (isRanged && src.file.hasSize() && start == 0 && size == qint64(src.file.size()))
        isRanged = false;

    FileShareDownloader *downloader = src.downloader;
    if (!src.downloader || isRanged) {
        downloader = new FileShareDownloader(acc, sourceId, src.file, src.jids, src.uris, this);

        if (!isRanged) {// so src.downloader wasn't set but has to now
            src.downloader = downloader;
            connect(downloader, &FileShareDownloader::finished, this, [this,sourceId](){
                auto it = d->sources.find(sourceId);
                if (it == d->sources.end())
                    return;
                Private::Source &src = *it;
                src.downloader->disconnect(this);
                src.downloader->deleteLater();

                if (src.downloader->isSuccess()) {
                    QFile tmpFile(src.downloader->fileName());
                    tmpFile.open(QIODevice::ReadOnly);
                    auto h = src.downloader->jingleFile().hash(Hash::Sha1);
                    QString sha1hash;
                    if (!h.isValid() || h.data().isEmpty()) {
                        // no sha1 hash
                        Hash h(Hash::Sha1);
                        if (h.computeFromDevice(&tmpFile)) {
                            sha1hash = QString::fromLatin1(h.data().toHex());
                        }
                    } else {
                        sha1hash = QString::fromLatin1(h.data().toHex());
                    }
                    tmpFile.close();

                    QVariantMap vm;
                    vm.insert(QString::fromLatin1("type"), src.file.mediaType());
                    vm.insert(QString::fromLatin1("uris"), src.uris);
                    vm.insert(QString::fromLatin1("link"), QFileInfo(src.downloader->fileName()).absoluteFilePath());
                    auto thumb = src.file.thumbnail();
                    if (thumb.isValid())
                        vm.insert(QString::fromLatin1("thumbnail"), thumb.uri);
                    if (src.file.audioSpectrum().bars.count()) {
                        auto s = src.file.audioSpectrum();
                        QStringList sl;
                        sl.reserve(s.bars.count());
                        for (auto const &v: s.bars) sl.append(QString::number(v));
                        vm.insert(QString::fromLatin1("spectrum"), sl.join(','));
                        vm.insert(QString::fromLatin1("spectrum_coding"), s.coding);
                    }

                    saveToCache(sha1hash, QByteArray(), vm, FILE_TTL);
                    d->sources.erase(it); // now it's in cache so the source is not needed anymore
                } else {
                    src.downloader = nullptr;
                }
            });
            connect(downloader, &FileShareDownloader::destroyed, this, [this,sourceId](){
                auto it = d->sources.find(sourceId);
                if (it == d->sources.end())
                    return;
                it->downloader = nullptr;
            });

        } else
            downloader->setRange(start, size);
    }

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

        FileCacheItem *item = getCacheItem(h.data().toHex(), true);
        if (!item)
            return false;

        toAccept.append(qMakePair(ft,item));
    }

    for (auto const &p: toAccept) {
        auto ft = p.first;
        auto item = p.second;

        connect(ft, &Jingle::FileTransfer::Application::deviceRequested, this, [this,ft,item](quint64 offset, quint64 /*size*/){
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
            f->seek(offset);
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

class MediaDevice : public QIODevice
{
    Q_OBJECT
    QFile *file = nullptr;
    FileShareDownloader *downloader;
public:
    MediaDevice(FileShareDownloader *downloader, QObject *parent) :
        QObject(parent),
        downloader(downloader)
    {
        downloader->setParent(this);
        connect(downloader, &FileShareDownloader::finished, this, [this,downloader](){
            if (downloader->isSuccess() && isOpen()) {
                file = new QFile(downloader->fileName(), this);
                file->open(QIODevice::ReadOnly);
            }
            downloader->deleteLater();
            this->downloader = nullptr;
        }, Qt::QueuedConnection);
    }

    void close()
    {
        if (file)
            file->close();
        if (downloader)
            downloader->abort();
        QIODevice::close();
    }

    bool isSequential() const { return true; }

    qint64 bytesAvailable() const
    {
        if (file) {
            return file->size() - file->pos();
        }
        return 0;
    }

signals:
    void disconnected();

protected:
    qint64 readData(char *data, qint64 maxSize)
    {
        qint64 ret = file->read(data, maxSize);
        if (file->size() == file->pos())
            QTimer::singleShot(0, this, &MediaDevice::disconnected);
        else
            QTimer::singleShot(0, this, &MediaDevice::readyRead);
        return ret;
    }

    qint64 writeData(const char *data, qint64 maxSize)
    {
        Q_UNUSED(data)
        Q_UNUSED(maxSize)
        return 0;  // it's a readonly device
    }
};

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
                                                    rangesBa.size() - sizeof("bytes")).split(',');

    for (const auto &ba: arr) {
        auto trab = ba.trimmed();
        if (!trab.size()) {
            res->setStatusCode(qhttp::ESTATUS_BAD_REQUEST);
            return std::make_tuple(false, ret);
        }
        bool ok;
        qint64 start;
        qint64 end;
        if (trab[0] = '-') {
            res->setStatusCode(qhttp::ESTATUS_NOT_IMPLEMENTED);
            return std::make_tuple(false, ret);
        }

        auto l = trab.split('-');
        if (l.size() != 2) {
            res->setStatusCode(qhttp::ESTATUS_BAD_REQUEST);
            return std::make_tuple(false, ret);
        }

        start = l[0].toLongLong(&ok);
        if (ok && l[1].size())
            end = l[1].toLongLong(&ok);
        if (!ok || start > end) {
            res->setStatusCode(qhttp::ESTATUS_BAD_REQUEST);
            return std::make_tuple(false, ret);
        }

        if (!fullSizeKnown || start < fullSize) {
            ret.append(qMakePair(start, end - start + 1));
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
bool FileSharingManager::downloadHttpRequest(PsiAccount *acc, const QString &sourceId,
                                             qhttp::server::QHttpRequest* req, qhttp::server::QHttpResponse *res)
{
    qint64 requestedStart = 0;
    qint64 requestedSize = 0;
    bool isRanged = false;

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

    auto setupHeaders = [req,res](
            qint64 fileSize,
            QString contentType,
            QDateTime lastModified,
            bool isRanged,
            qint64 start,
            qint64 size
    ) -> bool
    {

        if (lastModified.isValid())
            res->addHeader("Last-Modified", lastModified.toString(Qt::RFC2822Date).toLatin1());
        if (contentType.count())
            res->addHeader("Content-Type", contentType.toLatin1());

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
        return true;
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
        if (!setupHeaders(fi.size(), item->metadata().value("type").toString(), fi.lastModified(), isRanged, requestedStart, size))
            return true; // handled but with error

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

    if (!handleRequestedRange(src.file.hasSize()? src.file.size() : -1)) {
        return true; // handled with error
    }

    if (isRanged && src.file.hasSize()) {
        if (requestedStart == 0 && requestedSize == qint64(src.file.size()))
            isRanged = false;
        else if (requestedStart + requestedSize > qint64(src.file.size()))
            requestedSize = src.file.size() - requestedStart; // don't request more than declared in share
    }

    FileShareDownloader *downloader = downloadShare(acc, sourceId, isRanged, requestedStart, requestedSize);
    if (!downloader)
        return false; // REVIEW probably 404 would be better

    downloader->setParent(res);
    connect(downloader, &FileShareDownloader::metaDataChanged, this, [downloader, setupHeaders, res](){
        qint64 start;
        qint64 size;
        std::tie(start, size) = downloader->range();
        auto const file = downloader->jingleFile();
        if (!setupHeaders(file.hasSize()? file.size() : -1, file.mediaType(),
                          file.date(), downloader->isRanged(), start, size)) {
            return;
        }

        // TODO decide about parents
        auto md = new MediaDevice(downloader, res);
        md->open(QIODevice::ReadOnly);

        connect(md, &MediaDevice::readyRead, res, [md, res]() {
            if (res->connection()->tcpSocket()->bytesToWrite() < HTTP_CHUNK)
                res->write(md->read(md->bytesAvailable() > HTTP_CHUNK? HTTP_CHUNK : md->bytesAvailable()));
        });

        connect(res, &qhttp::server::QHttpResponse::allBytesWritten, md, [md,res](){
            auto bytesAvail = md->bytesAvailable();
            if (!bytesAvail) return; // let's wait readyRead
            res->write(md->read(bytesAvail > HTTP_CHUNK? HTTP_CHUNK : bytesAvail));
        });

        connect(md, &MediaDevice::disconnected, res, [md, res](){

        });
    });

    return true;
}
#endif

#ifndef WEBKIT
QIODevice *FileSharingDeviceOpener::open(QUrl &url)
{
    if (url.scheme() != QLatin1String("share"))
        return nullptr;

    QString sourceId = url.path();
    if (sourceId.startsWith('/'))
        sourceId = sourceId.mid(1);

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
    path += QString("%1/shareddile/%2").arg(acc->id(), sourceId);
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

    auto md = new MediaDevice(downloader);
    md->open(QIODevice::ReadOnly);
    return md;
#endif
}

void FileSharingDeviceOpener::close(QIODevice *dev)
{
    dev->close();
    dev->deleteLater();
}
#endif

#include "filesharingmanager.moc"
