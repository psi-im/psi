/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "historyarchivevalues.h"

#include <QDateTime>
#include <QList>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlRecord>
#include <QString>
#include <QStringList>
#include <functional>
#include <optional>

namespace HistoryArchive {
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
// Read helpers distinguish a cache miss from SQL failure through an optional
// error accumulator. Nested queries preserve the first error; callers must
// discard the entire projection if it is valid.
bool              isDecodedRecord(const QSqlRecord &record);
QSqlRecord        resolvedRecord(QSqlDatabase db, qint64 eventId, QSqlError *error = nullptr);
QList<QSqlRecord> localRecords(QSqlDatabase db, const QString &account, const QString &peer, qint64 before, int limit,
                               QSqlError *error = nullptr);
DisplayedCursor   sentDisplayedCursor(QSqlDatabase db, const QString &account, const QString &peer,
                                      QSqlError *error = nullptr);
qint64   matchingDelivery(QSqlDatabase db, qint64 contact, const Entry &, bool archived, QSqlError *error = nullptr);
qint64   matchingOwnPlaintext(QSqlDatabase db, const Key &, const QString &id, const QString &sender, const QDateTime &,
                              QSqlError *error = nullptr);
Snapshot readSnapshot(QSqlDatabase db, const Key &, QSqlError *error = nullptr);
bool              storeSnapshot(QSqlDatabase db, const Key &, const Snapshot &);
Page              readPage(QSqlDatabase db, const Key &, const QString &before, int limit, QSqlError *error = nullptr);
Page              readAfter(QSqlDatabase db, const Key &, const QString &after, int limit, int snapshotCount = -1,
                            QSqlError *error = nullptr);
Page readIndex(QSqlDatabase db, const Key &, int index, int limit, int snapshotCount, QSqlError *error = nullptr);
// Caller has flushed pending EDB writes. This transaction includes events,
// identities, adjacency and the page checkpoint, or rolls all of them back.
bool storePage(QSqlDatabase db, const Key &, const QString &before, int limit, Page &,
               const std::function<bool(qint64)> &eventStored = {});
// Compatibility wrapper for device-local deletion, including suppression and
// transactional media-ref release. New runtime callers use the async EDB API.
// The caller must flush pending EDB writes first and must not send XEP-0424.
bool scrubStoredMessage(QSqlDatabase db, const Key &, qint64 eventId);
// All identities are scoped to an account, direct conversation and direction.
// Delete/check own transactions; a pure match may share the worker's pending batch.
LocalDeletionResult deleteLocalMessage(QSqlDatabase db, const Key &, qint64 eventId, const QString &clientId = {},
                                       int direction = 0);
LocalDeletionResult checkLocalSuppression(QSqlDatabase db, const Key &, Entry);
std::optional<bool> matchesLocalSuppression(QSqlDatabase db, const Key &, const Entry &);
// Internal transaction participants shared by live appends, MAM and migration.
std::optional<bool> applyLocalSuppression(QSqlDatabase         db, const Key &, Entry &,
                                          LocalDeletionResult *changes = nullptr);
bool                backfillLocalSuppressions(QSqlDatabase db, const std::function<void(qint64)> &progress = {});
// Erasure must invalidate coverage, including empty boundary pages.
bool invalidate(QSqlDatabase db, const QString &account, const QString &peer);
}
