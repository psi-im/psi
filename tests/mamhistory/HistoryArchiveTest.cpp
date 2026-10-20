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
    void confirmedResultRepairsAnUnverifiedProjection()
    {
        QVERIFY(migrate(db));
        auto p                 = page({ "one" }, false, false);
        p.entries.first().body = "provisional";
        QVERIFY(storePage(db, key, "", 1, p));
        p.stable               = true;
        p.entries.first().body = "confirmed";
        QVERIFY(storePage(db, key, "", 1, p));
        QCOMPARE(scalar("SELECT m_text FROM events").toString(), QString("confirmed"));
        QCOMPARE(scalar("SELECT history_state & 3 FROM events").toInt(), int(Archived));
    }

    void adaptiveSizesReuseCertifiedPagesAtUnstoredMucBoundary()
    {
        QVERIFY(migrate(db));
        auto p = page({ "a", "b", "c", "d" });
        QVERIFY(storePage(db, key, "join-history-uid", 4, p));
        auto smaller = readPage(db, key, "join-history-uid", 2);
        QVERIFY(smaller.found && smaller.stable && !smaller.complete);
        QCOMPARE(smaller.entries.size(), 2);
        QCOMPARE(smaller.entries.first().uid, QString("c"));
        QCOMPARE(smaller.entries.last().uid, QString("d"));
        // A larger request can extend across another verified page, without
        // inventing an event for the MUC-history boundary itself.
        auto older = page({ "old-1", "old-2" }, true);
        QVERIFY(storePage(db, key, "a", 2, older));
        auto larger = readPage(db, key, "join-history-uid", 8);
        QVERIFY(larger.found && larger.stable && larger.complete);
        QCOMPARE(larger.entries.size(), 6);
        QCOMPARE(larger.entries.first().uid, QString("old-1"));
        // The tail uses the same geometry-independent coverage rules offline.
        QVERIFY(storePage(db, key, "", 4, p));
        QCOMPARE(readPage(db, key, "", 2).entries.first().uid, QString("c"));
    }

    void forwardTailCheckpointConnectsToAlreadyDownloadedHistory()
    {
        QVERIFY(migrate(db));
        auto p = page({ "one", "two", "three" }, true);
        QVERIFY(storePage(db, key, "", 3, p));
        auto next     = page({ "four", "five", "six" }, false);
        next.previous = "three";
        QVERIFY(storePage(db, key, "", 3, next));
        const auto tail = readPage(db, key, "", 3);
        QVERIFY(tail.found);
        QCOMPARE(tail.entries.last().uid, QString("six"));
        const auto older = readPage(db, key, "four", 3);
        QVERIFY(older.found);
        QCOMPARE(older.entries.first().uid, QString("one"));
        QCOMPARE(older.entries.last().uid, QString("three"));
        QVERIFY(older.complete);
    }

    void ownPlaintextLookupRejectsOtherSendersAndAmbiguousIds()
    {
        QVERIFY(migrate(db));
        auto p                      = page({ "own" }, true);
        p.entries.first().direction = 1;
        p.entries.first().sender    = "me@example.org/desktop";
        p.entries.first().extra
            = "{\"psi:message\":{\"version\":1,\"id\":\"own-id\",\"from\":\"me@example.org/desktop\"}}";
        p.entries.first().messageId = "own-id";
        QVERIFY(storePage(db, key, "", 1, p));
        const auto time = p.entries.first().timestamp;
        QCOMPARE(matchingOwnPlaintext(db, key, "own-id", "me@example.org/desktop", time), p.entries.first().eventId);
        QCOMPARE(matchingOwnPlaintext(db, key, "own-id", "someone@example.org/phone", time), qint64(0));
        auto other    = key;
        other.account = "other";
        QCOMPARE(matchingOwnPlaintext(db, other, "own-id", "me@example.org/desktop", time), qint64(0));
        p.entries.first().uid  = "different-delivery";
        p.entries.first().body = "different body";
        QVERIFY(storePage(db, key, "", 1, p));
        QCOMPARE(matchingOwnPlaintext(db, key, "own-id", "me@example.org/desktop", time), qint64(0));
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
    void revisionsPersistAcrossDisconnectedPages()
    {
        QVERIFY(migrate(db));
        QSqlQuery q(db);
        QVERIFY(q.exec("INSERT INTO contacts VALUES(1,'account-a',1,'you@example.org',-1)"));
        // Edit 8 exists well outside the archive page containing original 6.
        auto insert = [this](int id, int seconds, const QString &stanza,
                             const QString &replace, const QString &body, int direction = 2) {
            QSqlQuery query(db);
            query.prepare("INSERT INTO events(id,contact_id,type,direction,date,history_date_ms,"
                          "m_text,extra_data) VALUES(:id,1,1,:direction,:date,:ms,:body,:extra)");
            const auto stamp=QDateTime::fromMSecsSinceEpoch(1700000000000LL+seconds*1000).toUTC();
            const auto envelope=QJsonObject{{"version",1},{"id",stanza},
                                             {"from","you@example.org/mobile"},
                                             {"to","me@example.org"},{"replace",replace}};
            query.bindValue(":id",id);
            query.bindValue(":direction",direction);
            query.bindValue(":date",stamp);
            query.bindValue(":ms",stamp.toMSecsSinceEpoch());
            query.bindValue(":body",body);
            query.bindValue(":extra",QString::fromUtf8(
                QJsonDocument(QJsonObject{{"psi:message",envelope}}).toJson(QJsonDocument::Compact)));
            return query.exec() && recordRevision(db,id,1,1,direction,
                                                   "you@example.org/mobile",stanza,replace);
        };
        QVERIFY(insert(1,1,"id-6",{},"6"));
        QVERIFY(insert(2,2,"unrelated",{},"other"));
        QVERIFY(insert(3,80,"id-8","id-7","8")); // Later revision before intermediate.
        QVERIFY(insert(4,50,"id-7","id-6","7"));
        const auto page = localRecords(db,key.account,key.peer,0,2);
        QCOMPARE(page.size(),2);
        QCOMPARE(page.first().value("id").toInt(),1);
        QCOMPARE(page.first().value("m_text").toString(),QStringLiteral("8"));
        QCOMPARE(page.last().value("m_text").toString(),QStringLiteral("other"));
        QCOMPARE(scalar("SELECT m_text FROM events WHERE id=1").toString(),QStringLiteral("6"));
        QCOMPARE(resolvedRecord(db,3).value("id").toInt(),1);
        QCOMPARE(resolvedRecord(db,3).value("m_text").toString(),QStringLiteral("8"));
        QVERIFY(resolvedRecord(db, 3).value("revision_date").toDateTime()
                > resolvedRecord(db, 3).value("date").toDateTime());
        const auto extra=QJsonDocument::fromJson(page.first().value("extra_data").toString().toUtf8())
                             .object().value("psi:message").toObject();
        QCOMPARE(extra.value("replace").toString(),QStringLiteral("id-7"));
        QVERIFY(migrate(db));
        QCOMPARE(localRecords(db,key.account,key.peer,0,2).size(),2);
        QVERIFY(insert(5,90,"id-9","id-8","different direction",1));
        QCOMPARE(localRecords(db,key.account,key.peer,0,5).size(),3);
    }

    void mamRevisionsAcrossDistinctCachedPagesResolveToOriginal()
    {
        QVERIFY(migrate(db));
        Page original;
        original.stable=true;
        auto o=entry("uid-original");
        o.body="6";
        o.messageId="client-6";
        original.entries.append(o);
        QVERIFY(storePage(db,key,"uid-after",1,original));

        Page edit;
        edit.stable=true;
        auto e=entry("uid-correction",300);
        e.body="8";
        e.messageId="client-8";
        e.replaceId="client-6";
        const auto envelope=QJsonObject{{"version",1},{"id",e.messageId},
                                         {"from",e.sender},{"to","me@example.org"},
                                         {"replace",e.replaceId}};
        e.extra=QString::fromUtf8(QJsonDocument(QJsonObject{
            {"psi:message",envelope}}).toJson(QJsonDocument::Compact));
        edit.entries.append(e);
        QVERIFY(storePage(db,key,"",1,edit));
        QCOMPARE(resolvedRecord(db,original.entries.first().eventId)
                     .value("m_text").toString(),QStringLiteral("8"));
        QCOMPARE(resolvedRecord(db,edit.entries.first().eventId)
                     .value("id").toLongLong(),original.entries.first().eventId);
        const auto logical=localRecords(db,key.account,key.peer,0,10);
        QCOMPARE(logical.size(),1);
        QCOMPARE(logical.first().value("m_text").toString(),QStringLiteral("8"));
        QCOMPARE(readPage(db,key,"",1).entries.first().uid,QStringLiteral("uid-correction"));
        QCOMPARE(readPage(db,key,"uid-after",1).entries.first().uid,QStringLiteral("uid-original"));
    }

    void migrationBackfillsSavedReplacementEdges()
    {
        QSqlQuery q(db);
        QVERIFY(q.exec("INSERT INTO contacts VALUES(1,'account-a',1,'you@example.org',-1)"));
        auto extra = [](QString id, QString replace) {
            return QString::fromUtf8(QJsonDocument(QJsonObject {
                {"psi:message",QJsonObject{{"version",1},{"id",id},
                 {"replace",replace},{"from","you@example.org/mobile"},
                 {"to","me@example.org"}}}
            }).toJson(QJsonDocument::Compact));
        };
        q.prepare("INSERT INTO events(id,contact_id,type,direction,date,m_text,extra_data)"
                  " VALUES(:id,1,1,2,:stamp,:text,:extra)");
        for (int i=0;i<2;++i) {
            q.bindValue(":id",i+1);
            q.bindValue(":stamp",QDateTime::fromMSecsSinceEpoch(1700000000000LL+i*60000));
            q.bindValue(":text",i ? "8" : "6");
            q.bindValue(":extra",extra(i ? "id-8" : "id-6",i ? "id-6" : QString()));
            QVERIFY(q.exec());
        }
        QVERIFY(migrate(db));
        QCOMPARE(localRecords(db,key.account,key.peer,0,10).size(),1);
        QCOMPARE(localRecords(db,key.account,key.peer,0,10).first().value("m_text").toString(),
                 QStringLiteral("8"));
        QVERIFY(migrate(db));
        QCOMPARE(scalar("SELECT COUNT(*) FROM events").toInt(),2);
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
