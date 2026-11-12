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
               "SELECT h.publication_id,f.id,f.complete,f.error,f.size,f.media_type FROM scope s JOIN "
               "message_media_hashes h ON "
               "h.publication_id=s.publication_id "
               "JOIN media_files.file_hashes fh ON fh.algorithm=h.algorithm AND fh.digest=h.digest "
               "JOIN media_files.files f ON f.id=fh.file_id AND f.partition='shares' "
               "AND (f.max_age IN (0,4294967295) OR f.created/1000+f.max_age>=CAST(strftime('%s','now') AS INTEGER)) "
               "JOIN message_media_resources m ON m.publication_id=h.publication_id "
               "WHERE m.size IS NULL OR m.size=f.size "
               "UNION ALL "
               "SELECT h.publication_id,f.id,f.complete,f.error,f.size,f.media_type FROM scope s JOIN "
               "message_media_hashes h ON "
               "h.publication_id=s.publication_id "
               "JOIN media_files.files f ON f.partition='shares:ranges' AND f.resource_key=h.range_key "
               "UNION ALL "
               "SELECT h.publication_id,f.id,f.complete,f.error,f.size,f.media_type FROM scope s "
               "JOIN message_media_hashes h ON h.publication_id=s.publication_id "
               "JOIN message_media_resources m ON m.publication_id=h.publication_id "
               "JOIN media_files.file_checksums fc ON fc.algorithm=h.algorithm AND fc.digest=h.digest AND fc.offset=0 "
               "JOIN media_files.files f ON f.id=fc.file_id AND f.partition='shares:ranges' AND "
               "f.scope_account=:account "
               "WHERE fc.length=f.size AND (m.size IS NULL OR m.size=f.size) "
               "UNION ALL "
               "SELECT m.publication_id,f.id,f.complete,f.error,f.size,f.media_type FROM scope s JOIN "
               "message_media_resources m ON "
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
    query.bindValue(":type", chat.contactKind >= 0 ? chat.contactKind : (chat.room ? 3 : 1));
    query.bindValue(":hidden", int(MessageState::LocalDeleted | MessageState::Retracted));
}

QString assetCte()
{
    // Resolve only occurrences visible in this conversation. Physical copies
    // can teach us additional equivalences (including URL-to-content mappings).
    // Aggregate file memberships before joining, avoiding a quadratic join on
    // repeated announcements of a popular file.
    return storageCte(QStringLiteral("SELECT r.publication_id ") + occurrenceFrom() + chatFilter())
        + QStringLiteral(
               ", known_types AS (SELECT publication_id,MIN(CASE WHEN media_type LIKE 'image/%' THEN media_type END) "
               "image, "
               "MIN(CASE WHEN media_type LIKE 'video/%' THEN media_type END) video, "
               "MIN(CASE WHEN media_type LIKE 'audio/%' THEN media_type END) audio, "
               "MIN(NULLIF(media_type,'')) mime FROM candidates WHERE complete=1 AND error='' "
               "AND media_type<>'application/octet-stream' GROUP BY publication_id), "
               "members AS (SELECT s.publication_id,m.asset_id,m.url_key,CASE WHEN t.image IS NOT NULL THEN 1 "
               "WHEN t.video IS NOT NULL OR t.audio IS NOT NULL OR m.mime LIKE 'audio/%' THEN 2 "
               "WHEN t.mime IS NOT NULL THEN 4 ELSE m.kind END kind,m.name, "
               "COALESCE(t.image,t.video,t.audio,t.mime,m.mime) mime FROM scope s "
               "JOIN message_media_resources m ON m.publication_id=s.publication_id "
               "LEFT JOIN known_types t ON t.publication_id=s.publication_id), "
               "url_members AS (SELECT DISTINCT m.asset_id,u.content_hash FROM members m "
               "JOIN media_files.url_content u ON u.account=:account AND u.url_key=m.url_key), "
               "selected_url_hashes AS (SELECT DISTINCT content_hash FROM url_members), "
               "url_aliases AS (SELECT s.content_hash,h.asset_id FROM selected_url_hashes s "
               "JOIN media_asset_aliases h ON h.account=:account AND h.alias='h:'||s.content_hash "
               "UNION ALL SELECT s.content_hash,h.asset_id FROM selected_url_hashes s "
               "JOIN media_files.url_content u ON u.account=:account AND u.content_hash=s.content_hash "
               "JOIN media_asset_aliases h ON h.account=:account AND h.alias='u:'||u.url_key), "
               "url_canonical AS (SELECT content_hash,MIN(asset_id) asset FROM url_aliases GROUP BY content_hash), "
               "url_assets AS (SELECT m.asset_id,MIN(k.asset) asset FROM url_members m "
               "JOIN url_canonical k ON k.content_hash=m.content_hash GROUP BY m.asset_id), "
               "selected_files AS (SELECT DISTINCT id FROM candidates), "
               "storage_aliases AS (SELECT s.id,h.asset_id FROM selected_files s "
               "JOIN media_files.file_hashes fh ON fh.file_id=s.id JOIN media_asset_aliases h "
               "ON h.account=:account AND h.alias='h:'||fh.algorithm||'+'||lower(hex(fh.digest)) "
               "UNION ALL SELECT s.id,h.asset_id FROM selected_files s "
               "JOIN media_files.files f ON f.id=s.id JOIN media_files.file_checksums fc ON fc.file_id=s.id "
               "AND fc.offset=0 AND fc.length=f.size JOIN media_asset_aliases h "
               "ON h.account=:account AND h.alias='h:'||fc.algorithm||'+'||lower(hex(fc.digest))), "
               "known_assets AS (SELECT id,MIN(asset_id) asset FROM storage_aliases GROUP BY id), "
               "file_assets AS (SELECT x.id,MIN(COALESCE(k.asset,m.asset_id)) asset FROM candidates x "
               "JOIN members m ON m.publication_id=x.publication_id LEFT JOIN known_assets k ON k.id=x.id GROUP BY "
               "x.id), "
               "canonical AS (SELECT m.asset_id,MIN(MIN(COALESCE(f.asset,m.asset_id),COALESCE(u.asset,m.asset_id))) "
               "asset FROM members m "
               "LEFT JOIN candidates x ON x.publication_id=m.publication_id "
               "LEFT JOIN file_assets f ON f.id=x.id LEFT JOIN url_assets u ON u.asset_id=m.asset_id GROUP BY "
               "m.asset_id), "
               "assets AS (SELECT c.asset,COALESCE(MIN(CASE WHEN m.kind=1 THEN m.publication_id END),"
               "MIN(CASE WHEN m.kind=2 THEN m.publication_id END),MIN(m.publication_id)) representative, "
               "MAX(m.kind=1)*1+MAX(m.kind=2)*2 kind_mask, "
               "MAX(COALESCE(a.status,0)) status, "
               "COALESCE(MAX(CASE WHEN a.status=2 THEN a.bytes END),MAX(a.bytes),0) bytes "
               "FROM members m JOIN canonical c ON c.asset_id=m.asset_id "
               "LEFT JOIN availability a ON a.publication_id=m.publication_id GROUP BY c.asset), "
               "filtered AS (SELECT * FROM assets WHERE "
               "(CASE WHEN (kind_mask&1)<>0 THEN 1 WHEN (kind_mask&2)<>0 THEN 2 ELSE 4 END & :types)<>0 "
               "AND ((1<<status)&:availability)<>0), "
               "ranked AS (SELECT *,ROW_NUMBER() OVER(ORDER BY asset) item_index,COUNT(*) OVER() total FROM "
               "filtered) ");
}
void bindAssets(QSqlQuery &q, const HistoryArchive::Key &chat, HistoryMediaRefs::Filter filter)
{
    bindChat(q, chat);
    q.bindValue(":types", filter.types & 7);
    q.bindValue(":availability", filter.availability & 7);
}
}

QList<HistoryMediaRefs::Link> HistoryMediaRefs::chatAssets(QSqlDatabase db, const HistoryArchive::Key &chat,
                                                           const Query &request, QSqlError *error)
{
    if (chat.account.isEmpty() || chat.peer.isEmpty() || request.limit < 1 || request.limit > 201) {
        if (error)
            *error = QSqlError("Invalid chat resource query", {}, QSqlError::StatementError);
        return {};
    }
    QString sql = assetCte();
    if (request.messagesFor) {
        sql += QStringLiteral(
                   "SELECT r.position,r.publication_id,r.ordinal,e.direction,substr(p.m_text,1,256),'',c.acc_id,c.jid,"
                   "e.history_date_ms,e.id,m.name,m.mime,m.kind,a.status,a.bytes,a.item_index,a.total,a.asset "
                   "FROM ranked a JOIN canonical k ON k.asset=a.asset JOIN message_media_resources m ON "
                   "m.asset_id=k.asset_id "
                   "JOIN message_media_refs r ON r.publication_id=m.publication_id ")
            + QStringLiteral("JOIN events p ON p.id=r.event_id JOIN contacts c ON c.id=p.contact_id ")
            + QStringLiteral("LEFT JOIN message_revisions rev ON rev.event_id=p.id "
                             "JOIN events e ON e.id=COALESCE(rev.root_id,p.id) "
                             "JOIN scope s ON s.publication_id=r.publication_id "
                             "WHERE a.asset=:asset AND e.id>:before GROUP BY e.id ORDER BY e.id LIMIT :limit");
    } else {
        sql += QStringLiteral(
            "SELECT r.position,r.publication_id,r.ordinal,e.direction,e.m_text,e.extra_data,c.acc_id,c.jid,"
            "e.history_date_ms,e.id,m.name,m.mime,m.kind,a.status,a.bytes,a.item_index,a.total,a.asset "
            "FROM ranked a JOIN message_media_refs r ON r.publication_id=a.representative "
            "JOIN members m ON m.publication_id=r.publication_id "
            "JOIN events e ON e.id=r.event_id JOIN contacts c ON c.id=e.contact_id ");
        if (!request.source.isEmpty()) {
            const auto anchors = QStringLiteral(
                "SELECT k.asset FROM media_asset_aliases h "
                "JOIN canonical k ON k.asset_id=h.asset_id WHERE h.account=:account "
                "AND h.alias IN (:hash,:url) UNION SELECT k.asset FROM candidates x "
                "JOIN members m ON m.publication_id=x.publication_id "
                "JOIN canonical k ON k.asset_id=m.asset_id JOIN media_files.file_hashes fh ON fh.file_id=x.id "
                "WHERE CAST(fh.algorithm||'+'||lower(hex(fh.digest)) AS TEXT)=:source "
                "UNION SELECT k.asset FROM candidates x JOIN members m ON m.publication_id=x.publication_id "
                "JOIN canonical k ON k.asset_id=m.asset_id JOIN media_files.file_checksums fc ON fc.file_id=x.id "
                "JOIN media_files.files f ON f.id=x.id WHERE fc.offset=0 AND fc.length=f.size "
                "AND CAST(fc.algorithm||'+'||lower(hex(fc.digest)) AS TEXT)=:source");
            if (request.adjacent)
                sql += QStringLiteral("WHERE a.asset") + (request.older ? QStringLiteral("<") : QStringLiteral(">"))
                    + QStringLiteral("COALESCE((SELECT MIN(asset) FROM (") + anchors
                    + QStringLiteral(")),NULLIF(:asset,0)) ");
            else
                sql += QStringLiteral("WHERE a.asset IN (") + anchors + QStringLiteral(") ");
        } else if (request.cursor.assetId)
            sql += request.older ? QStringLiteral("WHERE a.asset<:asset ") : QStringLiteral("WHERE a.asset>:asset ");
        sql += request.older ? QStringLiteral("ORDER BY a.asset DESC LIMIT :limit")
                             : QStringLiteral("ORDER BY a.asset ASC LIMIT :limit");
    }
    QSqlQuery q(db);
    q.prepare(sql);
    bindAssets(q, chat, request.filter);
    q.bindValue(":limit", request.limit);
    if (request.messagesFor) {
        q.bindValue(":asset", request.messagesFor);
        q.bindValue(":before", request.afterEvent);
    } else if (!request.source.isEmpty()) {
        q.bindValue(":hash", QStringLiteral("h:") + request.source);
        q.bindValue(":url", QStringLiteral("u:") + request.source);
        q.bindValue(":source", request.source);
        if (request.adjacent)
            q.bindValue(":asset", request.cursor.assetId);
    } else if (request.cursor.assetId)
        q.bindValue(":asset", request.cursor.assetId);
    QList<Link> result;
    if (q.exec())
        while (q.next()) {
            Link row { q.value(0).toLongLong(),      q.value(1).toByteArray(),
                       q.value(2).toInt(),           q.value(3).toInt(),
                       q.value(4).toString(),        q.value(5).toString(),
                       q.value(6).toString(),        q.value(7).toString(),
                       { q.value(17).toLongLong() }, { Availability(q.value(13).toInt()), q.value(14).toULongLong() } };
            row.name      = q.value(10).toString();
            row.mime      = q.value(11).toString();
            row.kind      = q.value(12).toInt();
            row.index     = q.value(15).toULongLong();
            row.total     = q.value(16).toULongLong();
            row.eventId   = q.value(9).toLongLong();
            row.timestamp = q.value(8).toLongLong();
            row.undated   = q.value(8).isNull();
            result.append(row);
        }
    if (q.lastError().isValid()) {
        if (error)
            *error = q.lastError();
        return {};
    }
    return result;
}

HistoryMediaRefs::Statistics HistoryMediaRefs::chatAssetStatistics(QSqlDatabase db, const HistoryArchive::Key &chat,
                                                                   Filter filter, QSqlError *error)
{
    QSqlQuery q(db);
    q.prepare(assetCte()
              + QStringLiteral("SELECT COALESCE(SUM(status=2),0),COALESCE(SUM(status=1),0),"
                               "COALESCE(SUM(status=0),0) FROM filtered"));
    bindAssets(q, chat, filter);
    if (q.exec() && q.next())
        return { q.value(0).toULongLong(), q.value(1).toULongLong(), q.value(2).toULongLong(), false };
    if (error)
        *error = q.lastError();
    return {};
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
