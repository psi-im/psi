/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "iris/xmpp_jid.h"
#include "messageattachments.h"
#include "historymediaqueries.h"

#include <QByteArray>
#include <QList>
#include <QSet>
#include <QSqlDatabase>
#include <QString>
#include <functional>
#include <optional>

// Which publication announces each attachment of a stored message. History holds only
// this link and its own copy of the message: the publication lives in the file database
// and can always be told again from here.
namespace HistoryMediaRefs {
// What a linked attachment announces, as stored with its message.
struct Announcement {
    QString                        account;
    XMPP::Jid                      announcer; // empty for a message this account sent itself
    MessageAttachments::Attachment attachment;
};

// Adds the links, for every message stored so far, in one transaction.
bool needsMigration(QSqlDatabase db);
// Compatibility for SFS rows written by older development versions; GUI only.
bool prepareLegacySfs(QSqlDatabase db);
bool migrate(QSqlDatabase db, const std::function<void(qint64)> &progress = {});

// Gives each attachment of a stored event a new publication ID, unless it has one.
// Runs inside the transaction storing the event.
bool link(QSqlDatabase db, qint64 eventId, bool *removed = nullptr);

// Decode an immutable worker projection on the GUI.
std::optional<Announcement> announcement(const Link &link);
}
