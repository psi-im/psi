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

class QMimeData;
class QImage;
class PsiAccount;
class FileSharingManager;
class FileCache;
class FileCacheItem;

namespace XMPP {
    class Message;
}

class FileSharingItem : public QObject
{
    Q_OBJECT

public:
    FileSharingItem(FileCacheItem *cache, PsiAccount *acc, FileSharingManager *manager);
    FileSharingItem(const QImage &image, PsiAccount *acc, FileSharingManager *manager);
    FileSharingItem(const QString &fileName, PsiAccount *acc, FileSharingManager *manager);
    ~FileSharingItem();

    bool setupMessage(XMPP::Message &msg);
    QIcon thumbnail(const QSize &size) const;
    QImage preview(const QSize &maxSize) const;
    QString displayName() const;
    QString fileName() const;

    inline bool isPublished() const { return httpFinished && jingleFinished; }
    void publish();
    inline const QStringList &uris() const { return readyUris; }
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
    size_t _fileSize;
    QStringList readyUris;
    QString _fileName;
    QString sha1hash;
    QString mimeType;
};

class FileSharingManager : public QObject
{
    Q_OBJECT
public:
    explicit FileSharingManager(QObject *parent = nullptr);

    static QString getCacheDir();
    FileCacheItem *getCacheItem(const QString &id, bool reborn = false);
    FileCacheItem *saveToCache(const QString &id, const QByteArray &data, const QVariantMap &metadata, unsigned int maxAge);
    QList<FileSharingItem *> fromMimeData(const QMimeData *data, PsiAccount *acc);
signals:

public slots:

private:
    FileCache *cache;
};

#endif // FILESHARINGMANAGER_H
