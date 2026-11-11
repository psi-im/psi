/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historymediaqueries.h"
#include "messagestate.h"

#include <QFileInfo>
#include <QSqlQuery>
#include <QUrl>
#include <QVariant>

namespace {
QString occurrenceFrom()
{
    return QStringLiteral("FROM contacts c JOIN events e ON e.contact_id=c.id "
                          "LEFT JOIN message_revisions root ON root.event_id=e.id "
                          "JOIN events p ON p.id=COALESCE((SELECT rev.event_id FROM message_revisions rev "
                          "JOIN events v ON v.id=rev.event_id WHERE rev.root_id=e.id "
                          "ORDER BY v.history_date_ms DESC,v.id DESC LIMIT 1),e.id) "
                          "JOIN message_media_refs r ON r.event_id=p.id ");
}
QString chatFilter()
{
    return QStringLiteral("WHERE c.acc_id=:account AND c.jid=:peer AND c.type=:type "
                          "AND (root.event_id IS NULL OR root.root_id=e.id) AND (e.message_flags & :hidden)=0 ");
}
// All candidates are physical file records. Hash aliases remain alternatives,
// not extra occurrences; account-scoped range keys prevent cross-account reuse.
// The canonical hash expression is explicitly TEXT, matching the URL mapping
// column and its expression index. Without that affinity SQLite may choose a
// full cache-file loop instead of an indexed hash lookup.
QString storageCte(const QString &scope)
{
    return QStringLiteral("WITH scope AS (") + scope
        + QStringLiteral(
               "), candidates AS ("
               "SELECT h.publication_id,f.id,f.complete,f.error,f.size FROM scope s JOIN message_media_hashes h ON "
               "h.publication_id=s.publication_id "
               "JOIN media_files.file_hashes fh ON fh.algorithm=h.algorithm AND fh.digest=h.digest "
               "JOIN media_files.files f ON f.id=fh.file_id AND f.partition='shares' "
               "AND (f.max_age IN (0,4294967295) OR f.created/1000+f.max_age>=CAST(strftime('%s','now') AS INTEGER)) "
               "JOIN message_media_resources m ON m.publication_id=h.publication_id "
               "WHERE m.size IS NULL OR m.size=f.size "
               "UNION ALL "
               "SELECT h.publication_id,f.id,f.complete,f.error,f.size FROM scope s JOIN message_media_hashes h ON "
               "h.publication_id=s.publication_id "
               "JOIN media_files.files f ON f.partition='shares:ranges' AND f.resource_key=h.range_key "
               "UNION ALL "
               "SELECT h.publication_id,f.id,f.complete,f.error,f.size FROM scope s "
               "JOIN message_media_hashes h ON h.publication_id=s.publication_id "
               "JOIN message_media_resources m ON m.publication_id=h.publication_id "
               "JOIN media_files.file_checksums fc ON fc.algorithm=h.algorithm AND fc.digest=h.digest AND fc.offset=0 "
               "JOIN media_files.files f ON f.id=fc.file_id AND f.partition='shares:ranges' AND "
               "f.scope_account=:account "
               "WHERE fc.length=f.size AND (m.size IS NULL OR m.size=f.size) "
               "UNION ALL "
               "SELECT m.publication_id,f.id,f.complete,f.error,f.size FROM scope s JOIN message_media_resources m ON "
               "m.publication_id=s.publication_id "
               "JOIN message_media_refs r ON r.publication_id=m.publication_id "
               "JOIN events e ON e.id=r.event_id JOIN contacts c ON c.id=e.contact_id "
               "JOIN media_files.url_content u ON u.account=c.acc_id AND u.url_key=m.url_key "
               "JOIN media_files.file_hashes fh ON CAST(fh.algorithm || '+' || lower(hex(fh.digest)) AS "
               "TEXT)=u.content_hash "
               "JOIN media_files.files f ON f.id=fh.file_id AND f.partition='shares' "
               "AND (f.max_age IN (0,4294967295) OR f.created/1000+f.max_age>=CAST(strftime('%s','now') AS INTEGER))), "
               "availability AS (SELECT x.publication_id,MAX(CASE "
               "WHEN x.complete=1 AND x.error='' THEN 2 "
               "WHEN EXISTS(SELECT 1 FROM media_files.file_ranges fr WHERE fr.file_id=x.id) THEN 1 ELSE 0 END) status,"
               "COALESCE(MAX(CASE WHEN x.complete=1 AND x.error='' THEN x.size END), "
               "MAX(COALESCE((SELECT SUM(fr.length) FROM media_files.file_ranges fr WHERE fr.file_id=x.id),0)),0) "
               "bytes "
               "FROM candidates x GROUP BY x.publication_id) ");
}
void bindChat(QSqlQuery &query, const HistoryArchive::Key &chat)
{
    query.bindValue(":account", chat.account);
    query.bindValue(":peer", chat.peer);
    query.bindValue(":type", chat.room ? 3 : 1);
    query.bindValue(":hidden", int(MessageState::LocalDeleted | MessageState::Retracted));
}
}

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
                                                          Cursor cursor, bool older, int limit, QSqlError *error,
                                                          bool withStorage)
{
    if (chat.account.isEmpty() || chat.peer.isEmpty() || limit < 1 || limit > 201) {
        if (error)
            *error = QSqlError("Invalid chat media query", {}, QSqlError::StatementError);
        return {};
    }
    const QString order   = older ? QStringLiteral(" DESC") : QStringLiteral(" ASC");
    QString       filters = chatFilter();
    if (cursor.valid()) {
        if (cursor.undated) {
            filters += older ? QStringLiteral("AND e.history_date_ms IS NULL AND (e.id,r.ordinal)<(:event,:ordinal) ")
                             : QStringLiteral("AND ((e.history_date_ms IS NULL AND (e.id,r.ordinal)>(:event,:ordinal)) "
                                              "OR e.history_date_ms IS NOT NULL) ");
        } else {
            filters += older ? QStringLiteral("AND ((e.history_date_ms,e.id,r.ordinal)<(:time,:event,:ordinal) "
                                              "OR e.history_date_ms IS NULL) ")
                             : QStringLiteral("AND (e.history_date_ms,e.id,r.ordinal)>(:time,:event,:ordinal) ");
        }
    }
    const auto ordering = QStringLiteral("ORDER BY e.history_date_ms") + order + QStringLiteral(",e.id") + order
        + QStringLiteral(",r.ordinal") + order + QStringLiteral(" LIMIT :limit");
    const auto scope = QStringLiteral("SELECT r.publication_id ") + occurrenceFrom() + filters + ordering;
    QString    sql   = (withStorage ? storageCte(scope) : QString())
        + QStringLiteral(
                      "SELECT r.position,r.publication_id,r.ordinal,p.m_text,p.extra_data,e.direction,c.acc_id,c.jid,"
                      "e.history_date_ms,e.id")
        + (withStorage ? QStringLiteral(",COALESCE(a.status,0),COALESCE(a.bytes,0) ") : QStringLiteral(" "))
        + occurrenceFrom()
        + (withStorage ? QStringLiteral("LEFT JOIN availability a ON a.publication_id=r.publication_id ") : QString())
        + filters + ordering;
    QSqlQuery query(db);
    query.prepare(sql);
    bindChat(query, chat);
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
                              query.value(8).isNull() },
                            withStorage
                                ? Storage { Availability(query.value(10).toInt()), query.value(11).toULongLong() }
                                : Storage() });
    if (query.lastError().isValid()) {
        if (error && !error->isValid())
            *error = query.lastError();
        return {};
    }
    return result;
}

HistoryMediaRefs::Statistics HistoryMediaRefs::chatStatistics(QSqlDatabase db, const HistoryArchive::Key &chat,
                                                              QSqlError *error)
{
    if (chat.account.isEmpty() || chat.peer.isEmpty()) {
        if (error)
            *error = QSqlError("Invalid chat media scope", {}, QSqlError::StatementError);
        return {};
    }
    QSqlQuery query(db);
    query.prepare(
        storageCte(QStringLiteral("SELECT r.publication_id ") + occurrenceFrom() + chatFilter())
        + QStringLiteral("SELECT COALESCE(SUM(COALESCE(a.status,0)=2),0),COALESCE(SUM(COALESCE(a.status,0)=1),0),"
                         "COALESCE(SUM(COALESCE(a.status,0)=0),0) FROM scope r ")
        + QStringLiteral("LEFT JOIN availability a ON a.publication_id=r.publication_id"));
    bindChat(query, chat);
    if (query.exec() && query.next())
        return { query.value(0).toULongLong(), query.value(1).toULongLong(), query.value(2).toULongLong(), false };
    if (error && !error->isValid())
        *error = query.lastError();
    return {};
}

bool HistoryMediaRefs::attachFileDatabase(QSqlDatabase db, const QString &path, QSqlError *error)
{
    const QFileInfo file(path);
    const auto      canonical = file.canonicalFilePath();
    if (!file.isFile() || canonical.isEmpty()) {
        if (error)
            *error = QSqlError("File registry unavailable", {}, QSqlError::ConnectionError);
        return false;
    }
    QSqlQuery query(db);
    if (query.exec("PRAGMA database_list")) {
        while (query.next())
            if (query.value(1).toString() == QLatin1String("media_files")) {
                if (QFileInfo(query.value(2).toString()).canonicalFilePath() == canonical)
                    return true;
                if (error)
                    *error = QSqlError("File registry changed during history access", {}, QSqlError::ConnectionError);
                return false;
            }
    }
    query.finish();
    // SQLite URI mode prevents accidental writes or creation by this reader.
    QUrl uri = QUrl::fromLocalFile(canonical);
    uri.setQuery("mode=ro");
    query.prepare("ATTACH DATABASE :path AS media_files");
    query.bindValue(":path", uri.toString(QUrl::FullyEncoded));
    if (query.exec())
        return true;
    if (error && !error->isValid())
        *error = query.lastError();
    return false;
}
