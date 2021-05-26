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

    enum class FileType : char {
        RemoteFile, // not yet downloaded
        LocalLink,  // an unmanager by cache local file
        TempFile,   // a temporary file supposed to be eventually moved to the cache
        LocalFile   // cached local file
    };

    enum Flag {
        HttpFinished    = 0x1,
        JingleFinished  = 0x2,
        PublishNotified = 0x4,
        SizeKnown       = 0x8,
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    using HashSums = QList<XMPP::Hash>;

    FileSharingItem(FileCacheItem *cache, PsiAccount *acc, FileSharingManager *manager);
    FileSharingItem(const XMPP::MediaSharing &ms, const XMPP::Jid &from, PsiAccount *acc, FileSharingManager *manager);
    FileSharingItem(const QImage &image, PsiAccount *acc, FileSharingManager *manager);
    FileSharingItem(const QString &fileName, PsiAccount *acc, FileSharingManager *manager);
    FileSharingItem(const QString &mime, const QByteArray &data, const QVariantMap &metaData, PsiAccount *acc,
                    FileSharingManager *manager);
    ~FileSharingItem();

    QIcon                     thumbnail(const QSize &size) const;
    QImage                    preview(const QSize &maxSize) const;
    QString                   displayName() const;
    QString                   fileName() const;
    inline const QString &    mimeType() const { return _mimeType; }
    inline const HashSums &   sums() const { return _sums; }
    inline QVariantMap        metaData() const { return _metaData; }
    inline quint64            fileSize() const { return _fileSize; }
    inline bool               isSizeKnown() const { return bool(_flags & SizeKnown); }
    inline const QStringList &uris() const { return _uris; }

    // reborn flag updates ttl for the item
    FileCacheItem *          cache(bool reborn = false) const;
    inline bool              isCached() const { return cache() != nullptr; }
    PsiAccount *             account() const;
    inline const QStringList log() const { return _log; }

    XMPP::Reference toReference(const XMPP::Jid &selfJid) const;
    void            publish(const XMPP::Jid &myJid);
    inline bool     isPublished() const { return _flags & HttpFinished && _flags & JingleFinished; }

    /**
     * @brief FileSharingItem::download starts downloading SIMS
     * @param isRanged - where it's a ranged request
     * @param start    - range start
     * @param size     - range end
     * @return downloader object
     *
     * It's responsibility of the caller to delete downloader when it's done, or one can call setSelfDelete(true)
     */
    FileShareDownloader *download(bool isRanged = false, qint64 start = 0, quint64 size = 0);

    // accept public internet uri and returns it's type
    static SourceType  sourceType(const QString &uri);
    static QStringList sortSourcesByPriority(const QStringList &uris);

    QUrl simpleSource() const;

private:
    bool initFromCache(FileCacheItem *cache = nullptr);

signals:
    void publishFinished();
    void publishProgress(size_t transferredBytes);
    void downloadFinished();
    void logChanged();

private:
    PsiAccount *         _acc        = nullptr;
    FileSharingManager * _manager    = nullptr;
    FileShareDownloader *_downloader = nullptr;
    FileType             _fileType;
    Flags                _flags;
    quint64              _fileSize = 0;
    QDateTime            _modifyTime; // utc
    QStringList          _uris;
    QString              _fileName; // file name with path
    HashSums             _sums;
    QString              _mimeType;
    QString              _description;
    QVariantMap          _metaData;
    QStringList          _log;
    QList<XMPP::Jid>     _jids;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FileSharingItem::Flags)

// Q_DECLARE_METATYPE(FileSharingItem::Ptr)
