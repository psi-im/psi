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
#include "filesharingmanager.h"
#include "fileutil.h"
#include "httpfileupload.h"
#include "psiaccount.h"
#include "userlist.h"
#include "xmpp_client.h"
#include "xmpp_reference.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFileIconProvider>
#include <QImageReader>
#include <QMimeDatabase>
#include <QPainter>
#include <QTemporaryFile>

#define TEMP_TTL (7 * 24 * 3600)
#define FILE_TTL (365 * 24 * 3600)

using namespace XMPP;

// ======================================================================
// FileSharingItem
// ======================================================================
FileSharingItem::FileSharingItem(FileCacheItem *cache, PsiAccount *acc,
                                 FileSharingManager *manager) :
    QObject(manager),
    _cache(cache),
    _acc(acc),
    _manager(manager),
    _isTempFile(false)
{
    initFromCache();
}

FileSharingItem::FileSharingItem(const MediaSharing &ms, const Jid &from, PsiAccount *acc, FileSharingManager *manager) :
    _acc(acc),
    _manager(manager)
{
    _sizeKnown = ms.file.hasSize();
    _fileSize  = ms.file.size();
    _fileName  = ms.file.name();
    _mimeType  = ms.file.mediaType();
    _readyUris = ms.sources;
    _jids << from;
    for (auto const &h : ms.file.hashes()) {
        if (!_cache) {
            auto c = manager->getCacheItem(h);
            if (c)
                _cache = c;
        }
        _sums.insert(h.type(), h);
    }

    // TODO remaining
}

FileSharingItem::FileSharingItem(const QImage &image, PsiAccount *acc,
                                 FileSharingManager *manager) :
    QObject(manager),
    _acc(acc),
    _manager(manager),
    _isImage(true),
    _isTempFile(false),
    _sizeKnown(true)
{
    QByteArray ba;
    QBuffer    buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG", 0);
    auto sha1hash = Hash::from(Hash::Sha1, ba);
    _mimeType     = QString::fromLatin1("image/png");
    _fileSize     = size_t(ba.size());

    _cache = manager->getCacheItem(sha1hash, true);
    if (_cache) {
        _fileName = _cache->fileName();
        // TODO fill _sums
    } else {
        _sums.insert(Hash::Sha1, sha1hash);
        QTemporaryFile file(QDir::tempPath() + QString::fromLatin1("/psishare-XXXXXX.png"));
        file.open();
        file.write(ba);
        file.setAutoRemove(false);
        _fileName   = file.fileName();
        _isTempFile = true;
        file.close();
    }
}

FileSharingItem::FileSharingItem(const QString &fileName, PsiAccount *acc,
                                 FileSharingManager *manager) :
    QObject(manager),
    _acc(acc),
    _manager(manager),
    _isImage(false),
    _isTempFile(false),
    _sizeKnown(true),
    _fileName(fileName)
{
    QFile file(fileName);
    _fileSize     = size_t(file.size());
    auto sha1hash = Hash::from(Hash::Sha1, &file);
    _cache        = manager->getCacheItem(sha1hash, true);

    if (_cache) {
        initFromCache();
    } else {
        _sums.insert(Hash::Sha1, sha1hash);
        file.seek(0);
        _mimeType = QMimeDatabase().mimeTypeForFileNameAndData(fileName, &file).name();
        _isImage  = QImageReader::supportedMimeTypes().contains(_mimeType.toLatin1());
    }
}

FileSharingItem::FileSharingItem(const QString &mime, const QByteArray &data,
                                 const QVariantMap &metaData, PsiAccount *acc,
                                 FileSharingManager *manager) :
    QObject(manager),
    _acc(acc),
    _manager(manager),
    _isImage(false),
    _isTempFile(false),
    _sizeKnown(true),
    _modifyTime(QDateTime::currentDateTimeUtc()),
    _metaData(metaData)
{
    auto sha1hash = Hash::from(Hash::Sha1, data);
    _mimeType     = mime;
    _fileSize     = size_t(data.size());

    _cache = manager->getCacheItem(sha1hash, true);
    if (_cache) {
        _fileName = _cache->fileName();
    } else {
        _sums.insert(Hash::Sha1, sha1hash);
        QMimeDatabase  db;
        QString        fileExt = db.mimeTypeForData(data).suffixes().value(0);
        QTemporaryFile file(QDir::tempPath() + QString::fromLatin1("/psi-XXXXXX") + (fileExt.isEmpty() ? fileExt : QString('.') + fileExt));
        file.open();
        file.write(data);
        file.setAutoRemove(false);
        _fileName   = file.fileName();
        _isTempFile = true;
        file.close();
    }
}

FileSharingItem::~FileSharingItem()
{
    if (!_cache && _isTempFile && !_fileName.isEmpty()) {
        QFile f(_fileName);
        if (f.exists())
            f.remove();
    }
}

void FileSharingItem::initFromCache()
{
    auto md      = _cache->metadata();
    _mimeType    = md.value(QString::fromLatin1("type")).toString();
    _isImage     = QImageReader::supportedMimeTypes().contains(_mimeType.toLatin1());
    QString link = md.value(QString::fromLatin1("link")).toString();
    if (link.isEmpty()) {
        _fileName = _cache->fileName();
        _fileSize = _cache->size();
    } else {
        _fileName = link;
        _fileSize = size_t(QFileInfo(_fileName).size());
    }
    _sizeKnown = true;
    _sums.insert(_cache->id().type(), _cache->id());
    _readyUris = md.value(QString::fromLatin1("uris")).toStringList();

    QString httpScheme(QString::fromLatin1("http"));
    QString xmppScheme(QString::fromLatin1("xmpp")); // jingle ?
    for (const auto &u : _readyUris) {
        QUrl url(u);
        auto scheme = url.scheme();
        if (scheme.startsWith(httpScheme)) {
            _httpFinished = true;
        } else if (scheme == xmppScheme) {
            _jingleFinished = true;
        }
    }
}

Reference FileSharingItem::toReference() const
{
    QStringList uris(_readyUris);

    UserListItem *u = _acc->find(_acc->jid());
    if (u->userResourceList().isEmpty())
        return Reference();
    Jid selfJid = u->jid().withResource(u->userResourceList().first().name());

    uris.append(QString::fromLatin1("xmpp:%1?jingle-ft").arg(selfJid.full()));
    uris = sortSourcesByPriority(uris);
    std::reverse(uris.begin(), uris.end());

    Jingle::FileTransfer::File jfile;
    QFileInfo                  fi(_fileName);
    jfile.setDate(fi.lastModified());
    for (auto const &h : _sums)
        jfile.addHash(h);
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
            pixData, png, _isTempFile ? TEMP_TTL : FILE_TTL);
        Thumbnail thumb(QByteArray(), png, quint32(thumbSize.width()),
                        quint32(thumbSize.height()));
        thumb.uri = QLatin1String("cid:") + bob.cid();
        jfile.setThumbnail(thumb);
    }

    auto bhg = _metaData.value(QLatin1String("amplitudes")).toByteArray();
    if (bhg.size()) {
        jfile.setAmplitudes(bhg);
    }

    Reference    r(Reference::Data, uris.first());
    MediaSharing ms;
    ms.file    = jfile;
    ms.sources = uris;

    r.setMediaSharing(ms);

    return r;
}

QIcon FileSharingItem::thumbnail(const QSize &size) const
{
    QImage image;
    if (_isImage && image.load(_fileName)) {
        auto   img = image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QImage back(64, 64, QImage::Format_ARGB32_Premultiplied);
        back.fill(Qt::transparent);
        QPainter painter(&back);
        auto     imgRect = img.rect();
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
        auto ext = FileUtil::mimeToFileExt(_mimeType);
        return QString("psi-%1.%2")
            .arg(QString::fromLatin1(_sums.cbegin().value().data().toHex()), ext)
            .replace("/", "");
    }
    return QFileInfo(_fileName).fileName();
}

QString FileSharingItem::fileName() const { return _fileName; }

void FileSharingItem::publish()
{
    auto checkFinished = [this]() {
        // if we didn't emit yet finished signal and everything is finished
        if (!_finishNotified && _httpFinished && _jingleFinished) {
            QVariantMap meta = _metaData;
            meta["type"]     = _mimeType;
            if (_readyUris.count()) // if ever published something on external service
                // like http
                meta["uris"] = _readyUris;
            if (_isTempFile) {
                _cache = _manager->moveToCache(_sums.cbegin().value(), _fileName, meta, TEMP_TTL);
            } else {
                meta["link"] = _fileName;
                _cache       = _manager->saveToCache(_sums.cbegin().value(), QByteArray(), meta, FILE_TTL);
            }
            _finishNotified = true;
            emit publishFinished();
        }
    };

    if (!_httpFinished) {
        auto hm = _acc->client()->httpFileUploadManager();
        if (hm->discoveryStatus() == HttpFileUploadManager::DiscoNotFound) {
            _httpFinished = true;
            checkFinished();
        } else {
            auto hfu = hm->upload(_fileName, displayName(), _mimeType);
            hfu->setParent(this);
            connect(hfu, &HttpFileUpload::progress, this,
                    [this](qint64 bytesReceived, qint64 bytesTotal) {
                        Q_UNUSED(bytesTotal)
                        emit publishProgress(size_t(bytesReceived));
                    });
            connect(hfu, &HttpFileUpload::finished, this, [hfu, this, checkFinished]() {
                _httpFinished = true;
                if (hfu->success()) {
                    _log.append(tr("Published on HttpUpload service"));
                    _readyUris.append(hfu->getHttpSlot().get.url);
                } else {
                    _log.append(QString("%1: %2").arg(
                        tr("Failed to publish on HttpUpload service"),
                        hfu->statusString()));
                }
                emit logChanged();
                checkFinished();
            });
        }
    }
    if (!_jingleFinished) {
        // FIXME we have to add muc jids here if shared with muc
        // readyUris.append(QString::fromLatin1("xmpp:%1?jingle").arg(acc->jid().full()));
        _jingleFinished = true;
        checkFinished();
    }
}

FileShareDownloader *FileSharingItem::download(bool isRanged, qint64 start, qint64 size)
{
    if (isRanged && _sizeKnown && start == 0 && size == _fileSize)
        isRanged = false;

    if (!isRanged && _downloader) { // let's wait till first one is finished
        // qWarning("%s download is in progress already", qPrintable(src.file.name()));
        return _downloader;
    }

    XMPP::Jingle::FileTransfer::File file;
    file.setDate(_modifyTime);
    file.setMediaType(_mimeType);
    file.setName(_fileName);
    if (_sizeKnown)
        file.setSize(_fileSize);

    FileShareDownloader *downloader = new FileShareDownloader(
        _acc, _sums.values(), file, _jids, _readyUris, this);
    if (isRanged) {
        downloader->setRange(start, size);
        return downloader;
    }

    _downloader = downloader;
    connect(downloader, &FileShareDownloader::finished, this,
            [this]() {
                _downloader->disconnect(this);
                _downloader->deleteLater();

                if (!_downloader->isSuccess()) {
                    emit downloadFinished();
                    return;
                }

                if (_modifyTime.isValid())
                    FileUtil::setModificationTime(_downloader->fileName(), _modifyTime);

                auto thumbMetaType = _metaData.value(QString::fromLatin1("thumb-mt")).toString();
                auto thumbUri      = _metaData.value(QString::fromLatin1("thumb-uri")).toString();
                auto amplitudes    = _metaData.value(QString::fromLatin1("amplitudes")).toByteArray();

                QVariantMap vm;
                vm.insert(QString::fromLatin1("type"), _mimeType);
                vm.insert(QString::fromLatin1("uris"), _readyUris);
                if (thumbUri.size()) { // then thumbMetaType is not empty too
                    vm.insert(QString::fromLatin1("thumb-mt"), thumbMetaType);
                    vm.insert(QString::fromLatin1("thumb-uri"), thumbUri);
                }
                if (amplitudes.size()) {
                    vm.insert(QString::fromLatin1("amplitudes"), amplitudes);
                }

                auto hIt = _sums.cbegin();
                _manager->moveToCache(hIt.value(), _downloader->fileName(), vm, FILE_TTL);

                emit downloadFinished();
            });

    connect(downloader, &FileShareDownloader::destroyed, this,
            [this]() {
                _downloader = nullptr;
            });

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

// try take http or ftp source to be passed directly to media backend
QUrl FileSharingItem::simpleSource() const
{
    auto    sorted = sortSourcesByPriority(_readyUris);
    QString srcUrl = sorted.last();
    auto    t      = sourceType(srcUrl);
    if (t == SourceType::HTTP || t == SourceType::FTP) {
        return QUrl(srcUrl);
    }
    return QUrl();
}
