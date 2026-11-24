/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "iris/xmpp_jid.h"
#include "iris/jingle-session.h"
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

    // Persisted values: keep stable across schema upgrades.
    enum class AccessScope { Public = 0, SameServer = 1, LinkOnly = 2 };
    struct ServingPolicy {
        AccessScope access;
        XMPP::Jingle::EncryptionPolicy encryption;
        constexpr ServingPolicy(AccessScope access = AccessScope::Public,
                                XMPP::Jingle::EncryptionPolicy encryption = XMPP::Jingle::EncryptionPolicy::Disabled) :
            access(access), encryption(encryption) { }
    };

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

    // Legacy hash-addressed publications remain compatible with old links.
    // An explicit publication ID instead identifies one independent serving decision.
    // Only this ID may be replaced; sharing the same bytes does not alter other grants.
    bool serve(const QString &account, const XMPP::Jid &self, const XMPP::MediaSharing &media,
               const QString &localPath, const PublicationId &id = {}, ServingPolicy policy = {});
    void stopServing(const QString &account, const PublicationId &id);
    void stopServing(const QString &account, const QList<XMPP::Hash> &hashes);
    bool isServed(const QString &account, const QList<XMPP::Hash> &hashes) const;
    // Resolve a request to the hashes of ONE explicitly served publication.
    // Never use the request's other, untrusted hashes to look up cached bytes.
    struct ServedRecord : Record {
        PublicationId id;
        ServingPolicy policy;
    };
    // An explicit ID never falls back to a legacy hash grant. LinkOnly is
    // deliberately refused until a protected capability proof is implemented.
    std::optional<ServedRecord> servedRecord(const QString &account, const QList<XMPP::Hash> &requested,
                                              const PublicationId &id = {}, const XMPP::Jid &peer = {}) const;

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
