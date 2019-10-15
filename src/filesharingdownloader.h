/*
 * filesharingdownloader.h - file sharing downloader
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

#include <QIODevice>

class PsiAccount;

namespace XMPP {
class Jid;
class Message;
class Hash;
namespace Jingle {
    class Session;
    namespace FileTransfer {
        class File;
    }
}
}

class FileShareDownloader : public QIODevice {
    Q_OBJECT
public:
    FileShareDownloader(PsiAccount *acc, const QList<XMPP::Hash> &sums, const XMPP::Jingle::FileTransfer::File &file,
                        const QList<XMPP::Jid> &jids, const QStringList &uris, QObject *manager);
    ~FileShareDownloader();

    bool                       isSuccess() const;
    bool                       isConnected() const;
    bool                       open(QIODevice::OpenMode mode = QIODevice::ReadOnly) override;
    void                       abort();
    void                       setRange(qint64 start, qint64 size);
    bool                       isRanged() const;
    std::tuple<qint64, qint64> range() const;

    QString                                 fileName() const;
    const XMPP::Jingle::FileTransfer::File &jingleFile() const;

    bool   isSequential() const override;
    qint64 bytesAvailable() const override;

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

signals:
    void metaDataChanged();
    void disconnected();
    void finished(); // when last piece of data saved to file or on error
    void progress(size_t curSize, size_t fullSize);

private:
    class Private;
    QScopedPointer<Private> d;
};
