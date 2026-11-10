/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historymediarefs.h"

#include "historymessageextras.h"
#include "iris/xmpp_message.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>
#include <QVariant>
#include <utility>
#include <algorithm>

namespace {
const QString tableName = QStringLiteral("message_media_refs");
const QString mayHaveAttachments = QStringLiteral("(instr(extra_data,'\"urn:xmpp:reference:0\"')>0 "
                                                   "OR instr(extra_data,'\"urn:xmpp:sfs:0\"')>0 "
                                                   "OR instr(extra_data,'\"jabber:x:oob\"')>0)");

std::optional<QJsonArray> indexOf(const QString &body, const QString &json)
{
    const auto extra = QJsonDocument::fromJson(json.toUtf8()).object();
    if (extra.contains(QStringLiteral("psi:media-refs"))) {
        const auto index = extra.value(QStringLiteral("psi:media-refs")).toObject();
        const int  version = index.value(QStringLiteral("version")).toInt();
        if ((version != 1 && version != HistoryMessageExtras::mediaRefsVersion)
            || !index.value(QStringLiteral("items")).isArray())
            return {};
        return index.value(QStringLiteral("items")).toArray();
    }
    // Released history before this feature stored only SIMS/OOB. Those
    // parsers do not prepare JIDs. Old development SFS rows are prepared on
    // the GUI before the migration worker starts.
    if (extra.contains(XMPP::StatelessFileSharing::NS)) {
        if (QCoreApplication::instance() && QThread::currentThread() != QCoreApplication::instance()->thread()) {
            qWarning("[history-media] SFS attachment index must be prepared on the GUI");
            return {};
        }
    }
    XMPP::Message message;
    message.setBody(body);
    HistoryMessageExtras::restoreFileSharing(json, message);
    return HistoryMessageExtras::mediaRefs(message, 1);
}
}

bool HistoryMediaRefs::needsMigration(QSqlDatabase db)
{
    if (!db.tables().contains(tableName))
        return true;
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT position,fingerprint FROM message_media_refs LIMIT 0")))
        return true;
    return false;
}

bool HistoryMediaRefs::prepareLegacySfs(QSqlDatabase db)
{
    Q_ASSERT(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread());
    if (!db.transaction())
        return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT id,m_text,extra_data FROM events WHERE "
                                 "instr(extra_data,'\"urn:xmpp:sfs:0\"')>0 "
                                 "AND instr(extra_data,'\"psi:media-refs\"')=0"));
    bool      ok = query.exec();
    QSqlQuery update(db);
    update.prepare(QStringLiteral("UPDATE events SET extra_data=:extra WHERE id=:id"));
    while (ok && query.next()) {
        const auto json  = query.value(2).toString();
        const auto index = indexOf(query.value(1).toString(), json);
        if (!index) {
            ok = false;
            break;
        }
        auto extra = QJsonDocument::fromJson(json.toUtf8()).object();
        extra.insert(QStringLiteral("psi:media-refs"), QJsonObject { { "version", 1 }, { "items", *index } });
        update.bindValue(QStringLiteral(":id"), query.value(0));
        update.bindValue(QStringLiteral(":extra"),
                         QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact)));
        ok = update.exec();
    }
    ok = ok && !query.lastError().isValid();
    query.finish();
    update.finish();
    if (ok && db.commit())
        return true;
    db.rollback();
    return false;
}

bool HistoryMediaRefs::migrate(QSqlDatabase db, const std::function<void(qint64)> &progress)
{
    if (!needsMigration(db))
        return true;
    if (!db.transaction())
        return false;
    QSqlQuery query(db);
    // The old development schema had neither a stable scan position nor an
    // attachment fingerprint. Rebuild derived links atomically; startup
    // reconciliation retires their old non-serving publications.
    bool ok = query.exec(QStringLiteral("DROP TABLE IF EXISTS message_media_refs"))
        && query.exec(QStringLiteral("CREATE TABLE message_media_refs ("
                                     "position INTEGER PRIMARY KEY AUTOINCREMENT, "
                                     "event_id INTEGER NOT NULL REFERENCES events(id) ON DELETE CASCADE, "
                                     "ordinal INTEGER NOT NULL, publication_id BLOB NOT NULL UNIQUE, "
                                     "fingerprint BLOB NOT NULL, UNIQUE(event_id,ordinal))"));
    qint64 last = 0;
    while (ok) {
        query.prepare(QStringLiteral("SELECT id FROM events WHERE id>:id AND ") + mayHaveAttachments
                      + QStringLiteral(" ORDER BY id LIMIT 500"));
        query.bindValue(QStringLiteral(":id"), last);
        ok = query.exec();
        QList<qint64> events;
        while (ok && query.next())
            events.append(query.value(0).toLongLong());
        ok = ok && !query.lastError().isValid();
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

bool HistoryMediaRefs::link(QSqlDatabase db, qint64 eventId, bool *removed)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT m_text,extra_data FROM events WHERE id=:id"));
    query.bindValue(QStringLiteral(":id"), eventId);
    if (!query.exec() || !query.next())
        return false;
    const auto index = indexOf(query.value(0).toString(), query.value(1).toString());
    if (!index)
        return false;
    query.finish();
    QHash<int, QByteArray> expected;
    for (const auto &value : *index) {
        const auto entry       = value.toObject();
        const int  ordinal     = entry.value(QStringLiteral("ordinal")).toInt(-1);
        const auto fingerprint = QByteArray::fromHex(entry.value(QStringLiteral("fingerprint")).toString().toLatin1());
        if (ordinal < 0 || expected.contains(ordinal) || fingerprint.size() != 32)
            return false;
        expected.insert(ordinal, fingerprint);
    }
    query.prepare(QStringLiteral("SELECT ordinal,fingerprint FROM message_media_refs WHERE event_id=:event"));
    query.bindValue(QStringLiteral(":event"), eventId);
    if (!query.exec())
        return false;
    QList<int> obsolete;
    while (query.next()) {
        const auto ordinal = query.value(0).toInt();
        if (!expected.contains(ordinal) || expected.value(ordinal) != query.value(1).toByteArray())
            obsolete.append(ordinal);
        else
            expected.remove(ordinal); // Replays must not consume AUTOINCREMENT positions or rewrite links.
    }
    if (query.lastError().isValid())
        return false;
    query.finish();
    query.prepare(QStringLiteral("DELETE FROM message_media_refs WHERE event_id=:event AND ordinal=:ordinal"));
    for (const auto ordinal : std::as_const(obsolete)) {
        query.bindValue(QStringLiteral(":event"), eventId);
        query.bindValue(QStringLiteral(":ordinal"), ordinal);
        if (!query.exec())
            return false;
    }
    if (removed && !obsolete.isEmpty())
        *removed = true;
    query.prepare(
        QStringLiteral("INSERT INTO message_media_refs(event_id,ordinal,publication_id,fingerprint) "
                       "VALUES(:event,:ordinal,:publication,:fingerprint)"));
    // Ordinals, and therefore scan positions, are deterministic within a message.
    auto ordinals = expected.keys();
    std::sort(ordinals.begin(), ordinals.end());
    for (const auto ordinal : ordinals) {
        query.bindValue(QStringLiteral(":event"), eventId);
        query.bindValue(QStringLiteral(":ordinal"), ordinal);
        query.bindValue(QStringLiteral(":publication"), QUuid::createUuid().toRfc4122());
        query.bindValue(QStringLiteral(":fingerprint"), expected.value(ordinal));
        if (!query.exec())
            return false;
    }
    return true;
}

std::optional<HistoryMediaRefs::Announcement> HistoryMediaRefs::announcement(const Link &link)
{
    XMPP::Message message;
    message.setBody(link.body);
    HistoryMessageExtras::restore(link.extra, message);
    const auto extra   = QJsonDocument::fromJson(link.extra.toUtf8()).object();
    const auto index   = extra.value(QStringLiteral("psi:media-refs")).toObject();
    const int  version = index.value(QStringLiteral("version")).toInt(1);
    const auto found   = HistoryMessageExtras::indexedFiles(message, version);
    if (link.ordinal < 0 || link.ordinal >= found.size())
        return {};
    const auto &file = found.at(link.ordinal);
    if (!file.hasContentHashes() && !file.url().isValid())
        return {};
    const bool incoming = link.direction != 1;
    return Announcement { link.account, !message.from().isEmpty() || !incoming ? message.from() : XMPP::Jid(link.peer),
                          file };
}
