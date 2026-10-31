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
    Entry effect(const QString &id, const QString &kind, const QString &target, int direction)
    {
        auto e = entry(id, 100);
        e.direction = direction;
        e.body = QStringLiteral("");
        auto extra = QJsonDocument::fromJson(e.extra.toUtf8()).object();
        extra.insert("psi:decoded", true);
        extra.insert("psi:events", QJsonObject { { kind, target } });
        e.extra = QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact));
        return e;
    }
    int flags(qint64 id) { return resolvedRecord(db, id).value("message_flags").toInt(); }
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
    void archiveSnapshotIsScopedAndDoesNotCertifyOrReplacePages()
    {
        QVERIFY(migrate(db));
        auto cached = page({ "one", "two" });
        QVERIFY(storePage(db, key, "three", 2, cached));
        const auto now = QDateTime::currentDateTimeUtc();
        Snapshot   observed { 12345, "latest-receipt", now, true };
        QVERIFY(storeSnapshot(db, key, observed));
        QCOMPARE(readSnapshot(db, key).resultCount, 12345);
        QCOMPARE(readSnapshot(db, key).lastUid, QString("latest-receipt"));
        QCOMPARE(readPage(db, key, "three", 2).entries.size(), 2);
        QCOMPARE(scalar("SELECT count(*) FROM events").toInt(), 2);
        auto other    = key;
        other.archive = "other-archive";
        QVERIFY(!readSnapshot(db, other).observedAt.isValid());
        other         = key;
        other.account = "other-account";
        QVERIFY(!readSnapshot(db, other).observedAt.isValid());
        QVERIFY(storeSnapshot(db, key, { 10, "obsolete", now.addSecs(-1), true }));
        QCOMPARE(readSnapshot(db, key).resultCount, 12345);
        // Retention/deletion changes the count, without deleting known bodies.
        QVERIFY(storeSnapshot(db, key, { 15, "new-last", now.addSecs(1), false }));
        QCOMPARE(readSnapshot(db, key).resultCount, 15);
        QVERIFY(!readSnapshot(db, key).stable);
        QCOMPARE(readPage(db, key, "three", 2).entries.size(), 2);
        QVERIFY(invalidate(db, key.account, key.peer));
        QVERIFY(!readSnapshot(db, key).observedAt.isValid());
        QCOMPARE(scalar("SELECT count(*) FROM events").toInt(), 2);
    }
    void migrationFromSparseSchemaKeepsBodiesAndCoverage()
    {
        QVERIFY(migrate(db));
        auto cached = page({ "sims-body" });
        QVERIFY(storePage(db, key, "edge", 1, cached));
        QSqlQuery q(db);
        QVERIFY(q.exec("DROP TABLE archive_snapshots"));
        QVERIFY(q.exec("UPDATE system SET value='0.3' WHERE key='version'"));
        QVERIFY(needsMigration(db));
        QVERIFY(migrate(db));
        QVERIFY(!needsMigration(db));
        QCOMPARE(readPage(db, key, "edge", 1).entries.first().uid, QString("sims-body"));
        QCOMPARE(scalar("SELECT count(*) FROM events").toInt(), 1);
    }
    void localDeletionScrubsEveryRevisionAndPreventsMamReplay()
    {
        QVERIFY(migrate(db));
        auto messages = page({ "original", "correction" }, true);
        messages.entries[0].body = QStringLiteral("secret original");
        messages.entries[1].body = QStringLiteral("secret correction");
        messages.entries[1].replaceId = QStringLiteral("original-client");
        QVERIFY(storePage(db, key, "", 2, messages));

        const auto root = messages.entries[0].eventId;
        QCOMPARE(resolvedRecord(db, messages.entries[1].eventId).value("m_text").toString(),
                 QStringLiteral("secret correction"));
        QCOMPARE(localRecords(db, key.account, key.peer, 0, 20).size(), 1);
        QVERIFY(scrubStoredMessage(db, key, root));
        QCOMPARE(localRecords(db, key.account, key.peer, 0, 20).size(), 0);
        QVERIFY(resolvedRecord(db, root).isEmpty());
        QVERIFY(resolvedRecord(db, messages.entries[1].eventId).isEmpty());
        QCOMPARE(scalar("SELECT count(*) FROM events WHERE (message_flags & 16)<>0").toInt(), 2);
        QCOMPARE(scalar("SELECT count(*) FROM events WHERE m_text='' AND extra_data='' "
                        "AND subject='' AND lang='' AND resource=''").toInt(), 2);
        QCOMPARE(scalar("SELECT count(*) FROM archive_items WHERE xml=''").toInt(), 2);
        QCOMPARE(scalar("SELECT count(*) FROM message_revisions").toInt(), 2);
        // Do not discard structural archive indices when hiding content.
        QCOMPARE(scalar("SELECT count(*) FROM archive_page_items").toInt(), 2);

        // A confirmed server replay may refresh ordering metadata but cannot
        // reinsert either version's body or retained raw forwarded stanza.
        messages.entries[0].body = QStringLiteral("resurrected original");
        messages.entries[1].body = QStringLiteral("resurrected correction");
        messages.entries[0].xml  = QStringLiteral("<result>original sensitive XML</result>");
        messages.entries[1].xml  = QStringLiteral("<result>correction sensitive XML</result>");
        QVERIFY(storePage(db, key, "", 2, messages));
        QCOMPARE(scalar("SELECT count(*) FROM events WHERE m_text='' AND extra_data=''").toInt(), 2);
        QCOMPARE(scalar("SELECT count(*) FROM archive_items WHERE xml=''").toInt(), 2);
        QCOMPARE(scalar("SELECT count(*) FROM events WHERE (message_flags & 16)<>0").toInt(), 2);
    }

    void localDeletionDoesNotAffectOtherAccounts()
    {
        QVERIFY(migrate(db));
        auto mine = page({ "same-uid" }, true);
        auto otherKey = key;
        otherKey.account = QStringLiteral("account-b");
        auto other = page({ "same-uid" }, true);
        other.entries[0].body = QStringLiteral("other account's content");
        QVERIFY(storePage(db, key, "", 1, mine));
        QVERIFY(storePage(db, otherKey, "", 1, other));
        QVERIFY(scrubStoredMessage(db, key, mine.entries.first().eventId));
        QCOMPARE(resolvedRecord(db, other.entries.first().eventId).value("m_text").toString(),
                 QStringLiteral("other account's content"));
        QCOMPARE(localRecords(db, otherKey.account, otherKey.peer, 0, 10).size(), 1);
    }

    void scrubbedMessageRetainsIdentityAliases_data()
    {
        QTest::addColumn<bool>("replay");
        QTest::newRow("state-reindex") << false;
        QTest::newRow("MAM-replay") << true;
    }
    void scrubbedMessageRetainsIdentityAliases()
    {
        QFETCH(bool, replay);
        QVERIFY(migrate(db));
        auto messages = page({ "secret" }, true);
        auto extra    = QJsonDocument::fromJson(messages.entries[0].extra.toUtf8()).object();
        extra.insert("psi:origin", QStringLiteral("origin-alias"));
        messages.entries[0].extra = QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact));
        QVERIFY(storePage(db, key, "", 1, messages));
        QCOMPARE(scalar("SELECT count(*) FROM message_state_refs WHERE kind=2 AND target='origin-alias'").toInt(), 1);
        QVERIFY(scrubStoredMessage(db, key, messages.entries[0].eventId));
        if (replay)
            QVERIFY(storePage(db, key, "", 1, messages));
        else
            QVERIFY(backfillMessageStates(db));
        QCOMPARE(scalar("SELECT count(*) FROM message_state_refs WHERE kind=2 AND target='origin-alias'").toInt(), 1);
        QCOMPARE(scalar("SELECT count(*) FROM events WHERE m_text='' AND extra_data=''").toInt(), 1);
        QCOMPARE(scalar("SELECT count(*) FROM archive_items WHERE xml=''").toInt(), 1);
        QVERIFY(resolvedRecord(db, messages.entries[0].eventId).isEmpty());
    }

    void scrubbingRejectsWrongScopeAndProtocolEvents()
    {
        QVERIFY(migrate(db));
        auto messages = page({ "message" }, true);
        QVERIFY(storePage(db, key, "", 1, messages));
        const auto id    = messages.entries[0].eventId;
        auto       wrong = key;
        wrong.account    = QStringLiteral("other-account");
        QVERIFY(!scrubStoredMessage(db, wrong, id));
        wrong      = key;
        wrong.peer = QStringLiteral("other@example.org");
        QVERIFY(!scrubStoredMessage(db, wrong, id));
        wrong      = key;
        wrong.room = true;
        QVERIFY(!scrubStoredMessage(db, wrong, id));
        QVERIFY(!scrubStoredMessage(db, key, 0));
        QVERIFY(!scrubStoredMessage(db, key, id + 100));
        QCOMPARE(resolvedRecord(db, id).value("m_text").toString(), QStringLiteral("hello"));

        auto marker    = page({ });
        marker.entries = { effect("read", "displayed", "message-client", 1) };
        QVERIFY(storePage(db, key, "marker-edge", 1, marker));
        QVERIFY(!scrubStoredMessage(db, key, marker.entries[0].eventId));
        QCOMPARE(flags(id), int(MessageState::Displayed));
        // A failed request must also release the transaction for a valid one.
        QVERIFY(scrubStoredMessage(db, key, id));
        QVERIFY(scrubStoredMessage(db, key, id));
        QCOMPARE(scalar(QStringLiteral("SELECT message_flags FROM events WHERE id=%1").arg(id)).toInt(),
                 int(MessageState::Displayed | MessageState::LocalDeleted));
    }

    void scrubbingRefusesAmbiguousCorrectionTargets()
    {
        QVERIFY(migrate(db));
        auto messages                 = page({ "first", "second", "correction" }, true);
        messages.entries[1].messageId = messages.entries[0].messageId;
        messages.entries[1].extra     = messages.entries[0].extra;
        messages.entries[2].replaceId = messages.entries[0].messageId;
        QVERIFY(storePage(db, key, "", 3, messages));
        QCOMPARE(scalar("SELECT count(*) FROM events").toInt(), 3);
        QVERIFY(!scrubStoredMessage(db, key, messages.entries[2].eventId));
        QCOMPARE(scalar("SELECT count(*) FROM events WHERE m_text='hello'").toInt(), 3);
        QCOMPARE(scalar("SELECT count(*) FROM events WHERE (message_flags & 16)<>0").toInt(), 0);
        QCOMPARE(scalar("SELECT count(*) FROM archive_items WHERE xml<>''").toInt(), 3);
    }

    void scrubbingFailureRollsBackEveryRevision()
    {
        QVERIFY(migrate(db));
        auto messages                 = page({ "original", "correction" }, true);
        messages.entries[1].replaceId = messages.entries[0].messageId;
        QVERIFY(storePage(db, key, "", 2, messages));
        QSqlQuery query(db);
        QVERIFY(query.exec("CREATE TRIGGER reject_scrub BEFORE UPDATE OF xml ON archive_items "
                           "WHEN old.uid='correction' AND new.xml='' "
                           "BEGIN SELECT RAISE(ABORT,'simulated scrub failure'); END"));
        QVERIFY(!scrubStoredMessage(db, key, messages.entries[1].eventId));
        QCOMPARE(scalar("SELECT count(*) FROM events WHERE m_text='hello'").toInt(), 2);
        QCOMPARE(scalar("SELECT count(*) FROM events WHERE (message_flags & 16)<>0").toInt(), 0);
        QCOMPARE(scalar("SELECT count(*) FROM archive_items WHERE xml<>''").toInt(), 2);
        QVERIFY(query.exec("DROP TRIGGER reject_scrub"));
        QVERIFY(scrubStoredMessage(db, key, messages.entries[1].eventId));
        QCOMPARE(scalar("SELECT count(*) FROM events WHERE m_text='' AND extra_data=''").toInt(), 2);
        QCOMPARE(scalar("SELECT count(*) FROM archive_items WHERE xml=''").toInt(), 2);
        QCOMPARE(scalar("SELECT count(*) FROM archive_page_items").toInt(), 2);
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

    void rsmCoordinatesSurviveAdaptiveCacheReadsAndIndexedIslands()
    {
        QVERIFY(migrate(db));
        auto tail        = page({ "tail-0", "tail-1", "tail-2", "tail-3" });
        tail.resultCount = 1000;
        tail.firstIndex  = 996;
        QVERIFY(storePage(db, key, "", 4, tail));
        const auto smaller = readPage(db, key, "", 2);
        QCOMPARE(smaller.resultCount, 1000);
        QCOMPARE(smaller.firstIndex, 998);
        auto island        = page({ "middle-0", "middle-1" });
        island.resultCount = 1000;
        island.firstIndex  = 500;
        island.checkpoint  = false;
        QVERIFY(storePage(db, key, "", 2, island));
        // A random context is not a certified tail page or archive beginning.
        const auto cachedTail = readPage(db, key, "", 4);
        QVERIFY(cachedTail.found);
        QCOMPARE(cachedTail.entries.first().uid, QString("tail-0"));
        QCOMPARE(cachedTail.firstIndex, 996);
        QCOMPARE(scalar("SELECT count(*) FROM events").toInt(), 6);
        QVERIFY(scalar("SELECT previous_uid FROM archive_items WHERE uid='middle-0'").isNull());
    }

    void migrateVersion02PreservesPayloadAndVerifiedCoverage()
    {
        QVERIFY(migrate(db));
        auto saved = page({ "sims-history" }, true);
        saved.entries.first().extra
            = R"({"reference":"<media-sharing xmlns='urn:xmpp:sims:1'/>","plugin-data":"keep"})";
        QVERIFY(storePage(db, key, "", 1, saved));
        const auto eventId = saved.entries.first().eventId;
        QSqlQuery  q(db);
        QVERIFY(q.exec("DROP TABLE archive_index_page_items"));
        QVERIFY(q.exec("DROP TABLE archive_index_pages"));
        QVERIFY(q.exec("DROP TABLE archive_page_items"));
        QVERIFY(q.exec("DROP TABLE archive_pages"));
        QVERIFY(q.exec("CREATE TABLE archive_pages(id INTEGER PRIMARY KEY,account TEXT,archive TEXT,peer TEXT,"
                       "before_uid TEXT,page_size INTEGER,item_count INTEGER,complete INTEGER,stable INTEGER,"
                       "UNIQUE(account,archive,peer,before_uid,page_size))"));
        QVERIFY(
            q.exec("CREATE TABLE archive_page_items(page_id INTEGER REFERENCES archive_pages(id) ON DELETE CASCADE,"
                   "position INTEGER,event_id INTEGER REFERENCES events(id),uid TEXT,PRIMARY KEY(page_id,position))"));
        q.prepare("INSERT INTO archive_pages VALUES(1,:account,:archive,:peer,'',1,1,1,1)");
        q.bindValue(":account", key.account);
        q.bindValue(":archive", key.archive);
        q.bindValue(":peer", key.peer);
        QVERIFY(q.exec());
        q.prepare("INSERT INTO archive_page_items VALUES(1,0,:event,'sims-history')");
        q.bindValue(":event", eventId);
        QVERIFY(q.exec());
        QVERIFY(q.exec("UPDATE system SET value='0.2' WHERE key='version'"));
        QVERIFY(needsMigration(db));
        const auto extra = scalar("SELECT extra_data FROM events").toString();
        QVERIFY(migrate(db));
        QVERIFY(!needsMigration(db));
        QCOMPARE(scalar("SELECT extra_data FROM events").toString(), extra);
        QCOMPARE(scalar("SELECT m_text FROM events").toString(), QString("hello"));
        const auto page = readPage(db, key, "", 1);
        QVERIFY(page.found && page.stable && page.complete);
        QCOMPARE(page.entries.first().eventId, eventId);
        QCOMPARE(page.resultCount, -1);
        QCOMPARE(page.firstIndex, -1);
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
        other      = key;
        other.peer = "someone@example.org";
        QCOMPARE(matchingOwnPlaintext(db, other, "own-id", "me@example.org/desktop", time), qint64(0));
        QCOMPARE(matchingOwnPlaintext(db, key, "own-id", "me@example.org/other-device", time), qint64(0));
        QCOMPARE(matchingOwnPlaintext(db, key, "own-id", "me@example.org/desktop", time.addSecs(301)), qint64(0));
        p.entries.first().uid  = "different-delivery";
        p.entries.first().body = "different body";
        QVERIFY(storePage(db, key, "", 1, p));
        QCOMPARE(matchingOwnPlaintext(db, key, "own-id", "me@example.org/desktop", time), qint64(0));
    }

    void ownPlaintextRecoversLegacyBareSenderAndUndatedMarker()
    {
        QVERIFY(migrate(db));
        auto  p     = page({ "own" }, true);
        auto &e     = p.entries.first();
        e.direction = 1;
        e.sender    = "me@example.org/desktop";
        e.messageId = "own-id";
        e.extra     = "{\"psi:decoded\":true,\"psi:message\":{\"version\":1,\"id\":\"own-id\","
                      "\"from\":\"me@example.org\"}}";
        QVERIFY(storePage(db, key, "", 1, p));
        QCOMPARE(matchingOwnPlaintext(db, key, e.messageId, e.sender, e.timestamp), e.eventId);
        QCOMPARE(matchingOwnPlaintext(db, key, e.messageId, "someone@example.org/desktop", e.timestamp), qint64(0));

        // finishSentMessage used to log encrypted protocol events with a bare
        // sender and no Message timestamp, despite having an event timestamp.
        QSqlQuery q(db);
        auto      extra = QJsonDocument::fromJson(e.extra.toUtf8()).object();
        extra.insert("psi:events", QJsonObject { { "displayed", "previous-message" } });
        q.prepare("UPDATE events SET date=NULL,history_date_ms=0,m_text='',extra_data=:extra");
        q.bindValue(":extra", QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact)));
        QVERIFY(q.exec());
        QCOMPARE(matchingOwnPlaintext(db, key, e.messageId, e.sender, e.timestamp), e.eventId);
        extra.insert("psi:decoded", false);
        q.bindValue(":extra", QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact)));
        QVERIFY(q.exec());
        QCOMPARE(matchingOwnPlaintext(db, key, e.messageId, e.sender, e.timestamp), qint64(0));
    }

    void archivedMarkerAdoptsLegacyUndatedOutgoingDelivery()
    {
        QVERIFY(migrate(db));
        QSqlQuery q(db);
        QVERIFY(q.exec("INSERT INTO contacts VALUES(1,'account-a',1,'you@example.org',-1)"));
        const QString extra = "{\"psi:decoded\":true,\"psi:events\":{\"displayed\":\"previous\"},"
                              "\"psi:message\":{\"version\":1,\"id\":\"own-marker\",\"from\":\"me@example.org\"}}";
        q.prepare("INSERT INTO events(id,contact_id,date,history_date_ms,type,direction,m_text,extra_data) "
                  "VALUES(42,1,NULL,0,1,1,'',:extra)");
        q.bindValue(":extra", extra);
        QVERIFY(q.exec());
        auto  p     = page({ "marker-uid" }, true);
        auto &e     = p.entries.first();
        e.messageId = "own-marker";
        e.sender    = "me@example.org/desktop";
        e.body      = QStringLiteral("");
        e.extra     = extra;
        e.direction = 1;
        QVERIFY(storePage(db, key, "", 1, p));
        QCOMPARE(e.eventId, qint64(42));
        QCOMPARE(scalar("SELECT count(*) FROM events").toInt(), 1);
        QCOMPARE(scalar("SELECT history_date_ms FROM events").toLongLong(), e.timestamp.toMSecsSinceEpoch());
        QVERIFY(scalar("SELECT date FROM events").toDateTime().isValid());
    }

    void failedReplayNeverReplacesSuccessfulProjection_data()
    {
        QTest::addColumn<bool>("stable");
        QTest::addColumn<bool>("marker");
        QTest::addColumn<bool>("replayStable");
        QTest::newRow("live-body") << false << false << true;
        QTest::newRow("archived-body") << true << false << true;
        QTest::newRow("live-marker") << false << true << true;
        QTest::newRow("archived-marker") << true << true << true;
        QTest::newRow("provisional-live-body") << false << false << false;
        QTest::newRow("provisional-archived-body") << true << false << false;
        QTest::newRow("provisional-live-marker") << false << true << false;
        QTest::newRow("provisional-archived-marker") << true << true << false;
    }

    void sentDisplayedCursorUsesIncomingOrderAndRejectsOtherScopes()
    {
        QVERIFY(migrate(db));
        auto messages                 = page({ "first", "second", "outgoing" });
        messages.entries[1].timestamp = messages.entries[0].timestamp.addSecs(10);
        messages.entries[2].direction = 1;
        QVERIFY(storePage(db, key, "message-edge", 3, messages));
        auto              markers = page({ "read-second", "read-first-later", "wrong-target", "peer-marker" });
        const QStringList targets { "second-client", "first-client", "outgoing-client", "outgoing-client" };
        for (int i = 0; i < markers.entries.size(); ++i) {
            auto &e     = markers.entries[i];
            e.direction = i == 3 ? 2 : 1;
            e.timestamp = messages.entries.first().timestamp.addSecs(100 + i);
            e.body      = QStringLiteral("");
            auto extra  = QJsonDocument::fromJson(e.extra.toUtf8()).object();
            extra.insert("psi:decoded", true);
            extra.insert("psi:events", QJsonObject { { "displayed", targets[i] } });
            e.extra = QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact));
        }
        QVERIFY(storePage(db, key, "marker-edge", 4, markers));
        const auto cursor = sentDisplayedCursor(db, key.account, key.peer);
        QCOMPARE(cursor.messageId, QString("second-client"));
        QCOMPARE(cursor.eventId, messages.entries[1].eventId);
        QCOMPARE(cursor.time, messages.entries[1].timestamp);
        QVERIFY(sentDisplayedCursor(db, "other-account", key.peer).messageId.isEmpty());
        QVERIFY(sentDisplayedCursor(db, key.account, "other-peer").messageId.isEmpty());

        // A peer reusing an ID for independent messages makes that target
        // ambiguous. It must not turn either unrelated row into "already read".
        auto duplicate                 = page({ "duplicate" });
        duplicate.entries[0].messageId = "second-client";
        duplicate.entries[0].extra     = messages.entries[1].extra;
        duplicate.entries[0].body      = "independent message";
        QVERIFY(storePage(db, key, "duplicate-edge", 1, duplicate));
        QCOMPARE(sentDisplayedCursor(db, key.account, key.peer).messageId, QString("first-client"));
    }

    void sentDisplayedCursorOnCorrectionKeepsTheOriginalReadPosition()
    {
        QVERIFY(migrate(db));
        auto messages = page({ "original", "unread-next", "correction" });
        messages.entries[2].replaceId = messages.entries[0].messageId;
        QVERIFY(storePage(db, key, "message-edge", 3, messages));
        auto &edited = messages.entries[2];
        auto marker = page({ "own-displayed" });
        auto &entry = marker.entries[0];
        entry.direction = 1;
        entry.body = QStringLiteral("");
        entry.extra = QString::fromUtf8(QJsonDocument(QJsonObject {
            { "psi:events", QJsonObject { { "displayed", edited.messageId } } }
        }).toJson(QJsonDocument::Compact));
        QVERIFY(storePage(db, key, "marker-edge", 1, marker));
        const auto cursor = sentDisplayedCursor(db, key.account, key.peer);
        QCOMPARE(cursor.messageId, messages.entries[0].messageId);
        QCOMPARE(cursor.eventId, messages.entries[0].eventId);
        QCOMPARE(cursor.time, messages.entries[0].timestamp);
        QVERIFY(cursor.time < messages.entries[1].timestamp);
        QCOMPARE(resolvedRecord(db, edited.eventId).value("id").toLongLong(), cursor.eventId);
    }

    void messageMasksAreIndependentOfRequestedPage_data()
    {
        QTest::addColumn<bool>("effectsFirst");
        QTest::newRow("events-before-target") << true;
        QTest::newRow("events-after-target") << false;
    }
    void messageMasksAreIndependentOfRequestedPage()
    {
        QFETCH(bool, effectsFirst);
        QVERIFY(migrate(db));
        auto original = page({ "original" });
        original.entries[0].direction = 1;
        original.requestIndex = original.firstIndex = 123;
        original.resultCount = 1000;
        auto controls = page({});
        controls.entries = { effect("read", "displayed", "original-client", 2),
                             effect("delivered", "receipt", "original-client", 2),
                             effect("retracted", "retract", "original-client", 1) };
        if (effectsFirst)
            QVERIFY(storePage(db, key, "control-edge", 3, controls));
        QVERIFY(storePage(db, key, "original-edge", 1, original));
        auto other = page({ "unread" });
        other.entries[0].direction = 1;
        other.entries[0].timestamp = original.entries[0].timestamp.addSecs(5);
        QVERIFY(storePage(db, key, "other-edge", 1, other));
        auto edit = page({ "edit" });
        edit.entries[0].direction = 1;
        edit.entries[0].replaceId = "original-client";
        edit.entries[0].timestamp = original.entries[0].timestamp.addSecs(10);
        edit.entries[0].body = "edited text";
        QVERIFY(storePage(db, key, "edit-edge", 1, edit));
        if (!effectsFirst)
            QVERIFY(storePage(db, key, "control-edge", 3, controls));

        const auto isolated = readIndex(db, key, 123, 1, 1000);
        QVERIFY(isolated.found);
        QCOMPARE(isolated.entries.size(), 1);
        QCOMPARE(isolated.entries[0].uid, QString("original"));
        QCOMPARE(flags(isolated.entries[0].eventId),
                 int(MessageState::Displayed | MessageState::Delivered | MessageState::Edited | MessageState::Retracted));
        QCOMPARE(resolvedRecord(db, isolated.entries[0].eventId).value("m_text").toString(), QString("edited text"));
        QCOMPARE(flags(other.entries[0].eventId), 0);
        // Derived state does not alter original text or archive coverage.
        QCOMPARE(scalar("SELECT m_text FROM events WHERE id=" + QString::number(original.entries[0].eventId)).toString(),
                 QString("hello"));
        QVERIFY(!needsMigration(db));
    }

    void displayedMaskCoversNewlyLoadedOlderPagesButNotUnreadMessages()
    {
        QVERIFY(migrate(db));
        auto anchor = page({ "anchor" });
        anchor.entries[0].timestamp = anchor.entries[0].timestamp.addSecs(50);
        QVERIFY(storePage(db, key, "anchor-edge", 1, anchor));
        auto marker = page({});
        marker.entries = { effect("own-marker", "displayed", "anchor-client", 1) };
        QVERIFY(storePage(db, key, "marker-edge", 1, marker));
        auto older = page({ "older" });
        QVERIFY(storePage(db, key, "older-edge", 1, older));
        auto newer = page({ "newer" });
        newer.entries[0].timestamp = anchor.entries[0].timestamp.addSecs(10);
        QVERIFY(storePage(db, key, "newer-edge", 1, newer));
        QVERIFY(flags(anchor.entries[0].eventId) & MessageState::Displayed);
        QVERIFY(flags(older.entries[0].eventId) & MessageState::Displayed);
        QCOMPARE(flags(newer.entries[0].eventId), 0);
        // Identical peer/IDs on another account have independent state.
        auto otherKey = key;
        otherKey.account = "other-account";
        auto duplicate = anchor;
        QVERIFY(storePage(db, otherKey, "anchor-edge", 1, duplicate));
        QCOMPARE(flags(duplicate.entries[0].eventId), 0);
    }

    void lateOriginalMovesDisplayedPrefixBackToTheLogicalPosition()
    {
        QVERIFY(migrate(db));
        auto edit = page({ "correction" });
        edit.entries[0].timestamp = edit.entries[0].timestamp.addSecs(10);
        edit.entries[0].replaceId = "original-client";
        QVERIFY(storePage(db, key, "edit-edge", 1, edit));
        auto other = page({ "unread" });
        other.entries[0].timestamp = other.entries[0].timestamp.addSecs(5);
        QVERIFY(storePage(db, key, "other-edge", 1, other));
        auto marker = page({});
        marker.entries = { effect("read-correction", "displayed", "correction-client", 1) };
        QVERIFY(storePage(db, key, "marker-edge", 1, marker));
        QVERIFY(flags(other.entries[0].eventId) & MessageState::Displayed);
        auto original = page({ "original" });
        QVERIFY(storePage(db, key, "original-edge", 1, original));
        QCOMPARE(flags(original.entries[0].eventId), int(MessageState::Displayed | MessageState::Edited));
        QCOMPARE(flags(edit.entries[0].eventId), flags(original.entries[0].eventId));
        QCOMPARE(flags(other.entries[0].eventId), 0);
    }

    void mucRetractionMaskRequiresTheSameAuthorAndOccupant()
    {
        QVERIFY(migrate(db));
        Key roomKey { key.account, "room@example.org", "room@example.org", true };
        auto original = page({ "room-uid" });
        auto &message = original.entries[0];
        message.type = 9;
        message.direction = 1; // MAM and older live logging need not classify our MUC echo alike.
        message.sender = "room@example.org/Alice";
        auto extra = QJsonDocument::fromJson(message.extra.toUtf8()).object();
        extra.insert("psi:occupant", "alice-1");
        message.extra = QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact));
        QVERIFY(storePage(db, roomKey, "original-edge", 1, original));
        const auto retract = [&](const QString &uid, const QString &sender, const QString &occupant) {
            auto p = page({});
            auto e = effect(uid, "retract", "room-uid", 2);
            e.type = 9;
            e.sender = sender;
            auto extra = QJsonDocument::fromJson(e.extra.toUtf8()).object();
            extra.insert("psi:occupant", occupant);
            e.extra = QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact));
            p.entries.append(e);
            return storePage(db, roomKey, uid + "-edge", 1, p);
        };
        QVERIFY(retract("wrong-author", "room@example.org/Bob", "bob-1"));
        QCOMPARE(flags(message.eventId), 0);
        QVERIFY(retract("reused-nickname", "room@example.org/Alice", "alice-2"));
        QCOMPARE(flags(message.eventId), 0);
        QVERIFY(retract("correct-author", "room@example.org/Alice", "alice-1"));
        QCOMPARE(flags(message.eventId), int(MessageState::Retracted));
    }

    void failedReplayCannotEraseMaterializedMessageState()
    {
        QVERIFY(migrate(db));
        auto original = page({ "original" });
        QVERIFY(storePage(db, key, "message-edge", 1, original));
        auto marker = page({});
        marker.entries = { effect("read", "displayed", "original-client", 1),
                           effect("retract", "retract", "original-client", 2) };
        QVERIFY(storePage(db, key, "marker-edge", 2, marker));
        auto failed = original;
        failed.entries[0].extra = "{\"psi:decoded\":false,\"psi:decryption-error\":true}";
        failed.entries[0].body.clear();
        QVERIFY(storePage(db, key, "message-edge", 1, failed));
        QCOMPARE(flags(original.entries[0].eventId), int(MessageState::Displayed | MessageState::Retracted));
    }

    void messageStateFailureRollsBackTheWholeArchivePage()
    {
        QVERIFY(migrate(db));
        auto original = page({ "original" });
        QVERIFY(storePage(db, key, "message-edge", 1, original));
        QSqlQuery query(db);
        QVERIFY(query.exec("CREATE TRIGGER reject_effect BEFORE INSERT ON message_state_refs WHEN new.kind=6 "
                           "BEGIN SELECT RAISE(ABORT,'simulated state write failure'); END"));
        auto retract = page({});
        retract.entries = { effect("retract", "retract", "original-client", 2) };
        QVERIFY(!storePage(db, key, "retract-edge", 1, retract));
        QCOMPARE(retract.entries[0].eventId, qint64(0));
        QCOMPARE(flags(original.entries[0].eventId), 0);
        QCOMPARE(scalar("SELECT COUNT(*) FROM events").toInt(), 1);
        QVERIFY(readPage(db, key, "message-edge", 1).found);
        QVERIFY(!readPage(db, key, "retract-edge", 1).found);
    }

    void failedReplayNeverReplacesSuccessfulProjection()
    {
        QFETCH(bool, stable);
        QFETCH(bool, marker);
        QFETCH(bool, replayStable);
        QVERIFY(migrate(db));
        auto  p     = page({ "decoded" }, true, stable);
        auto &e     = p.entries.first();
        auto  extra = QJsonDocument::fromJson(e.extra.toUtf8()).object();
        extra.insert("psi:decoded", true);
        extra.insert("plugin-data", "retained");
        e.replaceId = "original-message";
        if (marker) {
            e.body = QStringLiteral("");
            extra.insert("psi:events", QJsonObject { { "displayed", "previous-message" } });
            extra.insert("psi:protocol-only", true);
        }
        e.extra = QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact));
        QVERIFY(storePage(db, key, "", 1, p));
        const auto body   = e.body;
        auto       failed = extra;
        failed.remove("psi:events");
        failed.remove("psi:protocol-only");
        failed.insert("psi:decoded", false);
        failed.insert("psi:decryption-error", true);
        e.body = QStringLiteral("");
        e.replaceId.clear(); // Failed ciphertext does not expose correction edges.
        e.extra  = QString::fromUtf8(QJsonDocument(failed).toJson(QJsonDocument::Compact));
        p.stable = replayStable;
        QVERIFY(storePage(db, key, "", 1, p));
        const auto saved
            = QJsonDocument::fromJson(scalar("SELECT extra_data FROM events").toString().toUtf8()).object();
        QCOMPARE(scalar("SELECT m_text FROM events").toString(), body);
        QVERIFY(saved.value("psi:decoded").toBool());
        QVERIFY(!saved.value("psi:decryption-error").toBool());
        QCOMPARE(saved.value("psi:events"), extra.value("psi:events"));
        QCOMPARE(saved.value("plugin-data").toString(), QString("retained"));
        QCOMPARE(scalar("SELECT replace_id FROM message_revisions").toString(), QString("original-message"));
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
        QCOMPARE(scalar("SELECT value FROM system WHERE key='version'").toString(), QString("0.5"));
        QVERIFY(migrate(db));
        QCOMPARE(scalar("SELECT count(*) FROM events").toInt(), 1);
    }

    void migrationFrom04BackfillsMasksAndPlainXmlReferencesAtomically()
    {
        QSqlQuery query(db);
        QVERIFY(query.exec("UPDATE system SET value='0.4' WHERE key='version'"));
        QVERIFY(query.exec("ALTER TABLE events ADD COLUMN history_state INTEGER NOT NULL DEFAULT 0"));
        QVERIFY(query.exec("ALTER TABLE events ADD COLUMN history_date_ms INTEGER"));
        QVERIFY(query.exec("CREATE TABLE archive_items(account TEXT,archive TEXT,peer TEXT,uid TEXT,event_id INTEGER,"
                           "xml TEXT,previous_uid TEXT,next_uid TEXT,stable INTEGER,PRIMARY KEY(account,archive,peer,uid))"));
        QVERIFY(query.exec("INSERT INTO contacts VALUES(1,'account-a',1,'you@example.org',-1)"));
        auto original = entry("original");
        auto edited = entry("edited", 10);
        edited.replaceId = "original-client";
        auto extra = QJsonDocument::fromJson(edited.extra.toUtf8()).object();
        auto envelope = extra.value("psi:message").toObject();
        envelope.insert("replace", edited.replaceId);
        extra.insert("psi:message", envelope);
        edited.extra = QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact));
        auto retracted = effect("retracted", "retract", "original-client", 2);
        query.prepare("INSERT INTO events(id,contact_id,date,history_date_ms,type,direction,m_text,extra_data) "
                      "VALUES(:id,1,:date,:time,1,:direction,:body,:extra)");
        const QList<Entry> rows { original, edited, effect("read", "displayed", "original-client", 1), retracted };
        for (int i = 0; i < rows.size(); ++i) {
            query.bindValue(":id", i + 1);
            query.bindValue(":date", rows[i].timestamp);
            query.bindValue(":time", rows[i].timestamp.toMSecsSinceEpoch());
            query.bindValue(":direction", rows[i].direction);
            query.bindValue(":body", rows[i].body);
            query.bindValue(":extra", i == 2 ? QStringLiteral("{}") : rows[i].extra);
            QVERIFY(query.exec());
        }
        // Older MAM caches sometimes lacked psi:events, retaining only XML.
        const QString raw = "<result><forwarded><message id='read-client' from='me@example.org/desktop'>"
                            "<displayed xmlns='urn:xmpp:chat-markers:0' id='original-client'/>"
                            "</message></forwarded></result>";
        query.prepare("INSERT INTO archive_items(account,archive,peer,uid,event_id,xml,stable) "
                      "VALUES('account-a','me@example.org','you@example.org','read',3,:xml,1)");
        query.bindValue(":xml", raw);
        QVERIFY(query.exec());
        QVERIFY(needsMigration(db));
        QVERIFY(migrate(db));
        QVERIFY(!needsMigration(db));
        QCOMPARE(flags(1), int(MessageState::Displayed | MessageState::Edited | MessageState::Retracted));
        QCOMPARE(flags(2), flags(1));
        QCOMPARE(scalar("SELECT extra_data FROM events WHERE id=1").toString(), original.extra);
        QCOMPARE(scalar("SELECT m_text FROM events WHERE id=1").toString(), original.body);
        QCOMPARE(scalar("SELECT xml FROM archive_items WHERE event_id=3").toString(), raw);
        QCOMPARE(scalar("SELECT COUNT(*) FROM events").toInt(), 4);
        QCOMPARE(sentDisplayedCursor(db, key.account, key.peer).eventId, qint64(1));
        QVERIFY(query.exec("UPDATE events SET message_flags=message_flags | 128 WHERE id=1"));
        QVERIFY(migrate(db));
        QCOMPARE(flags(1), int(MessageState::Displayed | MessageState::Edited | MessageState::Retracted) | 128);
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
