/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historyarchive.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlQuery>
#include <QVariant>
#include <algorithm>

namespace HistoryArchive {
namespace {
    enum Kind { Client = 0, Origin = 1, Stanza = 2, Archive = 3 };
    struct Alias {
        int     kind;
        QString authority, value;
    };
    struct Stored {
        Entry        entry;
        qint64       root    = 0;
        bool         deleted = false, undecoded = false;
        QList<Alias> aliases;
    };

    bool run(QSqlQuery &q)
    {
        if (q.exec())
            return true;
        qWarning().noquote() << "[local-deletion]" << q.lastError().text();
        return false;
    }

    QList<Alias> aliases(const Key &key, const Entry &entry)
    {
        QList<Alias> result;
        const auto   extra = QJsonDocument::fromJson(entry.extra.toUtf8()).object();
        if (key.room || (entry.type != 0 && entry.type != 1) || extra.value("psi:protocol-only").toBool()
            || !extra.value("psi:events").toObject().isEmpty())
            return result;
        auto add = [&result](int kind, const QString &authority, const QString &value) {
            if (!value.isEmpty())
                result.append({ kind, authority, value });
        };
        add(Client, QStringLiteral(""), entry.messageId);
        add(Client, QStringLiteral(""), entry.replaceId);
        add(Origin, QStringLiteral(""), extra.value("psi:origin").toString());
        if (!key.archive.isEmpty()) {
            add(Archive, key.archive, entry.uid);
            for (const auto &value : extra.value("psi:ids").toArray()) {
                const auto id = value.toObject();
                // Only our authenticated archive's authority can assign a stable ID.
                if (id.value("by").toString() == key.archive)
                    add(Stanza, key.archive, id.value("id").toString());
            }
        }
        return result;
    }

    void bind(QSqlQuery &q, const Key &key, int direction, const Alias &alias)
    {
        q.bindValue(":account", key.account);
        q.bindValue(":peer", key.peer);
        q.bindValue(":direction", direction);
        q.bindValue(":kind", alias.kind);
        q.bindValue(":authority", alias.authority);
        q.bindValue(":value", alias.value);
    }

    bool remember(QSqlDatabase db, const Key &key, int direction, const QList<Alias> &values)
    {
        QSqlQuery q(db);
        q.prepare("INSERT OR IGNORE INTO message_suppressions(account,peer,direction,kind,authority,value) "
                  "VALUES(:account,:peer,:direction,:kind,:authority,:value)");
        for (const auto &alias : values) {
            bind(q, key, direction, alias);
            if (!run(q))
                return false;
        }
        return true;
    }

    std::optional<Stored> load(QSqlDatabase db, const Key &key, qint64 id)
    {
        QSqlQuery q(db);
        q.prepare("SELECT e.*,COALESCE(r.root_id,e.id) AS root,r.stanza_id,r.replace_id,"
                  "EXISTS(SELECT 1 FROM message_state_refs s WHERE s.event_id=e.id AND s.kind=0) AS content "
                  "FROM events e JOIN contacts c ON c.id=e.contact_id "
                  "LEFT JOIN message_revisions r ON r.event_id=e.id "
                  "WHERE e.id=:id AND c.acc_id=:account AND c.jid=:peer AND c.type=1 AND e.type IN (0,1)");
        q.bindValue(":id", id);
        q.bindValue(":account", key.account);
        q.bindValue(":peer", key.peer);
        if (!run(q) || !q.next())
            return {};
        Stored stored;
        stored.root          = q.value("root").toLongLong();
        stored.deleted       = q.value("message_flags").toInt() & MessageState::LocalDeleted;
        auto &entry          = stored.entry;
        entry.eventId        = id;
        entry.type           = q.value("type").toInt();
        entry.direction      = q.value("direction").toInt();
        entry.extra          = q.value("extra_data").toString();
        const auto extra     = QJsonDocument::fromJson(entry.extra.toUtf8()).object();
        const bool undecoded = stored.undecoded
            = (extra.contains("psi:decoded") && !extra.value("psi:decoded").toBool())
            || extra.value("psi:decryption-error").toBool();
        if (!stored.deleted && !q.value("content").toBool()
            && (!undecoded || extra.value("psi:protocol-only").toBool()
                || !extra.value("psi:events").toObject().isEmpty()))
            return {};
        entry.messageId = q.value("stanza_id").toString();
        entry.replaceId = q.value("replace_id").toString();
        stored.aliases  = aliases(key, entry);
        q.finish();
        // Scrubbed rows retain the identity index even when extra_data is gone.
        q.prepare("SELECT kind,target FROM message_state_refs WHERE event_id=:id AND kind IN (1,2)");
        q.bindValue(":id", id);
        if (!run(q))
            return {};
        while (q.next())
            stored.aliases.append(
                { q.value(0).toInt() == 1 ? Client : Origin, QStringLiteral(""), q.value(1).toString() });
        if (q.lastError().isValid())
            return {};
        q.finish();
        q.prepare("SELECT archive,uid FROM archive_items WHERE event_id=:id AND account=:account AND peer=:peer");
        q.bindValue(":id", id);
        q.bindValue(":account", key.account);
        q.bindValue(":peer", key.peer);
        if (!run(q))
            return {};
        while (q.next())
            stored.aliases.append({ Archive, q.value(0).toString(), q.value(1).toString() });
        if (q.lastError().isValid())
            return {};
        q.finish();
        q.prepare("SELECT authority,value FROM message_stanza_ids WHERE event_id=:id");
        q.bindValue(":id", id);
        if (!run(q))
            return {};
        while (q.next())
            if (key.archive.isEmpty() || q.value(0).toString() == key.archive)
                stored.aliases.append({ Stanza, q.value(0).toString(), q.value(1).toString() });
        if (q.lastError().isValid())
            return {};
        return stored;
    }

    std::optional<QList<qint64>> candidates(QSqlDatabase db, const Key &key, int direction, const Alias &alias)
    {
        QSqlQuery q(db);
        if (alias.kind == Archive) {
            q.prepare("SELECT i.event_id FROM archive_items i JOIN events e ON e.id=i.event_id "
                      "WHERE i.account=:account AND i.peer=:peer AND i.archive=:authority AND i.uid=:value "
                      "AND e.direction=:direction");
            q.bindValue(":authority", alias.authority);
        } else if (alias.kind == Client || alias.kind == Origin) {
            q.prepare("SELECT s.event_id FROM message_state_refs s JOIN contacts c ON c.id=s.contact_id "
                      "JOIN events e ON e.id=s.event_id WHERE c.acc_id=:account AND c.jid=:peer AND c.type=1 "
                      "AND e.direction=:direction AND s.kind=:kind AND s.target=:value");
            q.bindValue(":kind", alias.kind == Client ? 1 : 2);
        } else {
            q.prepare("SELECT s.event_id FROM message_stanza_ids s JOIN contacts c ON c.id=s.contact_id "
                      "JOIN events e ON e.id=s.event_id WHERE c.acc_id=:account AND c.jid=:peer AND c.type=1 "
                      "AND e.direction=:direction AND s.authority=:authority AND s.value=:value");
            q.bindValue(":authority", alias.authority);
        }
        q.bindValue(":account", key.account);
        q.bindValue(":peer", key.peer);
        q.bindValue(":direction", direction);
        q.bindValue(":value", alias.value);
        if (!run(q))
            return {};
        QList<qint64> ids;
        while (q.next())
            ids.append(q.value(0).toLongLong());
        if (q.lastError().isValid())
            return {};
        return ids;
    }

    bool scrub(QSqlDatabase db, const Key &key, const QList<qint64> &initial, LocalDeletionResult &changes,
               bool strict = true)
    {
        QList<qint64> pending = initial;
        QSet<qint64>  visited;
        QSet<qint64>  visitedRoots;
        const bool    hasMedia = db.tables().contains(QStringLiteral("message_media_refs"));
        while (!pending.isEmpty()) {
            const auto id = pending.takeLast();
            if (visited.contains(id))
                continue;
            visited.insert(id);
            const auto stored = load(db, key, id);
            if (!stored)
                return false;
            QSqlQuery q(db);
            // Reused client IDs can connect unrelated originals. Refuse the
            // destructive component, even if the renderer once merged it.
            q.prepare("SELECT 1 FROM message_revisions r JOIN message_revisions other "
                      "ON other.contact_id=r.contact_id AND other.type=r.type AND other.direction=r.direction "
                      "AND other.sender=r.sender AND other.stanza_id=r.stanza_id AND other.event_id<>r.event_id "
                      "WHERE r.root_id=:root LIMIT 1");
            q.bindValue(":root", stored->root);
            if (!run(q) || q.next() || q.lastError().isValid())
                return false;
            q.finish();
            if (!visitedRoots.contains(stored->root)) {
                visitedRoots.insert(stored->root);
                q.prepare("SELECT event_id FROM message_revisions WHERE root_id=:root");
                q.bindValue(":root", stored->root);
                if (!run(q))
                    return false;
                while (q.next())
                    pending.append(q.value(0).toLongLong());
                if (q.lastError().isValid())
                    return false;
                q.finish();
            }
            for (const auto &alias : stored->aliases) {
                const auto related = candidates(db, key, stored->entry.direction, alias);
                if (!related)
                    return false;
                // Reused aliases across distinct components are ambiguous too.
                for (const auto next : *related) {
                    const auto other = load(db, key, next);
                    if (!other || (strict && other->root != stored->root))
                        return false;
                }
            }
            if (!remember(db, key, stored->entry.direction, stored->aliases))
                return false;
            q.prepare("INSERT OR IGNORE INTO message_state_refs(event_id,contact_id,kind,target) "
                      "SELECT id,contact_id,0,'' FROM events WHERE id=:id");
            q.bindValue(":id", id);
            if (!run(q))
                return false;
            q.prepare("UPDATE events SET m_text='',subject='',lang='',extra_data='',resource='', "
                      "message_flags=message_flags | :deleted WHERE id=:id");
            q.bindValue(":id", id);
            q.bindValue(":deleted", int(MessageState::LocalDeleted));
            if (!run(q))
                return false;
            q.prepare("UPDATE archive_items SET xml='' WHERE event_id=:id");
            q.bindValue(":id", id);
            if (!run(q))
                return false;
            if (hasMedia) {
                q.prepare("DELETE FROM message_media_refs WHERE event_id=:id");
                q.bindValue(":id", id);
                if (!run(q))
                    return false;
            }
            if (!stored->deleted)
                changes.hiddenEvents.append(id);
            auto &ids = stored->entry.direction == 1 ? changes.outgoingIds : changes.incomingIds;
            for (const auto &alias : stored->aliases)
                if (alias.kind == Client && !ids.contains(alias.value))
                    ids.append(alias.value);
        }
        return true;
    }
}

std::optional<bool> matchesLocalSuppression(QSqlDatabase db, const Key &key, const Entry &entry)
{
    if (key.room || key.account.isEmpty() || key.peer.isEmpty())
        return false;
    const auto values = aliases(key, entry);
    QSqlQuery  q(db);
    q.prepare("SELECT 1 FROM message_suppressions WHERE account=:account AND peer=:peer "
              "AND direction=:direction AND kind=:kind AND authority=:authority AND value=:value");
    bool matched = false;
    for (const auto &alias : values) {
        bind(q, key, entry.direction, alias);
        if (!run(q))
            return {};
        if (q.next())
            matched = true;
        if (q.lastError().isValid())
            return {};
        q.finish();
    }
    return matched;
}

std::optional<bool> applyLocalSuppression(QSqlDatabase db, const Key &key, Entry &entry, LocalDeletionResult *changes)
{
    const auto matched = matchesLocalSuppression(db, key, entry);
    if (!matched || !*matched)
        return matched;
    const auto    values = aliases(key, entry);
    QList<qint64> ids;
    for (const auto &alias : values) {
        const auto found = candidates(db, key, entry.direction, alias);
        if (!found)
            return {};
        for (const auto id : *found) {
            const auto stored = load(db, key, id);
            if (!stored)
                return {};
            const auto known = matchesLocalSuppression(db, key, stored->entry);
            if (!known)
                return {};
            const bool sameEnvelope
                = stored->undecoded && !entry.uid.isEmpty()
                && std::any_of(stored->aliases.cbegin(), stored->aliases.cend(), [&entry, &key](const Alias &value) {
                       return value.kind == Archive && value.authority == key.archive && value.value == entry.uid;
                   });
            if (!stored->deleted && !*known && !sameEnvelope) {
                // A correction must not use a reused ID to delete another
                // already decoded delivery. Drop this conflicting announcement
                // without learning its aliases or certifying its cache position.
                entry.suppressed = true;
                entry.storable   = false;
                entry.body = entry.subject = entry.language = entry.extra = entry.xml = QStringLiteral("");
                return true;
            }
            ids.append(id);
        }
    }
    LocalDeletionResult retired;
    if (!scrub(db, key, ids, retired, false) || !remember(db, key, entry.direction, values))
        return {};
    auto &clientIds = entry.direction == 1 ? retired.outgoingIds : retired.incomingIds;
    for (const auto &alias : values)
        if (alias.kind == Client && !clientIds.contains(alias.value))
            clientIds.append(alias.value);
    if (changes) {
        changes->hiddenEvents.append(retired.hiddenEvents);
        changes->outgoingIds.append(retired.outgoingIds);
        changes->incomingIds.append(retired.incomingIds);
    }
    entry.suppressed = true;
    entry.body = entry.subject = entry.language = entry.extra = entry.xml = QStringLiteral("");
    return true;
}

LocalDeletionResult checkLocalSuppression(QSqlDatabase db, const Key &key, Entry entry)
{
    LocalDeletionResult result;
    if (!db.transaction())
        return result;
    const auto suppressed = applyLocalSuppression(db, key, entry, &result);
    if (suppressed && db.commit()) {
        result.success    = true;
        result.suppressed = *suppressed;
        return result;
    }
    db.rollback();
    return {};
}

LocalDeletionResult deleteLocalMessage(QSqlDatabase db, const Key &key, qint64 id, const QString &clientId,
                                       int direction)
{
    if (key.room || key.account.isEmpty() || key.peer.isEmpty() || !db.transaction())
        return {};
    LocalDeletionResult result;
    if (id <= 0 && !clientId.isEmpty() && (direction == 1 || direction == 2)) {
        const auto found = candidates(db, key, direction, { Client, QStringLiteral(""), clientId });
        if (found && found->size() == 1)
            id = found->first();
    }
    if (id > 0 && scrub(db, key, { id }, result) && db.commit()) {
        result.success = result.suppressed = true;
        return result;
    }
    db.rollback();
    return {};
}

bool scrubStoredMessage(QSqlDatabase db, const Key &key, qint64 id) { return deleteLocalMessage(db, key, id).success; }

bool backfillLocalSuppressions(QSqlDatabase db, const std::function<void(qint64)> &progress)
{
    qint64     after    = 0;
    const bool hasMedia = db.tables().contains(QStringLiteral("message_media_refs"));
    for (;;) {
        QSqlQuery q(db);
        q.prepare("SELECT e.id,c.acc_id,c.jid FROM events e JOIN contacts c ON c.id=e.contact_id "
                  "WHERE c.type=1 AND (e.message_flags & 16)<>0 AND e.id>:after ORDER BY e.id LIMIT 500");
        q.bindValue(":after", after);
        if (!q.exec())
            return false;
        struct Row {
            qint64 id;
            Key    key;
        };
        QList<Row> rows;
        while (q.next())
            rows.append({ q.value(0).toLongLong(), { q.value(1).toString(), {}, q.value(2).toString(), false } });
        if (q.lastError().isValid())
            return false;
        q.finish();
        if (rows.isEmpty())
            return true;
        for (const auto &row : rows) {
            const auto stored = load(db, row.key, row.id);
            if (!stored)
                return false;
            QList<Alias> safe;
            for (const auto &alias : stored->aliases) {
                const auto related = candidates(db, row.key, stored->entry.direction, alias);
                if (!related)
                    return false;
                bool conflict = false;
                for (const auto id : *related) {
                    const auto other = load(db, row.key, id);
                    if (!other)
                        return false;
                    conflict |= !other->deleted && other->root != stored->root;
                }
                if (!conflict)
                    safe.append(alias);
            }
            if (!remember(db, row.key, stored->entry.direction, safe))
                return false;
            if (hasMedia) {
                QSqlQuery release(db);
                release.prepare("DELETE FROM message_media_refs WHERE event_id=:id");
                release.bindValue(":id", row.id);
                if (!run(release))
                    return false;
            }
        }
        after = rows.last().id;
        if (progress)
            progress(after);
    }
}
}
