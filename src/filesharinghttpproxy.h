/*
 * filesharinghttpproxy.h - http proxy for shared files downloads
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

#ifndef FILESHARINGHTTPPROXY_H
#define FILESHARINGHTTPPROXY_H

#include <QObject>
#include <QPointer>

class FileCacheItem;
class FileSharingItem;
class FileShareDownloader;
class PsiAccount;

namespace qhttp { namespace server {
    class QHttpRequest;
    class QHttpResponse;
}}

class FileSharingHttpProxy : public QObject {
    Q_OBJECT
public:
    explicit FileSharingHttpProxy(PsiAccount *acc, const QString &sourceIdHex, qhttp::server::QHttpRequest *request,
                                  qhttp::server::QHttpResponse *response);
    ~FileSharingHttpProxy();

signals:

public slots:
private slots:
    void onMetadataChanged();
    void transfer();

private:
    int  parseHttpRangeRequest();
    void setupHeaders(qint64 fileSize, QString contentType, QDateTime lastModified, bool isRanged, qint64 rangeStart,
                      qint64 rangeSize);
    void proxyCache();

private:
    FileSharingItem *             item = nullptr;
    PsiAccount *                  acc;
    qhttp::server::QHttpRequest * request;
    qhttp::server::QHttpResponse *response;
    QPointer<FileShareDownloader> downloader;
    qint64                        requestedStart = 0;
    qint64                        requestedSize  = 0;  // if == 0 then all the remaining
    qint64                        bytesLeft      = -1; // -1 - unknown
    bool                          isRanged       = false;
    bool                          headersSent    = false;
};

#endif // FILESHARINGHTTPPROXY_H
