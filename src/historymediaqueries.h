/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

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
struct Link {
    qint64     position = 0;
    QByteArray publicationId;
    int        ordinal = 0, direction = 0;
    QString    body, extra, account, peer;
};
QList<Link>                     linksAfter(QSqlDatabase db, qint64 position, int limit, QSqlError *error = nullptr);
std::optional<QSet<QByteArray>> linked(QSqlDatabase db, const QList<QByteArray> &ids, QSqlError *error = nullptr);
}
