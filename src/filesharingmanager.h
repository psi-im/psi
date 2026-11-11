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

#include "chatmedia.h"
#include "filesharingitem.h"
#include "mediachunktypes.h"
#include "qite.h"

#include <QNetworkReply>
#include <QObject>
#include <functional>
#include <optional>

class EDBSqLite;
class FileCache;
class FileCacheItem;
class FileSharingDownloader;
class FileSharingMediaSave;
class FileDownload;
class FileReader;
class FileSharingManager;
class MessageView;
class PsiAccount;
class MediaChunkCache;
class FileSharingJingleSession;
class QFileInfo;
class QImage;
class QMimeData;

namespace qhttp { namespace server {
    class QHttpRequest;
    class QHttpResponse;
}}

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

    static QString   cacheDir();
    MediaChunkCache *mediaChunkCache();
    FileDownload    *download(FileSharingItem *item);
    ChatMedia       *chatMedia(PsiAccount *account, const HistoryArchive::Key &chat, QObject *parent);
    // Lazily resolve through the same item registry used by message views.
    FileSharingItem *chatMediaItem(const ChatMedia::Entry &, PsiAccount *account);
    FileReader *reader(FileSharingItem *item, QObject *parent, quint64 offset = 0, std::optional<quint64> length = {});
    using OpenDone = std::function<void(FileReader *, MediaChunkError)>;
    // Queued completion; the returned function cancels only this consumer.
    // Whole authenticated/unknown-length sources are shared and verified first.
    MediaChunkAbort           open(FileSharingItem *, QObject *owner, OpenDone, quint64 maximumBytes = 0);
    void        removeIncomplete(const std::function<void(quint64, const QString &)> &completed);
    FileSharingJingleSession *jingleMediaSessions(PsiAccount *account);
    FileCacheItem *cacheItem(const QList<XMPP::Hash> &hashes, bool reborn = false, QString *fileName = nullptr);
    FileCacheItem *trackExternal(const QList<XMPP::Hash> &sums, const QFileInfo &file, const QVariantMap &metadata,
                                 unsigned int maxAge);
    FileCacheItem *cacheItem(const XMPP::Hash &id, bool reborn = false, QString *fileName = nullptr);
    FileCacheItem *moveToCache(const QList<XMPP::Hash> &sums, const QFileInfo &data, const QVariantMap &metadata,
                               unsigned int maxAge);
    std::optional<XMPP::Hash> contentForUrl(const QString &account, const XMPP::Hash &urlIdentity) const;
    bool rememberUrlContent(const QString &account, const XMPP::Hash &urlIdentity, const XMPP::Hash &contentHash);
    void forgetUrlContent(const QString &account, const XMPP::Hash &urlIdentity);

    FileSharingItem *item(const XMPP::Hash &id, PsiAccount *account = nullptr);
    // FileSharingItem* fromReference(const XMPP::Reference &ref, PsiAccount *acc);
    QList<FileSharingItem *> createFromMimeData(const QMimeData *data, PsiAccount *acc);
    QList<FileSharingItem *> fromFilesList(const QStringList &fileList, PsiAccount *acc);

    // Show what the message attaches (SIMS references, HTTP(S) OOB URLs as share
    // objects, other OOB URLs as links) in its view. The message is not changed.
    void fillMessageView(MessageView &mv, const XMPP::Message &m, PsiAccount *acc);

    // The largest attachment of the message the view may download by itself, 0 for none.
    quint64 autoDownloadLimit(const XMPP::Message &m, PsiAccount *acc) const;

    // Answers a remote request for a file on behalf of the account. Returns false when
    // the session is not a request, such as a file offer for the user to accept.
    bool answerFileRequest(XMPP::Jingle::Session *session, PsiAccount *account);
    // The account answers remote requests for the file from now on, or no longer does.
    bool serve(PsiAccount *account, const XMPP::MediaSharing &media, const QString &localPath);
    void stopServing(PsiAccount *account, const QList<XMPP::Hash> &hashes);
    // Every source the account was told of for the file.
    QStringList knownSources(PsiAccount *account, const QList<XMPP::Hash> &hashes) const;

    // What the catalog from before publications alone knows, once all accounts are loaded.
    void importLegacyCatalog(const QList<PsiAccount *> &accounts);
    // Publications of stored messages follow the history from now on.
    void followHistory(EDBSqLite *history);

signals:
    void chatMediaChanged();

public slots:

private:
    FileSharingItem *rangeItem(const XMPP::MediaSharing &, PsiAccount *);
    FileSharingItem *fromShare(const MessageFileShares::FileShare &, const XMPP::Jid &, PsiAccount *);
    class Private;
    std::unique_ptr<Private> d;
};

class FileSharingDeviceOpener : public ITEMediaOpener {
    PsiAccount *acc;

public:
    inline FileSharingDeviceOpener(PsiAccount *acc) : acc(acc) { }
    ~FileSharingDeviceOpener() override;

    static XMPP::Hash urlToSourceId(const QUrl &url);
    QIODevice        *open(QUrl &url) override;
    CancelOpen        openAsync(QUrl, QObject *, OpenDone) override;
    void              close(QIODevice *dev) override;
    void              abort(QIODevice *dev) override;
    QVariant          metadata(const QUrl &url) override;
    ITEMediaState    *mediaState(const QUrl &url) override;
    void              retry(const QUrl &url) override;
    using SaveCallback     = std::function<void(const QString &path, const QString &error)>;
    using ProgressCallback = std::function<void(qreal)>;
    bool saveAs(const QUrl &url, QWidget *parent, const SaveCallback &completed = {},
                const ProgressCallback &progress = {});
    bool saveTo(const QUrl &url, const QString &path, const SaveCallback &completed = {},
                const ProgressCallback &progress = {});
    void cancelSave(const QUrl &url);

    FileSharingItem         *sharedItem(const QString &id);
    FileSharingDeviceOpener *forAccount(const QString &id) const;
};

#endif // FILESHARINGMANAGER_H
