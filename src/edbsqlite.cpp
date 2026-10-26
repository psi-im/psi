/*
 * edbsqlite.cpp
 * Copyright (C) 2011  Aleksey Andreev
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "applicationinfo.h"
// #include "common.h"
#include "edbsqlite.h"
#include "filesharingmanager.h"
#include "historyupgrade.h"
#include "historymessageextras.h"
#include "iris/xmpp_forwarding.h"
#include "jidutil.h"
#include "psiaccount.h"
#include "psicontactlist.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDriver>
#include <QSqlError>
#include <QXmlStreamReader>

#define FAKEDELAY 0

using namespace XMPP;

namespace {
bool decodedRecord(const QSqlRecord &record)
{
    const auto extra = QJsonDocument::fromJson(record.value("extra_data").toString().toUtf8()).object();
    if (extra.contains("psi:decoded"))
        return extra.value("psi:decoded").toBool();
    // Legacy unprocessed encrypted MAM rows must be decoded, not mistaken for
    // an already processed empty user message.
    QXmlStreamReader reader(record.value("archive_xml").toString());
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QLatin1String("encrypted"))
            return false;
    }
    return true;
}
bool protocolOnlyRecord(const QSqlRecord &record)
{
    const auto extra = QJsonDocument::fromJson(record.value("extra_data").toString().toUtf8()).object();
    if (extra.contains("psi:protocol-only"))
        return extra.value("psi:protocol-only").toBool();
    if (!record.value("m_text").toString().isEmpty())
        return false;
    QDomDocument doc;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const bool parsed = bool(
        doc.setContent(record.value("archive_xml").toString(), QDomDocument::ParseOption::UseNamespaceProcessing));
#else
    const bool parsed = doc.setContent(record.value("archive_xml").toString(), true);
#endif
    if (!parsed)
        return false;
    const auto message = doc.documentElement().firstChildElement("forwarded").firstChildElement("message");
    return !message.isNull() && message.firstChildElement("body").isNull()
        && message.firstChildElement("subject").isNull() && message.firstChildElement("html").isNull()
        && message.firstChildElement("reference").isNull() && message.firstChildElement("x").isNull();
}
}

//----------------------------------------------------------------------------
// EDBSqLite
//----------------------------------------------------------------------------

EDBSqLite::EDBSqLite(PsiCon *psi) :
    EDB(psi), transactionsCounter(0), lastCommitTime(QDateTime::currentDateTime()), commitTimer(nullptr)
{
    status            = NotActive;
    QString      path = ApplicationInfo::historyDir() + "/history.db";
    QSqlDatabase db   = QSqlDatabase::addDatabase("QSQLITE", "history");
    db.setDatabaseName(path);
    if (!db.open()) {
        qWarning("%s\n%s", "EDBSqLite::EDBSqLite(): Can't open base.", qUtf8Printable(db.lastError().text()));
        return;
    }
    QSqlQuery query(db);
    query.exec("PRAGMA foreign_keys = ON;");
    maxUncommitedRecs = 3;
    maxUncommitedSecs = 1;
    commitByTimeoutSecs = 1;
    if (db.tables(QSql::Tables).size() == 0) {
        // no tables found.
        if (db.transaction()) {
            query.exec("CREATE TABLE `system` ("
                       "`key` TEXT, "
                       "`value` TEXT"
                       ");");
            query.exec("CREATE TABLE `accounts` ("
                       "`id` TEXT, "
                       "`lifetime` INTEGER"
                       ");");
            query.exec("CREATE TABLE `contacts` ("
                       "`id` INTEGER NOT NULL PRIMARY KEY ASC, "
                       "`acc_id` TEXT, "
                       "`type` INTEGER, "
                       "`jid` TEXT, "
                       "`lifetime` INTEGER"
                       ");");
            query.exec("CREATE TABLE `events` ("
                       "`id` INTEGER NOT NULL PRIMARY KEY ASC, "
                       "`contact_id` INTEGER NOT NULL REFERENCES `contacts`(`id`) ON DELETE CASCADE, "
                       "`resource` TEXT, "
                       "`date` TEXT, "
                       "`type` INTEGER, "
                       "`direction` INTEGER, "
                       "`subject` TEXT, "
                       "`m_text` TEXT, "
                       "`lang` TEXT, "
                       "`extra_data` TEXT"
                       ");");
            query.exec("CREATE INDEX `key` ON `system` (`key`);");
            query.exec("CREATE INDEX `jid` ON `contacts` (`jid`);");
            query.exec("CREATE INDEX `contact_id` ON `events` (`contact_id`);");
            query.exec("CREATE INDEX `date` ON `events` (`date`);");
            if (db.commit()) {
                status = Commited;
                setStorageParam("version", "0.1");
            }
        }
    } else
        status = Commited;

}

EDBSqLite::~EDBSqLite()
{
    commit();
    {
        QSqlDatabase db = QSqlDatabase::database("history", false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase("history");
}

EDBResult EDBSqLite::localHistoryPage(const QString &account, const Jid &peer, qint64 before, int limit)
{
    EDBResult result;
    if (!commit() || account.isEmpty() || peer.isEmpty() || limit <= 0)
        return result;
    const auto rows
        = HistoryArchive::localRecords(QSqlDatabase::database("history"), account, peer.full(), before, limit);
    for (const auto &row : rows) {
        if (auto event = getEvent(row))
            result.append(
                EDBItemPtr(new EDBItem(event, row.value("id").toString(), row.value("history_state").toInt(),
                                       row.value("archive_uid").toString(), row.value("revision_date").toDateTime(),
                                       protocolOnlyRecord(row), row.value("reaction_target").toString(),
                                       row.value("retraction_target").toString())));
    }
    return result;
}

EDBItemPtr EDBSqLite::resolvedHistoryEvent(qint64 eventId)
{
    if (eventId <= 0 || !commit())
        return {};
    const auto row = HistoryArchive::resolvedRecord(QSqlDatabase::database("history"),eventId);
    if (row.isEmpty())
        return {};
    const auto event = getEvent(row);
    return event ? EDBItemPtr(new EDBItem(event, row.value("id").toString(), row.value("history_state").toInt(),
                                          row.value("archive_uid").toString(), row.value("revision_date").toDateTime(),
                                          protocolOnlyRecord(row), row.value("reaction_target").toString(),
                                          row.value("retraction_target").toString()))
                 : EDBItemPtr();
}

EDBItemPtr EDBSqLite::archivedEvent(const HistoryArchive::Key &key, const QString &uid, const QDateTime &time)
{
    if (!commit())
        return {};
    QSqlQuery q(QSqlDatabase::database("history"));
    q.prepare("SELECT e.*,c.acc_id,c.jid,i.xml AS archive_xml FROM archive_items i JOIN events e ON e.id=i.event_id "
              "JOIN contacts c ON c.id=e.contact_id WHERE i.account=:account AND i.archive=:archive "
              "AND i.peer=:peer AND i.uid=:uid");
    q.bindValue(":account", key.account);
    q.bindValue(":archive", key.archive);
    q.bindValue(":peer", key.peer);
    q.bindValue(":uid", uid);
    if (!q.exec())
        return {};
    if (q.next()) {
        auto event = getEvent(q.record());
        if (event && decodedRecord(q.record()))
            return EDBItemPtr(new EDBItem(event, q.record().value("id").toString(), 0, uid, {},
                                          QJsonDocument::fromJson(q.record().value("extra_data").toString().toUtf8())
                                              .object()
                                              .value("psi:protocol-only")
                                              .toBool()));
    }
    if (!time.isValid())
        return {};
    q.finish();
    // A live stanza may already carry the authenticated archive-assigned ID
    // before any MAM page has associated it with its existing EDB event.
    q.prepare("SELECT e.*,c.acc_id,c.jid FROM events e JOIN contacts c ON c.id=e.contact_id "
              "WHERE c.acc_id=:account AND c.jid=:peer AND e.history_date_ms BETWEEN :start AND :end");
    q.bindValue(":account", key.account);
    q.bindValue(":peer", key.peer);
    q.bindValue(":start", time.toMSecsSinceEpoch() - 300000);
    q.bindValue(":end", time.toMSecsSinceEpoch() + 300000);
    if (!q.exec())
        return {};
    EDBItemPtr match;
    while (q.next()) {
        if (!decodedRecord(q.record()))
            continue;
        const auto event = getEvent(q.record());
        if (!event || event->type() != PsiEvent::Message)
            continue;
        const auto message = event.staticCast<MessageEvent>()->message();
        if (message.stanzaId(XMPP::Jid(key.archive)).id != uid)
            continue;
        if (match)
            return {}; // Ambiguous metadata must not suppress decryption.
        match = EDBItemPtr(new EDBItem(event, q.record().value("id").toString(), 0, uid));
    }
    return match;
}

HistoryArchive::Snapshot EDBSqLite::archiveSnapshot(const HistoryArchive::Key &key)
{
    if (!commit())
        return {};
    return HistoryArchive::readSnapshot(QSqlDatabase::database("history"), key);
}
bool EDBSqLite::saveArchiveSnapshot(const HistoryArchive::Key &key, const HistoryArchive::Snapshot &snapshot)
{
    return commit() && HistoryArchive::storeSnapshot(QSqlDatabase::database("history"), key, snapshot);
}

HistoryArchive::Page EDBSqLite::archivePage(const HistoryArchive::Key &key, const QString &before, int limit)
{
    if (!commit())
        return {};
    return HistoryArchive::readPage(QSqlDatabase::database("history"), key, before, limit);
}

HistoryArchive::Page EDBSqLite::archivePageAtIndex(const HistoryArchive::Key &key, int index, int limit, int snapshot)
{
    if (!commit())
        return {};
    return HistoryArchive::readIndex(QSqlDatabase::database("history"), key, index, limit, snapshot);
}
HistoryArchive::Page EDBSqLite::archivePageAfter(const HistoryArchive::Key &key, const QString &after, int limit,
                                                 int snapshot)
{
    if (!commit())
        return {};
    return HistoryArchive::readAfter(QSqlDatabase::database("history"), key, after, limit, snapshot);
}
bool EDBSqLite::mergeArchiveIsland(const HistoryArchive::Key &key, int limit, HistoryArchive::Page &page)
{
    return mergeArchivePage(key, QString(), limit, page);
}

EDBItemPtr EDBSqLite::ownPlaintext(const HistoryArchive::Key &key, const QString &id, const QString &sender,
                                   const QDateTime &time)
{
    if (!commit())
        return {};
    const auto eventId = HistoryArchive::matchingOwnPlaintext(QSqlDatabase::database("history"), key, id, sender, time);
    if (!eventId)
        return {};
    QSqlQuery q(QSqlDatabase::database("history"));
    q.prepare("SELECT e.*,c.acc_id,c.jid FROM events e JOIN contacts c ON c.id=e.contact_id WHERE e.id=:id");
    q.bindValue(":id", eventId);
    if (!q.exec() || !q.next())
        return {};
    const auto event = getEvent(q.record());
    return event ? EDBItemPtr(new EDBItem(event, QString::number(eventId))) : EDBItemPtr();
}

bool EDBSqLite::mergeArchivePage(const HistoryArchive::Key &key, const QString &before, int limit,
                                 HistoryArchive::Page &page)
{
    if (!commit())
        return false;
    const bool ok = HistoryArchive::storePage(QSqlDatabase::database("history"), key, before, limit, page);
    jidsCache.clear(); // A failed transaction must not retain an inserted contact ID.
    return ok;
}

bool EDBSqLite::init()
{
    if (status == NotActive)
        return false;
    auto db = QSqlDatabase::database("history");
    if (HistoryArchive::needsMigration(db)) {
        QSqlQuery  count(db);
        const bool empty = count.exec("SELECT 1 FROM events LIMIT 1") && !count.next();
        count.finish();
        const bool migrated = empty ? HistoryArchive::migrate(db) : HistoryUpgrade::upgradeDatabase(db.databaseName());
        if (!migrated) {
            status = NotActive;
            return false;
        }
    }

    return true;
}

int EDBSqLite::features() const { return SeparateAccounts | PrivateContacts | AllContacts | AllAccounts | ArchiveCache; }

int EDBSqLite::get(const QString &accId, const XMPP::Jid &jid, QDateTime date, int direction, int start, int len)
{
    item_query_req *r = new item_query_req;
    r->accId          = accId;
    r->j              = jid;
    r->type           = item_query_req::Type_get;
    r->start          = start;
    r->len            = len < 1 ? 1 : len;
    r->dir            = direction;
    r->date           = date;
    r->id             = genUniqueId();
    rlist.append(r);

    QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));
    return r->id;
}

int EDBSqLite::find(const QString &accId, const QString &str, const XMPP::Jid &jid, const QDateTime date, int direction)
{
    item_query_req *r = new item_query_req;
    r->accId          = accId;
    r->j              = jid;
    r->type           = item_query_req::Type_find;
    r->len            = 1;
    r->dir            = direction;
    r->findStr        = str;
    r->date           = date;
    r->id             = genUniqueId();
    rlist.append(r);

    QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));
    return r->id;
}

int EDBSqLite::append(const QString &accId, const XMPP::Jid &jid, const PsiEvent::Ptr &e, int type)
{
    item_query_req *r = new item_query_req;
    r->accId          = accId;
    r->j              = jid;
    r->jidType        = type;
    r->type           = item_query_req::Type_append;
    r->event          = e;
    if (!r->event) {
        qWarning("EDBSqLite::append(): Attempted to append incompatible type.");
        delete r;
        return 0;
    }
    r->id = genUniqueId();
    rlist.append(r);

    QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));

    return r->id;
}

int EDBSqLite::erase(const QString &accId, const XMPP::Jid &jid)
{
    item_query_req *r = new item_query_req;
    r->accId          = accId;
    r->j              = jid;
    r->type           = item_query_req::Type_erase;
    r->id             = genUniqueId();
    rlist.append(r);

    QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));

    return r->id;
}

QList<EDB::ContactItem> EDBSqLite::contacts(const QString &accId, int type)
{
    QList<ContactItem>        res;
    EDBSqLite::PreparedQuery *query = queryes.getPreparedQuery(QueryContactsList, accId.isEmpty(), true);
    query->bindValue(":type", type);
    if (!accId.isEmpty())
        query->bindValue(":acc_id", accId);
    if (query->exec()) {
        while (query->next()) {
            const QSqlRecord &rec = query->record();
            res.append(ContactItem(rec.value("acc_id").toString(), XMPP::Jid(rec.value("jid").toString())));
        }
        query->freeResult();
    }
    return res;
}

quint64 EDBSqLite::eventsCount(const QString &accId, const XMPP::Jid &jid)
{
    quint64                   res      = 0;
    bool                      fAccAll  = accId.isEmpty();
    bool                      fContAll = jid.isEmpty();
    EDBSqLite::PreparedQuery *query    = queryes.getPreparedQuery(QueryRowCount, fAccAll, fContAll);
    if (!fAccAll)
        query->bindValue(":acc_id", accId);
    if (!fContAll)
        query->bindValue(":jid", jid.full());
    if (query->exec()) {
        if (query->next())
            res = query->record().value("count").toULongLong();
        query->freeResult();
    }
    return res;
}

QString EDBSqLite::getStorageParam(const QString &key)
{
    QSqlQuery query(QSqlDatabase::database("history"));
    query.prepare("SELECT `value` FROM `system` WHERE `key` = :key;");
    query.bindValue(":key", key);
    if (query.exec() && query.next())
        return query.record().value("value").toString();
    return QString();
}

void EDBSqLite::setStorageParam(const QString &key, const QString &val)
{
    transaction(true);
    QSqlQuery query(QSqlDatabase::database("history"));
    if (val.isEmpty()) {
        query.prepare("DELETE FROM `system` WHERE `key` = :key;");
        query.bindValue(":key", key);
        query.exec();
    } else {
        query.prepare("SELECT COUNT(*) AS `count` FROM `system` WHERE `key` = :key;");
        query.bindValue(":key", key);
        if (query.exec() && query.next() && query.record().value("count").toULongLong() != 0) {
            query.prepare("UPDATE `system` SET `value` = :val WHERE `key` = :key;");
            query.bindValue(":key", key);
            query.bindValue(":val", val);
            query.exec();
        } else {
            query.prepare("INSERT INTO `system` (`key`, `value`) VALUES (:key, :val);");
            query.bindValue(":key", key);
            query.bindValue(":val", val);
            query.exec();
        }
    }
    commit();
}

void EDBSqLite::performRequests()
{
    if (rlist.isEmpty())
        return;

    item_query_req *r    = rlist.takeFirst();
    const int       type = r->type;

    if (type == item_query_req::Type_append) {
        bool b = appendEvent(r->accId, r->j, r->event, r->jidType);
        writeFinished(r->id, b);
    }

    else if (type == item_query_req::Type_get) {
        commit();
        bool      fContAll = r->j.isEmpty();
        bool      fAccAll  = r->accId.isEmpty();
        QueryType queryType;
        if (r->date.isNull()) {
            if (r->dir == Forward)
                queryType = QueryOldest;
            else
                queryType = QueryLatest;
        } else {
            if (r->dir == Backward)
                queryType = QueryDateBackward;
            else
                queryType = QueryDateForward;
        }
        EDBSqLite::PreparedQuery *query = queryes.getPreparedQuery(queryType, fAccAll, fContAll);
        if (!fContAll)
            query->bindValue(":jid", r->j.full());
        if (!fAccAll)
            query->bindValue(":acc_id", r->accId);
        if (!r->date.isNull())
            query->bindValue(":date", r->date.toMSecsSinceEpoch());
        query->bindValue(":start", r->start);
        query->bindValue(":cnt", r->len);
        EDBResult result;
        if (query->exec()) {
            while (query->next()) {
                PsiEvent::Ptr e(getEvent(query->record()));
                if (e) {
                    QString id = query->record().value("id").toString();
                    result.append(EDBItemPtr(new EDBItem(e, id, query->record().value("history_state").toInt(),
                                                         query->record().value("archive_uid").toString(), {},
                                                         protocolOnlyRecord(query->record()))));
                }
            }
            query->freeResult();
        }
        int beginRow;
        if (r->dir == Forward && r->date.isNull()) {
            beginRow = r->start;
        } else {
            int cnt = rowCount(r->accId, r->j, r->date);
            if (r->dir == Backward) {
                beginRow = cnt - r->len + 1;
                if (beginRow < 0)
                    beginRow = 0;
            } else {
                beginRow = cnt + 1;
            }
        }
        resultReady(r->id, result, beginRow);

    } else if (type == item_query_req::Type_find) {
        commit();
        bool                      fContAll = r->j.isEmpty();
        bool                      fAccAll  = r->accId.isEmpty();
        EDBSqLite::PreparedQuery *query    = queryes.getPreparedQuery(QueryFindText, fAccAll, fContAll);
        if (!fContAll)
            query->bindValue(":jid", r->j.full());
        if (!fAccAll)
            query->bindValue(":acc_id", r->accId);
        EDBResult result;
        if (query->exec()) {
            QString str = r->findStr.toLower();
            while (query->next()) {
                const QSqlRecord rec = query->record();
                if (!rec.value("m_text").toString().toLower().contains(str, Qt::CaseSensitive))
                    continue;
                PsiEvent::Ptr e(getEvent(rec));
                if (e) {
                    QString    id  = rec.value("id").toString();
                    EDBItemPtr eip = EDBItemPtr(new EDBItem(e, id, query->record().value("history_state").toInt(),
                                                            query->record().value("archive_uid").toString()));
                    result.append(eip);
                }
            }
            query->freeResult();
        }
        resultReady(r->id, result, 0);

    } else if (type == item_query_req::Type_erase) {
        writeFinished(r->id, eraseHistory(r->accId, r->j));
    }

    delete r;
}

bool EDBSqLite::appendEvent(const QString &accId, const XMPP::Jid &jid, const PsiEvent::Ptr &e, int jidType)
{
    QSqlDatabase db        = QSqlDatabase::database("history");
    const qint64 contactId = ensureJidRowId(accId, jid, jidType);
    if (contactId == 0)
        return false;

    QDateTime dTime;
    int       nType = 0;

    if (e->type() == PsiEvent::Message) {
        MessageEvent::Ptr me = e.staticCast<MessageEvent>();
        const Message    &m  = me->message().displayMessage();
        dTime                = m.timeStamp().isValid() ? m.timeStamp() : me->timeStamp();
        if (m.type() == Message::Type::Chat)
            nType = 1;
        else if (m.type() == Message::Type::Error)
            nType = 4;
        else if (m.type() == Message::Type::Headline)
            nType = 5;
        else if (m.type() == Message::Type::Groupchat)
            nType = 9;

    } else if (e->type() == PsiEvent::Auth) {
        AuthEvent::Ptr ae = e.staticCast<AuthEvent>();
        dTime             = ae->timeStamp();
        QString subType   = ae->authType();
        if (subType == "subscribe")
            nType = 3;
        else if (subType == "subscribed")
            nType = 6;
        else if (subType == "unsubscribe")
            nType = 7;
        else if (subType == "unsubscribed")
            nType = 8;
    } else
        return false;

    int nDirection = e->originLocal() ? 1 : 2;
    if (e->type() == PsiEvent::Message) {
        const auto           &m = e.staticCast<MessageEvent>()->message().displayMessage();
        HistoryArchive::Entry delivery;
        delivery.messageId = m.id();
        delivery.sender    = m.from().full();
        delivery.timestamp = dTime;
        delivery.type      = nType;
        delivery.direction = nDirection;
        delivery.body      = m.body();
        delivery.subject   = m.subject();
        // A live delivery can be logged after its MAM page finished. Reuse
        // that archived row rather than creating a second local original.
        if (HistoryArchive::matchingDelivery(db, contactId, delivery, true))
            return true;
    }
    if (!transaction(false))
        return false;

    PreparedQuery *query = queryes.getPreparedQuery(QueryInsertEvent, false, false);
    query->bindValue(":contact_id", contactId);
    query->bindValue(":resource", (jidType != GroupChatContact) ? jid.resource() : "");
    query->bindValue(":date", dTime);
    query->bindValue(":history_date_ms", dTime.isValid() ? dTime.toMSecsSinceEpoch() : qint64(0));
    query->bindValue(":type", nType);
    query->bindValue(":direction", nDirection);
    // Authentication events are local history, never MAM message stanzas.
    query->bindValue(":history_state",
                     e->type() == PsiEvent::Auth ? HistoryArchive::LocalOnly : HistoryArchive::Unverified);
    if (nType == 0 || nType == 1 || nType == 4 || nType == 5 || nType == 9) {
        MessageEvent::Ptr me   = e.staticCast<MessageEvent>();
        const Message    &m    = me->message().displayMessage();
        QString           lang = m.lang();
        query->bindValue(":subject", m.subject(lang));
        query->bindValue(":m_text", m.body(lang));
        query->bindValue(":lang", lang);
        query->bindValue(":extra_data", HistoryMessageExtras::encode(m));
        if (auto account = psi()->contactList()->getAccount(accId))
            psi()->fileSharingManager()->rememberMessage(m, account);
    } else {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        query->bindValue(":subject", QVariant(QVariant::String));
        query->bindValue(":m_text", QVariant(QVariant::String));
        query->bindValue(":lang", QVariant(QVariant::String));
        query->bindValue(":extra_data", QVariant(QVariant::String));
#else
        query->bindValue(":subject", QVariant(QMetaType::fromType<QString>()));
        query->bindValue(":m_text", QVariant(QMetaType::fromType<QString>()));
        query->bindValue(":lang", QVariant(QMetaType::fromType<QString>()));
        query->bindValue(":extra_data", QVariant(QMetaType::fromType<QString>()));
#endif
    }
    bool res = query->exec();
    if (res && e->type() == PsiEvent::Message) {
        const Message &m = e.staticCast<MessageEvent>()->message().displayMessage();
        if (!HistoryArchive::recordRevision(db,query->lastInsertId().toLongLong(),
                                             contactId,nType,nDirection,m.from().full(),
                                             m.id(),m.replaceId())) {
            rollback();
            return false;
        }
    }
    return res;
}

PsiEvent::Ptr EDBSqLite::getEvent(const QSqlRecord &record)
{
    PsiAccount *pa = psi()->contactList()->getAccount(record.value("acc_id").toString());

    int type = record.value("type").toInt();

    if (type == 0 || type == 1 || type == 4 || type == 5 || type == 9) {
        Message m;
        m.setTimeStamp(record.value("date").toDateTime());
        if (type == 1)
            m.setType(Message::Type::Chat);
        else if (type == 4)
            m.setType(Message::Type::Error);
        else if (type == 5)
            m.setType(Message::Type::Headline);
        else if (type == 9)
            m.setType(Message::Type::Groupchat);
        else
            m.setType(Message::Type::Normal);
        m.setFrom(Jid(record.value("jid").toString()));
        QVariant text = record.value("m_text");
        if (!text.isNull()) {
            m.setBody(text.toString());
            m.setLang(record.value("lang").toString());
            m.setSubject(record.value("subject").toString());
        }
        // Older development versions stored protocol-only MAM rows without
        // their event attributes. Recover those from the retained raw envelope.
        if (pa && !record.value("archive_xml").toString().isEmpty()) {
            QDomDocument doc;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
            const bool parsed = bool(doc.setContent(record.value("archive_xml").toString(),
                                                    QDomDocument::ParseOption::UseNamespaceProcessing));
#else
            const bool parsed = doc.setContent(record.value("archive_xml").toString(), true);
#endif
            XMPP::Forwarding wrapper;
            if (parsed && wrapper.fromXml(doc.documentElement().firstChildElement("forwarded"), pa->client())) {
                const auto raw = wrapper.message();
                if (raw.messageReceipt() == ReceiptReceived || !raw.displayedMarkerId().isEmpty()
                    || !raw.reactions().targetId.isEmpty()
                    || !raw.retraction().isEmpty())
                    m = raw;
            }
        }
        m.setSpooled(true);
        HistoryMessageExtras::restore(record.value("extra_data").toString(), m);
        if (QJsonDocument::fromJson(record.value("extra_data").toString().toUtf8())
                .object()
                .value("psi:decryption-error")
                .toBool())
            m.setBody(tr("Unable to decrypt archived message"));
        MessageEvent::Ptr me(new MessageEvent(m, pa));
        me->setOriginLocal((record.value("direction").toInt() == 1));
        return me.staticCast<PsiEvent>();
    }

    if (type == 2 || type == 3 || type == 6 || type == 7 || type == 8) {
        QString subType = "subscribe";
        // if(type == 2) { // Not used (stupid "system message" from Psi <= 0.8.6)
        if (type == 3)
            subType = "subscribe";
        else if (type == 6)
            subType = "subscribed";
        else if (type == 7)
            subType = "unsubscribe";
        else if (type == 8)
            subType = "unsubscribed";

        AuthEvent::Ptr ae(new AuthEvent(Jid(record.value("jid").toString()), subType, pa));
        ae->setTimeStamp(record.value("date").toDateTime());
        return ae.staticCast<PsiEvent>();
    }
    return PsiEvent::Ptr();
}

qint64 EDBSqLite::ensureJidRowId(const QString &accId, const XMPP::Jid &jid, int type)
{
    if (jid.isEmpty())
        return 0;
    QString sJid = (type == GroupChatContact) ? jid.full() : jid.bare();
    QString sKey = accId + "|" + sJid;
    qint64  id   = jidsCache.value(sKey, 0);
    if (id != 0)
        return id;

    EDBSqLite::PreparedQuery *query = queryes.getPreparedQuery(QueryJidRowId, false, false);
    query->bindValue(":jid", sJid);
    query->bindValue(":acc_id", accId);
    if (query->exec()) {
        if (query->first()) {
            id = query->record().value("id").toLongLong();
        } else {
            //
            QSqlQuery queryIns(QSqlDatabase::database("history"));
            queryIns.prepare("INSERT INTO `contacts` (`acc_id`, `type`, `jid`, `lifetime`)"
                             " VALUES (:acc_id, :type, :jid, -1);");
            queryIns.bindValue(":acc_id", accId);
            queryIns.bindValue(":type", type);
            queryIns.bindValue(":jid", sJid);
            if (queryIns.exec()) {
                id = queryIns.lastInsertId().toLongLong();
            }
        }
        query->freeResult();
        if (id != 0)
            jidsCache[sKey] = id;
    }
    return id;
}

int EDBSqLite::rowCount(const QString &accId, const XMPP::Jid &jid, QDateTime before)
{
    bool      fAccAll  = accId.isEmpty();
    bool      fContAll = jid.isEmpty();
    QueryType type;
    if (before.isNull())
        type = QueryRowCount;
    else
        type = QueryRowCountBefore;
    PreparedQuery *query = queryes.getPreparedQuery(type, fAccAll, fContAll);
    if (!fContAll)
        query->bindValue(":jid", jid.full());
    if (!fAccAll)
        query->bindValue(":acc_id", accId);
    if (!before.isNull())
        query->bindValue(":date", before.toMSecsSinceEpoch());
    int res = 0;
    if (query->exec()) {
        if (query->next()) {
            res = query->record().value("count").toInt();
        }
        query->freeResult();
    }
    return res;
}

bool EDBSqLite::eraseHistory(const QString &accId, const XMPP::Jid &jid)
{
    bool res = false;
    if (!transaction(true))
        return false;

    if (accId.isEmpty() && jid.isEmpty()) {
        QSqlQuery query(QSqlDatabase::database("history"));
        // if (query.exec("DELETE FROM `events`;"))
        if (query.exec("DELETE FROM `contacts`;")) {
            jidsCache.clear();
            res = true;
        }
    } else {
        PreparedQuery *query = queryes.getPreparedQuery(QueryJidRowId, false, false);
        query->bindValue(":jid", jid.full());
        query->bindValue(":acc_id", accId);
        if (query->exec()) {
            if (query->next()) {
                const qint64 id = query->record().value("id").toLongLong();
                QSqlQuery    query2(QSqlDatabase::database("history"));
                query2.prepare("DELETE FROM `events` WHERE `contact_id` = :id;");
                query2.bindValue(":id", id);
                if (query2.exec()) {
                    res = true;
                    query2.prepare("DELETE FROM `contacts` WHERE `id` = :id AND `lifetime` = -1;");
                    query2.bindValue(":id", id);
                    if (query2.exec()) {
                        if (query2.numRowsAffected() > 0)
                            jidsCache.clear();
                    } else
                        res = false;
                }
            }
            query->freeResult();
        }
    }
    if (res)
        res = HistoryArchive::invalidate(QSqlDatabase::database("history"), accId, jid.full());
    if (res)
        res = commit();
    else
        rollback();
    return res;
}

bool EDBSqLite::transaction(bool now)
{
    if (status == NotActive)
        return false;
    if (now || transactionsCounter >= maxUncommitedRecs
        || lastCommitTime.secsTo(QDateTime::currentDateTime()) >= maxUncommitedSecs)
        if (!commit())
            return false;

    if (status == Commited) {
        if (!QSqlDatabase::database("history").transaction())
            return false;
        status = NotCommited;
    }
    ++transactionsCounter;

    startAutocommitTimer();

    return true;
}

bool EDBSqLite::commit()
{
    if (status != NotActive) {
        if (status == Commited || QSqlDatabase::database("history").commit()) {
            transactionsCounter = 0;
            lastCommitTime      = QDateTime::currentDateTime();
            status              = Commited;
            stopAutocommitTimer();
            return true;
        }
    }
    return false;
}

bool EDBSqLite::rollback()
{
    if (status == NotCommited && QSqlDatabase::database("history").rollback()) {
        transactionsCounter = 0;
        lastCommitTime      = QDateTime::currentDateTime();
        status              = Commited;
        stopAutocommitTimer();
        return true;
    }
    return false;
}

void EDBSqLite::startAutocommitTimer()
{
    if (!commitTimer) {
        commitTimer = new QTimer(this);
        connect(commitTimer, SIGNAL(timeout()), this, SLOT(commit()));
        commitTimer->setSingleShot(true);
        commitTimer->setInterval(int(commitByTimeoutSecs) * 1000);
    }
    commitTimer->start();
}

void EDBSqLite::stopAutocommitTimer()
{
    if (commitTimer && commitTimer->isActive())
        commitTimer->stop();
}

// ****************** class PreparedQueryes ********************

EDBSqLite::QueryStorage::QueryStorage() { }

EDBSqLite::QueryStorage::~QueryStorage()
{
    const auto &qList = queryList.values();
    for (EDBSqLite::PreparedQuery *q : qList) {
        if (q)
            delete q;
    }
}

EDBSqLite::PreparedQuery *EDBSqLite::QueryStorage::getPreparedQuery(QueryType type, bool allAccounts, bool allContacts)
{
    QueryProperty             queryProp(type, allAccounts, allContacts);
    EDBSqLite::PreparedQuery *q = queryList.value(queryProp, nullptr);
    if (q != nullptr)
        return q;

    q = new EDBSqLite::PreparedQuery(QSqlDatabase::database("history"));
    q->setForwardOnly(true);
    q->prepare(getQueryString(type, allAccounts, allContacts));
    queryList[queryProp] = q;
    return q;
}

EDBSqLite::PreparedQuery::PreparedQuery(QSqlDatabase db) : QSqlQuery(db) { }

QString EDBSqLite::QueryStorage::getQueryString(QueryType type, bool allAccounts, bool allContacts)
{
    QString queryStr;
    switch (type) {
    case QueryContactsList:
        queryStr = "SELECT `acc_id`, `jid` FROM `contacts` WHERE `type` = :type";
        if (!allAccounts)
            queryStr.append(" AND `acc_id` = :acc_id");
        queryStr.append(" ORDER BY `jid`;");
        break;
    case QueryLatest:
    case QueryOldest:
    case QueryDateBackward:
    case QueryDateForward:
        queryStr = "SELECT `acc_id`, `events`.`id`, `jid`, `date`, `events`.`type`, `direction`, `subject`, `m_text`, "
                   "`lang`, `extra_data`, `history_state`, "
                   "(SELECT uid FROM archive_items WHERE event_id=events.id LIMIT 1) AS archive_uid, "
                   "(SELECT xml FROM archive_items WHERE event_id=events.id LIMIT 1) AS archive_xml"
                   " FROM `events`, `contacts`"
                   " WHERE `contacts`.`id` = `contact_id`";
        if (!allContacts)
            queryStr.append(" AND `jid` = :jid");
        if (!allAccounts)
            queryStr.append(" AND `acc_id` = :acc_id");
        if (type == QueryDateBackward)
            queryStr.append(" AND `history_date_ms` < :date");
        else if (type == QueryDateForward)
            queryStr.append(" AND `history_date_ms` >= :date");
        if (type == QueryLatest || type == QueryDateBackward)
            queryStr.append(" ORDER BY `history_date_ms` DESC, `events`.`id` DESC");
        else
            queryStr.append(" ORDER BY `history_date_ms` ASC, `events`.`id` ASC");
        queryStr.append(" LIMIT :start, :cnt;");
        break;
    case QueryRowCount:
    case QueryRowCountBefore:
        queryStr = "SELECT count(*) AS `count`"
                   " FROM `events`, `contacts`"
                   " WHERE `contacts`.`id` = `contact_id`";
        if (!allContacts)
            queryStr.append(" AND `jid` = :jid");
        if (!allAccounts)
            queryStr.append(" AND `acc_id` = :acc_id");
        if (type == QueryRowCountBefore)
            queryStr.append(" AND `history_date_ms` < :date");
        queryStr.append(";");
        break;
    case QueryJidRowId:
        queryStr = "SELECT `id` FROM `contacts` WHERE `jid` = :jid AND acc_id = :acc_id;";
        break;
    case QueryFindText:
        queryStr = "SELECT `acc_id`, `events`.`id`, `jid`, `date`, `events`.`type`, `direction`, `subject`, `m_text`, "
                   "`lang`, `extra_data`, `history_state`, "
                   "(SELECT uid FROM archive_items WHERE event_id=events.id LIMIT 1) AS archive_uid, "
                   "(SELECT xml FROM archive_items WHERE event_id=events.id LIMIT 1) AS archive_xml"
                   " FROM `events`, `contacts`"
                   " WHERE `contacts`.`id` = `contact_id`";
        if (!allContacts)
            queryStr.append(" AND `jid` = :jid");
        if (!allAccounts)
            queryStr.append(" AND `acc_id` = :acc_id");
        queryStr.append(" AND `m_text` IS NOT NULL");
        queryStr.append(" ORDER BY `history_date_ms`, `events`.`id`;");
        break;
    case QueryInsertEvent:
        queryStr = "INSERT INTO `events` ("
                   "`contact_id`, `resource`, `date`, `type`, `direction`, `subject`, `m_text`, `lang`, `extra_data`, "
                   "`history_state`, `history_date_ms`"
                   ") VALUES ("
                   ":contact_id, :resource, :date, :type, :direction, :subject, :m_text, :lang, :extra_data, "
                   ":history_state, :history_date_ms"
                   ");";
        break;
    }
    return queryStr;
}

uint qHash(const QueryProperty &struc)
{
    uint res = struc.type;
    res <<= 8;
    res |= struc.allAccounts ? 1 : 0;
    res <<= 8;
    res |= struc.allContacts ? 1 : 0;
    return res;
}
