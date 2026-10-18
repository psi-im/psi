/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historyarchive.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QtTest>

using namespace HistoryArchive;
class HistoryArchiveTest : public QObject {
    Q_OBJECT
    QSqlDatabase db;
    Key          key { "account-a", "me@example.org", "you@example.org", false };
    QVariant     scalar(const QString &sql)
    {
        QSqlQuery q(db);
        if (!q.exec(sql) || !q.next())
            return {};
        return q.value(0);
    }
    Entry entry(const QString &uid, int second = 0)
    {
        Entry e;
        e.uid       = uid;
        e.xml       = "<result id='" + uid + "'/>";
        e.timestamp = QDateTime::fromMSecsSinceEpoch(1700000000000 + second * 1000).toUTC();
        e.body      = "hello";
        e.sender    = "you@example.org/phone";
        e.messageId = uid + "-client";
        e.extra
            = QString::fromUtf8(QJsonDocument(QJsonObject { { "psi:message",
                                                              QJsonObject { { "version", 1 },
                                                                            { "id", e.messageId },
                                                                            { "from", e.sender },
                                                                            { "to", "me@example.org/desktop" } } } })
                                    .toJson(QJsonDocument::Compact));
        return e;
    }
    Page page(QStringList ids, bool complete = false, bool stable = true)
    {
        Page p;
        p.complete = complete;
        p.stable   = stable;
        int i      = 0;
        for (const auto &uid : ids)
            p.entries.append(entry(uid, i++));
        return p;
    }
private slots:
    void init()
    {
        db = QSqlDatabase::addDatabase("QSQLITE", "test-history");
        db.setDatabaseName(":memory:");
        QVERIFY(db.open());
        QSqlQuery q(db);
        QVERIFY(q.exec("PRAGMA foreign_keys=ON"));
        QVERIFY(q.exec("CREATE TABLE system(key TEXT,value TEXT)"));
        QVERIFY(q.exec("INSERT INTO system VALUES('version','0.1')"));
        QVERIFY(
            q.exec("CREATE TABLE contacts(id INTEGER PRIMARY KEY,acc_id TEXT,type INTEGER,jid TEXT,lifetime INTEGER)"));
        QVERIFY(q.exec(
            "CREATE TABLE events(id INTEGER PRIMARY KEY,contact_id INTEGER REFERENCES contacts(id) ON DELETE CASCADE,"
            "resource TEXT,date TEXT,type INTEGER,direction INTEGER,subject TEXT,m_text TEXT,lang TEXT,extra_data "
            "TEXT)"));
    }
    void cleanup()
    {
        db.close();
        db = {};
        QSqlDatabase::removeDatabase("test-history");
    }
    void migrationKeepsLegacyData()
    {
        QSqlQuery q(db);
        QVERIFY(q.exec("INSERT INTO contacts VALUES(1,'account-a',1,'you@example.org',-1)"));
        QVERIFY(q.exec("INSERT INTO events "
                       "VALUES(17,1,'phone','2020-01-01T10:00:00',1,2,'subject','original','ru','{\"jabber:x:oob\":[["
                       "\"https://example.org/file\"]]}')"));
        QVERIFY(migrate(db));
        QCOMPARE(scalar("SELECT history_state FROM events WHERE id=17").toInt(), int(Unverified));
        QCOMPARE(scalar("SELECT m_text FROM events WHERE id=17").toString(), QString("original"));
        QCOMPARE(scalar("SELECT extra_data FROM events WHERE id=17").toString(),
                 QString("{\"jabber:x:oob\":[[\"https://example.org/file\"]]}"));
        QCOMPARE(scalar("SELECT value FROM system WHERE key='version'").toString(), QString("0.2"));
        QVERIFY(migrate(db));
        QCOMPARE(scalar("SELECT count(*) FROM events").toInt(), 1);
    }
    void timeZonesAndEqualTimestampsUseSafeLocalPages()
    {
        QSqlQuery q(db);
        QVERIFY(q.exec("INSERT INTO contacts VALUES(1,'account-a',1,'you@example.org',-1)"));
        QVERIFY(q.exec("INSERT INTO events(id,contact_id,date,type,direction,m_text) VALUES"
                       "(1,1,'2023-11-14T12:00:00+03:00',1,2,'early local'),"
                       "(2,1,'2023-11-14T10:00:00Z',1,2,'later archive'),"
                       "(3,1,'2023-11-14T10:00:00Z',1,2,'same stamp')"));
        QVERIFY(migrate(db));
        const auto latest = localRecords(db, key.account, key.peer, 0, 2);
        QCOMPARE(latest.size(), 2);
        QCOMPARE(latest.first().value("id").toInt(), 2);
        QCOMPARE(latest.last().value("id").toInt(), 3);
        const auto older = localRecords(db, key.account, key.peer, 2, 2);
        QCOMPARE(older.size(), 1);
        QCOMPARE(older.first().value("id").toInt(), 1);
        QVERIFY(localRecords(db, "other", key.peer, 0, 2).isEmpty());
        QCOMPARE(scalar("SELECT date FROM events WHERE id=1").toString(), QString("2023-11-14T12:00:00+03:00"));
    }
    void migrationProvesNonMessageEventsLocalOnly()
    {
        QSqlQuery q(db);
        QVERIFY(q.exec("INSERT INTO contacts VALUES(1,'account-a',1,'you@example.org',-1)"));
        QVERIFY(q.exec("INSERT INTO events(id,contact_id,type) VALUES(1,1,3),(2,1,1)"));
        QVERIFY(migrate(db));
        QCOMPARE(scalar("SELECT history_state FROM events WHERE id=1").toInt(), int(LocalOnly));
        QCOMPARE(scalar("SELECT history_state FROM events WHERE id=2").toInt(), int(Unverified));
    }
    void futureSchemaIsNeverDowngraded()
    {
        QSqlQuery q(db);
        QVERIFY(q.exec("UPDATE system SET value='99.0'"));
        QVERIFY(!migrate(db));
        QCOMPARE(scalar("SELECT value FROM system WHERE key='version'").toString(), QString("99.0"));
        QVERIFY(q.exec("SELECT * FROM events"));
        QCOMPARE(q.record().indexOf("history_state"), -1);
    }
    void downgradeRowsReceiveOrderingKeysOnNextOpen()
    {
        QVERIFY(migrate(db));
        auto p = page({ "one" }, true);
        QVERIFY(storePage(db, key, "", 1, p));
        QSqlQuery q(db);
        QVERIFY(q.exec("INSERT INTO events(contact_id,date,type,direction,m_text) "
                       "SELECT id,'2026-10-18T10:00:00Z',1,2,'written by old client' FROM contacts"));
        QCOMPARE(scalar("SELECT count(*) FROM events WHERE history_date_ms IS NULL").toInt(), 1);
        QVERIFY(migrate(db));
        QCOMPARE(scalar("SELECT count(*) FROM events WHERE history_date_ms IS NULL").toInt(), 0);
        QCOMPARE(scalar("SELECT count(*) FROM events").toInt(), 2);
        QVERIFY(readPage(db, key, "", 1).found);
        QCOMPARE(localRecords(db, key.account, key.peer, 0, 2).last().value("m_text").toString(),
                 QString("written by old client"));
    }
    void migrationRollsBack()
    {
        QSqlQuery q(db);
        QVERIFY(q.exec("CREATE VIEW archive_items AS SELECT 1"));
        QVERIFY(!migrate(db));
        QCOMPARE(scalar("SELECT value FROM system WHERE key='version'").toString(), QString("0.1"));
        QVERIFY(q.exec("SELECT * FROM events"));
        QCOMPARE(q.record().indexOf("history_state"), -1);
    }
    void pageScopeAndAdjacency()
    {
        QVERIFY(migrate(db));
        auto p = page({ "z-old", "a-new" }); // UIDs are opaque, not sortable.
        QVERIFY(storePage(db, key, "boundary", 2, p));
        auto saved = readPage(db, key, "boundary", 2);
        QVERIFY(saved.found);
        QVERIFY(saved.stable);
        QVERIFY(!saved.complete);
        QCOMPARE(saved.entries.size(), 2);
        QCOMPARE(saved.entries.first().uid, QString("z-old"));
        QCOMPARE(scalar("SELECT history_state FROM events ORDER BY id LIMIT 1").toInt(), int(Archived | LinkedNext));
        QCOMPARE(scalar("SELECT history_state FROM events ORDER BY id DESC LIMIT 1").toInt(),
                 int(Archived | LinkedPrevious | LinkedNext));
        QVERIFY(!readPage(db, key, "other", 2).found);
        QVERIFY(!readPage(db, key, "boundary", 3).found);
        auto other    = key;
        other.account = "other";
        QVERIFY(!readPage(db, other, "boundary", 2).found);
        other         = key;
        other.archive = "other";
        QVERIFY(!readPage(db, other, "boundary", 2).found);
        other      = key;
        other.peer = "other";
        QVERIFY(!readPage(db, other, "boundary", 2).found);
    }
    void shiftedPageBoundaryUsesTrustedChain()
    {
        QVERIFY(migrate(db));
        auto tail = page({ "three", "four" });
        QVERIFY(storePage(db, key, "", 2, tail));
        auto earlier = page({ "one", "two" }, true);
        QVERIFY(storePage(db, key, "three", 2, earlier));
        // No request before "four" was ever cached. Its two predecessors
        // span the two original pages and form a complete connected page.
        auto shifted = readPage(db, key, "four", 2);
        QVERIFY(shifted.found);
        QVERIFY(shifted.stable);
        QVERIFY(!shifted.complete);
        QCOMPARE(shifted.entries.size(), 2);
        QCOMPARE(shifted.entries.first().uid, QString("two"));
        QCOMPARE(shifted.entries.last().uid, QString("three"));
        auto start = readPage(db, key, "two", 40);
        QVERIFY(start.found);
        QVERIFY(start.complete);
        QCOMPARE(start.entries.size(), 1);
        QCOMPARE(start.entries.first().uid, QString("one"));
        auto gap    = key;
        gap.account = "other";
        QVERIFY(!readPage(db, gap, "four", 2).found);
        QSqlQuery q(db);
        QVERIFY(q.exec("DELETE FROM events WHERE id=(SELECT event_id FROM archive_items WHERE uid='two')"));
        QVERIFY(!readPage(db, key, "four", 2).found);
    }
    void isolatedTrustedRowsNeverCoverGaps()
    {
        QVERIFY(migrate(db));
        auto a = page({ "one" });
        auto b = page({ "three" });
        QVERIFY(storePage(db, key, "two", 1, a));
        QVERIFY(storePage(db, key, "four", 1, b));
        QVERIFY(!readPage(db, key, "four", 2).found);
        QVERIFY(!readPage(db, key, "three", 2).found);
    }
    void completeEmptyPageLinksArchiveStart()
    {
        QVERIFY(migrate(db));
        auto tail = page({ "one" });
        QVERIFY(storePage(db, key, "", 1, tail));
        auto empty = page({}, true);
        QVERIFY(storePage(db, key, "one", 1, empty));
        auto start = readPage(db, key, "one", 40);
        QVERIFY(start.found);
        QVERIFY(start.complete);
        QVERIFY(start.entries.isEmpty());
        QVERIFY(scalar("SELECT history_state FROM events").toInt() & LinkedPrevious);
    }
    void liveLoggingCanFindAlreadyArchivedDelivery()
    {
        QVERIFY(migrate(db));
        auto p = page({ "one" }, true);
        QVERIFY(storePage(db, key, "", 1, p));
        const auto contact = scalar("SELECT id FROM contacts").toLongLong();
        QCOMPARE(matchingDelivery(db, contact, p.entries.first(), true), p.entries.first().eventId);
        auto other   = p.entries.first();
        other.sender = "other@example.org";
        QCOMPARE(matchingDelivery(db, contact, other, true), qint64(0));
        other      = p.entries.first();
        other.body = "another delivery";
        QCOMPARE(matchingDelivery(db, contact, other, true), qint64(0));
    }
    void stableMergeKeepsOriginalAndDeduplicates()
    {
        QVERIFY(migrate(db));
        auto provisional = page({ "one" }, false, false);
        QVERIFY(storePage(db, key, "", 1, provisional));
        const auto id = provisional.entries.first().eventId;
        QCOMPARE(scalar("SELECT history_state FROM events").toInt(), int(Unverified));
        QVERIFY(!readPage(db, key, "", 1).found);
        auto stable = page({ "one" }, true);
        QVERIFY(storePage(db, key, "", 1, stable));
        QCOMPARE(stable.entries.first().eventId, id);
        QCOMPARE(scalar("SELECT count(*) FROM events").toInt(), 1);
        QVERIFY(readPage(db, key, "", 1).complete);
        QVERIFY(storePage(db, key, "", 1, provisional));
        QVERIFY(!readPage(db, key, "", 1).found); // Never reuse older checkpoint after unstable response.
        QVERIFY((scalar("SELECT history_state FROM events").toInt() & ProvenanceMask) == Archived);
    }
    void mergeLegacyIdentity()
    {
        QVERIFY(migrate(db));
        auto      p = page({ "one" }, true);
        QSqlQuery q(db);
        QVERIFY(q.exec("INSERT INTO contacts VALUES(1,'account-a',1,'you@example.org',-1)"));
        q.prepare("INSERT INTO events(id,contact_id,date,type,direction,m_text,subject,extra_data) "
                  "VALUES(42,1,:date,1,2,'hello','',:extra)");
        q.bindValue(":date", p.entries.first().timestamp);
        q.bindValue(":extra", p.entries.first().extra);
        QVERIFY(q.exec());
        QVERIFY(storePage(db, key, "", 1, p));
        QCOMPARE(p.entries.first().eventId, qint64(42));
        QCOMPARE(scalar("SELECT count(*) FROM events").toInt(), 1);
    }
    void ambiguousOriginalsArePreserved()
    {
        QVERIFY(migrate(db));
        auto      p = page({ "one" });
        QSqlQuery q(db);
        QVERIFY(q.exec("INSERT INTO contacts VALUES(1,'account-a',1,'you@example.org',-1)"));
        q.prepare("INSERT INTO events(contact_id,date,type,direction,m_text,subject,extra_data) "
                  "VALUES(1,:date,1,2,'hello','',:extra)");
        q.bindValue(":date", p.entries.first().timestamp);
        q.bindValue(":extra", p.entries.first().extra);
        QVERIFY(q.exec());
        QVERIFY(q.exec());
        QVERIFY(storePage(db, key, "", 1, p));
        QCOMPARE(scalar("SELECT count(*) FROM events").toInt(), 3);
        QCOMPARE(scalar("SELECT count(*) FROM events WHERE history_state=0").toInt(), 2);
    }
    void invalidPageIsAtomic()
    {
        QVERIFY(migrate(db));
        auto p = page({ "one", "one" });
        QVERIFY(!storePage(db, key, "", 2, p));
        QCOMPARE(p.entries.first().eventId, qint64(0));
        QCOMPARE(scalar("SELECT count(*) FROM events").toInt(), 0);
        QCOMPARE(scalar("SELECT count(*) FROM contacts").toInt(), 0);
        QVERIFY(!readPage(db, key, "", 2).found);
    }
    void deletionInvalidatesWholePage()
    {
        QVERIFY(migrate(db));
        auto p = page({ "one", "two" }, true);
        QVERIFY(storePage(db, key, "boundary", 2, p));
        QSqlQuery q(db);
        QVERIFY(q.exec("DELETE FROM events WHERE id=1"));
        QVERIFY(!readPage(db, key, "boundary", 2).found);
        auto empty = page({}, true);
        QVERIFY(storePage(db, key, "begin", 2, empty));
        QVERIFY(readPage(db, key, "begin", 2).found);
        QVERIFY(invalidate(db, key.account, key.peer));
        QVERIFY(!readPage(db, key, "begin", 2).found);
    }
    void privacyOmissionDoesNotCertifyPage()
    {
        QVERIFY(migrate(db));
        auto p                    = page({ "one", "private" }, true);
        p.entries.last().storable = false;
        QVERIFY(storePage(db, key, "", 2, p));
        QCOMPARE(scalar("SELECT count(*) FROM events").toInt(), 1);
        QVERIFY(!readPage(db, key, "", 2).found);
    }
    void publicRoomIsNotPrivateOccupantHistory()
    {
        QVERIFY(migrate(db));
        auto room              = key;
        room.room              = true;
        room.peer              = "room@example.org";
        room.archive           = room.peer;
        auto p                 = page({ "one" }, true);
        p.entries.first().type = 9;
        QVERIFY(storePage(db, room, "", 1, p));
        QCOMPARE(scalar("SELECT type FROM contacts").toInt(), 3);
        QCOMPARE(scalar("SELECT type FROM events").toInt(), 9);
        QCOMPARE(scalar("SELECT count(*) FROM contacts WHERE type=2").toInt(), 0);
    }
};
QTEST_GUILESS_MAIN(HistoryArchiveTest)
#include "HistoryArchiveTest.moc"
