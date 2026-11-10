/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historysqlreadworker.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <utility>

namespace {
// A local-history page resolves each original and its latest revision via
// multiple SQL statements. One read transaction guarantees a single snapshot
// even while live messages or MAM pages are being committed concurrently.
class ReadSnapshot {
public:
    explicit ReadSnapshot(QSqlDatabase &db) : db_(db), active_(db_.transaction()) { }
    ~ReadSnapshot()
    {
        if (active_)
            db_.rollback();
    }
    bool active() const { return active_; }
    bool commit()
    {
        if (!active_)
            return false;
        if (!db_.commit()) {
            qWarning() << "[history-read] Failed to finish read snapshot:" << db_.lastError().text();
            return false;
        }
        active_ = false;
        return true;
    }
private:
    QSqlDatabase &db_;
    bool active_;
};
}

void HistorySqlReadWorker::open()
{
    Q_ASSERT(QThread::currentThread() == thread());
    connectionName_ = QStringLiteral("history-reader-%1")
                          .arg(qulonglong(reinterpret_cast<quintptr>(this)), 0, 16);
    db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    db_.setDatabaseName(path_);
    // The same worker serializes SQLite reads and new appends/erasures.
    // Migration and legacy archive APIs are still being moved off the GUI.
    db_.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=2000"));
    if (!db_.open()) {
        qWarning() << "[history-db] Cannot open worker SQLite connection:" << db_.lastError().text();
        return;
    }
    QSqlQuery policy(db_);
    // Each SQLite connection has its own synchronous setting. FULL commits
    // can now wait for disk on the worker without freezing Qt's GUI thread.
    if (!policy.exec(QStringLiteral("PRAGMA synchronous=FULL")))
        qWarning() << "[history-db] Could not enable FULL synchronization:" << policy.lastError().text();
    if (!policy.exec(QStringLiteral("PRAGMA foreign_keys=ON")))
        qWarning() << "[history-db] Cannot enable foreign keys:" << policy.lastError().text();
    policy.finish();

    // Retain the old EDB batching benefit, with a much shorter maximum
    // latency. This timer and all pending results live in the worker.
    appendCommitTimer_ = new QTimer(this);
    appendCommitTimer_->setSingleShot(true);
    // Preserve EDB's previous 3-record / 1-second commit thresholds.
    appendCommitTimer_->setInterval(1000);
    connect(appendCommitTimer_, &QTimer::timeout, this, &HistorySqlReadWorker::flushPendingAppends);
}

void HistorySqlReadWorker::read(HistoryReadRequest request)
{
    Q_ASSERT(QThread::currentThread() == thread());
    HistoryReadResult result;
    result.id = request.id;
    if (!db_.isOpen()) {
        emit completed(result);
        return;
    }
    // A reader must observe all earlier appends in the same worker queue.
    // Complete their transaction and notify write handles before reading.
    flushPendingAppends();
    ReadSnapshot snapshot(db_);
    if (!snapshot.active()) {
        qWarning() << "[history-read] Cannot open read transaction:" << db_.lastError().text();
        emit completed(result);
        return;
    }
    QSqlError  error;
    const auto finish = [&] {
        if (error.isValid())
            qWarning() << "[history-read] Could not read history projection:" << error.text();
        result.success = !error.isValid() && snapshot.commit();
        if (!result.success) {
            // Never expose an incomplete projection or turn a failed read
            // into a successfully restored empty cursor/page.
            result    = HistoryReadResult();
            result.id = request.id;
        }
        emit completed(result);
    };
    if (request.type == HistoryReadRequest::MediaLinks) {
        result.mediaLinks = HistoryMediaRefs::linksAfter(db_, request.before, request.limit, &error);
        finish();
        return;
    }
    if (request.type == HistoryReadRequest::MediaLinked) {
        const auto linked = HistoryMediaRefs::linked(db_, request.publicationIds, &error);
        if (linked)
            result.linkedPublications = *linked;
        finish();
        return;
    }
    if (request.type == HistoryReadRequest::ArchiveLookup) {
        QSqlQuery q(db_);
        q.prepare(QStringLiteral(
            "SELECT e.*,c.acc_id,c.jid,i.xml AS archive_xml FROM archive_items i "
            "JOIN events e ON e.id=i.event_id JOIN contacts c ON c.id=e.contact_id "
            "WHERE i.account=:account AND i.archive=:archive AND i.peer=:peer AND i.uid=:uid"));
        q.bindValue(QStringLiteral(":account"), request.archiveKey.account);
        q.bindValue(QStringLiteral(":archive"), request.archiveKey.archive);
        q.bindValue(QStringLiteral(":peer"), request.archiveKey.peer);
        q.bindValue(QStringLiteral(":uid"), request.cursor);
        if (!q.exec()) {
            error = q.lastError();
        } else if (q.next()) {
            result.rows.append(q.record());
            result.archiveExact = true;
        } else if (q.lastError().isValid()) {
            error = q.lastError();
        }
        q.finish();
        // Exact decoded archive hits are by far the common path. Avoid
        // transferring every message in the ten-minute fallback window for
        // each element of an already cached page.
        const bool decoded = result.archiveExact && HistoryArchive::isDecodedRecord(result.rows.first());
        if (!error.isValid() && !decoded && request.messageTime.isValid()) {
            q.prepare(QStringLiteral(
                "SELECT e.*,c.acc_id,c.jid FROM events e JOIN contacts c ON c.id=e.contact_id "
                "WHERE c.acc_id=:account AND c.jid=:peer AND e.history_date_ms BETWEEN :start AND :end"));
            q.bindValue(QStringLiteral(":account"), request.archiveKey.account);
            q.bindValue(QStringLiteral(":peer"), request.archiveKey.peer);
            q.bindValue(QStringLiteral(":start"), request.messageTime.toMSecsSinceEpoch() - 300000);
            q.bindValue(QStringLiteral(":end"), request.messageTime.toMSecsSinceEpoch() + 300000);
            if (!q.exec()) {
                error = q.lastError();
            } else {
                while (q.next())
                    result.rows.append(q.record());
                error = q.lastError();
            }
        }
        finish();
        return;
    }
    if (request.type == HistoryReadRequest::OwnPlaintext) {
        const auto id = HistoryArchive::matchingOwnPlaintext(db_, request.archiveKey, request.messageId, request.sender,
                                                             request.messageTime, &error);
        if (id > 0) {
            QSqlQuery q(db_);
            q.prepare(QStringLiteral("SELECT e.*,c.acc_id,c.jid FROM events e "
                                     "JOIN contacts c ON c.id=e.contact_id WHERE e.id=:id"));
            q.bindValue(QStringLiteral(":id"), id);
            if (q.exec() && q.next())
                result.rows.append(q.record());
            error = q.lastError();
        }
        finish();
        return;
    }
    if (request.type == HistoryReadRequest::ResolveBatch) {
        for (const auto id : request.eventIds) {
            if (id <= 0 || result.resolvedRows.contains(id))
                continue;
            const auto row = HistoryArchive::resolvedRecord(db_, id, &error);
            if (!row.isEmpty())
                result.resolvedRows.insert(id, row);
        }
        finish();
        return;
    }
    if (request.type == HistoryReadRequest::ArchiveBefore
        || request.type == HistoryReadRequest::ArchiveAfter
        || request.type == HistoryReadRequest::ArchiveIndex) {
        const auto &key = request.archiveKey;
        result.archiveSnapshot = HistoryArchive::readSnapshot(db_, key, &error);
        if (request.type == HistoryReadRequest::ArchiveBefore)
            result.archivePage = HistoryArchive::readPage(db_, key, request.cursor, request.limit, &error);
        else if (request.type == HistoryReadRequest::ArchiveAfter)
            result.archivePage
                = HistoryArchive::readAfter(db_, key, request.cursor, request.limit, request.snapshotCount, &error);
        else
            result.archivePage = HistoryArchive::readIndex(db_, key, request.archiveIndex, request.limit,
                                                           request.snapshotCount, &error);
        finish();
        return;
    }
    if (request.type == HistoryReadRequest::DisplayedCursor) {
        if (!request.account.isEmpty() && !request.peer.isEmpty()) {
            result.cursor = HistoryArchive::sentDisplayedCursor(db_, request.account, request.peer, &error);
            finish();
        } else {
            emit completed(result);
        }
        return;
    }
    if (request.type == HistoryReadRequest::LocalPage) {
        if (!request.account.isEmpty() && !request.peer.isEmpty() && request.limit > 0) {
            result.rows = HistoryArchive::localRecords(db_, request.account, request.peer, request.before,
                                                       request.limit, &error);
            finish();
        } else {
            emit completed(result);
        }
        return;
    }

    const bool allAccounts = request.account.isEmpty();
    const bool allContacts = request.peer.isEmpty();
    const bool backwards = request.direction == 1; // EDB::Backward
    const bool dateQuery = !request.date.isNull();

    QString sql =
        "SELECT contacts.acc_id, events.id, contacts.jid, events.date, events.type, direction, "
        "subject, m_text, lang, extra_data, history_state, "
        "COALESCE((SELECT root.message_flags FROM message_revisions r JOIN events root "
        "ON root.id=r.root_id WHERE r.event_id=events.id),events.message_flags) AS message_flags, "
        "(SELECT uid FROM archive_items WHERE event_id=events.id LIMIT 1) AS archive_uid, "
        "(SELECT xml FROM archive_items WHERE event_id=events.id LIMIT 1) AS archive_xml "
        "FROM events, contacts WHERE contacts.id=events.contact_id";
    if (!allContacts)
        sql += QStringLiteral(" AND contacts.jid=:jid");
    if (!allAccounts)
        sql += QStringLiteral(" AND contacts.acc_id=:acc_id");
    if (request.type == HistoryReadRequest::Get) {
        if (dateQuery)
            sql += backwards ? QStringLiteral(" AND history_date_ms < :date")
                             : QStringLiteral(" AND history_date_ms >= :date");
        sql += backwards ? QStringLiteral(" ORDER BY history_date_ms DESC, events.id DESC")
                         : QStringLiteral(" ORDER BY history_date_ms ASC, events.id ASC");
        sql += QStringLiteral(" LIMIT :start, :cnt");
    } else {
        sql += QStringLiteral(" AND m_text IS NOT NULL ORDER BY history_date_ms, events.id");
    }

    QSqlQuery q(db_);
    if (!q.prepare(sql)) {
        qWarning() << "[history-read] Cannot prepare history query:" << q.lastError().text();
        emit completed(result);
        return;
    }
    if (!allContacts)
        q.bindValue(QStringLiteral(":jid"), request.peer);
    if (!allAccounts)
        q.bindValue(QStringLiteral(":acc_id"), request.account);
    if (request.type == HistoryReadRequest::Get) {
        if (dateQuery)
            q.bindValue(QStringLiteral(":date"), request.date.toMSecsSinceEpoch());
        q.bindValue(QStringLiteral(":start"), request.start);
        q.bindValue(QStringLiteral(":cnt"), request.limit);
    }
    if (!q.exec()) {
        qWarning() << "[history-read] Cannot execute history query:" << q.lastError().text();
        emit completed(result);
        return;
    }

    const QString needle = request.text.toLower();
    while (q.next()) {
        const QSqlRecord row = q.record();
        if (request.type == HistoryReadRequest::Find
            && !row.value(QStringLiteral("m_text")).toString().toLower().contains(needle, Qt::CaseSensitive))
            continue;
        result.rows.append(row);
    }
    error = q.lastError();
    q.finish();

    if (request.type == HistoryReadRequest::Get) {
        if (!backwards && !dateQuery) {
            result.beginRow = request.start;
        } else {
            QString countSql = QStringLiteral("SELECT count(*) FROM events, contacts "
                                               "WHERE contacts.id=events.contact_id");
            if (!allContacts)
                countSql += QStringLiteral(" AND contacts.jid=:jid");
            if (!allAccounts)
                countSql += QStringLiteral(" AND contacts.acc_id=:acc_id");
            if (dateQuery)
                countSql += QStringLiteral(" AND history_date_ms < :date");
            QSqlQuery count(db_);
            if (!count.prepare(countSql)) {
                result.rows.clear();
                emit completed(result);
                return;
            }
            if (!allContacts)
                count.bindValue(QStringLiteral(":jid"), request.peer);
            if (!allAccounts)
                count.bindValue(QStringLiteral(":acc_id"), request.account);
            if (dateQuery)
                count.bindValue(QStringLiteral(":date"), request.date.toMSecsSinceEpoch());
            if (!count.exec() || !count.next()) {
                result.rows.clear();
                emit completed(result);
                return;
            }
            const int total = count.value(0).toInt();
            result.beginRow = backwards ? qMax(0, total - request.limit + 1) : total + 1;
        }
    }
    finish();
}

void HistorySqlReadWorker::write(HistoryWriteRequest request)
{
    Q_ASSERT(QThread::currentThread() == thread());
    HistoryWriteResult result;
    result.id = request.id;
    if (!db_.isOpen()) {
        emit writeCompleted(result);
        return;
    }
    // Erase and atomic MAM stores are barriers: they must neither overtake
    // queued appends nor be accidentally included in an append batch.
    if (request.type != HistoryWriteRequest::Append)
        flushPendingAppends();
    if (request.type == HistoryWriteRequest::StorePage) {
        // storePage owns its transaction: message rows, identity, revisions,
        // adjacency and persistent cursor either all commit or roll back.
        result.archivePage = request.archivePage;
        result.success     = HistoryArchive::storePage(
            db_, request.archiveKey, request.before, request.limit, result.archivePage, [this, &result](qint64 id) {
                return !eventStoredHook_ || eventStoredHook_(db_, id, &result.mediaLinksRemoved);
            });
        emit writeCompleted(result);
        return;
    }
    // Claim the SQLite writer slot *before* reading contacts or revisions.
    // A deferred BEGIN can otherwise observe an old WAL snapshot and fail
    // with SQLITE_BUSY_SNAPSHOT when a second connection commits first.
    // BEGIN IMMEDIATE honors the configured busy timeout without GUI stalls.
    if (!appendTransactionOpen_) {
        QSqlQuery begin(db_);
        if (!begin.exec(QStringLiteral("BEGIN IMMEDIATE TRANSACTION"))) {
            qWarning() << "[history-db] Cannot begin history write:" << begin.lastError().text();
            emit writeCompleted(result);
            return;
        }
        begin.finish();
        if (request.type == HistoryWriteRequest::Append)
            appendTransactionOpen_ = true;
    }

    // One write is one transaction. In particular, events, revisions and
    // message-state materialization are never committed separately.
    bool ok = false;
    if (request.type == HistoryWriteRequest::Append) {
        const auto &event = request.encoded;
        // JID resource can contain '/', so never try to reconstruct it
        // by splitting the full JID string in this Qt-only worker.
        const QString jid = request.contactPeer;
        if (event.valid && !request.account.isEmpty() && !jid.isEmpty()) {
            QSqlQuery contact(db_);
            contact.prepare(QStringLiteral("SELECT id FROM contacts WHERE acc_id=:account AND jid=:jid"));
            contact.bindValue(QStringLiteral(":account"), request.account);
            contact.bindValue(QStringLiteral(":jid"), jid);
            qint64 contactId = 0;
            if (contact.exec()) {
                if (contact.next())
                    contactId = contact.value(0).toLongLong();
                contact.finish();
                if (!contactId) {
                    contact.prepare(QStringLiteral("INSERT INTO contacts(acc_id,type,jid,lifetime) "
                                                    "VALUES(:account,:type,:jid,-1)"));
                    contact.bindValue(QStringLiteral(":account"), request.account);
                    contact.bindValue(QStringLiteral(":type"), request.jidType);
                    contact.bindValue(QStringLiteral(":jid"), jid);
                    if (contact.exec())
                        contactId = contact.lastInsertId().toLongLong();
                }
            }
            if (contactId) {
                HistoryArchive::Entry delivery;
                delivery.messageId = event.messageId;
                delivery.sender = event.sender;
                delivery.timestamp = event.timestamp;
                delivery.type = event.type;
                delivery.direction = event.direction;
                delivery.body = event.matchBody;
                delivery.subject = event.matchSubject;
                // Only a confirmed existing archived delivery can suppress
                // a duplicate. Old servers may have no stable server IDs.
                QSqlError  error;
                const bool existing
                    = event.isMessage && HistoryArchive::matchingDelivery(db_, contactId, delivery, true, &error);
                if (error.isValid()) {
                    qWarning() << "[history-db] Could not match live delivery:" << error.text();
                } else if (existing) {
                    ok = true;
                } else {
                    QSqlQuery q(db_);
                    q.prepare(QStringLiteral(
                        "INSERT INTO events(contact_id,resource,date,type,direction,subject,m_text,lang,extra_data,"
                        "history_state,history_date_ms) VALUES(:contact,:resource,:date,:type,:direction,"
                        ":subject,:body,:lang,:extra,:state,:timestamp)"));
                    q.bindValue(QStringLiteral(":contact"), contactId);
                    q.bindValue(QStringLiteral(":resource"),
                                request.jidType == 2 ? QString() : request.resource);
                    q.bindValue(QStringLiteral(":date"), event.timestamp);
                    q.bindValue(QStringLiteral(":type"), event.type);
                    q.bindValue(QStringLiteral(":direction"), event.direction);
                    q.bindValue(QStringLiteral(":subject"), event.isMessage ? QVariant(event.subject) : QVariant());
                    q.bindValue(QStringLiteral(":body"), event.isMessage ? QVariant(event.body) : QVariant());
                    q.bindValue(QStringLiteral(":lang"), event.isMessage ? QVariant(event.language) : QVariant());
                    q.bindValue(QStringLiteral(":extra"), event.isMessage ? QVariant(event.extras) : QVariant());
                    q.bindValue(QStringLiteral(":state"),
                                event.isMessage ? HistoryArchive::Unverified : HistoryArchive::LocalOnly);
                    q.bindValue(QStringLiteral(":timestamp"),
                                event.timestamp.isValid() ? event.timestamp.toMSecsSinceEpoch() : qint64(0));
                    ok = q.exec();
                    if (ok) {
                        result.inserted = true;
                        const qint64 eventId = q.lastInsertId().toLongLong();
                        if (event.isMessage)
                            ok = HistoryArchive::recordRevision(db_, eventId, contactId, event.type, event.direction,
                                                                event.sender, event.messageId, event.replaceId)
                                && HistoryArchive::recordMessageStates(db_, { eventId })
                                && (!eventStoredHook_ || eventStoredHook_(db_, eventId, &result.mediaLinksRemoved));
                    }
                }
            }
        }
    } else if (request.type == HistoryWriteRequest::Erase) {
        if (request.account.isEmpty() && request.peer.isEmpty()) {
            QSqlQuery q(db_);
            ok = q.exec(QStringLiteral("DELETE FROM contacts"));
        } else if (!request.account.isEmpty() && !request.peer.isEmpty()) {
            QSqlQuery contact(db_);
            contact.prepare(QStringLiteral("SELECT id FROM contacts WHERE acc_id=:account AND jid=:jid"));
            contact.bindValue(QStringLiteral(":account"), request.account);
            contact.bindValue(QStringLiteral(":jid"), request.peer);
            if (contact.exec() && contact.next()) {
                const qint64 contactId = contact.value(0).toLongLong();
                contact.finish();
                QSqlQuery q(db_);
                q.prepare(QStringLiteral("DELETE FROM events WHERE contact_id=:id"));
                q.bindValue(QStringLiteral(":id"), contactId);
                if (q.exec()) {
                    q.prepare(QStringLiteral("DELETE FROM contacts WHERE id=:id AND lifetime=-1"));
                    q.bindValue(QStringLiteral(":id"), contactId);
                    ok = q.exec();
                }
            }
        }
        if (ok)
            ok = HistoryArchive::invalidate(db_, request.account, request.peer);
    }

    if (request.type == HistoryWriteRequest::Append) {
        if (!ok) {
            // No successful callback may escape a failed batch. All writes
            // in the transaction roll back together, including revisions.
            db_.rollback();
            appendTransactionOpen_ = false;
            if (appendCommitTimer_)
                appendCommitTimer_->stop();
            for (auto &pending : pendingAppends_) {
                pending.success = false;
                pending.inserted = false;
                pending.mediaLinksRemoved = false;
                emit writeCompleted(pending);
            }
            pendingAppends_.clear();
            result.inserted = false;
            result.mediaLinksRemoved = false;
            emit writeCompleted(result);
            return;
        }
        pendingAppends_.append(result);
        if (pendingAppends_.size() >= 3) {
            flushPendingAppends();
        } else if (appendCommitTimer_ && !appendCommitTimer_->isActive()) {
            appendCommitTimer_->start();
        }
        return;
    }

    // Erase operations are standalone transactions and notify only after
    // commit. MAM StorePage already committed inside HistoryArchive.
    if (ok)
        ok = db_.commit();
    if (!ok)
        db_.rollback();
    result.success = ok;
    if (!ok)
        result.inserted = false;
    emit writeCompleted(result);
}

void HistorySqlReadWorker::flushPendingAppends()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!appendTransactionOpen_)
        return;
    if (appendCommitTimer_)
        appendCommitTimer_->stop();
    // FULL-sync durability applies at each batch commit, not each message.
    const bool committed = db_.commit();
    if (!committed) {
        qWarning() << "[history-db] Failed to commit append batch:" << db_.lastError().text();
        db_.rollback();
    }
    appendTransactionOpen_ = false;
    for (auto &result : pendingAppends_) {
        result.success = committed;
        if (!committed) {
            result.inserted = false;
            result.mediaLinksRemoved = false;
        }
        emit writeCompleted(result);
    }
    pendingAppends_.clear();
}

void HistorySqlReadWorker::close()
{
    Q_ASSERT(QThread::currentThread() == thread());
    flushPendingAppends(); // Shutdown is also a durability boundary.
    db_.close();
    db_ = QSqlDatabase();
    if (!connectionName_.isEmpty())
        QSqlDatabase::removeDatabase(connectionName_);
    emit stopped();
}
