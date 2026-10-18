/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historyarchive.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QXmlStreamReader>
#include <QVariant>

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
    bool revisionsExist(QSqlDatabase db)
    {
        QSqlQuery q(db);
        return q.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='message_revisions'")
            && q.next();
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

bool migrate(QSqlDatabase db)
{
    QSqlQuery version(db);
    if (!version.exec("SELECT value FROM system WHERE key='version'"))
        return false;
    const QString originalVersion = version.next() ? version.value(0).toString() : QString();
    if (!originalVersion.isEmpty() && originalVersion != QLatin1String("0.1")
        && originalVersion != QLatin1String("0.2")) {
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
    }
    const bool backfillRevisions = !revisionsExist(db);
    const QStringList statements {
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
        "complete INTEGER NOT NULL, stable INTEGER NOT NULL, "
        "UNIQUE(account,archive,peer,before_uid,page_size))",
        "CREATE TABLE IF NOT EXISTS archive_page_items ("
        "page_id INTEGER NOT NULL REFERENCES archive_pages(id) ON DELETE CASCADE, position INTEGER NOT NULL, "
        "event_id INTEGER NOT NULL REFERENCES events(id) ON DELETE CASCADE, uid TEXT NOT NULL, "
        "PRIMARY KEY(page_id,position))",
        "CREATE INDEX IF NOT EXISTS archive_page_event ON archive_page_items(event_id)",
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
    // Authentication/old client system records are provably local events.
    if (ok && !hasState)
        ok = exec(db, "UPDATE events SET history_state=2 WHERE history_state=0 AND type IN (2,3,6,7,8)");
    if (ok && originalVersion != QLatin1String("0.2")) {
        QSqlQuery version(db);
        ok = version.exec("DELETE FROM system WHERE key='version'")
            && version.exec("INSERT INTO system(key,value) VALUES('version','0.2')");
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
            if (!read.exec()) { ok = false; break; }
            struct Legacy {
                qint64 id, contact;
                int type, direction;
                QString sender, stanza, replace;
            };
            QList<Legacy> rows;
            int count = 0;
            while (read.next()) {
                ++count;
                last = read.value(0).toLongLong();
                const auto envelope = QJsonDocument::fromJson(read.value(4).toString().toUtf8())
                                          .object().value("psi:message").toObject();
                if (envelope.value("id").toString().isEmpty())
                    continue;
                QString replace = envelope.value("replace").toString();
                if (replace.isEmpty())
                    replace = cachedReplaceId(read.value(5).toString());
                rows.append({last,read.value(1).toLongLong(),read.value(2).toInt(),
                             read.value(3).toInt(),envelope.value("from").toString(),
                             envelope.value("id").toString(),replace});
            }
            read.finish();
            for (const auto &item : rows)
                if (!recordRevision(db,item.id,item.contact,item.type,item.direction,
                                    item.sender,item.stanza,item.replace)) {
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

bool recordRevision(QSqlDatabase db, qint64 eventId, qint64 contactId, int type,
                    int direction, const QString &sender,
                    const QString &messageId, const QString &replaceId)
{
    if (eventId <= 0 || contactId <= 0 || messageId.isEmpty()
        || (type == 9 && sender.isEmpty()))
        return true; // Cannot prove identity without an ID or MUC sender.
    const QString who = type == 9 ? sender : QStringLiteral("direct");
    QSqlQuery q(db);
    q.prepare("INSERT INTO message_revisions("
              "event_id,contact_id,type,direction,sender,stanza_id,replace_id,root_id)"
              " VALUES(:id,:contact,:type,:direction,:sender,:stanza,:replace,:root)"
              " ON CONFLICT(event_id) DO UPDATE SET"
              " stanza_id=excluded.stanza_id,"
              " replace_id=CASE WHEN excluded.replace_id<>'' THEN excluded.replace_id"
              " ELSE message_revisions.replace_id END");
    q.bindValue(":id",eventId);
    q.bindValue(":contact",contactId);
    q.bindValue(":type",type);
    q.bindValue(":direction",direction);
    q.bindValue(":sender",who);
    q.bindValue(":stanza",messageId);
    q.bindValue(":replace",replaceId);
    q.bindValue(":root",eventId);
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
    q.bindValue(":id",eventId);
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
        update.bindValue(":root",ids.first());
        update.bindValue(":id",id);
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
    q.bindValue(":id",eventId);
    if (!q.exec() || !q.next())
        return {};
    const qint64 root = q.value(0).toLongLong();
    q.finish();
    q.prepare("SELECT e.*,c.acc_id,c.jid,"
              "(SELECT uid FROM archive_items WHERE event_id=e.id LIMIT 1) AS archive_uid"
              " FROM events e JOIN contacts c ON c.id=e.contact_id WHERE e.id=:id");
    q.bindValue(":id",root);
    if (!q.exec() || !q.next())
        return {};
    QSqlRecord record = q.record();
    q.finish();
    q.prepare("SELECT e.m_text,e.subject,e.lang,e.extra_data,"
              "(SELECT uid FROM archive_items WHERE event_id=e.id LIMIT 1)"
              " FROM message_revisions r JOIN events e ON e.id=r.event_id"
              " WHERE r.root_id=:root ORDER BY e.history_date_ms DESC,e.id DESC LIMIT 1");
    q.bindValue(":root",root);
    if (q.exec() && q.next()) {
        const char *names[] = {"m_text","subject","lang","extra_data"};
        for (int i=0;i<4;++i) {
            const int col = record.indexOf(QLatin1String(names[i]));
            if (col >= 0)
                record.setValue(col,q.value(i));
        }
        if (record.value("archive_uid").toString().isEmpty()) {
            const int col = record.indexOf("archive_uid");
            if (col >= 0)
                record.setValue(col,q.value(4));
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
        const auto resolved = resolvedRecord(db,id);
        if (!resolved.isEmpty())
            result.append(resolved);
    }
    return result;
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
        const auto envelope
            = QJsonDocument::fromJson(q.value(2).toString().toUtf8()).object().value("psi:message").toObject();
        if (envelope.value("id").toString() != entry.messageId || envelope.value("from").toString() != entry.sender
            || !q.value(1).toDateTime().isValid() || qAbs(q.value(1).toDateTime().msecsTo(entry.timestamp)) > 300000)
            continue;
        if (match)
            return 0;
        match = q.value(0).toLongLong();
    }
    return match;
}

Page readPage(QSqlDatabase db, const Key &key, const QString &before, int limit)
{
    Page page;
    if (limit <= 0 || key.account.isEmpty() || key.archive.isEmpty() || key.peer.isEmpty())
        return page;
    QSqlQuery q(db);
    q.prepare("SELECT id,item_count,complete,stable FROM archive_pages WHERE " + scope
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
        if (before.isEmpty())
            return page;
        QString       cursor = before;
        QSet<QString> seen;
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
            cursor = previous;
            q.finish();
        }
        page.found = page.stable = true;
        return page;
    }
    const auto id    = q.value(0);
    const int  count = q.value(1).toInt();
    page.complete    = q.value(2).toBool();
    page.stable      = q.value(3).toBool();
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

bool storePage(QSqlDatabase db, const Key &key, const QString &before, int limit, Page &page)
{
    if (key.account.isEmpty() || key.archive.isEmpty() || key.peer.isEmpty() || limit <= 0
        || page.entries.size() > limit || !db.transaction())
        return false;
    const auto fail = [&db, &page] {
        db.rollback();
        for (auto &entry : page.entries)
            entry.eventId = 0;
        return false;
    };
    QSqlQuery q(db);
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
        if (page.stable) {
            q.prepare("UPDATE events SET history_state=(history_state & ~3) | 1 WHERE id=:id");
            q.bindValue(":id", eventId);
            if (!q.exec())
                return fail();
        }
        if (!recordRevision(db,eventId,contact,entry.type,entry.direction,
                            entry.sender,entry.messageId,entry.replaceId))
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
    // Failed/unstable queries cannot leave an older reusable checkpoint behind.
    q.prepare("DELETE FROM archive_pages WHERE " + scope + " AND before_uid=:before AND page_size=:limit");
    bindKey(q, key);
    q.bindValue(":before", nonNull(before));
    q.bindValue(":limit", limit);
    if (!q.exec())
        return fail();
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
        for (int i = 0; i < page.entries.size(); ++i) {
            const auto   &entry    = page.entries.at(i);
            const QString previous = i > 0 ? page.entries.at(i - 1).uid : QString();
            const QString next     = i + 1 < page.entries.size() ? page.entries.at(i + 1).uid : before;
            // NULL denotes an unknown neighbour. Empty string is a proven
            // archive start; there is deliberately no permanent "live end".
            q.prepare("UPDATE archive_items SET previous_uid=COALESCE(:previous,previous_uid), "
                      "next_uid=COALESCE(:next,next_uid) WHERE "
                      + scope + " AND uid=:uid");
            bindKey(q, key);
            q.bindValue(":uid", entry.uid);
            q.bindValue(":previous", i > 0 || page.complete ? nonNull(previous) : previous);
            q.bindValue(":next", next);
            if (!q.exec())
                return fail();
            const int flags = (i > 0 || page.complete ? LinkedPrevious : 0) | (!next.isEmpty() ? LinkedNext : 0);
            q.prepare("UPDATE events SET history_state=history_state | :flags WHERE id=:id");
            q.bindValue(":flags", flags);
            q.bindValue(":id", entry.eventId);
            if (!q.exec())
                return fail();
        }
        // Record even short non-final pages: their exact request is complete,
        // although it must not be combined into a larger assumed full page.
        q.prepare("INSERT INTO archive_pages(account,archive,peer,before_uid,page_size,item_count,complete,stable) "
                  "VALUES(:account,:archive,:peer,:before,:limit,:count,:complete,1)");
        bindKey(q, key);
        q.bindValue(":before", nonNull(before));
        q.bindValue(":limit", limit);
        q.bindValue(":count", page.entries.size());
        q.bindValue(":complete", page.complete);
        if (!q.exec())
            return fail();
        const auto pageId = q.lastInsertId();
        for (int i = 0; i < page.entries.size(); ++i) {
            q.prepare(
                "INSERT INTO archive_page_items(page_id,position,event_id,uid) VALUES(:page,:position,:event,:uid)");
            q.bindValue(":page", pageId);
            q.bindValue(":position", i);
            q.bindValue(":event", page.entries.at(i).eventId);
            q.bindValue(":uid", page.entries.at(i).uid);
            if (!q.exec())
                return fail();
        }
    }
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
    q.prepare("DELETE FROM archive_pages" + (filter.isEmpty() ? QString() : " WHERE " + filter));
    if (!account.isEmpty())
        q.bindValue(":account", account);
    if (!peer.isEmpty())
        q.bindValue(":peer", peer);
    return q.exec();
}
}
