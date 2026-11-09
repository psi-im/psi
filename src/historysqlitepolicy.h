/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

// The SQL worker owns ordinary writes and MAM page commits, so fsync no
// longer blocks the GUI. Keep SQLite's FULL transaction durability and normal
// automatic WAL checkpointing; a separate checkpoint thread is unnecessary.
namespace HistorySqlitePolicy {
inline bool configure(QSqlDatabase db)
{
    if (!db.isOpen())
        return false;
    QSqlQuery q(db);
    const bool wal = q.exec(QStringLiteral("PRAGMA journal_mode=WAL")) && q.next()
        && q.value(0).toString().compare(QLatin1String("wal"), Qt::CaseInsensitive) == 0;
    q.finish();
    if (!q.exec(QStringLiteral("PRAGMA synchronous=FULL")))
        qWarning() << "Could not enable durable history synchronization:" << q.lastError().text();
    if (!wal)
        qWarning() << "History database does not support WAL; keeping durable journal mode";
    return wal;
}
}
