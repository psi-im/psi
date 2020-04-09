/*
 * bytearrayreply.h - Base class for QNetworReply'es returning QByteArray
 * Copyright (C) 2010  senu, Sergey Ilinykh
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

#ifndef BYTEARRAYREPLY_H
#define BYTEARRAYREPLY_H

#include <QBuffer>
#include <QNetworkReply>

class ByteArrayReply : public QNetworkReply {

    Q_OBJECT

public:
    ByteArrayReply(const QNetworkRequest &request, const QByteArray &ba = QByteArray(),
                   const QString &mimeType = QString(), QObject *parent = nullptr);

    /** Construct IconReply that fails with ContentAccessDenied error */
    // ByteArrayReply();
    ~ByteArrayReply();

    // reimplemented
    void   abort() override;
    qint64 readData(char *buffer, qint64 maxlen) override;
    qint64 bytesAvailable() const override;
    bool   open(OpenMode mode) override;

private slots:
    void signalError();

private:
    int        origLen;
    QByteArray data;
    QBuffer    buffer;
};

#endif // BYTEARRAYREPLY_H
