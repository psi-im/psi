/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historysqlitepolicy.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

class HistorySqlitePolicyTest final : public QObject {
    Q_OBJECT
private slots:
    void walUsesFullDurabilityAndAutomaticCheckpointing()
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
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("PRAGMA journal_mode")));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toString(), QStringLiteral("wal"));
            q.finish();
            QVERIFY(q.exec(QStringLiteral("PRAGMA synchronous")));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 2); // FULL, not NORMAL.
            q.finish();
            QVERIFY(q.exec(QStringLiteral("PRAGMA wal_autocheckpoint")));
            QVERIFY(q.next());
            QVERIFY(q.value(0).toInt() > 0); // Worker handles its own checkpoint on commit.
            q.finish();
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE messages(id INTEGER PRIMARY KEY, body TEXT)")));
            QVERIFY(db.transaction());
            QVERIFY(q.exec(QStringLiteral("INSERT INTO messages(body) VALUES('persisted')")));
            QVERIFY(db.commit());
            db.close();
        }
        QSqlDatabase::removeDatabase(connection);
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("SELECT body FROM messages")));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toString(), QStringLiteral("persisted"));
            q.finish();
            db.close();
        }
        QSqlDatabase::removeDatabase(connection);
    }

    void unsupportedWalKeepsFullSync()
    {
        const QString connection = QStringLiteral("history-policy-memory");
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
            db.setDatabaseName(QStringLiteral(":memory:"));
            QVERIFY(db.open());
            QVERIFY(!HistorySqlitePolicy::configure(db));
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("PRAGMA synchronous")));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 2);
            q.finish();
            db.close();
        }
        QSqlDatabase::removeDatabase(connection);
    }
};
QTEST_GUILESS_MAIN(HistorySqlitePolicyTest)
#include "HistorySqlitePolicyTest.moc"
