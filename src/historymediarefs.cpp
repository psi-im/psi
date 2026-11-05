/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historymediarefs.h"

#include "historymessageextras.h"
#include "iris/xmpp_message.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>
#include <utility>

namespace {
const QString tableName = QStringLiteral("message_media_refs");

// Spares nearly every message from being decoded.
const QString mayHaveAttachments = QStringLiteral("(instr(extra_data,'\"urn:xmpp:reference:0\"')>0 "
                                                   "OR instr(extra_data,'\"urn:xmpp:sfs:0\"')>0 "
                                                   "OR instr(extra_data,'\"jabber:x:oob\"')>0)");
}

bool HistoryMediaRefs::needsMigration(QSqlDatabase db) { return !db.tables().contains(tableName); }

bool HistoryMediaRefs::migrate(QSqlDatabase db, const std::function<void(qint64)> &progress)
{
    if (!needsMigration(db))
        return true;
    if (!db.transaction())
        return false;
    QSqlQuery query(db);
    bool      ok = query.exec(QStringLiteral("CREATE TABLE message_media_refs ("
                                                  "event_id INTEGER NOT NULL REFERENCES events(id) ON DELETE CASCADE, "
                                                  "ordinal INTEGER NOT NULL, publication_id BLOB NOT NULL, "
                                                  "PRIMARY KEY(event_id, ordinal))"))
        && query.exec(QStringLiteral("CREATE INDEX message_media_publication ON message_media_refs(publication_id)"));
    qint64 last = 0;
    while (ok) {
        query.prepare(QStringLiteral("SELECT id FROM events WHERE id>:id AND ") + mayHaveAttachments
                      + QStringLiteral(" ORDER BY id LIMIT 500"));
        query.bindValue(QStringLiteral(":id"), last);
        ok = query.exec();
        QList<qint64> events;
        while (ok && query.next())
            events.append(query.value(0).toLongLong());
        query.finish();
        if (events.isEmpty())
            break;
        for (const auto event : std::as_const(events))
            ok = ok && link(db, event);
        last = events.last();
        if (progress)
            progress(last);
    }
    if (ok && db.commit())
        return true;
    qWarning() << "[history-media] Cannot link stored attachments:" << query.lastError().text();
    db.rollback();
    return false;
}

bool HistoryMediaRefs::link(QSqlDatabase db, qint64 eventId)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT m_text,extra_data FROM events WHERE id=:id AND ") + mayHaveAttachments);
    query.bindValue(QStringLiteral(":id"), eventId);
    if (!query.exec())
        return false;
    if (!query.next())
        return true;
    // The history upgrade comes here on its worker thread.
    XMPP::Message message;
    message.setBody(query.value(0).toString());
    HistoryMessageExtras::restoreAttachments(query.value(1).toString(), message);
    const auto found = MessageAttachments::collect(message).attachments;
    query.finish();
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO message_media_refs(event_id,ordinal,publication_id) "
                                 "VALUES(:event,:ordinal,:publication)"));
    for (int ordinal = 0; ordinal < found.size(); ++ordinal) {
        if (!found.at(ordinal).isFile() && !found.at(ordinal).url().isValid())
            continue; // A mention, or a link for the user to follow.
        query.bindValue(QStringLiteral(":event"), eventId);
        query.bindValue(QStringLiteral(":ordinal"), ordinal);
        query.bindValue(QStringLiteral(":publication"), QUuid::createUuid().toRfc4122());
        if (!query.exec())
            return false;
    }
    return true;
}

QList<HistoryMediaRefs::Link> HistoryMediaRefs::linksAfter(QSqlDatabase db, qint64 position, int limit)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT rowid,publication_id FROM message_media_refs WHERE rowid>:position ORDER BY rowid LIMIT :limit"));
    query.bindValue(QStringLiteral(":position"), position);
    query.bindValue(QStringLiteral(":limit"), limit);
    QList<Link> links;
    if (query.exec())
        while (query.next())
            links.append({ query.value(0).toLongLong(), query.value(1).toByteArray() });
    return links;
}

std::optional<HistoryMediaRefs::Announcement> HistoryMediaRefs::announcement(QSqlDatabase      db,
                                                                             const QByteArray &publicationId)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT r.ordinal,e.m_text,e.extra_data,e.direction,c.acc_id,c.jid "
                                 "FROM message_media_refs r JOIN events e ON e.id=r.event_id "
                                 "JOIN contacts c ON c.id=e.contact_id WHERE r.publication_id=:id"));
    query.bindValue(QStringLiteral(":id"), publicationId);
    if (!query.exec() || !query.next())
        return {};
    XMPP::Message message;
    message.setBody(query.value(1).toString());
    HistoryMessageExtras::restore(query.value(2).toString(), message);
    const auto found   = MessageAttachments::collect(message).attachments;
    const int  ordinal = query.value(0).toInt();
    if (ordinal >= found.size())
        return {};
    // Rows from before messages kept their envelope know only the conversation.
    const bool incoming = query.value(3).toInt() != 1;
    return Announcement { query.value(4).toString(),
                          !message.from().isEmpty() || !incoming ? message.from()
                                                                 : XMPP::Jid(query.value(5).toString()),
                          found.at(ordinal) };
}

std::optional<QSet<QByteArray>> HistoryMediaRefs::linked(QSqlDatabase db, const QList<QByteArray> &publicationIds)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT 1 FROM message_media_refs WHERE publication_id=:id LIMIT 1"));
    QSet<QByteArray> result;
    for (const auto &id : publicationIds) {
        query.bindValue(QStringLiteral(":id"), id);
        if (!query.exec())
            return {};
        if (query.next())
            result.insert(id);
        query.finish();
    }
    return result;
}
