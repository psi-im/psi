/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historyarchive.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QXmlStreamReader>
#include <algorithm>

namespace HistoryArchive {
namespace {
    // Content is also indexed for messages without an ID. Protocol references
    // are retained even when their target has not been downloaded yet.
    // Persisted values: append new kinds without renumbering the existing ones.
    enum RefKind { Content = 0, ClientId = 1, OriginId = 2, RoomId = 3, Displayed = 4, Receipt = 5, Retract = 6 };
    using Ref = QPair<int, QString>;
    struct Event {
        qint64      id = 0, contact = 0, root = 0, time = 0;
        int         type = 0, direction = 0;
        QString     peer, sender, occupant, stanza, replace;
        QJsonObject extra;
        QList<Ref>  refs;
        bool        content = false, validTime = false, edited = false;
    };
    bool run(QSqlQuery &query)
    {
        if (query.exec())
            return true;
        qWarning().noquote() << "[history-message-state]" << query.lastError().text();
        return false;
    }

    // Old caches kept some control events only in archive XML. This is a
    // migration fallback for plaintext metadata, never a second decoder.
    void legacyMetadata(const QString &xml, QJsonObject &extra, bool &encrypted)
    {
        QXmlStreamReader reader(xml);
        QJsonObject      envelope = extra.value("psi:message").toObject();
        QJsonObject      events   = extra.value("psi:events").toObject();
        QJsonArray       ids      = extra.value("psi:ids").toArray();
        int              depth = 0, messageDepth = -1;
        QString          applyTo;
        while (!reader.atEnd()) {
            reader.readNext();
            if (reader.isEndElement()) {
                if (depth == messageDepth)
                    break;
                --depth;
                continue;
            }
            if (!reader.isStartElement())
                continue;
            ++depth;
            const auto name       = reader.name();
            const auto ns         = reader.namespaceUri();
            const auto attributes = reader.attributes();
            if (messageDepth < 0 && name == QLatin1String("message")) {
                messageDepth = depth;
                for (const auto &key : { "id", "from", "to" })
                    if (!envelope.contains(QLatin1String(key)))
                        envelope.insert(QLatin1String(key), attributes.value(QLatin1String(key)).toString());
            } else if (messageDepth > 0 && depth == messageDepth + 1) {
                const auto id = attributes.value(QStringLiteral("id")).toString();
                if (name == QLatin1String("encrypted"))
                    encrypted = true;
                else if (name == QLatin1String("displayed") && ns == QLatin1String("urn:xmpp:chat-markers:0"))
                    events.insert("displayed", id);
                else if (name == QLatin1String("received") && ns == QLatin1String("urn:xmpp:receipts"))
                    events.insert("receipt", id);
                else if (name == QLatin1String("retract") && ns.startsWith(QLatin1String("urn:xmpp:message-retract:")))
                    events.insert("retract", id);
                else if (name == QLatin1String("apply-to") && ns == QLatin1String("urn:xmpp:fasten:0"))
                    applyTo = id;
                else if (name == QLatin1String("replace") && ns == QLatin1String("urn:xmpp:message-correct:0"))
                    envelope.insert("replace", id);
                else if (name == QLatin1String("origin-id") && ns == QLatin1String("urn:xmpp:sid:0"))
                    extra.insert("psi:origin", id);
                else if (name == QLatin1String("stanza-id") && ns == QLatin1String("urn:xmpp:sid:0"))
                    ids.append(
                        QJsonObject { { "id", id }, { "by", attributes.value(QStringLiteral("by")).toString() } });
                else if (name == QLatin1String("occupant-id") && ns == QLatin1String("urn:xmpp:occupant-id:0"))
                    extra.insert("psi:occupant", id);
            } else if (!applyTo.isEmpty() && depth == messageDepth + 2 && name == QLatin1String("retract")
                       && ns == QLatin1String("urn:xmpp:message-retract:0"))
                events.insert("retract", applyTo);
        }
        extra.insert("psi:message", envelope);
        extra.insert("psi:events", events);
        extra.insert("psi:ids", ids);
    }

    bool loadEvent(QSqlDatabase db, qint64 id, Event &event)
    {
        event = {};
        QSqlQuery query(db);
        query.prepare("SELECT e.contact_id,e.date,e.history_date_ms,e.type,e.direction,e.extra_data,"
                      "e.m_text IS NOT NULL AS has_body,COALESCE(e.subject,'')<>'' AS has_subject,"
                      "c.jid,COALESCE(r.root_id,e.id) AS root,r.stanza_id,r.replace_id,r.sender,e.message_flags "
                      "FROM events e JOIN contacts c ON c.id=e.contact_id "
                      "LEFT JOIN message_revisions r ON r.event_id=e.id WHERE e.id=:id");
        query.bindValue(":id", id);
        if (!run(query) || !query.next())
            return false;
        const auto row  = query.record();
        event.id        = id;
        event.contact   = row.value("contact_id").toLongLong();
        event.root      = row.value("root").toLongLong();
        event.time      = row.value("history_date_ms").toLongLong();
        event.validTime = row.value("date").toDateTime().isValid();
        event.type      = row.value("type").toInt();
        event.direction = row.value("direction").toInt();
        event.peer      = row.value("jid").toString();
        event.extra     = QJsonDocument::fromJson(row.value("extra_data").toString().toUtf8()).object();
        query.finish();
        if (row.value("message_flags").toInt() & MessageState::LocalDeleted) {
            // Scrubbed payloads cannot reconstruct origin IDs or other aliases.
            // Keep the existing identity index through replay and state rebuilds;
            // never decode deleted XML or infer new effects from empty content.
            event.sender  = row.value("sender").toString();
            event.stanza  = row.value("stanza_id").toString();
            event.replace = row.value("replace_id").toString();
            query.prepare("SELECT kind,target FROM message_state_refs WHERE event_id=:id AND kind<=:last_kind "
                          "ORDER BY kind,target");
            query.bindValue(":id", id);
            query.bindValue(":last_kind", int(RoomId));
            if (!run(query))
                return false;
            while (query.next())
                event.refs.append({ query.value(0).toInt(), query.value(1).toString() });
            std::sort(event.refs.begin(), event.refs.end());
            event.content = event.refs.contains({ Content, QStringLiteral("") });
            return true;
        }
        if (event.type != 0 && event.type != 1 && event.type != 9)
            return true;
        if (event.extra.value("psi:decryption-error").toBool()
            || (event.extra.contains("psi:decoded") && !event.extra.value("psi:decoded").toBool()))
            return true;
        bool encrypted = false;
        if (!event.extra.contains("psi:decoded") || (event.type == 9 && !event.extra.contains("psi:occupant"))) {
            query.prepare("SELECT xml FROM archive_items WHERE event_id=:id LIMIT 1");
            query.bindValue(":id", id);
            if (!run(query))
                return false;
            if (query.next())
                legacyMetadata(query.value(0).toString(), event.extra, encrypted);
            query.finish();
            if (encrypted && !event.extra.value("psi:decoded").toBool())
                return true; // Unprocessed ciphertext is not evidence of an effect.
        }
        const auto envelope = event.extra.value("psi:message").toObject();
        event.sender        = envelope.value("from").toString();
        if (event.type == 9 && !row.value("sender").toString().isEmpty())
            event.sender = row.value("sender").toString();
        event.occupant = event.extra.value("psi:occupant").toString();
        event.stanza   = row.value("stanza_id").toString();
        if (event.stanza.isEmpty())
            event.stanza = envelope.value("id").toString();
        event.replace = row.value("replace_id").toString();
        if (event.replace.isEmpty())
            event.replace = envelope.value("replace").toString();
        const auto effects = event.extra.value("psi:events").toObject();
        event.content      = effects.isEmpty() && !event.extra.value("psi:protocol-only").toBool()
            && (row.value("has_body").toBool() || row.value("has_subject").toBool() || event.extra.contains("psi:html")
                || event.extra.contains("urn:xmpp:reference:0") || event.extra.contains("jabber:x:oob"));
        auto add = [&event](int kind, const QString &target) {
            if (kind == Content || !target.isEmpty())
                event.refs.append({ kind, target });
        };
        if (event.content) {
            add(Content, QStringLiteral(""));
            add(ClientId, event.stanza);
            add(OriginId, event.extra.value("psi:origin").toString());
            if (event.type == 9) {
                for (const auto &value : event.extra.value("psi:ids").toArray()) {
                    const auto assigned = value.toObject();
                    if (assigned.value("by").toString() == event.peer)
                        add(RoomId, assigned.value("id").toString());
                }
                query.prepare("SELECT uid FROM archive_items WHERE event_id=:id AND peer=:peer");
                query.bindValue(":id", id);
                query.bindValue(":peer", event.peer);
                if (!run(query))
                    return false;
                while (query.next())
                    add(RoomId, query.value(0).toString());
            }
        } else {
            // MUC read state requires one cursor per reader, not a single bit.
            if (event.type != 9) {
                add(Displayed, effects.value("displayed").toString());
                add(Receipt, effects.value("receipt").toString());
            }
            add(Retract, effects.value("retract").toString());
        }
        std::sort(event.refs.begin(), event.refs.end());
        event.refs.erase(std::unique(event.refs.begin(), event.refs.end()), event.refs.end());
        return true;
    }

    bool setFlags(QSqlDatabase db, qint64 root, int flags)
    {
        QSqlQuery query(db);
        query.prepare("UPDATE events SET message_flags=message_flags | :flags WHERE id=:id");
        query.bindValue(":id", root);
        query.bindValue(":flags", flags);
        return run(query);
    }

    bool applyEffect(QSqlDatabase db, const Event &effect, const Ref &ref)
    {
        QSqlQuery query(db);
        // A direct marker names message@id. Retractions can also name an older
        // origin ID; MUC retractions use room IDs, with origin IDs as fallback.
        const QString kinds = ref.first != Retract ? QStringLiteral("1")
            : effect.type == 9                     ? QStringLiteral("2,3")
                                                   : QStringLiteral("1,2");
        query.prepare("SELECT DISTINCT COALESCE(r.root_id,e.id) FROM message_state_refs ids "
                      "JOIN events e ON e.id=ids.event_id LEFT JOIN message_revisions r ON r.event_id=e.id "
                      "WHERE ids.contact_id=:contact AND ids.target=:target AND ids.kind IN ("
                      + kinds
                      + ") AND (:room=1 OR e.direction=:direction) AND "
                        "((:room=1 AND e.type=9) OR (:room=0 AND e.type IN (0,1)))");
        query.bindValue(":contact", effect.contact);
        query.bindValue(":target", ref.second);
        query.bindValue(":direction", ref.first == Retract ? effect.direction : 3 - effect.direction);
        query.bindValue(":room", effect.type == 9);
        if (!run(query))
            return false;
        QList<qint64> roots;
        while (query.next())
            roots.append(query.value(0).toLongLong());
        query.finish();
        QList<Event> targets;
        for (const auto root : roots) {
            Event target;
            if (!loadEvent(db, root, target))
                return false;
            if (ref.first == Retract && effect.type == 9
                && (effect.sender.isEmpty() || effect.sender != target.sender
                    || ((!effect.occupant.isEmpty() || !target.occupant.isEmpty())
                        && effect.occupant != target.occupant)))
                continue;
            targets.append(target);
        }
        if (targets.size() != 1)
            return true; // Unknown or ambiguous references never change a message.
        const auto &target = targets.first();
        if (ref.first != Displayed)
            return setFlags(db, target.root, ref.first == Retract ? MessageState::Retracted : MessageState::Delivered);
        if (!target.validTime)
            return true;
        query.prepare("SELECT history_date_ms,id FROM events WHERE contact_id=:contact AND direction=:direction "
                      "AND (message_flags & 1)<>0 ORDER BY history_date_ms DESC,id DESC LIMIT 1");
        query.bindValue(":contact", target.contact);
        query.bindValue(":direction", target.direction);
        if (!run(query))
            return false;
        const bool   hadPrefix    = query.next();
        const qint64 previousTime = hadPrefix ? query.value(0).toLongLong() : 0;
        const qint64 previousId   = hadPrefix ? query.value(1).toLongLong() : 0;
        query.finish();
        if (hadPrefix && (target.time < previousTime || (target.time == previousTime && target.root <= previousId)))
            return true;
        query.prepare(
            "UPDATE events SET message_flags=message_flags | 1 "
            "WHERE contact_id=:contact AND direction=:direction AND type IN (0,1) "
            "AND (history_date_ms,id)<=(:time,:id) AND (message_flags & 1)=0 "
            "AND EXISTS(SELECT 1 FROM message_state_refs s WHERE s.event_id=events.id AND s.kind=0) "
            "AND NOT EXISTS(SELECT 1 FROM message_revisions r WHERE r.event_id=events.id AND r.root_id<>events.id)"
            + (hadPrefix ? QStringLiteral(" AND (history_date_ms,id)>(:previousTime,:previousId)") : QString()));
        query.bindValue(":contact", target.contact);
        query.bindValue(":direction", target.direction);
        query.bindValue(":time", target.time);
        query.bindValue(":id", target.root);
        if (hadPrefix) {
            query.bindValue(":previousTime", previousTime);
            query.bindValue(":previousId", previousId);
        }
        return run(query);
    }

    bool rebuildContact(QSqlDatabase db, qint64 contact)
    {
        QSqlQuery query(db);
        // Recompute only the managed bits; future extensions keep their own bits.
        query.prepare("UPDATE events SET message_flags=message_flags & ~15 WHERE contact_id=:contact");
        query.bindValue(":contact", contact);
        if (!run(query))
            return false;
        query.prepare("UPDATE events SET message_flags=message_flags | 4 WHERE contact_id=:contact AND id IN "
                      "(SELECT root_id FROM message_revisions WHERE contact_id=:contact AND replace_id<>'')");
        query.bindValue(":contact", contact);
        if (!run(query))
            return false;
        query.prepare("SELECT event_id,kind,target FROM message_state_refs WHERE contact_id=:contact AND kind>=4");
        query.bindValue(":contact", contact);
        if (!run(query))
            return false;
        while (query.next()) {
            Event effect;
            if (!loadEvent(db, query.value(0).toLongLong(), effect)
                || !applyEffect(db, effect, { query.value(1).toInt(), query.value(2).toString() }))
                return false;
        }
        return true;
    }

    bool indexEvent(QSqlDatabase db, qint64 id, Event &event, bool &rebuild)
    {
        if (!loadEvent(db, id, event))
            return false;
        QSqlQuery query(db);
        if (event.content && !event.stanza.isEmpty()) {
            query.prepare("SELECT 1 FROM message_revisions WHERE event_id=:id");
            query.bindValue(":id", id);
            if (!run(query))
                return false;
            const bool indexed = query.next();
            query.finish();
            if (!indexed
                && (!recordRevision(db, id, event.contact, event.type, event.direction, event.sender, event.stanza,
                                    event.replace)
                    || !loadEvent(db, id, event)))
                return false;
        }
        query.prepare("SELECT kind,target FROM message_state_refs WHERE event_id=:id ORDER BY kind,target");
        query.bindValue(":id", id);
        if (!run(query))
            return false;
        QList<Ref> previous;
        while (query.next())
            previous.append({ query.value(0).toInt(), query.value(1).toString() });
        query.finish();
        std::sort(previous.begin(), previous.end());
        rebuild |= !previous.isEmpty() && previous != event.refs;
        if (previous != event.refs) {
            query.prepare("DELETE FROM message_state_refs WHERE event_id=:id");
            query.bindValue(":id", id);
            if (!run(query))
                return false;
            query.prepare(
                "INSERT INTO message_state_refs(event_id,contact_id,kind,target) VALUES(:id,:contact,:kind,:target)");
            for (const auto &ref : event.refs) {
                query.bindValue(":id", id);
                query.bindValue(":contact", event.contact);
                query.bindValue(":kind", ref.first);
                query.bindValue(":target", ref.second);
                if (!run(query))
                    return false;
            }
        }
        if (event.content) {
            query.prepare("SELECT EXISTS(SELECT 1 FROM message_revisions WHERE root_id=:root AND replace_id<>''),"
                          "EXISTS(SELECT 1 FROM message_revisions r JOIN events e ON e.id=r.event_id "
                          "WHERE r.root_id=:root AND r.root_id<>r.event_id AND (e.message_flags & 15)<>0)");
            query.bindValue(":root", event.root);
            if (!run(query) || !query.next())
                return false;
            // An original arriving after its correction can move the provisional
            // root backwards. Rebuild prefixes using the new anchored positions.
            event.edited = query.value(0).toBool();
            rebuild |= query.value(1).toBool();
            query.finish();
        }
        QSet<QString> checked;
        for (const auto &ref : event.refs) {
            if (ref.first < ClientId || ref.first > RoomId)
                continue;
            if (checked.contains(ref.second))
                continue;
            checked.insert(ref.second);
            query.prepare("SELECT COUNT(DISTINCT COALESCE(r.root_id,e.id)) FROM message_state_refs s "
                          "JOIN events e ON e.id=s.event_id LEFT JOIN message_revisions r ON r.event_id=e.id "
                          "WHERE s.contact_id=:contact AND s.target=:target AND s.kind BETWEEN 1 AND 3 AND "
                          "(:room=1 OR e.direction=:direction) "
                          "AND (:room=0 OR r.sender=:sender OR r.sender IS NULL)");
            query.bindValue(":contact", event.contact);
            query.bindValue(":target", ref.second);
            query.bindValue(":direction", event.direction);
            query.bindValue(":room", event.type == 9);
            query.bindValue(":sender", event.sender);
            if (!run(query) || !query.next())
                return false;
            rebuild |= query.value(0).toInt() > 1;
            query.finish();
        }
        return true;
    }

    bool applyEvent(QSqlDatabase db, const Event &event)
    {
        if (event.content) {
            if ((event.edited || !event.replace.isEmpty()) && !setFlags(db, event.root, MessageState::Edited))
                return false;
            QSqlQuery query(db);
            if (event.type != 9 && event.validTime) {
                // A newly loaded old page inherits the already persisted prefix.
                query.prepare(
                    "SELECT history_date_ms,id FROM events WHERE contact_id=:contact AND direction=:direction "
                    "AND (message_flags & 1)<>0 ORDER BY history_date_ms DESC,id DESC LIMIT 1");
                query.bindValue(":contact", event.contact);
                query.bindValue(":direction", event.direction);
                if (!run(query))
                    return false;
                const bool read = query.next()
                    && (event.time < query.value(0).toLongLong()
                        || (event.time == query.value(0).toLongLong() && event.root <= query.value(1).toLongLong()));
                query.finish();
                if (read && !setFlags(db, event.root, MessageState::Displayed))
                    return false;
            }
            QSet<qint64>  effects;
            QSet<QString> checked;
            for (const auto &ref : event.refs) {
                if (ref.first == Content || checked.contains(ref.second))
                    continue;
                checked.insert(ref.second);
                query.prepare(
                    "SELECT event_id FROM message_state_refs WHERE contact_id=:contact AND target=:target AND kind>=4");
                query.bindValue(":contact", event.contact);
                query.bindValue(":target", ref.second);
                if (!run(query))
                    return false;
                while (query.next())
                    effects.insert(query.value(0).toLongLong());
                query.finish();
            }
            for (const auto id : effects) {
                Event effect;
                if (!loadEvent(db, id, effect))
                    return false;
                for (const auto &ref : effect.refs)
                    if (ref.first >= Displayed && !applyEffect(db, effect, ref))
                        return false;
            }
        } else {
            for (const auto &ref : event.refs)
                if (ref.first >= Displayed && !applyEffect(db, event, ref))
                    return false;
        }
        return true;
    }
}

bool recordMessageStates(QSqlDatabase db, const QList<qint64> &eventIds)
{
    QList<Event> events;
    QSet<qint64> rebuild;
    for (const auto id : eventIds) {
        Event event;
        bool  changed = false;
        if (!indexEvent(db, id, event, changed))
            return false;
        if (changed)
            rebuild.insert(event.contact);
        events.append(event);
    }
    for (const auto &event : events)
        if (!rebuild.contains(event.contact) && !applyEvent(db, event))
            return false;
    for (const auto contact : rebuild)
        if (!rebuildContact(db, contact))
            return false;
    return true;
}

bool backfillMessageStates(QSqlDatabase db, const std::function<void(qint64)> &progress)
{
    QSet<qint64> contacts;
    qint64       after = 0;
    for (;;) {
        QSqlQuery query(db);
        query.prepare("SELECT id FROM events WHERE id>:id ORDER BY id LIMIT 500");
        query.bindValue(":id", after);
        if (!run(query))
            return false;
        QList<qint64> ids;
        while (query.next())
            ids.append(query.value(0).toLongLong());
        query.finish();
        if (ids.isEmpty())
            break;
        for (const auto id : ids) {
            Event event;
            bool  ignored = false;
            if (!indexEvent(db, id, event, ignored))
                return false;
            contacts.insert(event.contact);
        }
        after = ids.last();
        if (progress)
            progress(after);
    }
    for (const auto contact : contacts)
        if (!rebuildContact(db, contact))
            return false;
    return true;
}
}
