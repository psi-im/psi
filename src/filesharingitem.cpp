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
#include "filesharingpublication.h"
#include "fileutil.h"
#include "iris/xmpp_client.h"
#include "iris/xmpp_message.h"
#include "iris/xmpp_reference.h"
#include "iris/xmpp_thumbs.h"
#include "psiaccount.h"
#include "userlist.h"

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
    QObject(manager), _acc(acc), _manager(manager), _fileType(FileType::RemoteFile)
{
    _sums = ms.file.computedHashes();
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

    // TODO remaining
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
    auto uris = sortSourcesByPriority(_uris);
    uris.removeDuplicates();
    std::reverse(uris.begin(), uris.end());
    if (uris.isEmpty())
        return {};

    Reference r(Reference::Data, uris.first());
    MediaSharing ms;
    ms.file = fileInfo();
    ms.sources = uris;
    r.setMediaSharing(ms);
    return r;
}

Jingle::FileTransfer::File FileSharingItem::fileInfo() const
{
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
    if (_fileName.isEmpty()) {
        auto ext = FileUtil::mimeToFileExt(_mimeType);
        return QString("psi-%1.%2").arg(QString::fromLatin1(_sums[0].toHex()), ext).replace("/", "");
    }
    return QFileInfo(_fileName).fileName();
}

QString FileSharingItem::fileName() const { return _fileName; }

QString FileSharingItem::localFilePath() const { return _fileType == FileType::RemoteFile ? QString() : _fileName; }

void FileSharingItem::mergeSources(const MediaSharing &media, const Jid &peer)
{
    _uris = media.sources + _uris;
    _uris.removeDuplicates();
    for (const auto &hash : media.file.computedHashes())
        if (!_sums.contains(hash))
            _sums.append(hash);
    if (!peer.isEmpty() && !_jids.contains(peer))
        _jids.append(peer);
    if (!media.file.amplitudes().isEmpty())
        _metaData.insert(QStringLiteral("amplitudes"), media.file.amplitudes());
}

FileCacheItem *FileSharingItem::cache(bool reborn) const
{
    for (const auto &h : _sums) {
        auto c = _manager->cacheItem(h, reborn);
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
    if (!QFileInfo(_fileName).isReadable())
        return false;
    meta["link"] = _fileName;
    return _manager->saveToCache(_sums, {}, meta, FILE_TTL) != nullptr;
}

void FileSharingItem::publish(const XMPP::Jid &myJid)
{
    Q_ASSERT(_fileType != FileType::RemoteFile);
    if (_publication || !_acc)
        return;

    _published = false;
    // Cache temporary files before registering a factory that will serve them.
    const bool serveLocally = savePublishedFile();
    auto *publication = new FileSharingPublication(_acc->client(), fileInfo(), _fileName, myJid, this);
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

FileShareDownloader *FileSharingItem::download(std::optional<Range> range)
{
    // if (range && range->start == 0 && _fileSize && range->size == *_fileSize)
    //     range = {};

    XMPP::Jingle::FileTransfer::File file;
    file.setDate(_modifyTime);
    file.setMediaType(_mimeType);
    file.setName(_fileName);
    if (_fileSize)
        file.setSize(*_fileSize);
    for (auto const &h : std::as_const(_sums)) {
        file.addHash(h);
    }

    auto downloaderRange = std::optional<FileShareDownloader::Range> {};
    if (range) {
        downloaderRange = FileShareDownloader::Range { range->start, range->size };
    }

    // if (_downloader && _downloader->requestedRange() == downloaderRange) {
    //     qWarning("double download for the same file: %s", qPrintable(_fileName));
    //     return _downloader;
    // }

    auto downloader = new FileShareDownloader(_acc, _sums, file, _jids, _uris, this);
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

        auto cache = _manager->moveToCache(_sums, QFileInfo(dlFileName), vm, FILE_TTL);
        if (cache) {
            // TODO set some item flags?
            _fileType = FileType::LocalFile;
            _fileName = _manager->cacheDir() + "/" + cache->fileName();
        }

        emit downloadFinished();
    });

    connect(downloader, &FileShareDownloader::failed, this, [downloader, this]() {
        downloader->disconnect(this);
        // downloader = nullptr;
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
