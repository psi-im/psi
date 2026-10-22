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

#include "iris/jingle-ft.h"
#include <memory>
#include <optional>

class PsiAccount;
class FileSharingJingleSession;

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
    struct Range {
        quint64 start;
        quint64 size; // 0 - all the remaining
    };

    FileShareDownloader(PsiAccount *acc, const QList<XMPP::Hash> &sums, const XMPP::Jingle::FileTransfer::File &file,
                        const QList<XMPP::Jid> &jids, const QStringList &uris, QObject *manager);
    ~FileShareDownloader();

    std::optional<quint64>      responseSize() const; // Actual upstream metadata, not advertised size.
    bool                        isSuccess() const;
    bool                        isConnected() const;
    bool                        hasIntegrityError() const;
    bool                        open(QIODevice::OpenMode mode = QIODevice::ReadOnly) override;
    void                        abort();
    void                        setRequestRange(const std::optional<Range> &range);
    void                        setJingleSessionPool(FileSharingJingleSession *pool);
    const std::optional<Range> &requestRange() const;
    const std::optional<Range> &responseRange() const;

    QString                                 takeFile() const;
    const XMPP::Jingle::FileTransfer::File &jingleFile() const;

    bool   isSequential() const override;
    qint64 bytesAvailable() const override;

    void setSelfDelete(bool enable);
    void preferSource(const QString &uri);

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

signals:
    // Validated payload. Pooled media requests may release their receipt later,
    // after the successor content has been accepted.
    void transferCompleted();
    void fileMetadataReceived(const XMPP::Jingle::FileTransfer::File &file);
    void metaDataChanged();
    void disconnected();
    void failed(); // when last piece of data saved to file or on error
    void cacheReady();
    void progress(size_t curSize, size_t fullSize);

private:
    class Private;
    std::unique_ptr<Private> d;
};
