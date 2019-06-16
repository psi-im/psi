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
#include "httpfileupload.h"
#include "filecache.h"

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

FileSharingItem::FileSharingItem(FileCacheItem *cache, PsiAccount *acc, FileSharingManager *manager) :
    QObject(manager),
    cache(cache),
    acc(acc),
    manager(manager),
    isTempFile(false)
{
    initFromCache();
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
    if (httpFinished && jingleFinished) {
        QVariantMap mime;
        mime["type"] = mimeType;
        mime["uris"] = readyUris;
        if (isTempFile) {
            QFile f(_fileName);
            f.open(QIODevice::ReadOnly);
            auto d = f.readAll();
            f.close();
            cache = manager->saveToCache(sha1hash, d, mime, 7 * 24 * 3600);
        } else {
            mime["link"] = _fileName;
            cache = manager->saveToCache(sha1hash, QByteArray(), mime, 365 * 24 * 3600);
        }
        emit published();
    }
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
        QTemporaryFile file(QString::fromLatin1("psishare"));
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
        mimeType = QMimeDatabase().mimeTypeForFileNameAndData(fileName, &file).name();
        isImage = QImageReader::supportedMimeTypes().contains(mimeType.toLatin1());
    }
}

FileSharingItem::~FileSharingItem()
{

}

bool FileSharingItem::setupMessage(XMPP::Message &msg)
{
    if (!readyUris.size()) {
        return false;
    }
    msg.setBody(readyUris.join(' '));
    msg.setHTML(HTMLElement());
    return true;
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
    if (isImage && image.load(_fileName)) {
        auto s = image.size().boundedTo(maxSize);
        return image.scaled(s, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

QString FileSharingItem::displayName() const
{
    if (isTempFile) {
        return QString("psi-%1.png").arg(sha1hash);
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
        auto hfu = hm->upload(_fileName, QString("psi-%1.png").arg(sha1hash), mimeType);
        hfu->setParent(this);
        connect(hfu, &HttpFileUpload::finished, this, [hfu, this]() {
            httpFinished = true;
            if (hfu->success()) {
                readyUris.append(hfu->getHttpSlot().get.url);
            }
            checkFinished();
            hfu->deleteLater();
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
FileSharingManager::FileSharingManager(QObject *parent) : QObject(parent),
    cache(new FileCache(getCacheDir(), this))
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

FileCacheItem *FileSharingManager::getCacheItem(const QString &id, bool reborn)
{
    return cache->get(id, reborn);
}

FileCacheItem * FileSharingManager::saveToCache(const QString &id, const QByteArray &data,
                                                const QVariantMap &metadata, unsigned int maxAge)
{
    return cache->append(id, data, metadata, maxAge);
}

QList<FileSharingItem*> FileSharingManager::fromMimeData(const QMimeData *data, PsiAccount *acc)
{
    QList<FileSharingItem*> ret;
    QStringList files;
    if (!data->hasImage() && data->hasUrls()) {
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
    if (files.isEmpty() && !data->hasImage()) {
        return ret;
    }

    if (files.isEmpty()) { // so we have an image
        QImage img = data->imageData().value<QImage>();
        auto item = new FileSharingItem(img, acc, this);
        ret.append(item);
    } else {
        for (auto const &f: files) {
            auto item = new FileSharingItem(f, acc, this);
            ret.append(item);
        }
    }

    return ret;
}

