/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historysqlreadworker.h"

#include <QCoreApplication>
#include <QSqlError>
#include <QSqlQuery>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

class HistorySqlReadWorkerTest final : public QObject {
    Q_OBJECT
private slots:
    void queuedReadsPreserveLegacyMessagesAndOrdering()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("history.db"));
        const QString connection = QStringLiteral("history-reader-test-write");
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
            db.setDatabaseName(path);
            QVERIFY2(db.open(), qPrintable(db.lastError().text()));
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("PRAGMA journal_mode=WAL")));
            q.finish();
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE contacts(id INTEGER PRIMARY KEY,acc_id TEXT,jid TEXT)")));
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE events("
                "id INTEGER PRIMARY KEY,contact_id INTEGER,date TEXT,history_date_ms INTEGER,"
                "type INTEGER,direction INTEGER,subject TEXT,m_text TEXT,lang TEXT,extra_data TEXT,"
                "history_state INTEGER DEFAULT 0,message_flags INTEGER DEFAULT 0)")));
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE message_revisions(event_id INTEGER,root_id INTEGER)")));
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE archive_items(event_id INTEGER,uid TEXT,archive TEXT,xml TEXT)")));
            QVERIFY(q.exec(QStringLiteral("INSERT INTO contacts VALUES (1,'account','old.example/peer')")));
            // A legacy server has no MAM, origin-id or stanza-id. Three
            // identical messages (including their timestamp) remain separate.
            QVERIFY(db.transaction());
            for (int i = 1; i <= 3; ++i) {
                q.prepare(QStringLiteral("INSERT INTO events(id,contact_id,date,history_date_ms,type,direction,"
                    "m_text,extra_data) VALUES(:id,1,'2026-11-09T19:00:00Z',1000,1,2,'hello','{}')"));
                q.bindValue(QStringLiteral(":id"), i);
                QVERIFY(q.exec());
            }
            QVERIFY(db.commit());
            db.close();
        }
        QSqlDatabase::removeDatabase(connection);

        qRegisterMetaType<HistoryReadRequest>("HistoryReadRequest");
        qRegisterMetaType<HistoryReadResult>("HistoryReadResult");
        QThread thread;
        auto *reader = new HistorySqlReadWorker(path);
        reader->moveToThread(&thread);
        connect(&thread, &QThread::started, reader, &HistorySqlReadWorker::open);
        connect(reader, &HistorySqlReadWorker::stopped, &thread, &QThread::quit, Qt::DirectConnection);
        connect(&thread, &QThread::finished, reader, &QObject::deleteLater);
        QSignalSpy spy(reader, &HistorySqlReadWorker::completed);
        QVERIFY(spy.isValid());
        thread.start();

        const auto send = [reader](HistoryReadRequest request) {
            return QMetaObject::invokeMethod(reader, "read", Qt::QueuedConnection,
                                             Q_ARG(HistoryReadRequest, request));
        };
        HistoryReadRequest local;
        local.id = 1;
        local.type = HistoryReadRequest::LocalPage;
        local.account = QStringLiteral("account");
        local.peer = QStringLiteral("old.example/peer");
        local.limit = 3;
        QVERIFY(send(local));
        QTRY_COMPARE(spy.count(), 1);
        auto result = qvariant_cast<HistoryReadResult>(spy.takeFirst().first());
        QVERIFY(result.success);
        QCOMPARE(result.rows.size(), 3);
        for (int i = 0; i < 3; ++i)
            QCOMPARE(result.rows.at(i).value(QStringLiteral("id")).toInt(), i + 1);

        // Requesting the next page with the oldest local ID cannot replay
        // the same three records. No server-provided identity is necessary.
        local.id = 2;
        local.before = 1;
        QVERIFY(send(local));
        QTRY_COMPARE(spy.count(), 1);
        result = qvariant_cast<HistoryReadResult>(spy.takeFirst().first());
        QVERIFY(result.success);
        QVERIFY(result.rows.isEmpty());

        HistoryReadRequest get;
        get.id = 3;
        get.type = HistoryReadRequest::Get;
        get.account = local.account;
        get.peer = local.peer;
        get.direction = 1; // EDB::Backward
        get.limit = 2;
        QVERIFY(send(get));
        QTRY_COMPARE(spy.count(), 1);
        result = qvariant_cast<HistoryReadResult>(spy.takeFirst().first());
        QVERIFY(result.success);
        QCOMPARE(result.rows.size(), 2);
        QCOMPARE(result.rows.first().value(QStringLiteral("id")).toInt(), 3);
        QCOMPARE(result.rows.last().value(QStringLiteral("id")).toInt(), 2);
        QCOMPARE(result.beginRow, 2);

        HistoryReadRequest find;
        find.id = 4;
        find.type = HistoryReadRequest::Find;
        find.account = local.account;
        find.peer = local.peer;
        find.text = QStringLiteral("HELLO");
        QVERIFY(send(find));
        QTRY_COMPARE(spy.count(), 1);
        result = qvariant_cast<HistoryReadResult>(spy.takeFirst().first());
        QVERIFY(result.success);
        QCOMPARE(result.rows.size(), 3);

        // A writer can commit another live message concurrently. A worker
        // snapshot never leaks an uncommitted row or loses earlier messages.
        const QString concurrentName = QStringLiteral("history-reader-concurrent-writer");
        {
            auto writer = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), concurrentName);
            writer.setDatabaseName(path);
            QVERIFY(writer.open());
            QVERIFY(writer.transaction());
            QSqlQuery q(writer);
            QVERIFY(q.exec(QStringLiteral("INSERT INTO events(id,contact_id,date,history_date_ms,type,direction,"
                "m_text,extra_data) VALUES(4,1,'2026-11-09T19:00:00Z',1000,1,2,'hello','{}')")));
            get.id = 5;
            get.limit = 10;
            QVERIFY(send(get));
            QTRY_COMPARE(spy.count(), 1);
            result = qvariant_cast<HistoryReadResult>(spy.takeFirst().first());
            QVERIFY(result.success);
            QCOMPARE(result.rows.size(), 3);
            QVERIFY(writer.commit());

            get.id = 6;
            QVERIFY(send(get));
            QTRY_COMPARE(spy.count(), 1);
            result = qvariant_cast<HistoryReadResult>(spy.takeFirst().first());
            QVERIFY(result.success);
            QCOMPARE(result.rows.size(), 4);
            QCOMPARE(result.rows.first().value(QStringLiteral("id")).toInt(), 4);
            writer.close();
        }
        QSqlDatabase::removeDatabase(concurrentName);

        QVERIFY(QMetaObject::invokeMethod(reader, "close", Qt::QueuedConnection));
        QVERIFY(thread.wait(5000));
    }
};

QTEST_GUILESS_MAIN(HistorySqlReadWorkerTest)
#include "HistorySqlReadWorkerTest.moc"
