/*
 * filesharingmanager.h - file sharing manager
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

#pragma once

#include "xmpp_hash.h"
#include "xmpp_reference.h"

#include <QDateTime>
#include <QObject>

class FileCacheItem;
class FileShareDownloader;
class FileSharingManager;
class PsiAccount;

namespace XMPP {
class Jid;
class MediaSharing;
class Reference;
}

class FileSharingItem : public QObject {
    Q_OBJECT

public:
    enum class SourceType { // from lowest priority to highest
        None,
        BOB,
        FTP,
        Jingle,
        HTTP
    };

    //using Ptr  = QSharedPointer<FileSharingItem>;
    //using List = QList<Ptr>;
    using HashSums = QMap<XMPP::Hash::Type, XMPP::Hash>;

    FileSharingItem(FileCacheItem *cache, PsiAccount *acc, FileSharingManager *manager);
    FileSharingItem(const XMPP::MediaSharing &ms, const XMPP::Jid &from, PsiAccount *acc, FileSharingManager *manager);
    FileSharingItem(const QImage &image, PsiAccount *acc, FileSharingManager *manager);
    FileSharingItem(const QString &fileName, PsiAccount *acc, FileSharingManager *manager);
    FileSharingItem(const QString &mime, const QByteArray &data, const QVariantMap &metaData,
                    PsiAccount *acc, FileSharingManager *manager);
    ~FileSharingItem();

    QIcon                  thumbnail(const QSize &size) const;
    QImage                 preview(const QSize &maxSize) const;
    QString                displayName() const;
    QString                fileName() const;
    inline const QString & mimeType() const { return _mimeType; }
    inline const HashSums &sums() const { return _sums; }
    inline QVariantMap     metaData() const { return _metaData; }
    inline FileCacheItem * cache() const { return _cache; }
    qint64                 fileSize() const { return _fileSize; }
    inline bool            isSizeKnown() const { return _sizeKnown; }

    inline bool               isCached() const { return _cache != nullptr; }
    inline bool               isPublished() const { return _httpFinished && _jingleFinished; }
    inline const QStringList &uris() const { return _readyUris; }
    PsiAccount *              account() const;
    inline const QStringList  log() const { return _log; }

    XMPP::Reference      toReference() const;
    void                 publish();
    FileShareDownloader *download(bool isRanged = false, qint64 start = 0, qint64 size = 0);

    // accept public internet uri and returns it's type
    static SourceType  sourceType(const QString &uri);
    static QStringList sortSourcesByPriority(const QStringList &uris);

    QUrl simpleSource() const;

private:
    void initFromCache();

signals:
    void publishFinished();
    void publishProgress(size_t transferredBytes);
    void downloadFinished();
    void logChanged();

private:
    FileCacheItem *      _cache          = nullptr;
    PsiAccount *         _acc            = nullptr;
    FileSharingManager * _manager        = nullptr;
    FileShareDownloader *_downloader     = nullptr;
    bool                 _isImage        = false;
    bool                 _isTempFile     = false;
    bool                 _httpFinished   = false;
    bool                 _jingleFinished = false;
    bool                 _finishNotified = false;
    bool                 _sizeKnown      = false;
    qint64               _fileSize       = 0;
    QDateTime            _modifyTime; // utc
    QStringList          _readyUris;
    QString              _fileName; // file name without path
    HashSums             _sums;
    QString              _mimeType;
    QString              _description;
    QVariantMap          _metaData;
    QStringList          _log;
    QList<XMPP::Jid>     _jids;
};

//Q_DECLARE_METATYPE(FileSharingItem::Ptr)
