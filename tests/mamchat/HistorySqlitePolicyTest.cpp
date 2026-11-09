/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historysqlitepolicy.h"
#include "historywalcheckpoint.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QSignalSpy>
#include <QThread>

class HistorySqlitePolicyTest : public QObject {
    Q_OBJECT
private slots:
    void walCommitAndCheckpointRunOnSeparateThreads()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("history.db"));
        const QString connection = QStringLiteral("history-policy-test");
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QVERIFY(HistorySqlitePolicy::configure(db));
            {
                QSqlQuery q(db);
                QVERIFY(q.exec(QStringLiteral("PRAGMA journal_mode")));
                QVERIFY(q.next());
                QCOMPARE(q.value(0).toString(), QStringLiteral("wal"));
                q.finish();
                QVERIFY(q.exec(QStringLiteral("PRAGMA synchronous")));
                QVERIFY(q.next());
                QCOMPARE(q.value(0).toInt(), 1); // NORMAL, not FULL.
                q.finish();
            }
            {
                HistoryWalCheckpoint worker(path);
                QSignalSpy ready(&worker, &HistoryWalCheckpoint::ready);
                QSignalSpy checkpointed(&worker, &HistoryWalCheckpoint::checkpointCompleted);
                QVERIFY(ready.isValid());
                QVERIFY(checkpointed.isValid());
                worker.start();
                QTRY_COMPARE(ready.count(), 1);
                QVERIFY(HistorySqlitePolicy::useBackgroundCheckpoint(db));
                {
                    QSqlQuery q(db);
                    QVERIFY(q.exec(QStringLiteral("PRAGMA wal_autocheckpoint")));
                    QVERIFY(q.next());
                    QCOMPARE(q.value(0).toInt(), 0);
                    q.finish();
                    QVERIFY(q.exec(QStringLiteral("CREATE TABLE messages(id INTEGER PRIMARY KEY, body TEXT)")));
                    QVERIFY(db.transaction());
                    QVERIFY(q.exec(QStringLiteral("INSERT INTO messages(body) VALUES('persisted')")));
                    QVERIFY(db.commit());
                }
                for (int i = 0; i < 20; ++i)
                    worker.requestCheckpoint(); // Requests coalesce without shared locks.
                QTRY_COMPARE(checkpointed.count(), 1);
                worker.requestCheckpoint(); // A completed request permits the next checkpoint.
                QTRY_COMPARE(checkpointed.count(), 2);
            }
            db.close();
        }
        QSqlDatabase::removeDatabase(connection);
        // Confirm rows remain readable after WAL checkpoint and connection close.
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            {
                QSqlQuery q(db);
                QVERIFY(q.exec(QStringLiteral("SELECT body FROM messages")));
                QVERIFY(q.next());
                QCOMPARE(q.value(0).toString(), QStringLiteral("persisted"));
            }
            db.close();
        }
        QSqlDatabase::removeDatabase(connection);
    }

    void unsupportedWalDoesNotSelectNormalSync()
    {
        // SQLite :memory: databases cannot use WAL; do not downgrade their
        // synchronization while silently leaving the journal in DELETE mode.
        const QString connection = QStringLiteral("history-policy-memory");
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
            db.setDatabaseName(QStringLiteral(":memory:"));
            QVERIFY(db.open());
            QVERIFY(!HistorySqlitePolicy::configure(db));
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("PRAGMA synchronous")));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 2); // FULL default.
            q.finish();
            db.close();
        }
        QSqlDatabase::removeDatabase(connection);
    }
};
QTEST_GUILESS_MAIN(HistorySqlitePolicyTest)
#include "HistorySqlitePolicyTest.moc"
