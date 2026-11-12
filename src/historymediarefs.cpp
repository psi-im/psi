/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historymediarefs.h"

#include "fileidentity.h"
#include "historymessageextras.h"
#include "iris/xmpp_message.h"
#include <limits>

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
#include <algorithm>
#include <utility>

namespace {
const QString tableName          = QStringLiteral("message_media_refs");
const QString mayHaveAttachments = QStringLiteral("(instr(extra_data,'\"urn:xmpp:reference:0\"')>0 "
                                                  "OR instr(extra_data,'\"urn:xmpp:sfs:0\"')>0 "
                                                  "OR instr(extra_data,'\"jabber:x:oob\"')>0 "
                                                  "OR instr(extra_data,'\"psi:media-refs\"')>0 "
                                                  "OR lower(trim(m_text)) LIKE 'aesgcm://%')");

// Assets identify content independently of the messages that announce it.
// Only immutable aliases are shared; message metadata stays on its occurrence.
bool bindAsset(QSqlDatabase db, const QString &account, qint64 event, int ordinal, const QJsonObject &resource)
{
    QStringList aliases;
    for (const auto &value : resource.value("hashes").toArray())
        aliases.append(QStringLiteral("h:") + value.toObject().value("id").toString());
    if (aliases.isEmpty() && !resource.value("url").toString().isEmpty())
        aliases.append(QStringLiteral("u:") + resource.value("url").toString());
    if (aliases.isEmpty())
        return false;
    QSqlQuery    q(db);
    QSet<qint64> matches;
    for (const auto &alias : aliases) {
        q.prepare("SELECT asset_id FROM media_asset_aliases WHERE account=:account AND alias=:alias");
        q.bindValue(":account", account);
        q.bindValue(":alias", alias);
        if (!q.exec())
            return false;
        if (q.next())
            matches.insert(q.value(0).toLongLong());
    }
    // A contradictory announcement must not unite unrelated content identities.
    for (auto it = matches.begin(); it != matches.end();) {
        bool compatible = true;
        q.prepare("SELECT alias FROM media_asset_aliases WHERE asset_id=:asset");
        q.bindValue(":asset", *it);
        if (!q.exec())
            return false;
        while (q.next()) {
            const auto known = q.value(0).toString();
            for (const auto &alias : aliases)
                if (known.startsWith("h:") && alias.startsWith("h:")
                    && known.section('+', 0, 0) == alias.section('+', 0, 0) && known != alias)
                    compatible = false;
        }
        if (resource.contains("size")) {
            // Two indexed range probes avoid scanning every earlier occurrence
            // each time a frequently shared file is linked again.
            q.prepare("SELECT 1 FROM message_media_resources WHERE asset_id=:asset AND size<:size "
                      "UNION ALL SELECT 1 FROM message_media_resources WHERE asset_id=:asset AND size>:size LIMIT 1");
            q.bindValue(":asset", *it);
            q.bindValue(":size", resource.value("size").toString().toLongLong());
            if (!q.exec())
                return false;
            compatible &= !q.next();
        }
        if (compatible)
            ++it;
        else
            it = matches.erase(it);
    }
    qint64 asset = 0;
    if (matches.isEmpty()) {
        q.prepare("INSERT INTO media_assets(account) VALUES(:account)");
        q.bindValue(":account", account);
        if (!q.exec())
            return false;
        asset = q.lastInsertId().toLongLong();
    } else {
        asset = *std::min_element(matches.cbegin(), matches.cend());
        for (const auto previous : matches) {
            if (previous == asset)
                continue;
            for (const auto &sql : { "UPDATE message_media_resources SET asset_id=:asset WHERE asset_id=:previous",
                                     "UPDATE media_asset_aliases SET asset_id=:asset WHERE asset_id=:previous" }) {
                q.prepare(sql);
                q.bindValue(":asset", asset);
                q.bindValue(":previous", previous);
                if (!q.exec())
                    return false;
            }
            q.prepare("DELETE FROM media_assets WHERE id=:previous");
            q.bindValue(":previous", previous);
            if (!q.exec())
                return false;
        }
    }
    for (const auto &alias : aliases) {
        q.prepare("INSERT OR IGNORE INTO media_asset_aliases(account,alias,asset_id) VALUES(:account,:alias,:asset)");
        q.bindValue(":account", account);
        q.bindValue(":alias", alias);
        q.bindValue(":asset", asset);
        if (!q.exec())
            return false;
    }
    q.prepare(
        "UPDATE message_media_resources SET asset_id=:asset,kind=:kind,name=COALESCE(:name,''),mime=COALESCE(:mime,'') "
        "WHERE publication_id=(SELECT publication_id FROM message_media_refs WHERE event_id=:event AND "
        "ordinal=:ordinal)");
    q.bindValue(":asset", asset);
    q.bindValue(":kind", resource.value("kind").toInt(4));
    q.bindValue(":name", resource.value("name").toString());
    q.bindValue(":mime", resource.value("mime").toString());
    q.bindValue(":event", event);
    q.bindValue(":ordinal", ordinal);
    return q.exec();
}

std::optional<QJsonArray> indexOf(const QString &body, const QString &json)
{
    const auto extra = QJsonDocument::fromJson(json.toUtf8()).object();
    if (extra.contains(QStringLiteral("psi:media-refs"))) {
        const auto index   = extra.value(QStringLiteral("psi:media-refs")).toObject();
        const int  version = index.value(QStringLiteral("version")).toInt();
        if ((version != 1 && version != HistoryMessageExtras::mediaRefsVersion)
            || !index.value(QStringLiteral("items")).isArray())
            return {};
        const auto items = index.value(QStringLiteral("items")).toArray();
        if ((items.isEmpty() && !AesGcmUrl::parse(QUrl(body.trimmed())))
            || (!items.isEmpty() && std::all_of(items.cbegin(), items.cend(), [](const auto &value) {
                   return value.toObject().value("resource").toObject().contains("kind");
               })))
            return items;
        if (extra.contains(XMPP::StatelessFileSharing::NS) && QCoreApplication::instance()
            && QThread::currentThread() != QCoreApplication::instance()->thread())
            return {};
        XMPP::Message message;
        message.setBody(body);
        HistoryMessageExtras::restoreFileSharing(json, message);
        return HistoryMessageExtras::mediaRefs(message, version);
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
    return !query.exec(QStringLiteral("SELECT asset_id,kind,name,mime FROM message_media_resources LIMIT 0"));
}

bool HistoryMediaRefs::prepareLegacySfs(QSqlDatabase db)
{
    Q_ASSERT(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread());
    if (!db.transaction())
        return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id,m_text,extra_data FROM events WHERE "
        "instr(extra_data,'\"urn:xmpp:sfs:0\"')>0 AND "
        "(instr(extra_data,'\"psi:media-resources\":2')=0 OR instr(extra_data,'\"psi:media-refs\"')=0)"));
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
        auto       extra   = QJsonDocument::fromJson(json.toUtf8()).object();
        const auto version = extra.value("psi:media-refs").toObject().value("version").toInt(1);
        extra.insert(QStringLiteral("psi:media-refs"), QJsonObject { { "version", version }, { "items", *index } });
        extra.insert("psi:media-resources", 2);
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
    // Rebuild only obsolete link schemas. Adding resource bindings preserves
    // publication IDs, including the serving decisions that refer to them.
    const bool linksExist = query.exec(QStringLiteral("SELECT position,fingerprint FROM message_media_refs LIMIT 0"));
    query.finish();
    bool ok = linksExist;
    if (!linksExist) {
        ok = query.exec(QStringLiteral("DROP TABLE IF EXISTS message_media_refs"))
            && query.exec(QStringLiteral("CREATE TABLE message_media_refs ("
                                         "position INTEGER PRIMARY KEY AUTOINCREMENT, "
                                         "event_id INTEGER NOT NULL REFERENCES events(id) ON DELETE CASCADE, "
                                         "ordinal INTEGER NOT NULL, publication_id BLOB NOT NULL UNIQUE, "
                                         "fingerprint BLOB NOT NULL, UNIQUE(event_id,ordinal))"));
    }
    ok = ok
        && query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS message_media_resources ("
            "publication_id BLOB PRIMARY KEY NOT NULL REFERENCES message_media_refs(publication_id) ON DELETE CASCADE, "
            "size INTEGER, url_key TEXT)"))
        && query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS message_media_hashes ("
            "publication_id BLOB NOT NULL REFERENCES message_media_resources(publication_id) ON DELETE CASCADE, "
            "algorithm TEXT NOT NULL,digest BLOB NOT NULL,range_key TEXT, "
            "PRIMARY KEY(publication_id,algorithm,digest))"))
        && query.exec(
            QStringLiteral("CREATE INDEX IF NOT EXISTS message_media_range_key ON message_media_hashes(range_key)"));
    ok = ok
        && query.exec(
            "CREATE TABLE IF NOT EXISTS media_assets (id INTEGER PRIMARY KEY AUTOINCREMENT,account TEXT NOT NULL)")
        && query.exec(
            "CREATE TABLE IF NOT EXISTS media_asset_aliases (account TEXT NOT NULL,alias TEXT NOT NULL,"
            "asset_id INTEGER NOT NULL REFERENCES media_assets(id) ON DELETE CASCADE,PRIMARY KEY(account,alias))")
        && query.exec("CREATE INDEX IF NOT EXISTS media_asset_aliases_asset ON media_asset_aliases(asset_id)");
    // Extend the unreleased resource projection in place, including profiles
    // used during development. Stable publication IDs need no reconstruction.
    const auto columns = QList<QPair<QString, QString>> { { "asset_id", "INTEGER REFERENCES media_assets(id)" },
                                                          { "kind", "INTEGER NOT NULL DEFAULT 4" },
                                                          { "name", "TEXT NOT NULL DEFAULT ''" },
                                                          { "mime", "TEXT NOT NULL DEFAULT ''" } };
    for (const auto &column : columns) {
        if (!ok)
            break;
        if (!query.exec("SELECT " + column.first + " FROM message_media_resources LIMIT 0"))
            ok = query.exec("ALTER TABLE message_media_resources ADD COLUMN " + column.first + ' ' + column.second);
        query.finish();
    }
    ok = ok && query.exec("CREATE INDEX IF NOT EXISTS message_media_assets ON message_media_resources(asset_id,size)")
        && query.exec("CREATE TRIGGER IF NOT EXISTS media_asset_release AFTER DELETE ON message_media_resources "
                      "WHEN OLD.asset_id IS NOT NULL BEGIN DELETE FROM media_assets WHERE id=OLD.asset_id "
                      "AND NOT EXISTS(SELECT 1 FROM message_media_resources WHERE asset_id=OLD.asset_id); END");
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
    query.prepare(QStringLiteral(
        "SELECT e.m_text,e.extra_data,c.acc_id FROM events e JOIN contacts c ON c.id=e.contact_id WHERE e.id=:id"));
    query.bindValue(QStringLiteral(":id"), eventId);
    if (!query.exec() || !query.next())
        return false;
    const auto account = query.value(2).toString();
    const auto index   = indexOf(query.value(0).toString(), query.value(1).toString());
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
    query.prepare(QStringLiteral("INSERT INTO message_media_refs(event_id,ordinal,publication_id,fingerprint) "
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
    // Normalize the identities in the event's own transaction. Replays update
    // this projection without consuming a new scan position or publication ID.
    for (const auto &value : *index) {
        const auto entry    = value.toObject();
        const auto resource = entry.value("resource").toObject();
        bool       sizeOk   = false;
        const auto size     = resource.value("size").toString().toULongLong(&sizeOk);
        sizeOk &= size <= quint64(std::numeric_limits<qint64>::max());
        query.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO message_media_resources(publication_id,size,url_key) "
            "SELECT publication_id,:size,:url FROM message_media_refs WHERE event_id=:event AND ordinal=:ordinal"));
        query.bindValue(":event", eventId);
        query.bindValue(":ordinal", entry.value("ordinal").toInt());
        query.bindValue(":size", sizeOk ? QVariant(qlonglong(size)) : QVariant());
        query.bindValue(":url", resource.value("url").toString());
        if (!query.exec())
            return false;
        for (const auto &hashValue : resource.value("hashes").toArray()) {
            const auto hash = hashValue.toObject();
            query.prepare(
                QStringLiteral("INSERT OR IGNORE INTO message_media_hashes(publication_id,algorithm,digest,range_key) "
                               "SELECT publication_id,:algorithm,:digest,:key FROM message_media_refs WHERE "
                               "event_id=:event AND ordinal=:ordinal"));
            query.bindValue(":event", eventId);
            query.bindValue(":ordinal", entry.value("ordinal").toInt());
            query.bindValue(":algorithm", hash.value("algorithm").toString());
            query.bindValue(":digest", QByteArray::fromHex(hash.value("digest").toString().toLatin1()));
            query.bindValue(":key",
                            sizeOk ? FileIdentity::rangeKey(account, hash.value("id").toString(), size) : QString());
            if (!query.exec())
                return false;
        }
        if (!bindAsset(db, account, eventId, entry.value("ordinal").toInt(), resource))
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
