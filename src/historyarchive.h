/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QDateTime>
#include <QList>
#include <QSqlDatabase>
#include <QSqlRecord>
#include <QString>

namespace HistoryArchive {
// Low bits describe provenance; adjacency is independent of provenance.
// Unknown is deliberately the SQLite default, including imported history.
enum State { Unverified = 0, Archived = 1, LocalOnly = 2, ProvenanceMask = 3, LinkedPrevious = 4, LinkedNext = 8 };
struct Key {
    QString account, archive, peer;
    bool    room = false;
};
struct Entry {
    QString uid, xml;
    qint64  eventId = 0;
    // Application-parsed fields for the existing EDB events table.
    QDateTime timestamp;
    QString   body, subject, language, extra, messageId, sender, replaceId;
    int       type = 1, direction = 2;
    bool      storable = true;
};
struct Page {
    QList<Entry> entries;
    bool         found    = false;
    bool         complete = false;
    bool         stable   = false;
};

// Additive, atomic migration of the original EDB schema. Never reimports or
// rewrites existing event payloads. Safe to call again after an interrupted start.
bool migrate(QSqlDatabase db);
// Return a unique original delivery, or zero when identity is absent/ambiguous.
// Index revision relationships transactionally whenever a live or MAM event
// is persisted. The earliest known revision anchors the row; its newest
// known child supplies text regardless of requested page boundaries.
bool recordRevision(QSqlDatabase db, qint64 eventId, qint64 contactId, int type,
                    int direction, const QString &sender,
                    const QString &messageId, const QString &replaceId);
QSqlRecord resolvedRecord(QSqlDatabase db, qint64 eventId);
QList<QSqlRecord> localRecords(QSqlDatabase db, const QString &account, const QString &peer, qint64 before, int limit);
qint64            matchingDelivery(QSqlDatabase db, qint64 contact, const Entry &, bool archived);
Page              readPage(QSqlDatabase db, const Key &, const QString &before, int limit);
// Caller has flushed pending EDB writes. This transaction includes events,
// identities, adjacency and the page checkpoint, or rolls all of them back.
bool storePage(QSqlDatabase db, const Key &, const QString &before, int limit, Page &);
// Erasure must invalidate coverage, including empty boundary pages.
bool invalidate(QSqlDatabase db, const QString &account, const QString &peer);
}
