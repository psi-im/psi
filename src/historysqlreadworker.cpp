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
        if (!db_.commit())
            return false;
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
    // This connection must never write, block a writer, or migrate schemas.
    db_.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=1000"));
    if (!db_.open())
        qWarning() << "[history-read] Cannot open read-only SQLite connection:" << db_.lastError().text();
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
    ReadSnapshot snapshot(db_);
    if (!snapshot.active()) {
        qWarning() << "[history-read] Cannot open read transaction:" << db_.lastError().text();
        emit completed(result);
        return;
    }
    if (request.type == HistoryReadRequest::LocalPage) {
        if (!request.account.isEmpty() && !request.peer.isEmpty() && request.limit > 0) {
            result.rows = HistoryArchive::localRecords(db_, request.account, request.peer,
                                                       request.before, request.limit);
            result.success = snapshot.commit();
        }
        emit completed(result);
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
                emit completed(result);
                return;
            }
            const int total = count.value(0).toInt();
            result.beginRow = backwards ? qMax(0, total - request.limit + 1) : total + 1;
        }
    }
    result.success = snapshot.commit();
    if (!result.success) {
        qWarning() << "[history-read] Failed to finish read snapshot:" << db_.lastError().text();
        result.rows.clear();
    }
    emit completed(result);
}

void HistorySqlReadWorker::close()
{
    Q_ASSERT(QThread::currentThread() == thread());
    db_.close();
    db_ = QSqlDatabase();
    if (!connectionName_.isEmpty())
        QSqlDatabase::removeDatabase(connectionName_);
    emit stopped();
}
