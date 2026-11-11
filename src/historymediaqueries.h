/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "historyarchivevalues.h"
#include "mediafilter.h"
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
    qint64 assetId = 0;
    bool   valid() const { return assetId > 0; }
};
enum ContentType { Image = 1, Video = 2, Other = 4 };
using Filter = MediaFilter;
struct Query {
    Filter  filter;
    Cursor  cursor;
    bool    older = true;
    int     limit = 40;
    QString source;
    bool    adjacent    = false;
    qint64  messagesFor = 0;
    qint64  afterEvent  = 0;
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
    quint64 total() const { return complete + partial + remote; }
};
struct Link {
    qint64     position = 0;
    QByteArray publicationId;
    int        ordinal = 0, direction = 0;
    QString    body, extra, account, peer;
    Cursor     cursor;
    Storage    storage;
    QString    name, mime;
    int        kind  = Other;
    quint64    index = 0, total = 0;
    qint64     eventId = 0, timestamp = 0;
    bool       undated = false;
};
QList<Link> chatAssets(QSqlDatabase db, const HistoryArchive::Key &chat, const Query &, QSqlError *error = nullptr);
Statistics  chatAssetStatistics(QSqlDatabase db, const HistoryArchive::Key &chat, Filter, QSqlError *error = nullptr);
// Attach an existing file registry read-only; no file is created or migrated here.
bool                            attachFileDatabase(QSqlDatabase db, const QString &path, QSqlError *error = nullptr);
QList<Link>                     linksAfter(QSqlDatabase db, qint64 position, int limit, QSqlError *error = nullptr);
std::optional<QSet<QByteArray>> linked(QSqlDatabase db, const QList<QByteArray> &ids, QSqlError *error = nullptr);
}
