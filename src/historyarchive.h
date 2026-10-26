/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "messagestate.h"

#include <QDateTime>
#include <QList>
#include <QSqlDatabase>
#include <QSqlRecord>
#include <QString>
#include <functional>

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
// A scoped observation, not a claim that the entire archive is cached. Count
// describes MAM events and may be approximate. A valid observation with count
// -1 records that this server omitted RSM metadata (avoid probing every page).
struct Snapshot {
    int       resultCount = -1;
    QString   lastUid;
    QDateTime observedAt;
    bool      stable = false;
};
// Last known incoming position acknowledged by one of our stored displayed
// markers. Read directly from the materialized message-state mask.
struct DisplayedCursor {
    QString   messageId;
    QDateTime time;
    qint64    eventId = 0;
};
struct Page {
    Snapshot     snapshot;
    QList<Entry> entries;
    int          requestIndex  = -1;
    bool         queryComplete = false;
    int          resultCount = -1, firstIndex = -1; // Optional RSM event coordinates.
    bool         checkpoint = true;                 // Indexed/forward islands do not replace a tail checkpoint.
    QString      previous; // Proven neighbour before the first entry, for forward catch-up.
    bool         found    = false;
    bool         complete = false;
    bool         stable   = false;
};

// Additive, atomic migration of the original EDB schema. Never reimports or
// rewrites existing event payloads. Safe to call again after an interrupted start.
bool needsMigration(QSqlDatabase db);
bool migrate(QSqlDatabase db, const std::function<void(qint64)> &progress = {});
// Return a unique original delivery, or zero when identity is absent/ambiguous.
// Index revision relationships transactionally whenever a live or MAM event
// is persisted. The earliest known revision anchors the row; its newest
// known child supplies text regardless of requested page boundaries.
bool recordRevision(QSqlDatabase db, qint64 eventId, qint64 contactId, int type, int direction, const QString &sender,
                    const QString &messageId, const QString &replaceId);
// Called inside the same transaction as event/revision writes. References
// survive unloaded targets; the materialized mask belongs to the root row.
bool recordMessageStates(QSqlDatabase db, const QList<qint64> &eventIds);
bool backfillMessageStates(QSqlDatabase db, const std::function<void(qint64)> &progress = {});
QSqlRecord        resolvedRecord(QSqlDatabase db, qint64 eventId);
QList<QSqlRecord> localRecords(QSqlDatabase db, const QString &account, const QString &peer, qint64 before, int limit);
DisplayedCursor   sentDisplayedCursor(QSqlDatabase db, const QString &account, const QString &peer);
qint64            matchingDelivery(QSqlDatabase db, qint64 contact, const Entry &, bool archived);
qint64 matchingOwnPlaintext(QSqlDatabase db, const Key &, const QString &id, const QString &sender, const QDateTime &);
Snapshot          readSnapshot(QSqlDatabase db, const Key &);
bool              storeSnapshot(QSqlDatabase db, const Key &, const Snapshot &);
Page   readPage(QSqlDatabase db, const Key &, const QString &before, int limit);
Page              readAfter(QSqlDatabase db, const Key &, const QString &after, int limit, int snapshotCount = -1);
Page              readIndex(QSqlDatabase db, const Key &, int index, int limit, int snapshotCount);
// Caller has flushed pending EDB writes. This transaction includes events,
// identities, adjacency and the page checkpoint, or rolls all of them back.
bool storePage(QSqlDatabase db, const Key &, const QString &before, int limit, Page &);
// Erasure must invalidate coverage, including empty boundary pages.
bool invalidate(QSqlDatabase db, const QString &account, const QString &peer);
}
