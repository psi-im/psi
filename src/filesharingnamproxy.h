/*
 * filesharingnamproxy.h - proxy network access reply for shared files
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

#ifndef FILESHARINGNAMPROXY_H
#define FILESHARINGNAMPROXY_H

#include <QNetworkReply>
#include <QPointer>

class FileSharingItem;
class FileShareDownloader;
class PsiAccount;
class QFile;

class FileSharingNAMReply : public QNetworkReply {
    Q_OBJECT

    FileSharingItem *             item = nullptr;
    PsiAccount *                  acc;
    QPointer<FileShareDownloader> downloader;
    qint64                        requestedStart = 0;
    qint64                        requestedSize  = 0;  // if == 0 then all the remaining
    qint64                        bytesLeft      = -1; // -1 - unknown
    bool                          isRanged       = false;
    bool                          headersSent    = false;
    QFile *                       cachedFile     = nullptr;

public:
    FileSharingNAMReply(PsiAccount *acc, const QString &sourceIdHex, const QNetworkRequest &req);
    ~FileSharingNAMReply();

    // reimplemented
    void   abort() override;
    qint64 readData(char *buffer, qint64 maxlen) override;
    qint64 bytesAvailable() const override;

private slots:
    void onMetadataChanged();
    void init();

private:
    void setupHeaders(qint64 fileSize, QString contentType, QDateTime lastModified, bool isRanged, qint64 rangeStart,
                      qint64 rangeSize);
    void finishWithError(QNetworkReply::NetworkError error, int httpCode = 0, const char *httpStatus = nullptr);
};

#endif // FILESHARINGNAMPROXY_H
