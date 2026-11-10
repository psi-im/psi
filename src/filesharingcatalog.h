/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "iris/xmpp_jid.h"
#include "iris/xmpp_reference.h"
#include "messagefileshares.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QSqlError>
#include <optional>

// What was announced, by whom and from which sources, per account, and which files an
// account answers remote requests for. One publication per announcement: an attachment
// of a stored message, or a file the account shares. Knows nothing of bytes or transfers.
// Without the file database nothing is known and nothing is served.
class FileSharingCatalog {
public:
    using PublicationId = QByteArray;

    // All that one account knows of one file, from all its publications together.
    struct Record {
        XMPP::MediaSharing media;
        QStringList        peers;
        bool               urlOnly = false; // known by its address, and media has only that as a source
        QString            localPath;
        qint64             localSize = 0, localModified = 0;
        QString            readableLocalPath() const;
    };

    // Forgets URL-to-content mappings that have outlived the files they name.
    FileSharingCatalog();

    struct Announcement {
        PublicationId                  id;
        QString                        account;
        XMPP::Jid                      announcer;
        MessageFileShares::FileShare   file;
    };

    // The publications of stored messages. FileSharingPublicationSync keeps them in line
    // with history, so that a message that is not stored leaves nothing here.
    bool                 add(const PublicationId &id, const QString &account, const XMPP::Jid &announcer,
                             const MessageFileShares::FileShare &attachment);
    bool                 addBatch(const QList<Announcement> &announcements);
    bool                 remove(const QList<PublicationId> &ids);
    QList<PublicationId> missing(const QList<PublicationId> &ids, QSqlError *error = nullptr) const;
    QList<PublicationId> announcementsAfter(const PublicationId &id, int limit, QSqlError *error = nullptr) const;

    // By a hash of the file, or by the address of a URL-only one.
    std::optional<Record> find(const QString &account, const XMPP::Hash &hash) const;

    // Files this account answers remote requests for, because it published them or joined
    // their publication. This outlives any message: a remembered descriptor or a cached
    // copy serves nothing. Serving a file again replaces its publication.
    bool serve(const QString &account, const XMPP::Jid &self, const XMPP::MediaSharing &media,
               const QString &localPath);
    void stopServing(const QString &account, const QList<XMPP::Hash> &hashes);
    bool isServed(const QString &account, const QList<XMPP::Hash> &hashes) const;
    // Resolve a request to the hashes of ONE explicitly served publication.
    // Never use the request's other, untrusted hashes to look up cached bytes.
    std::optional<Record> servedRecord(const QString &account, const QList<XMPP::Hash> &requested) const;

    // An OOB address identity is NOT a content checksum. Keep its lookup in
    // a separate table, referencing only verified downloaded content hashes.
    std::optional<XMPP::Hash> contentForUrl(const QString &account, const XMPP::Hash &urlIdentity) const;
    bool rememberUrlContent(const QString &account, const XMPP::Hash &urlIdentity, const XMPP::Hash &contentHash);
    // For a mapping whose content left the file cache.
    void forgetUrlContent(const QString &account, const XMPP::Hash &urlIdentity);

    // Mappings older than this are dropped when the catalog opens, like the cached files they name.
    static constexpr qint64 UrlContentTtl = 365 * 24 * 3600;

    // Takes once what only the old catalog knows: the files the given accounts shared
    // (account ID to JID) and the URL mappings. The rest comes back from history.
    // Returns whether that is done, now or earlier, so that the old catalog can go.
    bool importLegacy(const QString &path, const QHash<QString, XMPP::Jid> &accounts);
};
