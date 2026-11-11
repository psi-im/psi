/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historymediaqueries.h"
#include "messagestate.h"

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

QList<HistoryMediaRefs::Link> HistoryMediaRefs::chatLinks(QSqlDatabase db, const HistoryArchive::Key &chat,
                                                          Cursor cursor, bool older, int limit, QSqlError *error)
{
    if (chat.account.isEmpty() || chat.peer.isEmpty() || limit < 1 || limit > 201) {
        if (error)
            *error = QSqlError("Invalid chat media query", {}, QSqlError::StatementError);
        return {};
    }
    const QString order = older ? QStringLiteral(" DESC") : QStringLiteral(" ASC");
    // Keep the logical message position but announce only its current revision,
    // matching HistoryArchive::resolvedRecord and localRecords.
    QString sql = QStringLiteral(
        "SELECT r.position,r.publication_id,r.ordinal,p.m_text,p.extra_data,e.direction,c.acc_id,c.jid,"
        "e.history_date_ms,e.id "
        "FROM contacts c JOIN events e ON e.contact_id=c.id "
        "LEFT JOIN message_revisions root ON root.event_id=e.id "
        "JOIN events p ON p.id=COALESCE((SELECT rev.event_id FROM message_revisions rev "
        "JOIN events v ON v.id=rev.event_id WHERE rev.root_id=e.id "
        "ORDER BY v.history_date_ms DESC,v.id DESC LIMIT 1),e.id) "
        "JOIN message_media_refs r ON r.event_id=p.id "
        "WHERE c.acc_id=:account AND c.jid=:peer AND c.type=:type "
        "AND (root.event_id IS NULL OR root.root_id=e.id) AND (e.message_flags & :hidden)=0 ");
    if (cursor.valid()) {
        if (cursor.undated) {
            sql += older ? QStringLiteral("AND e.history_date_ms IS NULL AND (e.id,r.ordinal)<(:event,:ordinal) ")
                         : QStringLiteral("AND ((e.history_date_ms IS NULL AND (e.id,r.ordinal)>(:event,:ordinal)) "
                                          "OR e.history_date_ms IS NOT NULL) ");
        } else {
            sql += older ? QStringLiteral("AND ((e.history_date_ms,e.id,r.ordinal)<(:time,:event,:ordinal) "
                                          "OR e.history_date_ms IS NULL) ")
                         : QStringLiteral("AND (e.history_date_ms,e.id,r.ordinal)>(:time,:event,:ordinal) ");
        }
    }
    sql += QStringLiteral("ORDER BY e.history_date_ms") + order + QStringLiteral(",e.id") + order
        + QStringLiteral(",r.ordinal") + order + QStringLiteral(" LIMIT :limit");
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":account", chat.account);
    query.bindValue(":peer", chat.peer);
    query.bindValue(":type", chat.room ? 3 : 1);
    query.bindValue(":hidden", int(MessageState::LocalDeleted | MessageState::Retracted));
    query.bindValue(":limit", limit);
    if (cursor.valid()) {
        if (!cursor.undated)
            query.bindValue(":time", cursor.timestamp);
        query.bindValue(":event", cursor.eventId);
        query.bindValue(":ordinal", cursor.ordinal);
    }
    QList<Link> result;
    if (query.exec())
        while (query.next())
            result.append({ query.value(0).toLongLong(),
                            query.value(1).toByteArray(),
                            query.value(2).toInt(),
                            query.value(5).toInt(),
                            query.value(3).toString(),
                            query.value(4).toString(),
                            query.value(6).toString(),
                            query.value(7).toString(),
                            { query.value(8).toLongLong(), query.value(9).toLongLong(), query.value(2).toInt(),
                              query.value(8).isNull() } });
    if (query.lastError().isValid()) {
        if (error && !error->isValid())
            *error = query.lastError();
        return {};
    }
    return result;
}
