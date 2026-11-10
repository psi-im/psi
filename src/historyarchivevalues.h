/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "messagestate.h"
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

// Immutable history projections shared by protocol serialization, the SQL
// worker and presentation. This header has no SQL or protocol object dependency.
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
    bool      storable   = true;
    bool      suppressed = false; // An archive position without a renderable/decryptable payload.
};
struct LocalDeletionResult {
    bool          success = false, suppressed = false;
    QList<qint64> hiddenEvents;
    QStringList   incomingIds, outgoingIds;
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
    Snapshot            snapshot;
    LocalDeletionResult localChanges;
    QList<Entry>        entries;
    int                 requestIndex  = -1;
    bool                queryComplete = false;
    int                 resultCount = -1, firstIndex = -1; // Optional RSM event coordinates.
    bool                checkpoint = true;                 // Indexed/forward islands do not replace a tail checkpoint.
    QString             previous; // Proven neighbour before the first entry, for forward catch-up.
    bool                found    = false;
    bool                complete = false;
    bool                stable   = false;
};

}
