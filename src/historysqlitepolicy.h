/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>

// The history database is both a durable local journal and a re-fetchable MAM
// cache. WAL/NORMAL keeps transactions consistent on a crash, while avoiding a
// per-commit fdatasync on the GUI thread. A power failure may lose transactions
// not yet synced by the background checkpoint (unlike FULL).
//
// Do not disable SQLite's automatic checkpoint until a background checkpoint
// connection has actually opened. Otherwise a failed worker could leave a WAL
// growing without bound.
namespace HistorySqlitePolicy {
inline bool configure(QSqlDatabase db)
{
    if (!db.isOpen())
        return false;
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA journal_mode=WAL")) || !q.next()
        || q.value(0).toString().compare(QLatin1String("wal"), Qt::CaseInsensitive) != 0) {
        qWarning() << "History database does not support WAL; keeping the existing durable journal mode";
        return false;
    }
    q.finish();
    if (!q.exec(QStringLiteral("PRAGMA synchronous=NORMAL"))) {
        qWarning() << "Could not select WAL/NORMAL history synchronization";
        return false;
    }
    return true;
}

inline bool useBackgroundCheckpoint(QSqlDatabase db)
{
    // Per-connection setting; all other connections retain their own policy.
    QSqlQuery q(db);
    return q.exec(QStringLiteral("PRAGMA wal_autocheckpoint=0"));
}
}
