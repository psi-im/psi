/*
 * filesharingmanager.h - file sharing manager
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

#ifndef FILESHARINGMANAGER_H
#define FILESHARINGMANAGER_H

#include <QObject>

#include "xmpp_reference.h"
#ifndef WEBKIT
# include "qite.h"
#endif

class QMimeData;
class QImage;
class PsiAccount;
class FileSharingManager;
class FileCache;
class FileCacheItem;

namespace XMPP {
    class Message;
    namespace Jingle {
        class Session;
    }
}

class FileShareDownloader: public QObject
{
    Q_OBJECT
public:
    FileShareDownloader(PsiAccount *acc, const QString &sourceId, const XMPP::Jingle::FileTransfer::File &file,
                        const QList<XMPP::Jid> &jids, const QStringList &uris, FileSharingManager *manager);
    ~FileShareDownloader();

    bool isSuccess() const;
    void start();
    void abort();

    QString fileName() const;
signals:
    void started();
    void finished();
    void progress(size_t curSize, size_t fullSize);
private:
    class Private;
    QScopedPointer<Private> d;
};

class FileSharingItem : public QObject
{
    Q_OBJECT

public:
    FileSharingItem(FileCacheItem *cache, PsiAccount *acc, FileSharingManager *manager);
    FileSharingItem(const QImage &image, PsiAccount *acc, FileSharingManager *manager);
    FileSharingItem(const QString &fileName, PsiAccount *acc, FileSharingManager *manager);
    FileSharingItem(const QString &mime, const QByteArray &data, const QVariantMap &metaData,
                    PsiAccount *acc, FileSharingManager *manager);
    ~FileSharingItem();

    QIcon thumbnail(const QSize &size) const;
    QImage preview(const QSize &maxSize) const;
    QString displayName() const;
    QString fileName() const;
    inline const QString &hash() const { return sha1hash; }

    inline bool isPublished() const { return httpFinished && jingleFinished; }
    void publish();
    inline const QStringList &uris() const { return readyUris; }
    PsiAccount *account() const;
    XMPP::Reference toReference() const;
private:
    void initFromCache();
    void checkFinished();

signals:
    void publishFinished();
    void publishProgress(size_t transferredBytes);
private:
    FileCacheItem *cache = nullptr;
    PsiAccount *acc;
    FileSharingManager *manager;
    bool isImage = false;
    bool isTempFile = false;
    bool httpFinished = false;
    bool jingleFinished = false;
    bool finishNotified = false;
    size_t _fileSize;
    QStringList readyUris;
    QString _fileName;
    QString sha1hash;
    QString mimeType;
    QString _description;
    QVariantMap metaData;
};

class FileSharingManager : public QObject
{
    Q_OBJECT
public:
    enum class SourceType { // from lowest priority to highest
        None,
        BOB,
        FTP,
        Jingle,
        HTTP
    };

    explicit FileSharingManager(QObject *parent = nullptr);
    ~FileSharingManager();

    static QString getCacheDir();
    FileCacheItem *getCacheItem(const QString &id, bool reborn = false);

    // id - usually hex(sha1(image data))
    FileCacheItem *saveToCache(const QString &id, const QByteArray &data, const QVariantMap &metadata, unsigned int maxAge);
    //FileSharingItem* fromReference(const XMPP::Reference &ref, PsiAccount *acc);
    QList<FileSharingItem *> fromMimeData(const QMimeData *data, PsiAccount *acc);
    QList<FileSharingItem *> fromFilesList(const QStringList &fileList, PsiAccount *acc);

    // registers source for file and returns share id for future access to the source
    QString registerSource(const XMPP::Jingle::FileTransfer::File &file, const XMPP::Jid &source, const QStringList &uris);
    QString downloadThumbnail(const QString &sourceId);
    QUrl simpleSource(const QString &sourceId) const;
    FileShareDownloader *downloadShare(PsiAccount *acc, const QString &sourceId);
    void saveDownloadedSource(const QString &sourceId, const QString &hash, const QString &absPath);

    // returns false if unable to accept automatically
    bool jingleAutoAcceptDownloadRequest(XMPP::Jingle::Session *session);

    static SourceType sourceType(const QString &uri);
    static QStringList sortSourcesByPriority(const QStringList &uris);
signals:

public slots:

private:
    class Private;
    QScopedPointer<Private> d;
};

#ifndef WEBKIT
class FileSharingDeviceOpener : public ITEMediaOpener
{
    PsiAccount *acc;
public:
    inline FileSharingDeviceOpener(PsiAccount *acc) :
        acc(acc){}

    QIODevice *open(QUrl &url) override;
    void close(QIODevice *dev) override;
};
#endif

#endif // FILESHARINGMANAGER_H
