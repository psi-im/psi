/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "iris/xmpp_reference.h"
#include <QSqlDatabase>
#include <optional>

// Persistent descriptors, independent of disposable downloaded payloads. Keys
// include the account and hash algorithm. History retains its own SIMS snapshot
// so a missing catalog never makes a stored message unreadable.
class FileSharingStore {
public:
    struct Record {
        XMPP::Reference reference;
        QStringList     peers;
        QString         localPath;
        QString         localSize, localModified;
        QString         readableLocalPath() const;
    };
    explicit FileSharingStore(const QString &path);
    ~FileSharingStore();
    FileSharingStore(const FileSharingStore &)                = delete;
    FileSharingStore     &operator=(const FileSharingStore &) = delete;
    bool                  remember(const QString &account, const XMPP::Reference &, const XMPP::Jid &peer,
                                   const QString &trustedLocalPath = {});
    std::optional<Record> find(const QString &account, const XMPP::Hash &hash) const;
    // An OOB address identity is NOT a content checksum. Keep its lookup in
    // a separate table, referencing only verified downloaded content hashes.
    std::optional<XMPP::Hash> contentForUrl(const QString &account, const XMPP::Hash &urlIdentity) const;
    bool rememberUrlContent(const QString &account, const XMPP::Hash &urlIdentity,
                            const XMPP::Hash &contentHash);
    // For a mapping whose content left the file cache.
    void forgetUrlContent(const QString &account, const XMPP::Hash &urlIdentity);

    // Files this account answers remote requests for: it published them or joined
    // their publication. A remembered descriptor or a cached copy serves nothing.
    bool markServed(const QString &account, const QList<XMPP::Hash> &hashes);
    void unmarkServed(const QString &account, const QList<XMPP::Hash> &hashes);
    bool isServed(const QString &account, const QList<XMPP::Hash> &hashes) const;
    // Runs once per account. Publications older than the list above are the files
    // the account itself shared and held a local copy of.
    void importOwnPublications(const QString &account, const XMPP::Jid &self);

    // Mappings older than this are dropped when the catalog opens, like the cached files they name.
    static constexpr qint64 UrlContentTtl = 365 * 24 * 3600;

private:
    QString      name_;
    QSqlDatabase db_;
    bool         ready_ = false;
};
