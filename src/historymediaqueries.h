/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "historyarchivevalues.h"
#include <QByteArray>
#include <QList>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QString>
#include <optional>

// SQL projections contain values only. Protocol/JID reconstruction belongs to
// the GUI; these queries share the existing ordered history worker.
namespace HistoryMediaRefs {
struct Cursor {
    qint64 timestamp = 0, eventId = 0;
    int    ordinal = 0;
    bool   undated = false;
    bool   valid() const { return eventId > 0; }
};
// Values are also emitted by the SQL projection.
enum class Availability { Remote = 0, Partial = 1, Complete = 2 };
struct Storage {
    Availability availability    = Availability::Remote;
    quint64      downloadedBytes = 0;
};
struct Statistics {
    quint64 complete = 0, partial = 0, remote = 0;
    bool    stale = false;
};
struct Link {
    qint64     position = 0;
    QByteArray publicationId;
    int        ordinal = 0, direction = 0;
    QString    body, extra, account, peer;
    Cursor     cursor;
    Storage    storage;
};
QList<Link> chatLinks(QSqlDatabase db, const HistoryArchive::Key &chat, Cursor cursor, bool older, int limit,
                      QSqlError *error = nullptr, bool withStorage = false);
Statistics  chatStatistics(QSqlDatabase db, const HistoryArchive::Key &chat, QSqlError *error = nullptr);
// Attach an existing file registry read-only; no file is created or migrated here.
bool                            attachFileDatabase(QSqlDatabase db, const QString &path, QSqlError *error = nullptr);
QList<Link>                     linksAfter(QSqlDatabase db, qint64 position, int limit, QSqlError *error = nullptr);
std::optional<QSet<QByteArray>> linked(QSqlDatabase db, const QList<QByteArray> &ids, QSqlError *error = nullptr);
}
