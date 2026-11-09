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
#include "historyeventfactory.h"
#include "historysqlitepolicy.h"
#include "historywalcheckpoint.h"
#include "historysqlreadworker.h"
#include <QThread>
#include "historymessageextras.h"
#include "iris/xmpp_forwarding.h"
#include "jidutil.h"
#include "psiaccount.h"
#include "psicontactlist.h"

#include <QElapsedTimer>
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
    walEnabled_ = HistorySqlitePolicy::configure(db);
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
    // All pending reads finish or are discarded before the database is
    // closed. The worker owns a distinct, read-only SQLite connection.
    if (historyReadThread_) {
        if (historyReadThread_->isRunning()) {
            emit historyReadStopRequested();
            historyReadThread_->wait();
        } else {
            delete historyReader_;
        }
        historyReader_ = nullptr;
    }
    qDeleteAll(pendingReads_);
    pendingReads_.clear();
    qDeleteAll(pendingWrites_);
    pendingWrites_.clear();
    pendingLocalPages_.clear();
    pendingDisplayedCursors_.clear();
    pendingArchiveReads_.clear();
    pendingArchiveWrites_.clear();
    pendingResolved_.clear();
    qDeleteAll(rlist);
    rlist.clear();
    if (checkpointTimer_)
        checkpointTimer_->stop();
    // A final WAL sync may take time on a slow disk, but only the background
    // worker executes it. Do not close its separate connection on this thread.
    delete checkpointWorker_;
    checkpointWorker_ = nullptr;
    {
        QSqlDatabase db = QSqlDatabase::database("history", false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase("history");
}

EDBResult EDBSqLite::decodeLocalRecords(const QList<QSqlRecord> &rows)
{
    EDBResult result;
    for (const auto &row : rows) {
        if (auto event = HistoryEventFactory::decode(psi(), row))
            result.append(
                EDBItemPtr(new EDBItem(event, row.value("id").toString(), row.value("history_state").toInt(),
                                       row.value("archive_uid").toString(), row.value("revision_date").toDateTime(),
                                       protocolOnlyRecord(row), row.value("reaction_target").toString(),
                                       row.value("retraction_target").toString(),
                                       MessageState::Flags(row.value("message_flags").toInt()))));
    }
    return result;
}

EDBResult EDBSqLite::localHistoryPage(const QString &account, const Jid &peer, qint64 before, int limit)
{
    if (!commit() || account.isEmpty() || peer.isEmpty() || limit <= 0)
        return {};
    return decodeLocalRecords(
        HistoryArchive::localRecords(QSqlDatabase::database("history"), account, peer.full(), before, limit));
}

void EDBSqLite::requestLocalHistoryPage(const QString &account, const Jid &peer, qint64 before, int limit,
                                      QObject *receiver, std::function<void(EDBResult, bool)> finished)
{
    if (!receiver)
        return;
    if (!historyReadThread_) {
        EDB::requestLocalHistoryPage(account, peer, before, limit, receiver, std::move(finished));
        return;
    }
    auto *r = new item_query_req;
    r->type = item_query_req::Type_local;
    r->accId = account;
    r->j = peer;
    r->localBefore = before;
    r->len = limit;
    r->id = genUniqueId();
    pendingLocalPages_.insert(r->id, { receiver, std::move(finished) });
    rlist.append(r);
    QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));
}

EDBItemPtr EDBSqLite::resolvedHistoryEvent(qint64 eventId)
{
    if (eventId <= 0 || !commit())
        return {};
    const auto row = HistoryArchive::resolvedRecord(QSqlDatabase::database("history"),eventId);
    if (row.isEmpty())
        return {};
    const auto event = HistoryEventFactory::decode(psi(), row);
    return event ? EDBItemPtr(new EDBItem(event, row.value("id").toString(), row.value("history_state").toInt(),
                                          row.value("archive_uid").toString(), row.value("revision_date").toDateTime(),
                                          protocolOnlyRecord(row), row.value("reaction_target").toString(),
                                          row.value("retraction_target").toString(),
                                          MessageState::Flags(row.value("message_flags").toInt())))
                 : EDBItemPtr();
}

void EDBSqLite::requestResolvedHistoryEvents(
    const QList<qint64> &ids, QObject *receiver,
    std::function<void(QHash<qint64, EDBItemPtr>, bool)> finished)
{
    if (!receiver)
        return;
    if (!historyReadThread_) {
        EDB::requestResolvedHistoryEvents(ids, receiver, std::move(finished));
        return;
    }
    auto *r = new item_query_req;
    r->id = genUniqueId();
    r->type = item_query_req::Type_resolved;
    r->eventIds = ids;
    pendingResolved_.insert(r->id, { receiver, std::move(finished) });
    rlist.append(r);
    QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));
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
        auto event = HistoryEventFactory::decode(psi(), q.record());
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
        const auto event = HistoryEventFactory::decode(psi(), q.record());
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

HistoryArchive::DisplayedCursor EDBSqLite::sentDisplayedCursor(const QString &account, const XMPP::Jid &peer)
{
    return commit() ? HistoryArchive::sentDisplayedCursor(QSqlDatabase::database("history"), account, peer.full())
                    : HistoryArchive::DisplayedCursor {};
}

void EDBSqLite::requestSentDisplayedCursor(
    const QString &account, const XMPP::Jid &peer, QObject *receiver,
    std::function<void(HistoryArchive::DisplayedCursor, bool)> finished)
{
    if (!receiver)
        return;
    if (!historyReadThread_) {
        EDB::requestSentDisplayedCursor(account, peer, receiver, std::move(finished));
        return;
    }
    auto *r = new item_query_req;
    r->type = item_query_req::Type_displayed;
    r->accId = account;
    r->j = peer;
    r->id = genUniqueId();
    pendingDisplayedCursors_.insert(r->id, { receiver, std::move(finished) });
    rlist.append(r);
    QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));
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
    const auto event = HistoryEventFactory::decode(psi(), q.record());
    return event ? EDBItemPtr(new EDBItem(event, QString::number(eventId))) : EDBItemPtr();
}

bool EDBSqLite::mergeArchivePage(const HistoryArchive::Key &key, const QString &before, int limit,
                                 HistoryArchive::Page &page)
{
    if (!commit())
        return false;
    const bool ok = HistoryArchive::storePage(QSqlDatabase::database("history"), key, before, limit, page);
    // All contact inserts are now owned by the worker and are transactional.
    return ok;
}

void EDBSqLite::requestArchivePage(
    ArchiveReadMode mode, const HistoryArchive::Key &key, const QString &cursor, int index, int limit,
    int snapshotCount, QObject *receiver,
    std::function<void(HistoryArchive::Page, HistoryArchive::Snapshot, bool)> finished)
{
    if (!receiver)
        return;
    if (!historyReadThread_) {
        EDB::requestArchivePage(mode, key, cursor, index, limit, snapshotCount, receiver, std::move(finished));
        return;
    }
    auto *r = new item_query_req;
    r->id = genUniqueId();
    r->type = item_query_req::Type_archiveRead;
    r->archiveKey = key;
    r->archiveCursor = cursor;
    r->archiveMode = mode;
    r->archiveIndex = index;
    r->len = limit;
    r->snapshotCount = snapshotCount;
    pendingArchiveReads_.insert(r->id, { receiver, std::move(finished) });
    rlist.append(r);
    QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));
}

void EDBSqLite::requestStoreArchivePage(
    const HistoryArchive::Key &key, const QString &before, int limit, HistoryArchive::Page page,
    QObject *receiver, std::function<void(HistoryArchive::Page, bool)> finished)
{
    if (!receiver)
        return;
    if (!historyReadThread_) {
        EDB::requestStoreArchivePage(key, before, limit, std::move(page), receiver, std::move(finished));
        return;
    }
    auto *r = new item_query_req;
    r->id = genUniqueId();
    r->type = item_query_req::Type_archiveWrite;
    r->archiveKey = key;
    r->archiveCursor = page.checkpoint ? before : QString();
    r->archivePage = std::move(page);
    r->len = limit;
    pendingArchiveWrites_.insert(r->id, { receiver, std::move(finished) });
    rlist.append(r);
    QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));
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

    if (walEnabled_ && !checkpointWorker_) {
        const QString path = db.databaseName();
        // Once the worker has opened its own SQLite connection, turn off
        // SQLite's default checkpoint-on-commit on the GUI connection.
        checkpointWorker_ = new HistoryWalCheckpoint(path);
        // A Qt queued signal guarantees that a late worker startup cannot
        // dereference an EDB facade already destroyed during shutdown.
        connect(checkpointWorker_, &HistoryWalCheckpoint::ready, this, [this] {
            auto connection = QSqlDatabase::database(QStringLiteral("history"), false);
            if (!connection.isOpen() || !HistorySqlitePolicy::useBackgroundCheckpoint(connection)) {
                qWarning() << "Cannot disable foreground WAL auto-checkpoint for history";
                return;
            }
            checkpointTimer_ = new QTimer(this);
            checkpointTimer_->setInterval(15000);
            connect(checkpointTimer_, &QTimer::timeout, this, [this] {
                if (checkpointWorker_)
                    checkpointWorker_->requestCheckpoint();
            });
            checkpointTimer_->start();
        });
        checkpointWorker_->start();
    }
    // Keep EDB and PsiEvent reconstruction in the GUI thread. SQL reads run
    // on a separate read-only connection and return plain QSqlRecord values.
    if (!historyReadThread_) {
        qRegisterMetaType<HistoryReadRequest>("HistoryReadRequest");
        qRegisterMetaType<HistoryReadResult>("HistoryReadResult");
        qRegisterMetaType<HistoryWriteRequest>("HistoryWriteRequest");
        qRegisterMetaType<HistoryWriteResult>("HistoryWriteResult");
        historyReadThread_ = new QThread(this);
        historyReader_ = new HistorySqlReadWorker(db.databaseName());
        historyReader_->moveToThread(historyReadThread_);
        connect(historyReadThread_, &QThread::started, historyReader_, &HistorySqlReadWorker::open);
        connect(this, &EDBSqLite::historyReadRequested, historyReader_,
                &HistorySqlReadWorker::read, Qt::QueuedConnection);
        connect(this, &EDBSqLite::historyWriteRequested, historyReader_,
                &HistorySqlReadWorker::write, Qt::QueuedConnection);
        connect(historyReader_, &HistorySqlReadWorker::writeCompleted, this,
                &EDBSqLite::onHistoryWriteCompleted, Qt::QueuedConnection);
        connect(historyReader_, &HistorySqlReadWorker::completed, this,
                &EDBSqLite::onHistoryReadCompleted, Qt::QueuedConnection);
        connect(this, &EDBSqLite::historyReadStopRequested, historyReader_,
                &HistorySqlReadWorker::close, Qt::QueuedConnection);
        connect(historyReader_, &HistorySqlReadWorker::stopped,
                historyReadThread_, &QThread::quit, Qt::DirectConnection);
        connect(historyReadThread_, &QThread::finished, historyReader_, &QObject::deleteLater);
        historyReadThread_->start(QThread::LowPriority);
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
    // PsiEvent and PsiAccount belong to the GUI. SQL receives only values.
    r->encoded = HistoryEventFactory::encode(r->event);
    if (!r->encoded.valid) {
        qWarning("EDBSqLite::append(): Cannot serialize this event.");
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

void EDBSqLite::onHistoryWriteCompleted(HistoryWriteResult written)
{
    if (pendingArchiveWrites_.contains(written.id)) {
        auto pending = pendingArchiveWrites_.take(written.id);
        if (pending.receiver)
            pending.finished(std::move(written.archivePage), written.success);
        return;
    }
    auto *r = pendingWrites_.take(written.id);
    if (!r)
        return;
    if (written.success && written.inserted && r->type == item_query_req::Type_append
        && r->event && r->event->type() == PsiEvent::Message) {
        if (auto *account = psi()->contactList()->getAccount(r->accId))
            psi()->fileSharingManager()->rememberMessage(
                r->event.staticCast<MessageEvent>()->message().displayMessage(), account);
    }
    writeFinished(r->id, written.success);
    delete r;
}

void EDBSqLite::onHistoryReadCompleted(HistoryReadResult read)
{
    if (pendingResolved_.contains(read.id)) {
        auto pending = pendingResolved_.take(read.id);
        if (pending.receiver) {
            QHash<qint64, EDBItemPtr> results;
            if (read.success) {
                for (auto it = read.resolvedRows.cbegin(); it != read.resolvedRows.cend(); ++it) {
                    const auto &row = it.value();
                    if (const auto event = HistoryEventFactory::decode(psi(), row)) {
                        results.insert(it.key(), EDBItemPtr(new EDBItem(
                            event, row.value("id").toString(), row.value("history_state").toInt(),
                            row.value("archive_uid").toString(), row.value("revision_date").toDateTime(),
                            protocolOnlyRecord(row), row.value("reaction_target").toString(),
                            row.value("retraction_target").toString(),
                            MessageState::Flags(row.value("message_flags").toInt()))));
                    }
                }
            }
            pending.finished(std::move(results), read.success);
        }
        return;
    }
    if (pendingArchiveReads_.contains(read.id)) {
        auto pending = pendingArchiveReads_.take(read.id);
        if (pending.receiver)
            pending.finished(std::move(read.archivePage), std::move(read.archiveSnapshot), read.success);
        return;
    }
    if (pendingDisplayedCursors_.contains(read.id)) {
        auto pending = pendingDisplayedCursors_.take(read.id);
        if (pending.receiver)
            pending.finished(read.cursor, read.success);
        return;
    }
    if (pendingLocalPages_.contains(read.id)) {
        auto pending = pendingLocalPages_.take(read.id);
        // Closing a tab invalidates its presentation request, not an already
        // completed SQL operation. Never call a destroyed receiver.
        if (pending.receiver)
            pending.finished(read.success ? decodeLocalRecords(read.rows) : EDBResult(), read.success);
        return;
    }
    auto *r = pendingReads_.take(read.id);
    if (!r)
        return; // A cancelled request must never reach a stale UI handle.
    EDBResult events;
    for (const auto &row : read.rows) {
        const auto event = HistoryEventFactory::decode(psi(), row);
        if (!event)
            continue;
        events.append(EDBItemPtr(new EDBItem(
            event, row.value("id").toString(), row.value("history_state").toInt(),
            row.value("archive_uid").toString(), {},
            protocolOnlyRecord(row), {}, {},
            MessageState::Flags(row.value("message_flags").toInt()))));
    }
    resultReady(r->id, events, read.beginRow);
    delete r;
}

void EDBSqLite::performRequests()
{
    if (rlist.isEmpty())
        return;
    auto *r = rlist.takeFirst();
    const int type = r->type;

    // Migration/init is the only reason a request could arrive without the
    // database worker. Never silently drop it or fall back to GUI SQL.
    if (!historyReadThread_) {
        if (type == item_query_req::Type_local) {
            auto pending = pendingLocalPages_.take(r->id);
            if (pending.receiver)
                pending.finished({}, false);
        } else if (type == item_query_req::Type_displayed) {
            auto pending = pendingDisplayedCursors_.take(r->id);
            if (pending.receiver)
                pending.finished({}, false);
        } else if (type == item_query_req::Type_archiveRead) {
            auto pending = pendingArchiveReads_.take(r->id);
            if (pending.receiver)
                pending.finished({}, {}, false);
        } else if (type == item_query_req::Type_resolved) {
            auto pending = pendingResolved_.take(r->id);
            if (pending.receiver)
                pending.finished({}, false);
        } else if (type == item_query_req::Type_archiveWrite) {
            auto pending = pendingArchiveWrites_.take(r->id);
            if (pending.receiver)
                pending.finished({}, false);
        } else if (type == item_query_req::Type_append || type == item_query_req::Type_erase) {
            writeFinished(r->id, false);
        } else {
            resultReady(r->id, {}, 0);
        }
        delete r;
        return;
    }

    if (type == item_query_req::Type_get || type == item_query_req::Type_find
        || type == item_query_req::Type_local || type == item_query_req::Type_displayed
        || type == item_query_req::Type_archiveRead || type == item_query_req::Type_resolved) {
        HistoryReadRequest read;
        read.id = r->id;
        read.account = r->accId;
        read.peer = r->j.full();
        read.text = r->findStr;
        read.date = r->date;
        read.direction = r->dir;
        read.start = r->start;
        read.limit = r->len;
        read.before = r->localBefore;
        read.archiveKey = r->archiveKey;
        read.cursor = r->archiveCursor;
        read.archiveIndex = r->archiveIndex;
        read.snapshotCount = r->snapshotCount;
        read.eventIds = r->eventIds;
        if (type == item_query_req::Type_local)
            read.type = HistoryReadRequest::LocalPage;
        else if (type == item_query_req::Type_displayed)
            read.type = HistoryReadRequest::DisplayedCursor;
        else if (type == item_query_req::Type_resolved)
            read.type = HistoryReadRequest::ResolveBatch;
        else if (type == item_query_req::Type_find)
            read.type = HistoryReadRequest::Find;
        else if (type == item_query_req::Type_archiveRead)
            read.type = r->archiveMode == ArchiveReadMode::After ? HistoryReadRequest::ArchiveAfter
                : r->archiveMode == ArchiveReadMode::Index ? HistoryReadRequest::ArchiveIndex
                                                            : HistoryReadRequest::ArchiveBefore;
        else
            read.type = HistoryReadRequest::Get;

        if (type == item_query_req::Type_get || type == item_query_req::Type_find)
            pendingReads_.insert(r->id, r);
        else
            delete r; // Receiver-scoped callbacks are tracked independently.
        emit historyReadRequested(read);
        return;
    }

    HistoryWriteRequest write;
    write.id = r->id;
    write.type = type == item_query_req::Type_append ? HistoryWriteRequest::Append
        : type == item_query_req::Type_archiveWrite ? HistoryWriteRequest::StorePage : HistoryWriteRequest::Erase;
    write.account = r->accId;
    write.peer = r->j.full();
    write.contactPeer = r->jidType == GroupChatContact ? r->j.full() : r->j.bare();
    write.resource = r->j.resource();
    write.jidType = r->jidType;
    write.encoded = r->encoded;
    write.archiveKey = r->archiveKey;
    write.before = r->archiveCursor;
    write.archivePage = r->archivePage;
    write.limit = r->len;
    if (type == item_query_req::Type_archiveWrite)
        delete r;
    else
        pendingWrites_.insert(r->id, r);
    emit historyWriteRequested(write);
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
        const bool pending = status == NotCommited;
        QElapsedTimer elapsed;
        if (pending)
            elapsed.start();
        const bool committed = status == Commited || QSqlDatabase::database("history").commit();
        if (pending && elapsed.elapsed() > 1000)
            qWarning() << "[history-db] GUI-thread SQLite commit took" << elapsed.elapsed() << "ms";
        if (committed) {
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
        queryStr = QStringLiteral("SELECT acc_id,jid FROM contacts WHERE type=:type");
        if (!allAccounts)
            queryStr += QStringLiteral(" AND acc_id=:acc_id");
        queryStr += QStringLiteral(" ORDER BY jid");
        break;
    case QueryRowCount:
        queryStr = QStringLiteral("SELECT count(*) AS count FROM events, contacts "
                                  "WHERE contacts.id=contact_id");
        if (!allContacts)
            queryStr += QStringLiteral(" AND jid=:jid");
        if (!allAccounts)
            queryStr += QStringLiteral(" AND acc_id=:acc_id");
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
