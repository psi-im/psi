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
#include "xmpp_thumbs.h"

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
FileSharingItem::FileSharingItem(FileCacheItem *cache, PsiAccount *acc, FileSharingManager *manager) :
    QObject(manager), _acc(acc), _manager(manager)
{
    initFromCache(cache);
}

FileSharingItem::FileSharingItem(const MediaSharing &ms, const Jid &from, PsiAccount *acc,
                                 FileSharingManager *manager) :
    _acc(acc),
    _manager(manager), _fileType(FileType::RemoteFile)
{
    _sums = ms.file.computedHashes();
    if (initFromCache()) {
        _uris = (ms.sources.toSet() + uris().toSet()).toList();
    } else {
        _fileName = ms.file.name();
        _mimeType = ms.file.mediaType();
        _uris     = ms.sources;
    }

    if (ms.file.hasSize()) {
        _flags |= SizeKnown;
        _fileSize = ms.file.size();
    }

    _jids << from;

    QByteArray ampl = ms.file.amplitudes();
    if (ampl.size()) {
        _metaData.insert(QLatin1String("amplitudes"), ampl);
    }

    // TODO remaining
}

FileSharingItem::FileSharingItem(const QImage &image, PsiAccount *acc, FileSharingManager *manager) :
    QObject(manager), _acc(acc), _manager(manager), _fileType(FileType::TempFile), _flags(SizeKnown)
{
    QByteArray ba;
    QBuffer    buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG", 0);
    _sums.insert(Hash::Sha1, Hash::from(Hash::Sha1, ba));

    if (!initFromCache()) {
        _mimeType = QString::fromLatin1("image/png");
        _fileSize = qint64(ba.size());
        QTemporaryFile file(QDir::tempPath() + QString::fromLatin1("/psishare-XXXXXX.png"));
        file.open();
        file.write(ba);
        file.setAutoRemove(false);
        _fileName = file.fileName();
        file.close();
    }
}

FileSharingItem::FileSharingItem(const QString &fileName, PsiAccount *acc, FileSharingManager *manager) :
    QObject(manager), _acc(acc), _manager(manager), _fileType(FileType::LocalLink), _flags(SizeKnown),
    _fileName(fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return;

    auto h = Hash::from(Hash::Sha1, &file);
    if (!h.isValid())
        return;

    _sums.insert(Hash::Sha1, h);

    if (!initFromCache()) {
        file.seek(0);
        _fileSize = qint64(file.size());
        _mimeType = QMimeDatabase().mimeTypeForFileNameAndData(fileName, &file).name();
    }
}

FileSharingItem::FileSharingItem(const QString &mime, const QByteArray &data, const QVariantMap &metaData,
                                 PsiAccount *acc, FileSharingManager *manager) :
    QObject(manager),
    _acc(acc), _manager(manager), _fileType(FileType::TempFile), _flags(SizeKnown),
    _modifyTime(QDateTime::currentDateTimeUtc()), _metaData(metaData)
{
    _sums.insert(Hash::Sha1, Hash::from(Hash::Sha1, data));

    if (!initFromCache()) {
        _mimeType = mime;
        _fileSize = qint64(data.size());

        QMimeDatabase  db;
        QString        fileExt = db.mimeTypeForData(data).suffixes().value(0);
        QTemporaryFile file(QDir::tempPath() + QString::fromLatin1("/psi-XXXXXX")
                            + (fileExt.isEmpty() ? fileExt : QString('.') + fileExt));
        file.open();
        file.write(data);
        file.setAutoRemove(false);
        _fileName = file.fileName();
        file.close();
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

    _flags       = SizeKnown;
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
    if (urisvl.type() == QVariant::StringList)
        _uris = urisvl.toStringList();

    QString httpScheme(QString::fromLatin1("http"));
    QString xmppScheme(QString::fromLatin1("xmpp")); // jingle ?
    for (const auto &u : _uris) {
        QUrl url(u);
        auto scheme = url.scheme();
        if (scheme.startsWith(httpScheme)) {
            _flags |= HttpFinished;
        } else if (scheme == xmppScheme) {
            _flags |= JingleFinished;
        }
    }

    if ((_flags & (HttpFinished | JingleFinished)) == (HttpFinished | JingleFinished))
        _flags |= PublishNotified;

    return true;
}

Reference FileSharingItem::toReference(const Jid &selfJid) const
{
    QStringList uris(_uris);

    uris.append(QString::fromLatin1("xmpp:%1?jingle-ft").arg(selfJid.full()));
    uris = sortSourcesByPriority(uris);
    std::reverse(uris.begin(), uris.end());

    Jingle::FileTransfer::File jfile;
    QFileInfo                  fi(_fileName);
    jfile.setDate(fi.lastModified());
    for (auto const &h : _sums)
        jfile.addHash(h);
    jfile.setName(fi.fileName());
    jfile.setSize(fi.size());
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

    Reference    r(Reference::Data, uris.first());
    MediaSharing ms;
    ms.file    = jfile;
    ms.sources = uris;

    r.setMediaSharing(ms);

    return r;
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
        return QString("psi-%1.%2").arg(QString::fromLatin1(_sums[0].toHex()), ext).replace("/", "");
    }
    return QFileInfo(_fileName).fileName();
}

QString FileSharingItem::fileName() const { return _fileName; }

FileCacheItem *FileSharingItem::cache(bool reborn) const
{
    for (const auto &h : _sums) {
        auto c = _manager->cacheItem(h, reborn);
        if (c)
            return c;
    }
    return nullptr;
}

void FileSharingItem::publish(const XMPP::Jid &myJid)
{
    Q_UNUSED(myJid)
    Q_ASSERT(_fileType != FileType::RemoteFile);

    if (_flags & PublishNotified) {
        emit publishFinished();
        return;
    }

    auto checkFinished = [this]() {
        // if we didn't emit yet finished signal and everything is finished
        auto ff = HttpFinished | JingleFinished;
        if (!(_flags & PublishNotified) && ((_flags & ff) == ff)) { // TODO also check if any of them succeed
            QVariantMap meta = _metaData;
            meta["type"]     = _mimeType;
            if (_uris.count()) // if ever published something on external service
                // like http
                meta["uris"] = _uris;
            if (_fileType == FileType::TempFile) {
                auto cache = _manager->moveToCache(_sums, _fileName, meta, TEMP_TTL);
                if (cache) {
                    _fileType  = FileType::LocalFile;
                    _fileName  = _manager->cacheDir() + "/" + cache->fileName();
                }
            } else {
                meta["link"] = _fileName;
                _manager->saveToCache(_sums, QByteArray(), meta, FILE_TTL);
            }
            _flags |= PublishNotified;
            emit publishFinished();
        }
    };

    if (!(_flags & HttpFinished)) {
        auto hm = _acc->client()->httpFileUploadManager();
        if (hm->discoveryStatus() == HttpFileUploadManager::DiscoNotFound) {
            _flags |= HttpFinished;
            checkFinished();
        } else {
            auto hfu = hm->upload(_fileName, displayName(), _mimeType);
            hfu->setParent(this);
            connect(hfu, &HttpFileUpload::progress, this, [this](qint64 bytesReceived, qint64 bytesTotal) {
                Q_UNUSED(bytesTotal)
                emit publishProgress(size_t(bytesReceived));
            });
            connect(hfu, &HttpFileUpload::finished, this, [hfu, this, checkFinished]() {
                _flags |= HttpFinished;
                if (hfu->success()) {
                    _log.append(tr("Published on HttpUpload service"));
                    _uris.append(hfu->getHttpSlot().get.url);
                } else {
                    _log.append(
                        QString("%1: %2").arg(tr("Failed to publish on HttpUpload service"), hfu->statusString()));
                }
                emit logChanged();
                checkFinished();
            });
        }
    }
    if (!(_flags & JingleFinished)) {
        // FIXME we have to add muc jids here if shared with muc
        // readyUris.append(QString::fromLatin1("xmpp:%1?jingle").arg(acc->jid().full()));
        _flags |= JingleFinished;
        checkFinished();
    }
}

FileShareDownloader *FileSharingItem::download(bool isRanged, qint64 start, qint64 size)
{
    if (isRanged && (_flags & SizeKnown) && start == 0 && size == _fileSize)
        isRanged = false;

    XMPP::Jingle::FileTransfer::File file;
    file.setDate(_modifyTime);
    file.setMediaType(_mimeType);
    file.setName(_fileName);
    if (_flags & SizeKnown)
        file.setSize(_fileSize);
    for (auto const &h : _sums) {
        file.addHash(h);
    }

    FileShareDownloader *downloader = new FileShareDownloader(_acc, _sums, file, _jids, _uris, this);
    if (isRanged) {
        downloader->setRange(start, size);
        return downloader;
    }

    if (_downloader) {
        qWarning("double download for the same file: %s", qPrintable(_fileName));
        return downloader; // seems like we are downloading this file twice, but what we can do?
    }

    _downloader = downloader;
    connect(downloader, &FileShareDownloader::cacheReady, this, [this]() {
        QString dlFileName = _downloader->takeFile();
        _downloader->disconnect(this);
        _downloader = nullptr;

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

        auto cache = _manager->moveToCache(_sums, dlFileName, vm, FILE_TTL);
        if (cache) {
            // TODO set some item flags?
            _fileType = FileType::LocalFile;
            _fileName = _manager->cacheDir() + "/" + cache->fileName();
        }

        emit downloadFinished();
    });

    connect(downloader, &FileShareDownloader::failed, this, [this]() {
        _downloader->disconnect(this);
        _downloader = nullptr;
        emit downloadFinished();
    });

    connect(downloader, &FileShareDownloader::destroyed, this, [this]() { _downloader = nullptr; });

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
