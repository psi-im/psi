/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historymediaqueries.h"

#include <QSqlQuery>
#include <QVariant>

QList<HistoryMediaRefs::Link> HistoryMediaRefs::linksAfter(QSqlDatabase db, qint64 position, int limit,
                                                           QSqlError *error)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT r.position,r.publication_id,r.ordinal,e.m_text,e.extra_data,e.direction,c.acc_id,c.jid "
        "FROM message_media_refs r JOIN events e ON e.id=r.event_id JOIN contacts c ON c.id=e.contact_id "
        "WHERE r.position>:position ORDER BY r.position LIMIT :limit"));
    query.bindValue(QStringLiteral(":position"), position);
    query.bindValue(QStringLiteral(":limit"), limit);
    QList<Link> links;
    if (query.exec()) {
        while (query.next())
            links.append({ query.value(0).toLongLong(), query.value(1).toByteArray(), query.value(2).toInt(),
                           query.value(5).toInt(), query.value(3).toString(), query.value(4).toString(),
                           query.value(6).toString(), query.value(7).toString() });
    }
    if (query.lastError().isValid()) {
        if (error && !error->isValid())
            *error = query.lastError();
        return {};
    }
    return links;
}

std::optional<QSet<QByteArray>> HistoryMediaRefs::linked(QSqlDatabase db, const QList<QByteArray> &ids,
                                                         QSqlError *error)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT 1 FROM message_media_refs WHERE publication_id=:id LIMIT 1"));
    QSet<QByteArray> result;
    for (const auto &id : ids) {
        query.bindValue(QStringLiteral(":id"), id);
        if (!query.exec()) {
            if (error && !error->isValid())
                *error = query.lastError();
            return {};
        }
        if (query.next())
            result.insert(id);
        if (query.lastError().isValid()) {
            if (error && !error->isValid())
                *error = query.lastError();
            return {};
        }
        query.finish();
    }
    return result;
}
