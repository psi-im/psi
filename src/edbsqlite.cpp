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
#include "historyeventfactory.h"
#include "historymediarefs.h"
#include "historymessageextras.h"
#include "historysqlitepolicy.h"
#include "historysqlreadworker.h"
#include "historyupgrade.h"
#include "iris/xmpp_forwarding.h"
#include "jidutil.h"
#include "messagefileshares.h"
#include "psiaccount.h"
#include "psicontactlist.h"
#include <QThread>

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDriver>
#include <QSqlError>

#define FAKEDELAY 0

using namespace XMPP;

namespace {
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

EDBSqLite::EDBSqLite(PsiCon *psi) : EDB(psi)
{

    QString      path = ApplicationInfo::historyDir() + "/history.db";
    QSqlDatabase db   = QSqlDatabase::addDatabase("QSQLITE", "history");
    db.setDatabaseName(path);
    if (!db.open()) {
        qWarning("%s\n%s", "EDBSqLite::EDBSqLite(): Can't open base.", qUtf8Printable(db.lastError().text()));
        return;
    }
    HistorySqlitePolicy::configure(db);
    QSqlQuery query(db);
    query.exec("PRAGMA foreign_keys = ON;");
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
                active_ = true;
                setStorageParam("version", "0.1");
            }
        }
    } else
        active_ = true;

}

EDBSqLite::~EDBSqLite()
{
    // Submit facade requests before the stop barrier, including writes whose
    // zero-delay dispatch has not run yet. Shutdown must flush both queues.
    if (historyReadThread_) {
        if (historyReadThread_->isRunning()) {
            while (!rlist.isEmpty())
                performRequests();
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
    pendingMessageLookups_.clear();
    qDeleteAll(rlist);
    rlist.clear();
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
    if (account.isEmpty() || peer.isEmpty() || limit <= 0)
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
        // A failed/uninitialized SQLite worker must not execute disk I/O on the GUI.
        QTimer::singleShot(0, receiver, [finished = std::move(finished)]() mutable { finished({}, false); });
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
    if (eventId <= 0)
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

void EDBSqLite::requestArchivedEvent(
    const HistoryArchive::Key &key, const QString &uid, const QDateTime &time, QObject *receiver,
    std::function<void(EDBItemPtr, bool)> finished)
{
    if (!receiver)
        return;
    if (!historyReadThread_) {
        // A failed/uninitialized SQLite worker must not execute disk I/O on the GUI.
        QTimer::singleShot(0, receiver, [finished = std::move(finished)]() mutable { finished({}, false); });
        return;
    }
    auto *r = new item_query_req;
    r->id = genUniqueId();
    r->type = item_query_req::Type_lookup;
    r->archiveKey = key;
    r->archiveCursor = uid;
    r->ownTime = time;
    pendingMessageLookups_.insert(r->id, { receiver, std::move(finished), key, uid, false });
    rlist.append(r);
    QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));
}

void EDBSqLite::requestOwnPlaintext(
    const HistoryArchive::Key &key, const QString &id, const QString &sender, const QDateTime &time,
    QObject *receiver, std::function<void(EDBItemPtr, bool)> finished)
{
    if (!receiver)
        return;
    if (!historyReadThread_) {
        // A failed/uninitialized SQLite worker must not execute disk I/O on the GUI.
        QTimer::singleShot(0, receiver, [finished = std::move(finished)]() mutable { finished({}, false); });
        return;
    }
    auto *r = new item_query_req;
    r->id = genUniqueId();
    r->type = item_query_req::Type_plaintext;
    r->archiveKey = key;
    r->ownMessageId = id;
    r->ownSender = sender;
    r->ownTime = time;
    pendingMessageLookups_.insert(r->id, { receiver, std::move(finished), key, {}, true });
    rlist.append(r);
    QTimer::singleShot(FAKEDELAY, this, SLOT(performRequests()));
}

void EDBSqLite::requestResolvedHistoryEvents(
    const QList<qint64> &ids, QObject *receiver,
    std::function<void(QHash<qint64, EDBItemPtr>, bool)> finished)
{
    if (!receiver)
        return;
    if (!historyReadThread_) {
        // A failed/uninitialized SQLite worker must not execute disk I/O on the GUI.
        QTimer::singleShot(0, receiver, [finished = std::move(finished)]() mutable { finished({}, false); });
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
        if (event && HistoryArchive::isDecodedRecord(q.record()))
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
        if (!HistoryArchive::isDecodedRecord(q.record()))
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

    return HistoryArchive::readSnapshot(QSqlDatabase::database("history"), key);
}
bool EDBSqLite::saveArchiveSnapshot(const HistoryArchive::Key &key, const HistoryArchive::Snapshot &snapshot)
{
    return HistoryArchive::storeSnapshot(QSqlDatabase::database("history"), key, snapshot);
}

HistoryArchive::Page EDBSqLite::archivePage(const HistoryArchive::Key &key, const QString &before, int limit)
{

    return HistoryArchive::readPage(QSqlDatabase::database("history"), key, before, limit);
}

HistoryArchive::Page EDBSqLite::archivePageAtIndex(const HistoryArchive::Key &key, int index, int limit, int snapshot)
{

    return HistoryArchive::readIndex(QSqlDatabase::database("history"), key, index, limit, snapshot);
}
HistoryArchive::Page EDBSqLite::archivePageAfter(const HistoryArchive::Key &key, const QString &after, int limit,
                                                 int snapshot)
{

    return HistoryArchive::readAfter(QSqlDatabase::database("history"), key, after, limit, snapshot);
}
bool EDBSqLite::mergeArchiveIsland(const HistoryArchive::Key &key, int limit, HistoryArchive::Page &page)
{
    return mergeArchivePage(key, QString(), limit, page);
}

HistoryArchive::DisplayedCursor EDBSqLite::sentDisplayedCursor(const QString &account, const XMPP::Jid &peer)
{
    return HistoryArchive::sentDisplayedCursor(QSqlDatabase::database("history"), account, peer.full());
}

void EDBSqLite::requestSentDisplayedCursor(
    const QString &account, const XMPP::Jid &peer, QObject *receiver,
    std::function<void(HistoryArchive::DisplayedCursor, bool)> finished)
{
    if (!receiver)
        return;
    if (!historyReadThread_) {
        // A failed/uninitialized SQLite worker must not execute disk I/O on the GUI.
        QTimer::singleShot(0, receiver, [finished = std::move(finished)]() mutable { finished({}, false); });
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

    const auto db = QSqlDatabase::database("history");
    bool       removed = false;
    const bool ok      = HistoryArchive::storePage(
        db, key, before, limit, page, [db, &removed](qint64 id) { return HistoryMediaRefs::link(db, id, &removed); });
    if (ok) {
        if (removed)
            emit mediaLinksRemoved();
        emit mediaLinksAdded();
    }
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
        // A failed/uninitialized SQLite worker must not execute disk I/O on the GUI.
        QTimer::singleShot(0, receiver, [finished = std::move(finished)]() mutable { finished({}, {}, false); });
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
        // A failed/uninitialized SQLite worker must not execute disk I/O on the GUI.
        QTimer::singleShot(0, receiver, [finished = std::move(finished)]() mutable { finished({}, false); });
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
    if (!active_)
        return false;
    auto db = QSqlDatabase::database("history");
    if (HistoryArchive::needsMigration(db) || HistoryMediaRefs::needsMigration(db)) {
        QSqlQuery  count(db);
        const bool empty = count.exec("SELECT 1 FROM events LIMIT 1") && !count.next();
        count.finish();
        const bool migrated = empty ? HistoryArchive::migrate(db) && HistoryMediaRefs::migrate(db)
                                    : HistoryUpgrade::upgradeDatabase(db.databaseName());
        if (!migrated) {
            active_ = false;
            return false;
        }
    }

    // Keep PsiEvent reconstruction in GUI. The worker owns SQL reads,
    // writes and MAM transactions; results cross threads as values.
    if (!historyReadThread_) {
        qRegisterMetaType<HistoryReadRequest>("HistoryReadRequest");
        qRegisterMetaType<HistoryReadResult>("HistoryReadResult");
        qRegisterMetaType<HistoryWriteRequest>("HistoryWriteRequest");
        qRegisterMetaType<HistoryWriteResult>("HistoryWriteResult");
        historyReadThread_ = new QThread(this);
        historyReader_ = new HistorySqlReadWorker(db.databaseName());
        // The callback carries no GUI objects and executes on the worker's SQLite connection.
        historyReader_->setEventStoredHook(
            [](QSqlDatabase db, qint64 id, bool *removed) { return HistoryMediaRefs::link(db, id, removed); });
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

void EDBSqLite::requestMediaLinks(qint64 position, int limit, QObject *receiver,
                                  std::function<void(QList<HistoryMediaRefs::Link>, bool)> finished)
{
    if (!receiver)
        return;
    auto *r        = new item_query_req;
    r->id          = genUniqueId();
    r->type        = item_query_req::Type_mediaLinks;
    r->localBefore = position;
    r->len         = limit;
    pendingMediaLinks_.insert(r->id, { receiver, std::move(finished) });
    rlist.append(r);
    QTimer::singleShot(0, this, SLOT(performRequests()));
}

void EDBSqLite::requestChatMediaLinks(const HistoryArchive::Key &chat, HistoryMediaRefs::Cursor cursor, bool older,
                                      int limit, QObject *receiver,
                                      std::function<void(QList<HistoryMediaRefs::Link>, bool)> finished,
                                      const QString                                           &fileDatabasePath)
{
    if (!receiver)
        return;
    auto *r             = new item_query_req;
    r->id               = genUniqueId();
    r->type             = item_query_req::Type_chatMediaLinks;
    r->archiveKey       = chat;
    r->fileDatabasePath = fileDatabasePath;
    r->mediaCursor      = cursor;
    r->dir              = older ? 1 : 0;
    r->len              = limit;
    pendingMediaLinks_.insert(r->id, { receiver, std::move(finished) });
    rlist.append(r);
    QTimer::singleShot(0, this, SLOT(performRequests()));
}

void EDBSqLite::requestChatMediaStatistics(const HistoryArchive::Key &chat, const QString &fileDatabasePath,
                                           QObject                                                *receiver,
                                           std::function<void(HistoryMediaRefs::Statistics, bool)> finished)
{
    if (!receiver)
        return;
    auto *r             = new item_query_req;
    r->id               = genUniqueId();
    r->type             = item_query_req::Type_chatMediaStatistics;
    r->archiveKey       = chat;
    r->fileDatabasePath = fileDatabasePath;
    pendingMediaStatistics_.insert(r->id, { receiver, std::move(finished) });
    rlist.append(r);
    QTimer::singleShot(0, this, SLOT(performRequests()));
}

void EDBSqLite::requestLinkedMedia(const QList<QByteArray> &ids, QObject *receiver,
                                   std::function<void(QSet<QByteArray>, bool)> finished)
{
    if (!receiver)
        return;
    auto *r           = new item_query_req;
    r->id             = genUniqueId();
    r->type           = item_query_req::Type_mediaLinked;
    r->publicationIds = ids;
    pendingMediaLinked_.insert(r->id, { receiver, std::move(finished) });
    rlist.append(r);
    QTimer::singleShot(0, this, SLOT(performRequests()));
}

void EDBSqLite::requestDeleteLocalMessage(const HistoryArchive::Key &key, qint64 id, const QString &clientId,
                                          int direction, QObject *receiver, std::function<void(bool)> finished)
{
    if (!receiver)
        return;
    auto *r                          = new item_query_req;
    r->id                            = genUniqueId();
    r->type                          = item_query_req::Type_deleteLocal;
    r->archiveKey                    = key;
    r->localBefore                   = id;
    r->suppressionIdentity.messageId = clientId;
    r->suppressionIdentity.direction = direction;
    pendingLocalDeletion_.insert(r->id, { receiver, [finished](bool, bool ok) { finished(ok); } });
    rlist.append(r);
    QTimer::singleShot(0, this, &EDBSqLite::performRequests);
}

void EDBSqLite::requestLocalSuppression(const HistoryArchive::Key &key, HistoryArchive::Entry identity,
                                        QObject *receiver, std::function<void(bool, bool)> finished)
{
    if (!receiver)
        return;
    auto *r                = new item_query_req;
    r->id                  = genUniqueId();
    r->type                = item_query_req::Type_checkSuppression;
    r->archiveKey          = key;
    r->suppressionIdentity = std::move(identity);
    pendingLocalDeletion_.insert(r->id, { receiver, std::move(finished) });
    rlist.append(r);
    QTimer::singleShot(0, this, &EDBSqLite::performRequests);
}

int EDBSqLite::features() const
{
    return SeparateAccounts | PrivateContacts | AllContacts | AllAccounts | ArchiveCache | LocalDeletion;
}

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
    QList<ContactItem> contacts;
    QSqlQuery q(QSqlDatabase::database("history"));
    QString sql = QStringLiteral("SELECT acc_id,jid FROM contacts WHERE type=:type");
    if (!accId.isEmpty())
        sql += QStringLiteral(" AND acc_id=:account");
    sql += QStringLiteral(" ORDER BY jid");
    if (!q.prepare(sql))
        return contacts;
    q.bindValue(QStringLiteral(":type"), type);
    if (!accId.isEmpty())
        q.bindValue(QStringLiteral(":account"), accId);
    if (q.exec())
        while (q.next())
            contacts.append(ContactItem(q.value(0).toString(), XMPP::Jid(q.value(1).toString())));
    return contacts;
}

quint64 EDBSqLite::eventsCount(const QString &accId, const XMPP::Jid &jid)
{
    QSqlQuery q(QSqlDatabase::database("history"));
    QString   sql = QStringLiteral(
        "SELECT count(*) FROM events e JOIN contacts c ON c.id=e.contact_id WHERE (e.message_flags & 16)=0");
    if (!accId.isEmpty())
        sql += QStringLiteral(" AND c.acc_id=:account");
    if (!jid.isEmpty())
        sql += QStringLiteral(" AND c.jid=:peer");
    if (!q.prepare(sql))
        return 0;
    if (!accId.isEmpty())
        q.bindValue(QStringLiteral(":account"), accId);
    if (!jid.isEmpty())
        q.bindValue(QStringLiteral(":peer"), jid.full());
    return q.exec() && q.next() ? q.value(0).toULongLong() : 0;
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
    // The legacy settings API is synchronous. Unlike message persistence,
    // these occasional settings writes do not share a long-lived transaction.
    QSqlDatabase db = QSqlDatabase::database("history");
    if (!db.isOpen() || !db.transaction())
        return;
    QSqlQuery q(db);
    bool ok = false;
    if (val.isEmpty()) {
        q.prepare(QStringLiteral("DELETE FROM system WHERE key=:key"));
        q.bindValue(QStringLiteral(":key"), key);
        ok = q.exec();
    } else {
        q.prepare(QStringLiteral("UPDATE system SET value=:val WHERE key=:key"));
        q.bindValue(QStringLiteral(":key"), key);
        q.bindValue(QStringLiteral(":val"), val);
        ok = q.exec();
        if (ok && q.numRowsAffected() == 0) {
            q.prepare(QStringLiteral("INSERT INTO system(key,value) VALUES(:key,:val)"));
            q.bindValue(QStringLiteral(":key"), key);
            q.bindValue(QStringLiteral(":val"), val);
            ok = q.exec();
        }
    }
    q.finish();
    if (ok)
        ok = db.commit();
    if (!ok) {
        db.rollback();
        qWarning() << "[history-db] Could not update storage parameter" << key << db.lastError().text();
    }
}

void EDBSqLite::onHistoryWriteCompleted(HistoryWriteResult written)
{
    if (written.success && !written.localDeletion.hiddenEvents.isEmpty())
        emit localMessagesDeleted(written.changedKey.account, written.changedKey.peer, written.localDeletion);
    if (pendingLocalDeletion_.contains(written.id)) {
        if (written.mediaLinksRemoved || (written.success && !written.localDeletion.hiddenEvents.isEmpty()))
            emit mediaLinksRemoved();
        auto pending = pendingLocalDeletion_.take(written.id);
        if (pending.receiver)
            pending.finished(written.localDeletion.suppressed, written.success);
        return;
    }
    if (written.success && (written.mediaLinksRemoved || !written.localDeletion.hiddenEvents.isEmpty()))
        emit mediaLinksRemoved();
    if (pendingArchiveWrites_.contains(written.id)) {
        auto pending = pendingArchiveWrites_.take(written.id);
        if (written.success)
            emit mediaLinksAdded();
        if (pending.receiver)
            pending.finished(std::move(written.archivePage), written.success);
        return;
    }
    auto *r = pendingWrites_.take(written.id);
    if (!r)
        return;
    if (written.success && written.inserted && r->type == item_query_req::Type_append && r->event
        && r->event->type() == PsiEvent::Message
        && MessageFileShares::hasFileShares(r->event.staticCast<MessageEvent>()->message().displayMessage()))
        emit mediaLinksAdded();
    if (written.success && r->type == item_query_req::Type_erase)
        emit mediaLinksRemoved();
    writeFinished(r->id, written.success);
    delete r;
}

void EDBSqLite::onHistoryReadCompleted(HistoryReadResult read)
{
    if (pendingMediaStatistics_.contains(read.id)) {
        auto pending = pendingMediaStatistics_.take(read.id);
        if (pending.receiver) pending.finished(read.mediaStatistics, read.success);
        return;
    }
    if (pendingMediaLinks_.contains(read.id)) {
        auto pending = pendingMediaLinks_.take(read.id);
        if (pending.receiver)
            pending.finished(std::move(read.mediaLinks), read.success);
        return;
    }
    if (pendingMediaLinked_.contains(read.id)) {
        auto pending = pendingMediaLinked_.take(read.id);
        if (pending.receiver)
            pending.finished(std::move(read.linkedPublications), read.success);
        return;
    }
    if (pendingMessageLookups_.contains(read.id)) {
        auto pending = pendingMessageLookups_.take(read.id);
        if (!pending.receiver)
            return;
        EDBItemPtr found;
        if (read.success) {
            if (pending.own) {
                if (!read.rows.isEmpty()) {
                    const auto &row = read.rows.first();
                    if (const auto event = HistoryEventFactory::decode(psi(), row))
                        found = EDBItemPtr(new EDBItem(event, row.value("id").toString()));
                }
            } else {
                int firstCandidate = 0;
                if (read.archiveExact && !read.rows.isEmpty()) {
                    firstCandidate = 1;
                    const auto &row = read.rows.first();
                    if (HistoryArchive::isDecodedRecord(row)) {
                        if (const auto event = HistoryEventFactory::decode(psi(), row))
                            found = EDBItemPtr(new EDBItem(
                                event, row.value("id").toString(), 0, pending.uid, {},
                                QJsonDocument::fromJson(row.value("extra_data").toString().toUtf8())
                                    .object().value("psi:protocol-only").toBool()));
                    }
                }
                if (!found) {
                    for (int i = firstCandidate; i < read.rows.size(); ++i) {
                        const auto &row = read.rows.at(i);
                        if (!HistoryArchive::isDecodedRecord(row))
                            continue;
                        const auto event = HistoryEventFactory::decode(psi(), row);
                        if (!event || event->type() != PsiEvent::Message
                            || event.staticCast<MessageEvent>()->message()
                                   .stanzaId(XMPP::Jid(pending.key.archive)).id != pending.uid)
                            continue;
                        if (found) {
                            found.clear(); // Ambiguous identity must not suppress decryption.
                            break;
                        }
                        found = EDBItemPtr(new EDBItem(event, row.value("id").toString(), 0, pending.uid));
                    }
                }
            }
        }
        pending.finished(found, read.success);
        return;
    }
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
        if (type == item_query_req::Type_deleteLocal || type == item_query_req::Type_checkSuppression) {
            auto pending = pendingLocalDeletion_.take(r->id);
            if (pending.receiver)
                pending.finished(false, false);
        } else if (type == item_query_req::Type_chatMediaStatistics) {
            auto pending = pendingMediaStatistics_.take(r->id);
            if (pending.receiver) pending.finished({}, false);
        } else if (type == item_query_req::Type_mediaLinks || type == item_query_req::Type_chatMediaLinks) {
            auto pending = pendingMediaLinks_.take(r->id);
            if (pending.receiver)
                pending.finished({}, false);
        } else if (type == item_query_req::Type_mediaLinked) {
            auto pending = pendingMediaLinked_.take(r->id);
            if (pending.receiver)
                pending.finished({}, false);
        } else if (type == item_query_req::Type_local) {
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
        } else if (type == item_query_req::Type_lookup || type == item_query_req::Type_plaintext) {
            auto pending = pendingMessageLookups_.take(r->id);
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

    if (type == item_query_req::Type_get || type == item_query_req::Type_find || type == item_query_req::Type_local
        || type == item_query_req::Type_displayed || type == item_query_req::Type_archiveRead
        || type == item_query_req::Type_resolved || type == item_query_req::Type_lookup
        || type == item_query_req::Type_plaintext || type == item_query_req::Type_mediaLinks
        || type == item_query_req::Type_mediaLinked || type == item_query_req::Type_chatMediaLinks || type == item_query_req::Type_chatMediaStatistics) {
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
        read.mediaCursor = r->mediaCursor;
        read.fileDatabasePath = r->fileDatabasePath;
        read.cursor = r->archiveCursor;
        read.archiveIndex = r->archiveIndex;
        read.snapshotCount = r->snapshotCount;
        read.eventIds = r->eventIds;
        read.publicationIds = r->publicationIds;
        read.sender = r->ownSender;
        read.messageId = r->ownMessageId;
        read.messageTime = r->ownTime;
        if (type == item_query_req::Type_mediaLinks)
            read.type = HistoryReadRequest::MediaLinks;
        else if (type == item_query_req::Type_chatMediaStatistics)
            read.type = HistoryReadRequest::ChatMediaStatistics;
        else if (type == item_query_req::Type_chatMediaLinks)
            read.type = HistoryReadRequest::ChatMediaLinks;
        else if (type == item_query_req::Type_mediaLinked)
            read.type = HistoryReadRequest::MediaLinked;
        else if (type == item_query_req::Type_local)
            read.type = HistoryReadRequest::LocalPage;
        else if (type == item_query_req::Type_displayed)
            read.type = HistoryReadRequest::DisplayedCursor;
        else if (type == item_query_req::Type_resolved)
            read.type = HistoryReadRequest::ResolveBatch;
        else if (type == item_query_req::Type_lookup)
            read.type = HistoryReadRequest::ArchiveLookup;
        else if (type == item_query_req::Type_plaintext)
            read.type = HistoryReadRequest::OwnPlaintext;
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
    write.type        = type == item_query_req::Type_deleteLocal ? HistoryWriteRequest::DeleteLocal
               : type == item_query_req::Type_checkSuppression   ? HistoryWriteRequest::CheckSuppression
               : type == item_query_req::Type_append             ? HistoryWriteRequest::Append
               : type == item_query_req::Type_archiveWrite       ? HistoryWriteRequest::StorePage
                                                                 : HistoryWriteRequest::Erase;
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
    write.eventId     = r->localBefore;
    write.identity    = r->suppressionIdentity;
    if (type == item_query_req::Type_archiveWrite || type == item_query_req::Type_deleteLocal
        || type == item_query_req::Type_checkSuppression)
        delete r;
    else
        pendingWrites_.insert(r->id, r);
    emit historyWriteRequested(write);
}

