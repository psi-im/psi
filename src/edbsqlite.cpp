/*
 * edbsqlite.cpp
 * Copyright (C) 2011   Aleksey Andreev
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

#include <QSqlError>
#include <QSqlDriver>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "edbsqlite.h"
//#include "common.h"
#include "applicationinfo.h"
#include "psicontactlist.h"
#include "jidutil.h"
#include "historyimp.h"

#define FAKEDELAY 0

using namespace XMPP;

//----------------------------------------------------------------------------
// EDBSqLite
//----------------------------------------------------------------------------

EDBSqLite::EDBSqLite(PsiCon *psi) : EDB(psi),
    transactionsCounter(0),
    lastCommitTime(QDateTime::currentDateTime()),
    commitTimer(nullptr),
    mirror_(nullptr)
{
    status = NotActive;
    QString path = ApplicationInfo::historyDir() + "/history.db";
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "history");
    db.setDatabaseName(path);
    if (!db.open()) {
        qWarning("%s\n%s", "EDBSqLite::EDBSqLite(): Can't open base.", qUtf8Printable(db.lastError().text()));
        return;
    }
    QSqlQuery query(db);
    query.exec("PRAGMA foreign_keys = ON;");
    setInsertingMode(Normal);
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
                setStorageParam("import_start", "yes");
            }
        }
    }
    else
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

bool EDBSqLite::init()
{
    if (status == NotActive)
        return false;

    if (!getStorageParam("import_start").isEmpty()) {
        if (!importExecute()) {
            status = NotActive;
            return false;
        }
    }

    setMirror(new EDBFlatFile(psi()));
    return true;
}

int EDBSqLite::features() const
{
    return SeparateAccounts | PrivateContacts | AllContacts | AllAccounts;
}

int EDBSqLite::get(const QString &accId, const XMPP::Jid &jid, QDateTime date, int direction, int start, int len)
{
    item_query_req *r = new item_query_req;
    r->accId = accId;
    r->j     = jid;
    r->type  = item_query_req::Type_get;
    r->start = start;
    r->len   = len < 1 ? 1 : len;
    r->dir   = direction;
    r->date  = date;
    r->id    = genUniqueId();
    rlist.append(r);

    QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));
    return r->id;
}

int EDBSqLite::find(const QString &accId, const QString &str, const XMPP::Jid &jid, const QDateTime date, int direction)
{
    item_query_req *r = new item_query_req;
    r->accId   = accId;
    r->j       = jid;
    r->type    = item_query_req::Type_find;
    r->len     = 1;
    r->dir     = direction;
    r->findStr = str;
    r->date    = date;
    r->id      = genUniqueId();
    rlist.append(r);

    QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));
    return r->id;
}

int EDBSqLite::append(const QString &accId, const XMPP::Jid &jid, const PsiEvent::Ptr &e, int type)
{
    item_query_req *r = new item_query_req;
    r->accId   = accId;
    r->j       = jid;
    r->jidType = type;
    r->type    = item_query_req::Type_append;
    r->event   = e;
    if ( !r->event ) {
        qWarning("EDBSqLite::append(): Attempted to append incompatible type.");
        delete r;
        return 0;
    }
    r->id      = genUniqueId();
    rlist.append(r);

    QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));

    if (mirror_)
        mirror_->append(accId, jid, e, type);

    return r->id;
}

int EDBSqLite::erase(const QString &accId, const XMPP::Jid &jid)
{
    item_query_req *r = new item_query_req;
    r->accId = accId;
    r->j     = jid;
    r->type  = item_query_req::Type_erase;
    r->id    = genUniqueId();
    rlist.append(r);

    QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));

    if (mirror_)
        mirror_->erase(accId, jid);

    return r->id;
}

QList<EDB::ContactItem> EDBSqLite::contacts(const QString &accId, int type)
{
    QList<ContactItem> res;
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
    quint64 res = 0;
    bool fAccAll  = accId.isEmpty();
    bool fContAll = jid.isEmpty();
    EDBSqLite::PreparedQuery *query = queryes.getPreparedQuery(QueryRowCount, fAccAll, fContAll);
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
    }
    else {
        query.prepare("SELECT COUNT(*) AS `count` FROM `system` WHERE `key` = :key;");
        query.bindValue(":key", key);
        if (query.exec() && query.next() && query.record().value("count").toULongLong() != 0) {
            query.prepare("UPDATE `system` SET `value` = :val WHERE `key` = :key;");
            query.bindValue(":key", key);
            query.bindValue(":val", val);
            query.exec();
        }
        else {
            query.prepare("INSERT INTO `system` (`key`, `value`) VALUES (:key, :val);");
            query.bindValue(":key", key);
            query.bindValue(":val", val);
            query.exec();
        }
    }
    commit();
}

void EDBSqLite::setInsertingMode(InsertMode mode)
{
    // in the case of a flow of new records
    if (mode == Import) {
        // Commit after 10000 inserts and every 5 seconds
        maxUncommitedRecs = 10000;
        maxUncommitedSecs = 5;
    } else {
        // Commit after 3 inserts and every 1 second
        maxUncommitedRecs = 3;
        maxUncommitedSecs = 1;
    }
    // Commit if there were no new additions for 1 second
    commitByTimeoutSecs = 1;
    //--
    commit();
}

void EDBSqLite::setMirror(EDBFlatFile *mirr)
{
    if (mirr != mirror_) {
        if (mirror_)
            delete mirror_;
        mirror_ = mirr;
    }
}

EDBFlatFile *EDBSqLite::mirror() const
{
    return mirror_;
}

void EDBSqLite::performRequests()
{
    if (rlist.isEmpty())
        return;

    item_query_req *r = rlist.takeFirst();
    const int type = r->type;

    if (type == item_query_req::Type_append) {
        bool b = appendEvent(r->accId, r->j, r->event, r->jidType);
        writeFinished(r->id, b);
    }

    else if (type == item_query_req::Type_get) {
        commit();
        bool fContAll = r->j.isEmpty();
        bool fAccAll  = r->accId.isEmpty();
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
            query->bindValue(":date", r->date);
        query->bindValue(":start", r->start);
        query->bindValue(":cnt", r->len);
        EDBResult result;
        if (query->exec()) {
            while (query->next()) {
                PsiEvent::Ptr e(getEvent(query->record()));
                if (e) {
                    QString id = query->record().value("id").toString();
                    result.append(EDBItemPtr(new EDBItem(e, id)));
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

    } else if(type == item_query_req::Type_find) {
        commit();
        bool fContAll = r->j.isEmpty();
        bool fAccAll  = r->accId.isEmpty();
        EDBSqLite::PreparedQuery *query = queryes.getPreparedQuery(QueryFindText, fAccAll, fContAll);
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
                    QString id = rec.value("id").toString();
                    EDBItemPtr eip = EDBItemPtr(new EDBItem(e, id));
                    result.append(eip);
                }
            }
            query->freeResult();
        }
        resultReady(r->id, result, 0);

    } else if(type == item_query_req::Type_erase) {
        writeFinished(r->id, eraseHistory(r->accId, r->j));
    }

    delete r;
}

bool EDBSqLite::appendEvent(const QString &accId, const XMPP::Jid &jid, const PsiEvent::Ptr &e, int jidType)
{
    QSqlDatabase db = QSqlDatabase::database("history");
    const qint64 contactId = ensureJidRowId(accId, jid, jidType);
    if (contactId == 0)
        return false;

    QDateTime dTime;
    int nType = 0;

    if (e->type() == PsiEvent::Message) {
        MessageEvent::Ptr me = e.staticCast<MessageEvent>();
        const Message &m = me->message().displayMessage();
        dTime = m.timeStamp();
        if (m.type() == "chat")
            nType = 1;
        else if(m.type() == "error")
            nType = 4;
        else if(m.type() == "headline")
            nType = 5;

    } else if (e->type() == PsiEvent::Auth) {
        AuthEvent::Ptr ae = e.staticCast<AuthEvent>();
        dTime = ae->timeStamp();
        QString subType = ae->authType();
        if(subType == "subscribe")
            nType = 3;
        else if(subType == "subscribed")
            nType = 6;
        else if(subType == "unsubscribe")
            nType = 7;
        else if(subType == "unsubscribed")
            nType = 8;
    } else
        return false;

    int nDirection = e->originLocal() ? 1 : 2;
    if (!transaction(false))
        return false;

    PreparedQuery *query = queryes.getPreparedQuery(QueryInsertEvent, false, false);
    query->bindValue(":contact_id", contactId);
    query->bindValue(":resource", (jidType != GroupChatContact) ? jid.resource() : "");
    query->bindValue(":date", dTime);
    query->bindValue(":type", nType);
    query->bindValue(":direction", nDirection);
    if (nType == 0 || nType == 1 || nType == 4 || nType == 5) {
        MessageEvent::Ptr me = e.staticCast<MessageEvent>();
        const Message &m = me->message().displayMessage();
        QString lang = m.lang();
        query->bindValue(":subject", m.subject(lang));
        query->bindValue(":m_text", m.body(lang));
        query->bindValue(":lang", lang);
        QString extraData;
        const UrlList &urls = m.urlList();
        if (!urls.isEmpty()) {
            QVariantMap xepList;
            QVariantList urlList;
            foreach (const Url &url, urls)
                if (!url.url().isEmpty()) {
                    QVariantList urlItem;
                    urlItem.append(QVariant(url.url()));
                    if (!url.desc().isEmpty())
                        urlItem.append(QVariant(url.desc()));
                    urlList.append(QVariant(urlItem));
                }
            xepList["jabber:x:oob"] = QVariant(urlList);
            QJsonDocument doc(QJsonObject::fromVariantMap(xepList));
            extraData = QString::fromUtf8(doc.toBinaryData());
        }
        query->bindValue(":extra_data", extraData);
    }
    else {
        query->bindValue(":subject", QVariant(QVariant::String));
        query->bindValue(":m_text", QVariant(QVariant::String));
        query->bindValue(":lang", QVariant(QVariant::String));
        query->bindValue(":extra_data", QVariant(QVariant::String));
    }
    bool res = query->exec();
    return res;
}

PsiEvent::Ptr EDBSqLite::getEvent(const QSqlRecord &record)
{
    PsiAccount *pa = psi()->contactList()->getAccount(record.value("acc_id").toString());

    int type = record.value("type").toInt();

    if(type == 0 || type == 1 || type == 4 || type == 5) {
        Message m;
        m.setTimeStamp(record.value("date").toDateTime());
        if(type == 1)
            m.setType("chat");
        else if(type == 4)
            m.setType("error");
        else if(type == 5)
            m.setType("headline");
        else
            m.setType("");
        m.setFrom(Jid(record.value("jid").toString()));
        QVariant text = record.value("m_text");
        if (!text.isNull()) {
            m.setBody(text.toString());
            m.setLang(record.value("lang").toString());
            m.setSubject(record.value("subject").toString());
        }
        m.setSpooled(true);
        QString extraStr = record.value("extra_data").toString();
        if (!extraStr.isEmpty()) {
            bool fOk;
            QJsonDocument doc = QJsonDocument::fromJson(extraStr.toUtf8());
            fOk = !doc.isNull();
            QVariantMap extraData = doc.object().toVariantMap();

            if (fOk) {
                foreach (const QVariant &urlItem, extraData["jabber:x:oob"].toList()) {
                    QVariantList itemList = urlItem.toList();
                    if (!itemList.isEmpty()) {
                        QString url = itemList.at(0).toString();
                        QString desc;
                        if (itemList.size() > 1)
                            desc = itemList.at(1).toString();
                        m.urlAdd(Url(url, desc));
                    }
                }
            }
        }
        MessageEvent::Ptr me(new MessageEvent(m, pa));
        me->setOriginLocal((record.value("direction").toInt() == 1));
        return me.staticCast<PsiEvent>();
    }

    if(type == 2 || type == 3 || type == 6 || type == 7 || type == 8) {
        QString subType = "subscribe";
        // if(type == 2) { // Not used (stupid "system message" from Psi <= 0.8.6)
        if(type == 3)
            subType = "subscribe";
        else if(type == 6)
            subType = "subscribed";
        else if(type == 7)
            subType = "unsubscribe";
        else if(type == 8)
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
    qint64 id = jidsCache.value(sKey, 0);
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
                id  = queryIns.lastInsertId().toLongLong();
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
    bool fAccAll  = accId.isEmpty();
    bool fContAll = jid.isEmpty();
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
        query->bindValue(":date", before);
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
        //if (query.exec("DELETE FROM `events`;"))
            if (query.exec("DELETE FROM `contacts`;")) {
                jidsCache.clear();
                res = true;
            }
    }
    else {
        PreparedQuery *query = queryes.getPreparedQuery(QueryJidRowId, false, false);
        query->bindValue(":jid", jid.full());
        query->bindValue(":acc_id", accId);
        if (query->exec()) {
            if (query->next()) {
                const qint64 id = query->record().value("id").toLongLong();
                QSqlQuery query2(QSqlDatabase::database("history"));
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
            lastCommitTime = QDateTime::currentDateTime();
            status = Commited;
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
        lastCommitTime = QDateTime::currentDateTime();
        status = Commited;
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
        commitTimer->setInterval(commitByTimeoutSecs * 1000);
    }
    commitTimer->start();
}

void EDBSqLite::stopAutocommitTimer()
{
    if (commitTimer && commitTimer->isActive())
        commitTimer->stop();
}

bool EDBSqLite::importExecute()
{
    bool res = true;
    HistoryImport *imp = new HistoryImport(psi());
    if (imp->isNeeded()) {
        if (imp->exec() != HistoryImport::ResultNormal) {
            res = false;
        }
    }
    delete imp;
    return res;
}

// ****************** class PreparedQueryes ********************

EDBSqLite::QueryStorage::QueryStorage()
{
}

EDBSqLite::QueryStorage::~QueryStorage()
{
    foreach (EDBSqLite::PreparedQuery *q, queryList.values()) {
        if (q)
            delete q;
    }
}

EDBSqLite::PreparedQuery *EDBSqLite::QueryStorage::getPreparedQuery(QueryType type, bool allAccounts, bool allContacts)
{
    QueryProperty queryProp(type, allAccounts, allContacts);
    EDBSqLite::PreparedQuery *q = queryList.value(queryProp, nullptr);
    if (q != nullptr)
        return q;

    q = new EDBSqLite::PreparedQuery(QSqlDatabase::database("history"));
    q->setForwardOnly(true);
    q->prepare(getQueryString(type, allAccounts, allContacts));
    queryList[queryProp] = q;
    return q;
}

EDBSqLite::PreparedQuery::PreparedQuery(QSqlDatabase db) : QSqlQuery(db)
{
}

QString EDBSqLite::QueryStorage::getQueryString(QueryType type, bool allAccounts, bool allContacts)
{
    QString queryStr;
    switch (type)
    {
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
            queryStr = "SELECT `acc_id`, `events`.`id`, `jid`, `date`, `events`.`type`, `direction`, `subject`, `m_text`, `lang`, `extra_data`"
                " FROM `events`, `contacts`"
                " WHERE `contacts`.`id` = `contact_id`";
            if (!allContacts)
                queryStr.append(" AND `jid` = :jid");
            if (!allAccounts)
                queryStr.append(" AND `acc_id` = :acc_id");
            if (type == QueryDateBackward)
                queryStr.append(" AND `date` < :date");
            else if (type == QueryDateForward)
                queryStr.append(" AND `date` >= :date");
            if (type == QueryLatest || type == QueryDateBackward)
                queryStr.append(" ORDER BY `date` DESC");
            else
                queryStr.append(" ORDER BY `date` ASC");
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
                queryStr.append(" AND `date` < :date");
            queryStr.append(";");
            break;
        case QueryJidRowId:
            queryStr = "SELECT `id` FROM `contacts` WHERE `jid` = :jid AND acc_id = :acc_id;";
            break;
        case QueryFindText:
            queryStr = "SELECT `acc_id`, `events`.`id`, `jid`, `date`, `events`.`type`, `direction`, `subject`, `m_text`, `lang`, `extra_data`"
                " FROM `events`, `contacts`"
                " WHERE `contacts`.`id` = `contact_id`";
            if (!allContacts)
                queryStr.append(" AND `jid` = :jid");
            if (!allAccounts)
                queryStr.append(" AND `acc_id` = :acc_id");
            queryStr.append(" AND `m_text` IS NOT NULL");
            queryStr.append(" ORDER BY `date`;");
            break;
        case QueryInsertEvent:
            queryStr = "INSERT INTO `events` ("
                "`contact_id`, `resource`, `date`, `type`, `direction`, `subject`, `m_text`, `lang`, `extra_data`"
                ") VALUES ("
                ":contact_id, :resource, :date, :type, :direction, :subject, :m_text, :lang, :extra_data"
                ");";
            break;
    }
    return queryStr;
}

uint qHash(const QueryProperty &struc)
{
    uint res = struc.type;
    res <<= 8;
    res |=  struc.allAccounts ? 1 : 0;
    res <<= 8;
    res |=  struc.allContacts ? 1 : 0;
    return res;
}
