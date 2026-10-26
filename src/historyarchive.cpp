/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historyarchive.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlError>
#include <QSqlField>
#include <QSqlQuery>
#include <QTimeZone>
#include <QVariant>
#include <QXmlStreamReader>

namespace HistoryArchive {
namespace {
    bool exec(QSqlDatabase db, const QString &sql)
    {
        QSqlQuery q(db);
        if (q.exec(sql))
            return true;
        qWarning().noquote() << "[history-cache]" << q.lastError().text();
        return false;
    }
    void bindKey(QSqlQuery &q, const Key &key)
    {
        q.bindValue(":account", key.account);
        q.bindValue(":archive", key.archive);
        q.bindValue(":peer", key.peer);
    }
    const QString scope = QStringLiteral("account=:account AND archive=:archive AND peer=:peer");
    QString       nonNull(const QString &s) { return s.isNull() ? QStringLiteral("") : s; }
    bool          ownSenderMatches(const QString &recorded, const QString &sender)
    {
        // Older outgoing EDB records stored the bare account JID (or no
        // sender). A recorded full JID must still match the exact resource.
        return recorded.isEmpty() || recorded == sender || recorded == sender.section(QLatin1Char('/'), 0, 0);
    }
    bool          revisionsExist(QSqlDatabase db)
    {
        QSqlQuery q(db);
        return q.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='message_revisions'") && q.next();
    }
    QString cachedReplaceId(const QString &xml)
    {
        QXmlStreamReader reader(xml);
        while (!reader.atEnd()) {
            reader.readNext();
            if (reader.isStartElement() && reader.name() == QLatin1String("replace")
                && reader.namespaceUri() == QLatin1String("urn:xmpp:message-correct:0"))
                return reader.attributes().value(QStringLiteral("id")).toString();
        }
        return {};
    }
}

bool needsMigration(QSqlDatabase db)
{
    QSqlQuery q(db);
    if (!q.exec("SELECT value FROM system WHERE key='version'") || !q.next() || q.value(0).toString() != "0.4"
        || !revisionsExist(db) || !db.tables().contains(QStringLiteral("archive_index_pages"))
        || !db.tables().contains(QStringLiteral("archive_snapshots")))
        return true;
    return !q.exec("SELECT 1 FROM events WHERE history_date_ms IS NULL LIMIT 1") || q.next();
}

bool migrate(QSqlDatabase db, const std::function<void(qint64)> &progress)
{
    QSqlQuery version(db);
    if (!version.exec("SELECT value FROM system WHERE key='version'"))
        return false;
    const QString originalVersion = version.next() ? version.value(0).toString() : QString();
    if (!originalVersion.isEmpty() && originalVersion != QLatin1String("0.1") && originalVersion != QLatin1String("0.2")
        && originalVersion != QLatin1String("0.3") && originalVersion != QLatin1String("0.4")) {
        qWarning("[history-cache] Unsupported history schema version");
        return false; // Never downgrade a database created by a future client.
    }
    version.finish();
    if (!db.transaction())
        return false;
    QSqlQuery columns(db);
    bool      hasState = false, hasTime = false;
    if (!columns.exec("PRAGMA table_info(events)")) {
        db.rollback();
        return false;
    }
    while (columns.next()) {
        hasState |= columns.value(1).toString() == QLatin1String("history_state");
        hasTime |= columns.value(1).toString() == QLatin1String("history_date_ms");
    }
    columns.finish();
    bool ok = hasState || exec(db, "ALTER TABLE events ADD COLUMN history_state INTEGER NOT NULL DEFAULT 0");
    ok      = ok && (hasTime || exec(db, "ALTER TABLE events ADD COLUMN history_date_ms INTEGER"));
    // Keep the original date text for lossless compatibility. SQLite's lexical
    // ISO date order is wrong when local and MAM timestamps use different UTC
    // offsets; use a separate UTC millisecond ordering key. NULL also covers
    // rows inserted by an older client after a downgrade.
    qint64 lastId = 0;
    while (ok) {
        QSqlQuery dates(db);
        dates.prepare("SELECT id,date FROM events WHERE history_date_ms IS NULL AND id>:id ORDER BY id LIMIT 1000");
        dates.bindValue(":id", lastId);
        if (!dates.exec()) {
            ok = false;
            break;
        }
        QList<QPair<qint64, qint64>> batch;
        while (dates.next()) {
            const auto date = dates.value(1).toDateTime();
            batch.append(qMakePair(dates.value(0).toLongLong(), date.isValid() ? date.toMSecsSinceEpoch() : qint64(0)));
        }
        dates.finish();
        if (batch.isEmpty())
            break;
        QSqlQuery update(db);
        update.prepare("UPDATE events SET history_date_ms=:time WHERE id=:id");
        for (const auto &row : batch) {
            update.bindValue(":id", row.first);
            update.bindValue(":time", row.second);
            if (!update.exec()) {
                ok = false;
                break;
            }
        }
        lastId = batch.last().first;
        if (progress)
            progress(lastId);
    }
    const bool        backfillRevisions = !revisionsExist(db);
    const QStringList statements {
        "CREATE TABLE IF NOT EXISTS archive_snapshots ("
        "account TEXT NOT NULL,archive TEXT NOT NULL,peer TEXT NOT NULL,"
        "result_count INTEGER NOT NULL,last_uid TEXT,observed_at INTEGER NOT NULL,stable INTEGER NOT NULL,"
        "PRIMARY KEY(account,archive,peer))",
        "CREATE TABLE IF NOT EXISTS archive_items ("
        "account TEXT NOT NULL, archive TEXT NOT NULL, peer TEXT NOT NULL, uid TEXT NOT NULL, "
        "event_id INTEGER NOT NULL REFERENCES events(id) ON DELETE CASCADE, xml TEXT NOT NULL, "
        "previous_uid TEXT, next_uid TEXT, stable INTEGER NOT NULL, "
        "PRIMARY KEY(account,archive,peer,uid))",
        "CREATE INDEX IF NOT EXISTS archive_event ON archive_items(event_id)",
        "CREATE INDEX IF NOT EXISTS history_contact_time ON events(contact_id,history_date_ms,id)",
        "CREATE INDEX IF NOT EXISTS history_account_peer ON contacts(acc_id,jid)",
        "CREATE INDEX IF NOT EXISTS history_unordered_dates ON events(id) WHERE history_date_ms IS NULL",
        "CREATE TABLE IF NOT EXISTS archive_pages ("
        "id INTEGER PRIMARY KEY, account TEXT NOT NULL, archive TEXT NOT NULL, peer TEXT NOT NULL, "
        "before_uid TEXT NOT NULL, page_size INTEGER NOT NULL, item_count INTEGER NOT NULL, "
        "complete INTEGER NOT NULL, stable INTEGER NOT NULL, result_count INTEGER, first_index INTEGER, "
        "UNIQUE(account,archive,peer,before_uid,page_size))",
        "CREATE TABLE IF NOT EXISTS archive_page_items ("
        "page_id INTEGER NOT NULL REFERENCES archive_pages(id) ON DELETE CASCADE, position INTEGER NOT NULL, "
        "event_id INTEGER NOT NULL REFERENCES events(id) ON DELETE CASCADE, uid TEXT NOT NULL, "
        "PRIMARY KEY(page_id,position))",
        "CREATE INDEX IF NOT EXISTS archive_page_event ON archive_page_items(event_id)",
        "CREATE TABLE IF NOT EXISTS archive_index_pages ("
        "id INTEGER PRIMARY KEY,account TEXT NOT NULL,archive TEXT NOT NULL,peer TEXT NOT NULL,"
        "request_index INTEGER NOT NULL,page_size INTEGER NOT NULL,item_count INTEGER NOT NULL,"
        "result_count INTEGER NOT NULL,first_index INTEGER NOT NULL,complete INTEGER NOT NULL,"
        "UNIQUE(account,archive,peer,request_index,page_size,result_count))",
        "CREATE INDEX IF NOT EXISTS archive_index_lookup ON "
        "archive_index_pages(account,archive,peer,result_count,first_index)",
        "CREATE TABLE IF NOT EXISTS archive_index_page_items ("
        "page_id INTEGER NOT NULL REFERENCES archive_index_pages(id) ON DELETE CASCADE,position INTEGER NOT NULL,"
        "event_id INTEGER NOT NULL REFERENCES events(id) ON DELETE CASCADE,uid TEXT NOT NULL,"
        "PRIMARY KEY(page_id,position))",
        "CREATE INDEX IF NOT EXISTS archive_index_page_event ON archive_index_page_items(event_id)",
        "CREATE TABLE IF NOT EXISTS message_revisions ("
        "event_id INTEGER PRIMARY KEY REFERENCES events(id) ON DELETE CASCADE,"
        "contact_id INTEGER NOT NULL,type INTEGER NOT NULL,direction INTEGER NOT NULL,"
        "sender TEXT NOT NULL,stanza_id TEXT NOT NULL,replace_id TEXT NOT NULL,"
        "root_id INTEGER NOT NULL)",
        "CREATE INDEX IF NOT EXISTS revision_stanza ON message_revisions("
        "contact_id,type,direction,sender,stanza_id)",
        "CREATE INDEX IF NOT EXISTS revision_replace ON message_revisions("
        "contact_id,type,direction,sender,replace_id)",
        "CREATE INDEX IF NOT EXISTS revision_root ON message_revisions(root_id)"
    };
    for (const auto &sql : statements)
        if (ok)
            ok = exec(db, sql);
    if (ok) {
        QSqlQuery columns(db);
        bool      hasCount = false, hasIndex = false;
        ok = columns.exec("PRAGMA table_info(archive_pages)");
        while (columns.next()) {
            hasCount |= columns.value(1).toString() == QLatin1String("result_count");
            hasIndex |= columns.value(1).toString() == QLatin1String("first_index");
        }
        columns.finish();
        ok = ok && (hasCount || exec(db, "ALTER TABLE archive_pages ADD COLUMN result_count INTEGER"))
            && (hasIndex || exec(db, "ALTER TABLE archive_pages ADD COLUMN first_index INTEGER"));
    }
    // Authentication/old client system records are provably local events.
    if (ok && !hasState)
        ok = exec(db, "UPDATE events SET history_state=2 WHERE history_state=0 AND type IN (2,3,6,7,8)");
    if (ok && originalVersion != QLatin1String("0.4")) {
        QSqlQuery version(db);
        ok = version.exec("DELETE FROM system WHERE key='version'")
            && version.exec("INSERT INTO system(key,value) VALUES('version','0.4')");
    }
    // Index old EDB identity metadata and previously cached MAM correction
    // XML once. Never rewrite the user's original event payloads.
    if (ok && backfillRevisions) {
        qint64 last = 0;
        while (ok) {
            QSqlQuery read(db);
            read.prepare("SELECT e.id,e.contact_id,e.type,e.direction,e.extra_data,"
                         "(SELECT xml FROM archive_items WHERE event_id=e.id LIMIT 1) "
                         "FROM events e WHERE e.id>:id AND e.type IN (1,9) "
                         "ORDER BY e.id LIMIT 500");
            read.bindValue(":id", last);
            if (!read.exec()) {
                ok = false;
                break;
            }
            struct Legacy {
                qint64  id, contact;
                int     type, direction;
                QString sender, stanza, replace;
            };
            QList<Legacy> rows;
            int           count = 0;
            while (read.next()) {
                ++count;
                last                = read.value(0).toLongLong();
                const auto envelope = QJsonDocument::fromJson(read.value(4).toString().toUtf8())
                                          .object()
                                          .value("psi:message")
                                          .toObject();
                if (envelope.value("id").toString().isEmpty())
                    continue;
                QString replace = envelope.value("replace").toString();
                if (replace.isEmpty())
                    replace = cachedReplaceId(read.value(5).toString());
                rows.append({ last, read.value(1).toLongLong(), read.value(2).toInt(), read.value(3).toInt(),
                              envelope.value("from").toString(), envelope.value("id").toString(), replace });
            }
            read.finish();
            if (progress)
                progress(last);
            for (const auto &item : rows)
                if (!recordRevision(db, item.id, item.contact, item.type, item.direction, item.sender, item.stanza,
                                    item.replace)) {
                    ok = false;
                    break;
                }
            if (count < 500)
                break;
        }
    }
    if (ok && db.commit())
        return true;
    db.rollback();
    return false;
}

bool recordRevision(QSqlDatabase db, qint64 eventId, qint64 contactId, int type, int direction, const QString &sender,
                    const QString &messageId, const QString &replaceId)
{
    if (eventId <= 0 || contactId <= 0 || messageId.isEmpty() || (type == 9 && sender.isEmpty()))
        return true; // Cannot prove identity without an ID or MUC sender.
    const QString who = type == 9 ? sender : QStringLiteral("direct");
    QSqlQuery     q(db);
    q.prepare("INSERT INTO message_revisions("
              "event_id,contact_id,type,direction,sender,stanza_id,replace_id,root_id)"
              " VALUES(:id,:contact,:type,:direction,:sender,:stanza,:replace,:root)"
              " ON CONFLICT(event_id) DO UPDATE SET"
              " stanza_id=excluded.stanza_id,"
              " replace_id=CASE WHEN excluded.replace_id<>'' THEN excluded.replace_id"
              " ELSE message_revisions.replace_id END");
    q.bindValue(":id", eventId);
    q.bindValue(":contact", contactId);
    q.bindValue(":type", type);
    q.bindValue(":direction", direction);
    q.bindValue(":sender", who);
    q.bindValue(":stanza", messageId);
    q.bindValue(":replace", nonNull(replaceId));
    q.bindValue(":root", eventId);
    if (!q.exec()) {
        qWarning().noquote() << "[history-revisions] insert:" << q.lastError().text();
        return false;
    }

    // Both traversal directions matter: original and correction may arrive
    // in separate pages, and the newer revision may be stored first.
    q.prepare("WITH RECURSIVE related(id) AS ("
              " SELECT :id UNION"
              " SELECT next.event_id FROM related chain"
              " JOIN message_revisions cur ON cur.event_id=chain.id"
              " JOIN message_revisions next ON next.contact_id=cur.contact_id"
              " AND next.type=cur.type AND next.direction=cur.direction"
              " AND next.sender=cur.sender"
              " AND ((cur.replace_id<>'' AND next.stanza_id=cur.replace_id)"
              " OR (next.replace_id<>'' AND next.replace_id=cur.stanza_id))"
              ") SELECT related.id FROM related JOIN events e ON e.id=related.id"
              " ORDER BY e.history_date_ms,e.id");
    q.bindValue(":id", eventId);
    if (!q.exec()) {
        qWarning().noquote() << "[history-revisions] traverse:" << q.lastError().text();
        return false;
    }
    QList<qint64> ids;
    while (q.next())
        ids.append(q.value(0).toLongLong());
    q.finish();
    if (ids.isEmpty())
        return false;
    QSqlQuery update(db);
    update.prepare("UPDATE message_revisions SET root_id=:root WHERE event_id=:id");
    for (auto id : ids) {
        update.bindValue(":root", ids.first());
        update.bindValue(":id", id);
        if (!update.exec()) {
            qWarning().noquote() << "[history-revisions] update:" << update.lastError().text();
            return false;
        }
    }
    return true;
}

QSqlRecord resolvedRecord(QSqlDatabase db, qint64 eventId)
{
    if (eventId <= 0)
        return {};
    QSqlQuery q(db);
    q.prepare("SELECT COALESCE(r.root_id,e.id) FROM events e"
              " LEFT JOIN message_revisions r ON r.event_id=e.id WHERE e.id=:id");
    q.bindValue(":id", eventId);
    if (!q.exec() || !q.next())
        return {};
    const qint64 root = q.value(0).toLongLong();
    q.finish();
    q.prepare("SELECT e.*,c.acc_id,c.jid,"
              "(SELECT uid FROM archive_items WHERE event_id=e.id LIMIT 1) AS archive_uid, "
              "(SELECT archive FROM archive_items WHERE event_id=e.id LIMIT 1) AS archive_jid, "
              "(SELECT xml FROM archive_items WHERE event_id=e.id LIMIT 1) AS archive_xml"
              " FROM events e JOIN contacts c ON c.id=e.contact_id WHERE e.id=:id");
    q.bindValue(":id", root);
    if (!q.exec() || !q.next())
        return {};
    QSqlRecord record   = q.record();
    const auto original = QJsonDocument::fromJson(record.value("extra_data").toString().toUtf8()).object();
    QString    target   = original.value("psi:origin").toString();
    if (record.value("type").toInt() == 9) {
        target.clear();
        for (const auto &value : original.value("psi:ids").toArray())
            if (value.toObject().value("by").toString() == record.value("jid").toString())
                target = value.toObject().value("id").toString();
        if (target.isEmpty())
            target = record.value("archive_uid").toString();
    } else if (target.isEmpty())
        target = original.value("psi:message").toObject().value("id").toString();
    auto targetField = record.field(record.indexOf("m_text"));
    targetField.setName("reaction_target");
    record.append(targetField);
    record.setValue("reaction_target", target);
    targetField.setName("retraction_target");
    record.append(targetField);
    record.setValue("retraction_target", record.value("type").toInt() == 9 ? target
        : original.value("psi:message").toObject().value("id").toString());
    q.finish();
    q.prepare("SELECT e.m_text,e.subject,e.lang,e.extra_data,"
              "(SELECT uid FROM archive_items WHERE event_id=e.id LIMIT 1),e.date"
              " FROM message_revisions r JOIN events e ON e.id=r.event_id"
              " WHERE r.root_id=:root ORDER BY e.history_date_ms DESC,e.id DESC LIMIT 1");
    q.bindValue(":root", root);
    if (q.exec() && q.next()) {
        auto revision = record.field(record.indexOf("date"));
        revision.setName("revision_date");
        record.append(revision);
        record.setValue("revision_date", q.value(5));
        const char *names[] = { "m_text", "subject", "lang", "extra_data" };
        for (int i = 0; i < 4; ++i) {
            const int col = record.indexOf(QLatin1String(names[i]));
            if (col >= 0)
                record.setValue(col, q.value(i));
        }
        if (record.value("archive_uid").toString().isEmpty()) {
            const int col = record.indexOf("archive_uid");
            if (col >= 0)
                record.setValue(col, q.value(4));
        }
    }
    q.finish();
    const auto extra = QJsonDocument::fromJson(record.value("extra_data").toString().toUtf8()).object();
    if (record.value("direction").toInt() == 1 && record.value("type").toInt() != 9
        && extra.value("psi:decryption-error").toBool()) {
        // A local preload precedes MAM replay. Recover an old failed outgoing
        // projection here as well, so an encrypted marker does not first
        // appear as an error message. The subsequent replay persists the
        // repaired projection; this read never changes archive coverage.
        const Key  key { record.value("acc_id").toString(), record.value("archive_jid").toString(),
                        record.value("jid").toString(), false };
        const auto envelope     = extra.value("psi:message").toObject();
        const auto revisionTime = record.value("revision_date").toDateTime();
        const auto saved
            = matchingOwnPlaintext(db, key, envelope.value("id").toString(), envelope.value("from").toString(),
                                   revisionTime.isValid() ? revisionTime : record.value("date").toDateTime());
        if (saved) {
            q.prepare("SELECT m_text,subject,lang,extra_data FROM events WHERE id=:id");
            q.bindValue(":id", saved);
            if (q.exec() && q.next()) {
                const char *names[] = { "m_text", "subject", "lang", "extra_data" };
                for (int i = 0; i < 4; ++i)
                    record.setValue(QLatin1String(names[i]), q.value(i));
            }
        }
    }
    return record;
}

QList<QSqlRecord> localRecords(QSqlDatabase db, const QString &account, const QString &peer, qint64 before, int limit)
{
    QList<QSqlRecord> result;
    if (account.isEmpty() || peer.isEmpty() || limit <= 0)
        return result;
    QSqlQuery q(db);
    QString   sql = "SELECT e.id FROM events e JOIN contacts c ON c.id=e.contact_id "
                    "LEFT JOIN message_revisions r ON r.event_id=e.id "
                    "WHERE c.acc_id=:account AND c.jid=:peer "
                    "AND (r.event_id IS NULL OR r.root_id=e.id)";
    if (before > 0)
        sql += " AND (e.history_date_ms,e.id) < (SELECT history_date_ms,id FROM events WHERE id=:before AND "
               "contact_id=c.id)";
    sql += " ORDER BY e.history_date_ms DESC,e.id DESC LIMIT :limit";
    q.prepare(sql);
    q.bindValue(":account", account);
    q.bindValue(":peer", peer);
    q.bindValue(":limit", limit);
    if (before > 0)
        q.bindValue(":before", before);
    QList<qint64> ids;
    if (q.exec())
        while (q.next())
            ids.prepend(q.value(0).toLongLong());
    q.finish();
    for (qint64 id : ids) {
        const auto resolved = resolvedRecord(db, id);
        if (!resolved.isEmpty())
            result.append(resolved);
    }
    return result;
}

DisplayedCursor sentDisplayedCursor(QSqlDatabase db, const QString &account, const QString &peer)
{
    DisplayedCursor cursor;
    if (account.isEmpty() || peer.isEmpty())
        return cursor;
    QSqlQuery markers(db);
    markers.prepare("SELECT e.extra_data,e.contact_id FROM events e JOIN contacts c ON c.id=e.contact_id "
                    "WHERE c.acc_id=:account AND c.jid=:peer AND e.direction=1 AND e.type IN (0,1) "
                    "AND e.extra_data LIKE '%\"displayed\"%'");
    markers.bindValue(":account", account);
    markers.bindValue(":peer", peer);
    if (!markers.exec())
        return cursor;
    QSet<QString> seen;
    QSqlQuery     target(db);
    // The revision index resolves a marker naming an earlier correction to
    // the original row, whose timestamp/order remains stable after edits.
    target.prepare("SELECT DISTINCT e.id,e.date FROM message_revisions r "
                   "JOIN events e ON e.id=r.root_id WHERE r.contact_id=:contact AND r.type IN (0,1) "
                   "AND r.direction=2 AND r.sender='direct' AND r.stanza_id=:id");
    while (markers.next()) {
        const auto extra = QJsonDocument::fromJson(markers.value(0).toString().toUtf8()).object();
        if (extra.value("psi:decryption-error").toBool()
            || (extra.contains("psi:decoded") && !extra.value("psi:decoded").toBool()))
            continue;
        const auto id = extra.value("psi:events").toObject().value("displayed").toString();
        if (id.isEmpty() || seen.contains(id))
            continue;
        seen.insert(id);
        target.bindValue(":contact", markers.value(1));
        target.bindValue(":id", id);
        if (!target.exec() || !target.next())
            continue;
        const auto eventId = target.value(0).toLongLong();
        const auto time    = target.value(1).toDateTime();
        if (!time.isValid() || target.next())
            continue; // Reused peer IDs must not acknowledge unrelated rows.
        if (!cursor.time.isValid() || time > cursor.time || (time == cursor.time && eventId > cursor.eventId))
            cursor = { id, time, eventId };
    }
    return cursor;
}

qint64 matchingDelivery(QSqlDatabase db, qint64 contact, const Entry &entry, bool archived)
{
    if (entry.messageId.isEmpty() || entry.sender.isEmpty() || !entry.timestamp.isValid())
        return 0;
    QSqlQuery q(db);
    q.prepare("SELECT id,date,extra_data FROM events WHERE contact_id=:contact AND direction=:direction "
              "AND type=:type AND m_text=:body AND COALESCE(subject,'')=:subject AND "
              + QString(archived ? "" : "NOT ") + "EXISTS(SELECT 1 FROM archive_items WHERE event_id=events.id)");
    q.bindValue(":contact", contact);
    q.bindValue(":direction", entry.direction);
    q.bindValue(":type", entry.type);
    q.bindValue(":body", nonNull(entry.body));
    q.bindValue(":subject", nonNull(entry.subject));
    if (!q.exec())
        return 0;
    qint64 match = 0;
    while (q.next()) {
        const auto    extra        = QJsonDocument::fromJson(q.value(2).toString().toUtf8()).object();
        const auto    envelope     = extra.value("psi:message").toObject();
        const QString sender = envelope.value("from").toString();
        const auto    timestamp    = q.value(1).toDateTime();
        const bool undatedOwnEvent = entry.direction == 1 && entry.body.isEmpty() && extra.value("psi:decoded").toBool()
            && !extra.value("psi:events").toObject().isEmpty();
        if (envelope.value("id").toString() != entry.messageId
            || (sender != entry.sender && !(entry.direction == 1 && ownSenderMatches(sender, entry.sender)))
            || (timestamp.isValid() ? qAbs(timestamp.msecsTo(entry.timestamp)) > 300000 : !undatedOwnEvent))
            continue;
        if (match)
            return 0;
        match = q.value(0).toLongLong();
    }
    return match;
}

qint64 matchingOwnPlaintext(QSqlDatabase db, const Key &key, const QString &id, const QString &sender,
                            const QDateTime &time)
{
    if (key.room || id.isEmpty() || sender.isEmpty() || !time.isValid()
        || sender.section(QLatin1Char('/'), 0, 0) != key.archive)
        return 0;
    QSqlQuery q(db);
    q.prepare("SELECT e.id,e.extra_data,e.m_text,e.history_date_ms FROM events e JOIN contacts c ON c.id=e.contact_id "
              "WHERE c.acc_id=:account AND c.jid=:peer AND e.direction=1 "
              "AND (e.history_date_ms BETWEEN :start AND :end OR e.history_date_ms=0 OR e.history_date_ms IS NULL)");
    q.bindValue(":account", key.account);
    q.bindValue(":peer", key.peer);
    q.bindValue(":start", time.toMSecsSinceEpoch() - 300000);
    q.bindValue(":end", time.toMSecsSinceEpoch() + 300000);
    qint64 found = 0;
    if (!q.exec())
        return 0;
    while (q.next()) {
        const auto extra = QJsonDocument::fromJson(q.value(1).toString().toUtf8()).object();
        const bool protocolEvent = !extra.value("psi:events").toObject().isEmpty();
        if ((extra.contains("psi:decoded") && !extra.value("psi:decoded").toBool())
            || extra.value("psi:decryption-error").toBool() || (q.value(2).toString().isEmpty() && !protocolEvent))
            continue;
        // Only known outgoing protocol events suffered from the missing
        // Message timestamp bug. Do not relax time matching for user text.
        if (q.value(3).toLongLong() == 0
            && (!protocolEvent || !q.value(2).toString().isEmpty() || !extra.value("psi:decoded").toBool()))
            continue;
        const auto envelope = extra.value("psi:message").toObject();
        if (envelope.value("id").toString() != id)
            continue;
        const auto from = envelope.value("from").toString();
        if (!ownSenderMatches(from, sender))
            continue;
        if (found)
            return 0; // A reused ID is ambiguous even among our own records.
        found = q.value(0).toLongLong();
    }
    return found;
}

Snapshot readSnapshot(QSqlDatabase db, const Key &key)
{
    QSqlQuery q(db);
    q.prepare("SELECT result_count,last_uid,observed_at,stable FROM archive_snapshots WHERE " + scope);
    bindKey(q, key);
    if (!q.exec() || !q.next())
        return {};
    return { q.value(0).toInt(), q.value(1).toString(),
             QDateTime::fromMSecsSinceEpoch(q.value(2).toLongLong(), QTimeZone(0)), q.value(3).toBool() };
}

bool storeSnapshot(QSqlDatabase db, const Key &key, const Snapshot &snapshot)
{
    if (!snapshot.observedAt.isValid() || snapshot.resultCount < -1 || key.account.isEmpty() || key.archive.isEmpty()
        || key.peer.isEmpty())
        return false;
    QSqlQuery q(db);
    // Never let delayed completion of an older request replace a newer view.
    q.prepare("INSERT OR REPLACE INTO archive_snapshots(account,archive,peer,result_count,last_uid,observed_at,stable) "
              "SELECT :account,:archive,:peer,:count,:uid,:time,:stable "
              "WHERE NOT EXISTS(SELECT 1 FROM archive_snapshots WHERE "
              + scope + " AND observed_at>:time)");
    bindKey(q, key);
    q.bindValue(":count", snapshot.resultCount);
    q.bindValue(":uid", nonNull(snapshot.lastUid));
    q.bindValue(":time", snapshot.observedAt.toMSecsSinceEpoch());
    q.bindValue(":stable", snapshot.stable);
    return q.exec();
}

Page readPage(QSqlDatabase db, const Key &key, const QString &before, int limit)
{
    Page page;
    if (limit <= 0 || key.account.isEmpty() || key.archive.isEmpty() || key.peer.isEmpty())
        return page;
    QSqlQuery q(db);
    q.prepare("SELECT id,item_count,complete,stable,result_count,first_index FROM archive_pages WHERE " + scope
              + " AND before_uid=:before AND page_size=:limit");
    bindKey(q, key);
    q.bindValue(":before", nonNull(before));
    q.bindValue(":limit", limit);
    if (!q.exec())
        return page;
    if (!q.next()) {
        q.finish();
        // Page boundaries move after new tail messages arrive. Reuse an
        // already verified contiguous chain at the new boundary, rather than
        // requiring identical historical request offsets forever.
        // Adaptive requests can ask for a different size at the SAME boundary.
        // The boundary need not have a stored event (e.g. MUC join-history).
        // Bootstrap from a certified page, then extend only via verified links.
        q.prepare("SELECT page_size FROM archive_pages WHERE " + scope
                  + " AND before_uid=:before AND stable=1 ORDER BY page_size DESC LIMIT 1");
        bindKey(q, key);
        q.bindValue(":before", nonNull(before));
        int savedSize = 0;
        if (q.exec() && q.next())
            savedSize = q.value(0).toInt();
        q.finish();
        if (savedSize > 0 && savedSize != limit) {
            const auto saved = readPage(db, key, before, savedSize);
            if (saved.found && saved.stable) {
                page = saved;
                while (page.entries.size() > limit) {
                    page.entries.removeFirst();
                    if (page.firstIndex >= 0)
                        ++page.firstIndex;
                    page.complete = false;
                }
                if (page.entries.size() == limit || page.complete)
                    return page;
            }
        }
        if (before.isEmpty() && page.entries.isEmpty())
            return page;
        QString       cursor = page.entries.isEmpty() ? before : page.entries.first().uid;
        QSet<QString> seen;
        for (const auto &entry : page.entries)
            seen.insert(entry.uid);
        while (page.entries.size() < limit) {
            q.prepare("SELECT previous_uid FROM archive_items WHERE " + scope + " AND uid=:uid AND stable=1");
            bindKey(q, key);
            q.bindValue(":uid", cursor);
            if (!q.exec() || !q.next() || q.value(0).isNull())
                return {};
            const QString previous = q.value(0).toString();
            q.finish();
            if (previous.isEmpty()) {
                page.complete = true;
                break;
            }
            if (seen.contains(previous))
                return {};
            seen.insert(previous);
            q.prepare("SELECT uid,xml,event_id FROM archive_items i JOIN events e ON e.id=i.event_id WHERE " + scope
                      + " AND uid=:uid AND next_uid=:next AND stable=1 AND (e.history_state & 3)=1");
            bindKey(q, key);
            q.bindValue(":uid", previous);
            q.bindValue(":next", cursor);
            if (!q.exec() || !q.next())
                return {};
            Entry entry;
            entry.uid     = q.value(0).toString();
            entry.xml     = q.value(1).toString();
            entry.eventId = q.value(2).toLongLong();
            page.entries.prepend(entry);
            if (page.firstIndex >= 0)
                --page.firstIndex;
            cursor = previous;
            q.finish();
        }
        // Reaching exactly the requested count can also reach the proven
        // archive start; preserve that boundary for the next UI request.
        if (!page.complete && page.entries.size() == limit) {
            q.prepare("SELECT previous_uid FROM archive_items WHERE " + scope + " AND uid=:uid AND stable=1");
            bindKey(q, key);
            q.bindValue(":uid", cursor);
            if (q.exec() && q.next() && !q.value(0).isNull() && q.value(0).toString().isEmpty())
                page.complete = true;
        }
        page.found = page.stable = true;
        return page;
    }
    const auto id    = q.value(0);
    const int  count = q.value(1).toInt();
    page.complete    = q.value(2).toBool();
    page.stable      = q.value(3).toBool();
    page.resultCount = q.value(4).isNull() ? -1 : q.value(4).toInt();
    page.firstIndex  = q.value(5).isNull() ? -1 : q.value(5).toInt();
    q.finish();
    q.prepare("SELECT i.uid,i.xml,i.event_id FROM archive_page_items p JOIN archive_items i "
              "ON i.event_id=p.event_id AND i.uid=p.uid JOIN events e ON e.id=i.event_id "
              "WHERE p.page_id=:id AND i.stable=1 AND (e.history_state & 3)=1 AND "
              + scope + " ORDER BY p.position");
    bindKey(q, key);
    q.bindValue(":id", id);
    if (!q.exec())
        return {};
    while (q.next()) {
        Entry entry;
        entry.uid     = q.value(0).toString();
        entry.xml     = q.value(1).toString();
        entry.eventId = q.value(2).toLongLong();
        page.entries.append(entry);
    }
    // Detect damaged or explicitly deleted cache entries. A short page is only
    // complete when the server said so (a server may impose a smaller limit).
    page.found = page.entries.size() == count;
    return page;
}

Page readAfter(QSqlDatabase db, const Key &key, const QString &after, int limit, int snapshotCount)
{
    Page page;
    if (after.isEmpty() || limit < 1 || limit > 40)
        return page;
    QString       cursor = after;
    QSet<QString> seen { after };
    QSqlQuery     q(db);
    while (page.entries.size() < limit) {
        q.prepare("SELECT next_uid FROM archive_items WHERE " + scope + " AND uid=:uid AND stable=1");
        bindKey(q, key);
        q.bindValue(":uid", cursor);
        if (!q.exec() || !q.next() || q.value(0).isNull())
            return {};
        const QString next = q.value(0).toString();
        q.finish();
        // There is no permanently certified live end. An uncovered suffix
        // requires a fresh bounded query, even if an old count suggested an end.
        if (next.isEmpty() || seen.contains(next))
            return {};
        seen.insert(next);
        q.prepare("SELECT i.uid,i.xml,i.event_id FROM archive_items i JOIN events e ON e.id=i.event_id WHERE " + scope
                  + " AND i.uid=:uid AND i.previous_uid=:previous AND i.stable=1 AND (e.history_state & 3)=1");
        bindKey(q, key);
        q.bindValue(":uid", next);
        q.bindValue(":previous", cursor);
        if (!q.exec() || !q.next())
            return {};
        Entry entry;
        entry.uid     = q.value(0).toString();
        entry.xml     = q.value(1).toString();
        entry.eventId = q.value(2).toLongLong();
        page.entries.append(entry);
        cursor = next;
        q.finish();
    }
    page.found = page.stable = true;
    page.checkpoint          = false;
    page.previous            = after;
    page.resultCount         = snapshotCount;
    return page;
}

Page readIndex(QSqlDatabase db, const Key &key, int index, int limit, int snapshotCount)
{
    Page page;
    if (index < 0 || limit < 1 || limit > 40 || snapshotCount < 0)
        return page;
    QSqlQuery q(db);
    q.prepare("SELECT id,item_count,first_index,complete FROM archive_index_pages WHERE " + scope
              + " AND result_count=:snapshot AND first_index<=:position AND first_index+item_count>:position"
                " AND (first_index+item_count>=:end OR complete=1) ORDER BY first_index DESC,page_size DESC LIMIT 1");
    bindKey(q, key);
    q.bindValue(":snapshot", snapshotCount);
    q.bindValue(":position", index);
    q.bindValue(":end", qint64(index) + limit);
    if (!q.exec() || !q.next())
        return page;
    const auto id       = q.value(0);
    const int  count    = q.value(1).toInt();
    const int  first    = q.value(2).toInt();
    const bool complete = q.value(3).toBool();
    q.finish();
    q.prepare("SELECT i.uid,i.xml,i.event_id FROM archive_index_page_items p JOIN archive_items i"
              " ON i.event_id=p.event_id AND i.uid=p.uid JOIN events e ON e.id=i.event_id"
              " WHERE p.page_id=:id AND i.stable=1 AND (e.history_state & 3)=1 AND "
              + scope + " ORDER BY p.position");
    bindKey(q, key);
    q.bindValue(":id", id);
    if (!q.exec())
        return {};
    while (q.next()) {
        Entry entry;
        entry.uid     = q.value(0).toString();
        entry.xml     = q.value(1).toString();
        entry.eventId = q.value(2).toLongLong();
        page.entries.append(entry);
    }
    if (page.entries.size() != count)
        return {};
    for (int offset = index - first; offset > 0 && !page.entries.isEmpty(); --offset)
        page.entries.removeFirst();
    while (page.entries.size() > limit)
        page.entries.removeLast();
    page.found = page.stable = true;
    page.checkpoint          = false;
    page.requestIndex        = index;
    page.resultCount         = snapshotCount;
    page.firstIndex          = index;
    page.complete            = index == 0;
    page.queryComplete       = complete && index - first + page.entries.size() == count;
    return page;
}

bool storePage(QSqlDatabase db, const Key &key, const QString &before, int limit, Page &page)
{
    if (key.account.isEmpty() || key.archive.isEmpty() || key.peer.isEmpty() || limit <= 0
        || page.entries.size() > limit || !db.transaction())
        return false;
    QSqlQuery  q(db);
    const auto fail = [&db, &page, &q] {
        if (q.lastError().isValid())
            qWarning().noquote() << "[history-cache] Could not store archive page:" << q.lastError().text();
        db.rollback();
        for (auto &entry : page.entries)
            entry.eventId = 0;
        return false;
    };
    q.prepare("SELECT id,type FROM contacts WHERE acc_id=:account AND jid=:peer");
    q.bindValue(":account", key.account);
    q.bindValue(":peer", key.peer);
    if (!q.exec())
        return fail();
    qint64 contact = 0;
    if (q.next()) {
        contact = q.value(0).toLongLong();
        // Public room cache must never reuse a MUC private contact.
        if (q.value(1).toInt() != (key.room ? 3 : 1))
            return fail();
    }
    q.finish();
    if (!contact) {
        q.prepare("INSERT INTO contacts(acc_id,type,jid,lifetime) VALUES(:account,:type,:peer,-1)");
        q.bindValue(":account", key.account);
        q.bindValue(":peer", key.peer);
        q.bindValue(":type", key.room ? 3 : 1);
        if (!q.exec())
            return fail();
        contact = q.lastInsertId().toLongLong();
    }
    QSet<QString> seen;
    bool          reusable = page.stable;
    for (auto &entry : page.entries) {
        entry.eventId = 0;
        if (entry.uid.isEmpty() || seen.contains(entry.uid) || !entry.timestamp.isValid())
            return fail();
        seen.insert(entry.uid);
        // A privacy hint must not enter persistent storage. Nor can an omitted
        // result be silently treated as a complete cached page.
        if (!entry.storable) {
            reusable = false;
            continue;
        }
        q.prepare("SELECT event_id FROM archive_items WHERE " + scope + " AND uid=:uid");
        bindKey(q, key);
        q.bindValue(":uid", entry.uid);
        if (!q.exec())
            return fail();
        qint64 eventId = q.next() ? q.value(0).toLongLong() : 0;
        q.finish();
        if (!eventId)
            eventId = matchingDelivery(db, contact, entry, false);
        const bool existingEvent = eventId > 0;
        bool       preserveProjection = existingEvent && !page.stable;
        if (!eventId) {
            q.prepare("INSERT INTO "
                      "events(contact_id,resource,date,history_date_ms,type,direction,subject,m_text,lang,extra_data,"
                      "history_state) "
                      "VALUES(:contact,'',:date,:time,:type,:direction,:subject,:body,:lang,:extra,:state)");
            q.bindValue(":contact", contact);
            q.bindValue(":date", entry.timestamp);
            q.bindValue(":time", entry.timestamp.toMSecsSinceEpoch());
            q.bindValue(":type", entry.type);
            q.bindValue(":direction", entry.direction);
            q.bindValue(":subject", nonNull(entry.subject));
            q.bindValue(":body", nonNull(entry.body));
            q.bindValue(":lang", nonNull(entry.language));
            q.bindValue(":extra", nonNull(entry.extra));
            q.bindValue(":state", page.stable ? Archived : Unverified);
            if (!q.exec())
                return fail();
            eventId = q.lastInsertId().toLongLong();
        }
        entry.eventId = eventId;
        if (existingEvent && page.stable) {
            q.prepare("SELECT extra_data,m_text FROM events WHERE id=:id");
            q.bindValue(":id", eventId);
            if (!q.exec() || !q.next())
                return fail();
            const QString originalExtra    = q.value(0).toString();
            auto          merged           = QJsonDocument::fromJson(originalExtra.toUtf8()).object();
            const bool    repairProjection = !merged.value("psi:decoded").toBool();
            const auto    incoming         = QJsonDocument::fromJson(entry.extra.toUtf8()).object();
            const bool    failedReplay = (incoming.contains("psi:decoded") && !incoming.value("psi:decoded").toBool())
                || incoming.value("psi:decryption-error").toBool();
            const bool savedPlaintext = merged.value("psi:decoded").toBool()
                || (!merged.contains("psi:decoded") && !q.value(1).toString().isEmpty());
            preserveProjection = failedReplay && savedPlaintext;
            q.finish();
            // Archive connectivity and plaintext availability are independent.
            // Even an unverified live row can contain successfully decrypted
            // text or protocol events. A replay failure must preserve both
            // its body and its metadata, including the decoded flag.
            if (!preserveProjection) {
                if (merged.isEmpty() && !originalExtra.isEmpty() && originalExtra != "{}")
                    merged.insert("psi:legacy-extra", originalExtra);
                // Replace known protocol fields, retaining extension/plugin data.
                for (const auto &name :
                     { "psi:message", "psi:events", "psi:html", "psi:ids", "psi:origin", "psi:encrypted",
                       "psi:encryption-protocol", "psi:decoded", "psi:has-body", "psi:decryption-error",
                       "psi:protocol-only", "urn:xmpp:reference:0", "jabber:x:oob" })
                    merged.remove(QLatin1String(name));
                for (auto it = incoming.begin(); it != incoming.end(); ++it)
                    merged.insert(it.key(), it.value());
                const QString combinedExtra = QString::fromUtf8(QJsonDocument(merged).toJson(QJsonDocument::Compact));
                // An unverified/provisional projection is repaired by a confirmed
                // server result. A trusted local plaintext body is not replaced
                // by a failed decryption's empty projection.
                q.prepare(
                    "UPDATE events SET m_text=CASE WHEN :repair OR (history_state & 3)=0 OR "
                    "(m_text='' AND :body<>'') THEN :body ELSE m_text END,"
                    "subject=CASE WHEN (history_state & 3)=0 THEN :subject ELSE subject END,"
                    "lang=CASE WHEN (history_state & 3)=0 THEN :lang ELSE lang END,extra_data=:extra WHERE id=:id");
                q.bindValue(":id", eventId);
                q.bindValue(":repair", repairProjection);
                q.bindValue(":body", nonNull(entry.body));
                q.bindValue(":subject", nonNull(entry.subject));
                q.bindValue(":lang", nonNull(entry.language));
                q.bindValue(":extra", combinedExtra);
                if (!q.exec())
                    return fail();
            }
        }
        if (page.stable) {
            q.prepare("UPDATE events SET history_state=(history_state & ~3) | 1,"
                      "date=CASE WHEN date IS NULL THEN :date ELSE date END,"
                      "history_date_ms=CASE WHEN history_date_ms IS NULL OR history_date_ms=0 THEN :time "
                      "ELSE history_date_ms END WHERE id=:id");
            q.bindValue(":id", eventId);
            q.bindValue(":date", entry.timestamp);
            q.bindValue(":time", entry.timestamp.toMSecsSinceEpoch());
            if (!q.exec())
                return fail();
        }
        if (!preserveProjection
            && !recordRevision(db, eventId, contact, entry.type, entry.direction, entry.sender, entry.messageId,
                               entry.replaceId))
            return fail();
        // Never overwrite an already stable payload with a provisional result.
        q.prepare(
            "INSERT INTO archive_items(account,archive,peer,uid,event_id,xml,stable) "
            "VALUES(:account,:archive,:peer,:uid,:event,:xml,:stable) "
            "ON CONFLICT(account,archive,peer,uid) DO UPDATE SET "
            "xml=CASE WHEN excluded.stable OR NOT archive_items.stable THEN excluded.xml ELSE archive_items.xml END, "
            "stable=MAX(archive_items.stable,excluded.stable)");
        bindKey(q, key);
        q.bindValue(":uid", entry.uid);
        q.bindValue(":event", eventId);
        q.bindValue(":xml", entry.xml);
        q.bindValue(":stable", page.stable);
        if (!q.exec())
            return fail();
    }
    if (page.checkpoint) {
        // Failed/unstable queries cannot leave an older reusable checkpoint behind.
        q.prepare("DELETE FROM archive_pages WHERE " + scope + " AND before_uid=:before AND page_size=:limit");
        bindKey(q, key);
        q.bindValue(":before", nonNull(before));
        q.bindValue(":limit", limit);
        if (!q.exec())
            return fail();
    }
    // The supplied cursor is the neighbour immediately after this result
    // page. Link it back as well, so new page offsets can follow the chain.
    if (!before.isEmpty() && (!page.entries.isEmpty() || page.complete)) {
        const QString previous
            = reusable ? (page.entries.isEmpty() ? QStringLiteral("") : page.entries.last().uid) : QString();
        q.prepare("UPDATE archive_items SET previous_uid=:previous WHERE " + scope + " AND uid=:uid");
        bindKey(q, key);
        q.bindValue(":uid", before);
        q.bindValue(":previous", previous);
        if (!q.exec())
            return fail();
        q.prepare("UPDATE events SET history_state=(history_state & ~4) | :flag WHERE id IN "
                  "(SELECT event_id FROM archive_items WHERE "
                  + scope + " AND uid=:uid)");
        bindKey(q, key);
        q.bindValue(":uid", before);
        q.bindValue(":flag", reusable ? LinkedPrevious : 0);
        if (!q.exec())
            return fail();
    }
    if (reusable) {
        if (!page.previous.isEmpty() && !page.entries.isEmpty()) {
            q.prepare("UPDATE archive_items SET next_uid=:next WHERE " + scope + " AND uid=:uid AND stable=1");
            bindKey(q, key);
            q.bindValue(":uid", page.previous);
            q.bindValue(":next", page.entries.first().uid);
            if (!q.exec())
                return fail();
        }
        for (int i = 0; i < page.entries.size(); ++i) {
            const auto   &entry    = page.entries.at(i);
            const QString previous = i > 0 ? page.entries.at(i - 1).uid : page.previous;
            const QString next     = i + 1 < page.entries.size() ? page.entries.at(i + 1).uid : before;
            // NULL denotes an unknown neighbour. Empty string is a proven
            // archive start; there is deliberately no permanent "live end".
            q.prepare("UPDATE archive_items SET previous_uid=COALESCE(:previous,previous_uid), "
                      "next_uid=COALESCE(:next,next_uid) WHERE "
                      + scope + " AND uid=:uid");
            bindKey(q, key);
            q.bindValue(":uid", entry.uid);
            q.bindValue(":previous", i > 0 || page.complete || !page.previous.isEmpty() ? nonNull(previous) : previous);
            q.bindValue(":next", next);
            if (!q.exec())
                return fail();
            const int flags = (i > 0 || page.complete || !page.previous.isEmpty() ? LinkedPrevious : 0)
                | (!next.isEmpty() ? LinkedNext : 0);
            q.prepare("UPDATE events SET history_state=history_state | :flags WHERE id=:id");
            q.bindValue(":flags", flags);
            q.bindValue(":id", entry.eventId);
            if (!q.exec())
                return fail();
        }
        if (page.checkpoint) {
            // Record even short non-final pages: their exact request is complete,
            // although it must not be combined into a larger assumed full page.
            q.prepare("INSERT INTO "
                      "archive_pages(account,archive,peer,before_uid,page_size,item_count,complete,stable,result_count,"
                      "first_index) "
                      "VALUES(:account,:archive,:peer,:before,:limit,:count,:complete,1,:rsm_count,:first_index)");
            bindKey(q, key);
            q.bindValue(":before", nonNull(before));
            q.bindValue(":limit", limit);
            q.bindValue(":count", page.entries.size());
            q.bindValue(":complete", page.complete);
            q.bindValue(":rsm_count", page.resultCount >= 0 ? QVariant(page.resultCount) : QVariant());
            q.bindValue(":first_index", page.firstIndex >= 0 ? QVariant(page.firstIndex) : QVariant());
            if (!q.exec())
                return fail();
            const auto pageId = q.lastInsertId();
            for (int i = 0; i < page.entries.size(); ++i) {
                q.prepare("INSERT INTO archive_page_items(page_id,position,event_id,uid) "
                          "VALUES(:page,:position,:event,:uid)");
                q.bindValue(":page", pageId);
                q.bindValue(":position", i);
                q.bindValue(":event", page.entries.at(i).eventId);
                q.bindValue(":uid", page.entries.at(i).uid);
                if (!q.exec())
                    return fail();
            }
        }
    }
    if (page.requestIndex >= 0 && page.resultCount >= 0 && page.firstIndex >= 0) {
        // Ordinal checkpoints belong to a count snapshot and must never share
        // the before-UID/tail key space. Payloads and internal links are shared.
        q.prepare("DELETE FROM archive_index_pages WHERE " + scope
                  + " AND (result_count<>:snapshot OR (request_index=:position AND page_size=:limit))");
        bindKey(q, key);
        q.bindValue(":snapshot", page.resultCount);
        q.bindValue(":position", page.requestIndex);
        q.bindValue(":limit", limit);
        if (!q.exec())
            return fail();
        if (reusable) {
            q.prepare("INSERT INTO archive_index_pages(account,archive,peer,request_index,page_size,item_count,"
                      "result_count,first_index,complete) VALUES(:account,:archive,:peer,:position,:limit,:count,"
                      ":snapshot,:first,:complete)");
            bindKey(q, key);
            q.bindValue(":position", page.requestIndex);
            q.bindValue(":limit", limit);
            q.bindValue(":count", page.entries.size());
            q.bindValue(":snapshot", page.resultCount);
            q.bindValue(":first", page.firstIndex);
            q.bindValue(":complete", page.queryComplete);
            if (!q.exec())
                return fail();
            const auto pageId = q.lastInsertId();
            for (int i = 0; i < page.entries.size(); ++i) {
                q.prepare("INSERT INTO archive_index_page_items(page_id,position,event_id,uid) "
                          "VALUES(:page,:at,:event,:uid)");
                q.bindValue(":page", pageId);
                q.bindValue(":at", i);
                q.bindValue(":event", page.entries.at(i).eventId);
                q.bindValue(":uid", page.entries.at(i).uid);
                if (!q.exec())
                    return fail();
            }
        }
    }
    if (page.snapshot.observedAt.isValid() && !storeSnapshot(db, key, page.snapshot))
        return fail();
    if (!db.commit())
        return fail();
    return true;
}

bool invalidate(QSqlDatabase db, const QString &account, const QString &peer)
{
    QSqlQuery q(db);
    QString   filter;
    if (!account.isEmpty())
        filter = "account=:account";
    if (!peer.isEmpty())
        filter += (filter.isEmpty() ? "" : " AND ") + QStringLiteral("peer=:peer");
    for (const auto &table : { QStringLiteral("archive_pages"), QStringLiteral("archive_index_pages"),
                               QStringLiteral("archive_snapshots") }) {
        q.prepare("DELETE FROM " + table + (filter.isEmpty() ? QString() : " WHERE " + filter));
        if (!account.isEmpty())
            q.bindValue(":account", account);
        if (!peer.isEmpty())
            q.bindValue(":peer", peer);
        if (!q.exec())
            return false;
    }
    return true;
}
}
