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

private:
    QString      name_;
    QSqlDatabase db_;
    bool         ready_ = false;
};
