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

#ifndef FILESHARINGMANAGER_H
#define FILESHARINGMANAGER_H

#include "filesharingitem.h"
#include "qite.h"

#include <QObject>

class FileCache;
class FileCacheItem;
class FileSharingDownloader;
class FileSharingManager;
class MessageView;
class PsiAccount;
class QFileInfo;
class QImage;
class QMimeData;

namespace qhttp {
namespace server {
    class QHttpRequest;
    class QHttpResponse;
}
}

namespace XMPP {
class Message;
namespace Jingle {
    class Session;
    namespace FileTransfer {
        class File;
    }
}
}

/**
 * @brief The FileSharingManager class
 * magic class for sharing files.
 *
 * Probably this thing requires some refactoring to follow SOLID principles.
 * We have next components:
 *  - publishing user content
 *  - handle share:// scheme
 *  - download shares
 *  - cache data
 */
class FileSharingManager : public QObject {
    Q_OBJECT
public:
    explicit FileSharingManager(QObject *parent = nullptr);
    ~FileSharingManager();

    static QString cacheDir();
    FileCacheItem *cacheItem(const XMPP::Hash &id, bool reborn = false, QString *fileName = nullptr);
    // id - usually hex(sha1(image data))
    FileCacheItem *saveToCache(const XMPP::Hash &id, const QByteArray &data, const QVariantMap &metadata, unsigned int maxAge);
    FileCacheItem *moveToCache(const XMPP::Hash &id, const QFileInfo &data, const QVariantMap &metadata, unsigned int maxAge);

    FileSharingItem *item(const XMPP::Hash &id);
    //FileSharingItem* fromReference(const XMPP::Reference &ref, PsiAccount *acc);
    QList<FileSharingItem *> fromMimeData(const QMimeData *data, PsiAccount *acc);
    QList<FileSharingItem *> fromFilesList(const QStringList &fileList, PsiAccount *acc);

    // registers source for file and returns share id for future access to the source
    void fillMessageView(MessageView &mv, const XMPP::Message &m, PsiAccount *acc);

    // returns false if unable to accept automatically
    bool jingleAutoAcceptIncomingDownloadRequest(XMPP::Jingle::Session *session);

#ifdef HAVE_WEBSERVER
    bool downloadHttpRequest(PsiAccount *acc, const QString &sourceIdHex, qhttp::server::QHttpRequest *req, qhttp::server::QHttpResponse *res);
#endif
signals:

public slots:

private:
    class Private;
    QScopedPointer<Private> d;
};

class FileSharingDeviceOpener : public ITEMediaOpener {
    PsiAccount *acc;

public:
    inline FileSharingDeviceOpener(PsiAccount *acc) :
        acc(acc) {}
    virtual ~FileSharingDeviceOpener() {}

    static XMPP::Hash urlToSourceId(const QUrl &url);
    QIODevice *       open(QUrl &url) override;
    void              close(QIODevice *dev) override;
    QVariant          metadata(const QUrl &url) override;

    FileSharingItem *sharedItem(const QString &id);
};

#endif // FILESHARINGMANAGER_H
