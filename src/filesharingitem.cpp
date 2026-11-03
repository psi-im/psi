/*
 * filesharingitem.h - shared file
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

#include "filesharingitem.h"
#include "filecache.h"
#include "filesharingchunksource.h"
#include "filesharinghashjob.h"
#include "filesharingmanager.h"
#include "filesharingpublication.h"
#include "fileutil.h"
#include "iris/xmpp_client.h"
#include "iris/xmpp_message.h"
#include "iris/xmpp_reference.h"
#include "iris/xmpp_thumbs.h"
#include "mediachunkcache.h"
#include "networkaccessmanager.h"
#include "psiaccount.h"
#include "psicon.h"
#include "userlist.h"

#include <QBuffer>
#include <QDir>
#include <QFileIconProvider>
#include <QImageReader>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QTemporaryFile>
#include <QUrl>

#include <algorithm>
#include <utility>

#define TEMP_TTL (7 * 24 * 3600)
#define FILE_TTL (365 * 24 * 3600)

using namespace XMPP;

// ======================================================================
// FileSharingItem
// ======================================================================
FileSharingItem::FileSharingItem(FileCacheItem *cache, PsiAccount *acc, FileSharingManager *manager) :
    QObject(manager), _acc(acc), _manager(manager)
{
    initFromCache(cache);
}

FileSharingItem::FileSharingItem(const MediaSharing &ms, const Jid &from, PsiAccount *acc,
                                 FileSharingManager *manager) :
    QObject(manager), _acc(acc), _manager(manager), _fileType(FileType::RemoteFile)
{
    _sums = ms.file.computedHashes();
    _receivedMetadata = ms.file;
    if (initFromCache()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        auto tmp  = ms.sources + uris();
        auto tmp2 = QSet<QString>(tmp.begin(), tmp.end());
        _uris     = QStringList(tmp2.begin(), tmp2.end());
#else
        _uris = (ms.sources + uris()).toSet().toList();
#endif
    } else {
        _fileName = ms.file.name();
        _mimeType = ms.file.mediaType();
        _uris     = ms.sources;
    }

    _fileSize = ms.file.size();
    _modifyTime  = ms.file.date();
    _description = ms.file.description();
    if (from.isValid() && !from.isEmpty())
        _jids << from;

    QByteArray ampl = ms.file.amplitudes();
    if (ampl.size()) {
        _metaData.insert(QLatin1String("amplitudes"), ampl);
    }

    if (_fileType == FileType::RemoteFile)
        FileSharingChunkSource::configure(this);
}

// OOB supplies only an HTTP URL: this SHA-256 is an address-routing key,
 // never a content digest. It must not enter checksum verification or the
 // verified content cache. Only the actual downloaded bytes can create one.
FileSharingItem::FileSharingItem(const QUrl &url, const XMPP::Hash &identity, PsiAccount *acc,
                                 FileSharingManager *manager, const QString &description) :
    QObject(manager), _urlOnly(true), _acc(acc), _manager(manager), _fileType(FileType::RemoteFile)
{
    _sums.append(identity);
    // Keep the fragment for opening the URL; HTTP requests never send it.
    _uris.append(url.toString(QUrl::FullyEncoded));
    _fileName = QFileInfo(url.path()).fileName();
    if (_fileName.isEmpty())
        _fileName = QStringLiteral("download");
    _urlDisplayName = _fileName;
    _description = description;
    _mimeType = QMimeDatabase().mimeTypeForFile(_fileName, QMimeDatabase::MatchExtension).name();
    // The URL identity never enters FileCache as a claimed checksum. Resolve
    // its separate persistent catalog mapping only if the verified payload
    // still exists in the content-addressed cache.
    if (_acc && _manager) {
        const auto verified = _manager->contentForUrl(_acc->id(), identity);
        QString cachedPath;
        if (verified && _manager->cacheItem(*verified, false, &cachedPath)) {
            _sums.append(*verified);
            _fileName = cachedPath;
            _fileSize = quint64(QFileInfo(cachedPath).size());
            _fileType = FileType::LocalFile;
        } else if (verified) {
            _manager->forgetUrlContent(_acc->id(), identity); // the cached file expired
        }
    }
}

FileSharingItem::FileSharingItem(const QImage &image, PsiAccount *acc, FileSharingManager *manager) :
    QObject(manager), _acc(acc), _manager(manager), _fileType(FileType::TempFile)
{
    QByteArray ba;
    QBuffer    buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG", 0);
    _sums.append(Hash::from(Hash::Sha1, ba));

    if (!initFromCache()) {
        _mimeType = QString::fromLatin1("image/png");
        _fileSize = quint64(ba.size());
        QTemporaryFile file(QDir::tempPath() + QString::fromLatin1("/psishare-XXXXXX.png"));
        if (file.open()) {
            file.write(ba);
            file.setAutoRemove(false);
            _fileName = file.fileName();
            file.close();
        } else {
            qWarning("failed to open %s", qPrintable(file.errorString()));
        }
    }
}

FileSharingItem::FileSharingItem(const QString &fileName, PsiAccount *acc, FileSharingManager *manager) :
    QObject(manager), _acc(acc), _manager(manager), _fileType(FileType::LocalLink), _fileName(fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return;

    auto h = Hash::from(Hash::Sha1, &file);
    if (!h.isValid())
        return;

    _sums.append(h);

    if (!initFromCache()) {
        file.seek(0);
        _fileSize = quint64(file.size());
        _mimeType = QMimeDatabase().mimeTypeForFileNameAndData(fileName, &file).name();
    }
}

FileSharingItem::FileSharingItem(const QString &mime, const QByteArray &data, const QVariantMap &metaData,
                                 PsiAccount *acc, FileSharingManager *manager) :
    QObject(manager), _acc(acc), _manager(manager), _fileType(FileType::TempFile),
    _modifyTime(QDateTime::currentDateTimeUtc()), _metaData(metaData)
{
    _sums.append(Hash::from(Hash::Sha1, data));

    if (!initFromCache()) {
        _mimeType = mime;
        _fileSize = quint64(data.size());

        QMimeDatabase  db;
        QString        fileExt = db.mimeTypeForData(data).suffixes().value(0);
        QTemporaryFile file(QDir::tempPath() + QString::fromLatin1("/psi-XXXXXX")
                            + (fileExt.isEmpty() ? fileExt : QString('.') + fileExt));
        if (file.open()) {
            file.write(data);
            file.setAutoRemove(false);
            _fileName = file.fileName();
            file.close();
        } else {
            qWarning("failed to open %s", qPrintable(file.errorString()));
        }
    }
}

FileSharingItem::~FileSharingItem()
{
    if (_fileType == FileType::TempFile && !_fileName.isEmpty()) {
        QFile f(_fileName);
        if (f.exists())
            f.remove();
    }
}

bool FileSharingItem::initFromCache(FileCacheItem *cache)
{
    if (!cache && _sums.size())
        cache = this->cache(true);

    if (!cache)
        return false;

    auto md      = cache->metadata();
    _mimeType    = md.value(QString::fromLatin1("type")).toString();
    QString link = md.value(QString::fromLatin1("link")).toString();
    if (link.isEmpty()) {
        _fileType = FileType::LocalFile;
        _fileName = _manager->cacheDir() + "/" + cache->fileName();
        _fileSize = cache->size();
    } else {
        _fileType = FileType::LocalLink;
        _fileName = link;
        // note the readability of the filename was aleady checked by this moment
        _fileSize = QFileInfo(_fileName).size();
    }

    _sums       = cache->sums();
    auto urisvl = md.value(QString::fromLatin1("uris"));
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (urisvl.type() == QVariant::StringList)
#else
    if (urisvl.typeId() == QMetaType::QStringList)
#endif
        _uris = urisvl.toStringList();

    return true;
}

Reference FileSharingItem::toReference() const
{
    // The URL identity is not an XEP-0300 checksum; don't advertise it
    // as a verified SIMS file hash when forwarding/sending files.
    if (_urlOnly)
        return {};
    auto uris = sortSourcesByPriority(_uris);
    uris.removeDuplicates();
    std::reverse(uris.begin(), uris.end());
    if (uris.isEmpty())
        return {};

    Reference r(Reference::Data, uris.first());
    MediaSharing ms;
    ms.file    = fileMetadata();
    ms.sources = uris;
    r.setMediaSharing(ms);
    return r;
}

Jingle::FileTransfer::File FileSharingItem::fileInfo() const
{
    Jingle::FileTransfer::File jfile;
    QFileInfo                  fi(_fileName);
    jfile.setDate(fi.lastModified());
    for (int i = _urlOnly ? 1 : 0; i < _sums.size(); ++i)
        jfile.addHash(_sums.at(i));
    jfile.setName(fi.fileName());
    jfile.setSize(quint64(fi.size()));
    jfile.setMediaType(_mimeType);
    jfile.setDescription(_description);

    QSize thumbSize(64, 64);
    auto  thumbPix = thumbnail(thumbSize).pixmap(thumbSize);
    if (!thumbPix.isNull()) {
        QByteArray pixData;
        QBuffer    buf(&pixData);
        thumbPix.save(&buf, "PNG");
        QString png(QString::fromLatin1("image/png"));
        auto    bob = _acc->client()->bobManager()->append(
            pixData, png,
            _fileType == FileType::TempFile ? TEMP_TTL : FILE_TTL); // TODO the ttl logic doesn't look valid
        Thumbnail thumb(QByteArray(), png, quint32(thumbSize.width()), quint32(thumbSize.height()));
        thumb.uri = QLatin1String("cid:") + bob.cid();
        jfile.setThumbnail(thumb);
    }

    auto bhg = _metaData.value(QLatin1String("amplitudes")).toByteArray();
    if (bhg.size()) {
        jfile.setAmplitudes(bhg);
    }

    return jfile;
}

QIcon FileSharingItem::thumbnail(const QSize &size) const
{
    if (_fileType == FileType::RemoteFile)
        return QIcon();

    QImage image;
    if (_mimeType.startsWith(QLatin1String("image")) && image.load(_fileName)) {
        auto   img = image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QImage back(64, 64, QImage::Format_ARGB32_Premultiplied);
        back.fill(Qt::transparent);
        QPainter painter(&back);
        auto     imgRect = img.rect();
        imgRect.moveCenter(back.rect().center());
        painter.drawImage(imgRect, img);
        return QIcon(QPixmap::fromImage(std::move(back)));
    }
    return QFileIconProvider().icon(QFileInfo(_fileName));
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
    if (_urlOnly && !_urlDisplayName.isEmpty())
        return _urlDisplayName;
    if (_fileName.isEmpty()) {
        auto ext = FileUtil::mimeToFileExt(_mimeType);
        return QString("psi-%1.%2").arg(QString::fromLatin1(_sums[0].toHex()), ext).replace("/", "");
    }
    return QFileInfo(_fileName).fileName();
}

QString FileSharingItem::fileName() const
{
    const auto local = localFilePath();
    return local.isEmpty() ? _fileName : local;
}

QString FileSharingItem::localFilePath() const
{
    if (!FileSharingChunkSource::error(const_cast<FileSharingItem *>(this)).isEmpty())
        return { };
    QString path;
    if (_manager->cacheItem(_sums, false, &path))
        return path;
    return _fileType == FileType::RemoteFile ? FileSharingChunkSource::localFile(const_cast<FileSharingItem *>(this))
                                             : _fileName;
}

void FileSharingItem::mergeSources(const MediaSharing &media, const Jid &peer)
{
    if (_receivedMetadata.isValid())
        _receivedMetadata.merge(media.file);
    else
        _receivedMetadata = media.file;
    _uris = media.sources + _uris;
    _uris.removeDuplicates();
    for (const auto &hash : media.file.computedHashes())
        if (!_sums.contains(hash))
            _sums.append(hash);
    if (!peer.isEmpty() && !_jids.contains(peer))
        _jids.append(peer);
    if (!media.file.amplitudes().isEmpty())
        _metaData.insert(QStringLiteral("amplitudes"), media.file.amplitudes());
    FileSharingChunkSource::configure(this);
}

FileCacheItem *FileSharingItem::cache(bool reborn) const
{
    if (!FileSharingChunkSource::error(const_cast<FileSharingItem *>(this)).isEmpty())
        return nullptr;
    for (int i = _urlOnly ? 1 : 0; i < _sums.size(); ++i) {
        auto c = _manager->cacheItem(_sums.at(i), reborn);
        if (c)
            return c;
    }
    return nullptr;
}

bool FileSharingItem::savePublishedFile()
{
    QVariantMap meta = _metaData;
    meta["type"]     = _mimeType;
    meta["uris"]     = _uris;
    if (auto existing = cache()) {
        auto metadata = existing->metadata();
        for (auto it = meta.cbegin(); it != meta.cend(); ++it)
            metadata.insert(it.key(), it.value());
        existing->setMetadata(metadata);
        return true;
    }
    if (_fileType == FileType::TempFile) {
        auto stored = _manager->moveToCache(_sums, QFileInfo(_fileName), meta, TEMP_TTL);
        if (!stored)
            return false;
        _fileType = FileType::LocalFile;
        _fileName = _manager->cacheDir() + "/" + stored->fileName();
        return true;
    }
    const auto path = localFilePath();
    if (!QFileInfo(path).isReadable())
        return false;
    meta["link"] = path;
    return _manager->saveToCache(_sums, {}, meta, FILE_TTL) != nullptr;
}

void FileSharingItem::publish(const XMPP::Jid &myJid)
{
    const auto path = localFilePath();
    if (path.isEmpty())
        return;
    if (_publication || !_acc)
        return;

    _published = false;
    // Cache temporary files before registering a factory that will serve them.
    const bool serveLocally = savePublishedFile();
    if (serveLocally)
        _manager->setServed(_acc, _sums, true);
    if (serveLocally && _fileSize) {
        auto cache = _manager->mediaChunkCache();
        if (cache->isCompleteFile(localFilePath()))
            cache->configure(FileSharingChunkSource::identity(this), *_fileSize, { }, { { "published", true } });
    }
    auto *publication = new FileSharingPublication(_acc->client(), fileMetadata(), localFilePath(), myJid, this);
    _publication = publication;
    connect(publication, &FileSharingPublication::progress, this,
            [this](qint64 bytes) { emit publishProgress(size_t(bytes)); });
    connect(publication, &FileSharingPublication::logMessage, this, [this](const QString &message) {
        _log.append(message);
        emit logChanged();
    });
    connect(publication, &FileSharingPublication::finished, this, [this, publication]() {
        _uris = publication->sources();
        _published = !_uris.isEmpty();
        _publication = nullptr;
        publication->deleteLater();
        if (!_published && _acc)
            _manager->setServed(_acc, _sums, false);
        // Also remove expired URLs from the cache when every method failed.
        savePublishedFile();
        if (_published && _acc) {
            Message snapshot;
            snapshot.setFrom(_acc->client()->jid());
            snapshot.setReferences({ toReference() });
            _manager->rememberMessage(snapshot, _acc);
        }
        emit publishFinished();
    });
    publication->start(_uris, serveLocally);
}

PsiAccount *FileSharingItem::account() const { return _acc; }

XMPP::Jingle::FileTransfer::File FileSharingItem::fileMetadata() const
{
    if (_receivedMetadata.isValid())
        return _receivedMetadata;
    return fileInfo();
}

FileShareDownloader *FileSharingItem::download(std::optional<Range> range, const QString &preferredSource)
{
    // if (range && range->start == 0 && _fileSize && range->size == *_fileSize)
    //     range = {};

    XMPP::Jingle::FileTransfer::File file;
    file.setDate(_modifyTime);
    file.setMediaType(_mimeType);
    file.setName(_fileName);
    if (_fileSize)
        file.setSize(*_fileSize);
    if (!_urlOnly)
        for (auto const &h : std::as_const(_sums))
            file.addHash(h);

    auto downloaderRange = std::optional<FileShareDownloader::Range> {};
    if (range) {
        downloaderRange = FileShareDownloader::Range { range->start, range->size };
    }

    // if (_downloader && _downloader->requestedRange() == downloaderRange) {
    //     qWarning("double download for the same file: %s", qPrintable(_fileName));
    //     return _downloader;
    // }

    auto downloader = new FileShareDownloader(_acc, _urlOnly ? HashSums() : _sums, file, _jids, _uris, this);
    if (!preferredSource.isEmpty())
        downloader->preferSource(preferredSource);
    if (downloaderRange) {
        downloader->setRequestRange(downloaderRange);
        return downloader;
    }
    connect(downloader, &FileShareDownloader::cacheReady, this, [this, downloader]() {
        QString dlFileName = downloader->takeFile();
        downloader->disconnect(this);
        // downloader = nullptr;

        if (_modifyTime.isValid())
            FileUtil::setModificationTime(dlFileName, _modifyTime);

        auto thumbMetaType = _metaData.value(QString::fromLatin1("thumb-mt")).toString();
        auto thumbUri      = _metaData.value(QString::fromLatin1("thumb-uri")).toString();
        auto amplitudes    = _metaData.value(QString::fromLatin1("amplitudes")).toByteArray();

        QVariantMap vm;
        vm.insert(QString::fromLatin1("type"), _mimeType);
        vm.insert(QString::fromLatin1("uris"), _uris);
        if (thumbUri.size()) { // then thumbMetaType is not empty too
            vm.insert(QString::fromLatin1("thumb-mt"), thumbMetaType);
            vm.insert(QString::fromLatin1("thumb-uri"), thumbUri);
        }
        if (amplitudes.size()) {
            vm.insert(QString::fromLatin1("amplitudes"), amplitudes);
        }

        // Cache insertion and item state changes are confined to this object's
        // thread, even when hashing multi-gigabyte URL-only downloads.
        const auto completeCache = [this, dlFileName, vm = std::move(vm)](const HashSums &actualHashes) {
            const bool     automatic = std::exchange(_autoDownloading, false);
            FileCacheItem *cache     = nullptr;
            if (!actualHashes.isEmpty())
                cache = _manager->moveToCache(actualHashes, QFileInfo(dlFileName), vm, FILE_TTL);
            _autoFailed |= automatic && !cache; // never retry unasked
            if (cache) {
                if (_urlOnly) {
                    for (const auto &hash : actualHashes)
                        if (!_sums.contains(hash))
                            _sums.append(hash);
                    _fileSize = QFileInfo(_manager->cacheDir() + "/" + cache->fileName()).size();
                    _manager->rememberUrlContent(_acc->id(), _sums.first(), actualHashes.first());
                }
                _fileType = FileType::LocalFile;
                _fileName = _manager->cacheDir() + "/" + cache->fileName();
            }
            emit downloadFinished();
        };
        if (_urlOnly) {
            // Hashing the whole payload on the GUI thread would stall Psi for
            // multi-gigabyte OOB files. The temporary file is kept alive until
            // the worker finishes, and cleaned up if this item is destroyed.
            FileSharingHashJob::sha256TemporaryFile(
                dlFileName, this, [completeCache](const XMPP::Hash &hash) {
                    completeCache(hash.isValid() ? HashSums { hash } : HashSums {});
                });
        } else {
            completeCache(_sums);
        }
    });

    connect(downloader, &FileShareDownloader::failed, this, [downloader, this]() {
        downloader->disconnect(this);
        // downloader = nullptr;
        _autoFailed |= std::exchange(_autoDownloading, false);
        emit downloadFinished();
    });

    // connect(downloader, &FileShareDownloader::destroyed, this, [this]() { _downloader = nullptr; });

    return downloader;
}

FileSharingItem::SourceType FileSharingItem::sourceType(const QString &uri)
{
    if (uri.startsWith(QLatin1String("http"))) {
        return SourceType::HTTP;
    } else if (uri.startsWith(QLatin1String("xmpp"))) {
        return SourceType::Jingle;
    } else if (uri.startsWith(QLatin1String("ftp"))) {
        return SourceType::FTP;
    } else if (uri.startsWith(QLatin1String("cid"))) {
        return SourceType::BOB;
    }
    return FileSharingItem::SourceType::None;
}

QStringList FileSharingItem::sortSourcesByPriority(const QStringList &uris)
{
    // sort uris by priority first
    QMultiMap<int, QString> sorted;
    for (auto const &u : uris) {
        auto type = sourceType(u);
        if (type != SourceType::None)
            sorted.insert(int(type), u);
    }

    return sorted.values();
}

void FileSharingItem::checkRemote()
{
    if (!_urlOnly || _remoteCheck != RemoteCheck::NotStarted || !_acc || isCached())
        return;
    const QUrl url = simpleSource();
    if (!url.isValid())
        return;
    _remoteCheck = RemoteCheck::Running;
    QNetworkRequest request(url.adjusted(QUrl::RemoveFragment));
    // As in Conversations: the size of the stored bytes, not of an encoding.
    request.setRawHeader("Accept-Encoding", "identity");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(30000);
#endif
    auto *reply = _acc->psi()->networkAccessManager()->head(request);
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        _remoteCheck     = RemoteCheck::Done;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300) {
            bool       ok   = false;
            const auto size = reply->rawHeader("Content-Length").trimmed().toULongLong(&ok);
            if (ok && size && !_fileSize) // a zero length tells nothing of a file
                _fileSize = size;
            // The extension of the file name in the URL wins over the server's type.
            const QString type
                = QString::fromLatin1(reply->rawHeader("Content-Type")).section(QLatin1Char(';'), 0, 0).trimmed();
            if (!type.isEmpty() && QFileInfo(_urlDisplayName).suffix().isEmpty())
                _mimeType = type.toLower();
        }
        emit remoteChecked();
        if (_pendingAutoLimit)
            autoDownload(std::exchange(_pendingAutoLimit, 0));
    });
}

bool FileSharingItem::downloadsItself(quint64 limit) const
{
    if (!limit || _autoFailed)
        return false;
    if (_fileSize)
        return *_fileSize <= limit;
    return _urlOnly && _remoteCheck != RemoteCheck::Done;
}

void FileSharingItem::autoDownload(quint64 limit)
{
    if (!limit || _autoDownloading || _autoFailed || isCached())
        return;
    if (_urlOnly && !_fileSize) {
        if (_remoteCheck != RemoteCheck::Done) {
            _pendingAutoLimit = std::max(_pendingAutoLimit, limit);
            checkRemote();
        }
        return;
    }
    if (!_fileSize || *_fileSize > limit)
        return;
    auto *downloader = download();
    if (!downloader)
        return;
    _autoDownloading = true;
    downloader->setMaximumSize(limit);
    downloader->setSelfDelete(true);
    // read just for cache
    connect(downloader, &FileShareDownloader::readyRead, this,
            [downloader]() { downloader->read(downloader->bytesAvailable()); });
    downloader->open();
}

// try take http or ftp source to be passed directly to media backend
QUrl FileSharingItem::simpleSource() const
{
    if (!_uris.size())
        return QUrl();

    auto    sorted = sortSourcesByPriority(_uris);
    QString srcUrl = sorted.last();
    auto    t      = sourceType(srcUrl);
    if (t == SourceType::HTTP || t == SourceType::FTP) {
        return QUrl(srcUrl);
    }
    return QUrl();
}
