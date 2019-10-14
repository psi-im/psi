/*
 * filesharingroxy.h - http proxy for shared files downloads
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

#ifndef FILESHARINGPROXY_H
#define FILESHARINGPROXY_H

#include <QObject>

class FileCacheItem;
class FileSharingItem;
class FileShareDownloader;
class PsiAccount;

namespace qhttp { namespace server {
    class QHttpRequest;
    class QHttpResponse;
}}

class FileSharingProxy : public QObject {
    Q_OBJECT
public:
    explicit FileSharingProxy(PsiAccount *acc, const QString &sourceIdHex, qhttp::server::QHttpRequest *request,
                              qhttp::server::QHttpResponse *response);

signals:

public slots:
private slots:
    void onMetadataChanged();

private:
    int  parseHttpRangeRequest();
    void setupHeaders(qint64 fileSize, QString contentType, QDateTime lastModified, bool isRanged, qint64 rangeStart,
                      qint64 rangeSize);
    void proxyCache(FileCacheItem *item);

private:
    FileSharingItem *             item = nullptr;
    PsiAccount *                  acc;
    qhttp::server::QHttpRequest * request;
    qhttp::server::QHttpResponse *response;
    FileShareDownloader *         downloader           = nullptr;
    qint64                        requestedStart       = 0;
    qint64                        requestedSize        = 0;
    bool                          isRanged             = false;
    bool                          upstreamDisconnected = false;
};

#endif // FILESHARINGPROXY_H
