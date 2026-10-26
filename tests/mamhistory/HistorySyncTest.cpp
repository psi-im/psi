/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historysync.h"
#include "historymessageextras.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtCrypto>
#include <QtTest>
#include <iris/xmpp.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_clientstream.h>
#include <iris/xmpp_encryption.h>
#include <iris/xmpp_tasks.h>
using namespace XMPP;

class ConnectorStub final : public Connector {
public:
    void        setOptHostPort(const QString &, quint16) override { }
    void        connectToServer(const QString &) override { }
    ByteStream *stream() const override { return nullptr; }
    void        done() override { }
};
class Wire final : public ClientStream {
public:
    explicit Wire(Connector *c) : ClientStream(c) { }
    QList<QDomElement> sent;
    void write(const Stanza &stanza) override { sent.append(stanza.element().cloneNode(true).toElement()); }
};
struct Fixture {
    ConnectorStub       connector;
    Wire                wire { &connector };
    Client              client;
    QSqlDatabase        db;
    QList<QDomDocument> documents;
    bool                online = true;
    HistoryArchive::Key key { "account", "alice@example.test", "bob@example.test", false };
    explicit Fixture(const QString &path = QStringLiteral(":memory:"))
    {
        client.start("example.test", "alice", {}, "desktop");
        client.connectToServer(&wire, Jid("alice@example.test/desktop"));
        db = QSqlDatabase::addDatabase("QSQLITE", "sync-test");
        db.setDatabaseName(path);
        if (!db.open())
            qFatal("Cannot open test database");
        QSqlQuery q(db);
        if (!q.exec("PRAGMA foreign_keys=ON"))
            qFatal("Cannot enable database integrity checks");
        if (!db.tables().isEmpty())
            return;
        for (const auto &sql :
             { "PRAGMA foreign_keys=ON", "CREATE TABLE system(key TEXT,value TEXT)",
               "INSERT INTO system VALUES('version','0.1')",
               "CREATE TABLE contacts(id INTEGER PRIMARY KEY,acc_id TEXT,type INTEGER,jid TEXT,lifetime INTEGER)",
               "CREATE TABLE events(id INTEGER PRIMARY KEY,contact_id INTEGER REFERENCES contacts(id),resource "
               "TEXT,date TEXT,type INTEGER,direction INTEGER,subject TEXT,m_text TEXT,lang TEXT,extra_data TEXT)" })
            if (!q.exec(sql))
                qFatal("Cannot initialize test database");
        if (!HistoryArchive::migrate(db))
            qFatal("Cannot migrate test database");
    }
    ~Fixture()
    {
        db.close();
        db = {};
        QSqlDatabase::removeDatabase("sync-test");
    }
    QDomElement xml(const QString &text)
    {
        QDomDocument doc;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        if (!doc.setContent(text, QDomDocument::ParseOption::UseNamespaceProcessing))
            qFatal("Invalid test XML");
#else
        if (!doc.setContent(text, true))
            qFatal("Invalid test XML");
#endif
        documents.append(doc);
        return documents.last().documentElement();
    }
    QDomElement result(const QString &uid, int second, const QString &payload = "<body>hello</body>",
                       const QString &id = {})
    {
        const auto stamp
            = QDateTime::fromString("2026-10-18T10:00:00Z", Qt::ISODate).addSecs(second).toString(Qt::ISODate);
        return xml(QString("<result xmlns='urn:xmpp:mam:2' id='%1'><forwarded xmlns='urn:xmpp:forward:0'>"
                           "<delay xmlns='urn:xmpp:delay' stamp='%2'/><message xmlns='jabber:client' type='chat' "
                           "from='bob@example.test/phone' to='alice@example.test/desktop' "
                           "id='%3'>%4</message></forwarded></result>")
                       .arg(uid, stamp, id.isEmpty() ? uid : id, payload));
    }
    void seed(const QString &before = {})
    {
        HistoryArchive::Page  page;
        HistoryArchive::Entry entry;
        entry.uid       = "seed";
        entry.timestamp = QDateTime::fromString("2026-10-18T10:00:00Z", Qt::ISODate);
        entry.body      = "hello";
        entry.messageId = "seed";
        entry.sender    = "bob@example.test/phone";
        Message original;
        original.setId("seed");
        original.setFrom(Jid(entry.sender));
        original.setTo(Jid("alice@example.test/desktop"));
        original.setType(Message::Type::Chat);
        entry.extra      = HistoryMessageExtras::encode(original);
        const auto   raw = result("seed", 0);
        QDomDocument doc;
        doc.appendChild(doc.importNode(raw, true));
        entry.xml    = doc.toString(-1);
        page.entries = { entry };
        page.stable = page.complete = true;
        if (!HistoryArchive::storePage(db, key, before, 40, page))
            qFatal("Cannot seed archive");
    }
    HistorySync::Context context()
    {
        HistorySync::Context c;
        c.accountId = key.account;
        c.owner     = Jid(client.jid().bare()); // PsiAccount::jid() has no resource.
        c.client    = &client;
        c.connected = [this] { return online; };
        c.logging   = [] { return true; };
        c.read      = [this](const HistoryArchive::Key &k, const QString &before, int limit) {
            return HistoryArchive::readPage(db, k, before, limit);
        };
        c.merge = [this](const HistoryArchive::Key &k, const QString &before, int limit, HistoryArchive::Page &p) {
            return HistoryArchive::storePage(db, k, before, limit, p);
        };
        c.readIndex = [this](const HistoryArchive::Key &k, int index, int limit, int snapshot) {
            return HistoryArchive::readIndex(db, k, index, limit, snapshot);
        };
        c.readAfter = [this](const HistoryArchive::Key &k, const QString &after, int limit, int snapshot) {
            return HistoryArchive::readAfter(db, k, after, limit, snapshot);
        };
        c.lookup = [this](const HistoryArchive::Key &, const QString &uid, const QDateTime &) {
            HistorySync::CachedMessage cached;
            QSqlQuery                  q(db);
            q.prepare("SELECT e.m_text,e.extra_data,e.date FROM archive_items i JOIN events e ON e.id=i.event_id WHERE "
                      "i.uid=:uid");
            q.bindValue(":uid", uid);
            if (q.exec() && q.next()) {
                const auto extra = QJsonDocument::fromJson(q.value(1).toString().toUtf8()).object();
                if (extra.contains("psi:decoded") && !extra.value("psi:decoded").toBool())
                    return cached;
                cached.message.setBody(q.value(0).toString());
                cached.message.setType(Message::Type::Chat);
                HistoryMessageExtras::restore(q.value(1).toString(), cached.message);
                cached.message.setTimeStamp(q.value(2).toDateTime());
                cached.protocolOnly = QJsonDocument::fromJson(q.value(1).toString().toUtf8())
                                          .object()
                                          .value("psi:protocol-only")
                                          .toBool();
            }
            return cached;
        };
        c.ownPlaintext
            = [this](const HistoryArchive::Key &k, const QString &id, const QString &sender, const QDateTime &time) {
                  HistorySync::CachedMessage cached;
                  const auto                 eventId = HistoryArchive::matchingOwnPlaintext(db, k, id, sender, time);
                  QSqlQuery                  q(db);
                  q.prepare("SELECT m_text,extra_data FROM events WHERE id=:id");
                  q.bindValue(":id", eventId);
                  if (eventId && q.exec() && q.next()) {
                      cached.message.setType(Message::Type::Chat);
                      cached.message.setBody(q.value(0).toString());
                      HistoryMessageExtras::restore(q.value(1).toString(), cached.message);
                      cached.message.setFrom(Jid(sender));
                      cached.message.setTo(Jid(k.peer));
                      cached.message.setTimeStamp(time);
                      cached.protocolOnly = !cached.message.displayedMarkerId().isEmpty();
                  }
                  return cached;
              };
        return c;
    }
    bool logOwnMessage(bool marker, const QString &target = QStringLiteral("previous-message"))
    {
        QSqlQuery q(db);
        if (!q.exec("INSERT OR IGNORE INTO contacts VALUES(1,'account',1,'bob@example.test',-1)"))
            return false;
        Message message;
        message.setId("own-id");
        message.setType(Message::Type::Chat);
        message.setFrom(Jid(client.jid().bare())); // Legacy outgoing logging.
        message.setTo(Jid(key.peer));
        message.setWasEncrypted(true);
        if (marker)
            message.setDisplayedMarkerId(target);
        else
            message.setBody("sent before restart");
        q.prepare("INSERT INTO events(contact_id,date,history_date_ms,type,direction,m_text,extra_data) "
                  "VALUES(1,:date,:time,1,1,:body,:extra)");
        const auto time = QDateTime::fromString("2026-10-18T10:00:01Z", Qt::ISODate);
        q.bindValue(":date", marker ? QDateTime() : time);
        q.bindValue(":time", marker ? qint64(0) : time.toMSecsSinceEpoch());
        q.bindValue(":body", marker ? QStringLiteral("") : message.body());
        q.bindValue(":extra", HistoryMessageExtras::encode(message));
        if (!q.exec())
            return false;
        const auto id = q.lastInsertId().toLongLong();
        return HistoryArchive::recordRevision(db, id, 1, 1, 1, message.from().full(), message.id(), {})
            && HistoryArchive::recordMessageStates(db, { id });
    }
    bool reply(const QDomElement &request, const QList<QDomElement> &results, bool complete = true, int firstIndex = -1,
               int count = -1)
    {
        QString    first, last;
        const auto qid = request.firstChildElement("query").attribute("queryid");
        for (const auto &raw : results) {
            if (first.isEmpty())
                first = raw.attribute("id");
            last = raw.attribute("id");
            QDomDocument doc;
            auto         message = doc.createElement("message");
            doc.appendChild(message);
            message.setAttribute("from", key.archive);
            auto result = doc.importNode(raw, true).toElement();
            result.setAttribute("queryid", qid);
            message.appendChild(result);
            documents.append(doc);
            if (!client.rootTask()->take(message))
                return false;
        }
        return client.rootTask()->take(xml(
            QString("<iq from='%1' type='result' id='%2'><fin xmlns='urn:xmpp:mam:2' complete='%3'>"
                    "<set xmlns='http://jabber.org/protocol/rsm'><first%6>%4</first><last>%5</last>%7</set></fin></iq>")
                .arg(key.archive, request.attribute("id"), complete ? "true" : "false", first, last,
                     firstIndex < 0 ? QString() : QString(" index='%1'").arg(firstIndex),
                     count < 0 ? QString() : QString("<count>%1</count>").arg(count))));
    }
};

class ArchiveCipher final : public EncryptionMethod {
public:
    int          decryptions = 0;
    bool         replay      = false;
    bool         fail        = false;
    QString      id() const override { return "test-archive"; }
    QString      name() const override { return id(); }
    Capabilities capabilities() const override { return XmppStanza; }
    Features     features() const override { return {}; }
    bool         canDecrypt(const QDomElement &element) const override
    {
        return !element.firstChildElement("encrypted").isNull();
    }
    class Session final : public EncryptedSession {
    public:
        ArchiveCipher *method;
        Session(ArchiveCipher *m, const EncryptionContext &c) : EncryptedSession(m->id(), c), method(m) { }
        EncryptionJob *decrypt(const QDomElement &stanza) override
        {
            ++method->decryptions;
            method->replay = context().isArchiveReplay();
            auto *job      = new EncryptionJob(this);
            if (method->fail) {
                job->fail(EncryptionJob::Error::NoRecipients, "Controlled missing-key error");
                return job;
            }
            QDomDocument doc;
            auto         plain = doc.importNode(stanza, true).toElement();
            doc.appendChild(plain);
            plain.removeChild(plain.firstChildElement("encrypted"));
            auto body = doc.createElementNS("jabber:client", "body");
            body.appendChild(doc.createTextNode("decrypted"));
            plain.appendChild(body);
            QTimer::singleShot(0, job, [job, plain] { job->complete(plain); });
            return job;
        }
    };
    EncryptedSession *startSession(Capabilities, const EncryptionContext &context) override
    {
        return new Session(this, context);
    }
};

class HistorySyncTest final : public QObject {
    Q_OBJECT
private slots:
    void cachedPageWithoutRsmMetadataDoesNotProbeArchive()
    {
        Fixture f;
        f.seed("edge");
        auto context = f.context();
        context.readSnapshot = [&f](const HistoryArchive::Key &key) { return HistoryArchive::readSnapshot(f.db, key); };
        HistorySync task(context, f.key.peer, false, "edge");
        QSignalSpy  done(&task, &HistorySync::finished);
        task.go();
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(task.success());
        QVERIFY(task.fromCache());
        QCOMPARE(f.wire.sent.size(), 0); // No metadata-only MAM request on a cached page.
        QCOMPARE(task.resultCount(), -1);
        QCOMPARE(task.firstResultIndex(), -1);
        QCOMPARE(task.archiveSnapshot().lastUid, QString());
        QCOMPARE(task.archive().size(), 1);
        QCOMPARE(task.archive().first().attribute("id"), QString("seed"));
        HistorySync again(context, f.key.peer, false, "edge");
        QSignalSpy  againDone(&again, &HistorySync::finished);
        again.go();
        QTRY_COMPARE(againDone.size(), 1);
        QCOMPARE(f.wire.sent.size(), 0);
    }
    void ignoredIndexCannotPolluteCachedHistory()
    {
        Fixture f;
        f.seed();
        HistorySync::Query query { HistorySync::Query::Index, {}, 500, 12, 1000 };
        HistorySync        task(f.context(), query, f.key.peer, false);
        QSignalSpy         done(&task, &HistorySync::finished);
        task.go();
        QTRY_COMPARE(f.wire.sent.size(), 1);
        QVERIFY(f.reply(f.wire.sent.last(), { f.result("wrong-beginning", 1) }, false, -1, 1000));
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(!task.success());
        QSqlQuery q(f.db);
        QVERIFY(q.exec("SELECT count(*) FROM events"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1); // Only the original seed remains.
        QCOMPARE(HistoryArchive::readPage(f.db, f.key, "", 40).entries.last().uid, QString("seed"));
    }
    void omittedCountKeepsAnOlderActualSnapshotAsAnEstimate()
    {
        Fixture    f;
        const auto observed = QDateTime::currentDateTimeUtc().addSecs(-10);
        QVERIFY(HistoryArchive::storeSnapshot(f.db, f.key, { 900, "old-tail", observed, true }));
        auto context         = f.context();
        context.readSnapshot = [&f](const HistoryArchive::Key &key) { return HistoryArchive::readSnapshot(f.db, key); };
        HistorySync::Query query { HistorySync::Query::After, "old-tail", -1, 1 };
        HistorySync        task(context, query, f.key.peer, false);
        QSignalSpy         done(&task, &HistorySync::finished);
        task.go();
        QTRY_COMPARE(f.wire.sent.size(), 1);
        QVERIFY(f.reply(f.wire.sent.last(), { f.result("new-tail", 1) }, true));
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(task.success());
        QCOMPARE(task.resultCount(), -1); // This response gave no count.
        QCOMPARE(task.archiveSnapshot().resultCount, 900);
        QCOMPARE(task.archiveSnapshot().lastUid, QString("old-tail"));
        QCOMPARE(task.archiveSnapshot().observedAt, observed);
    }
    void cachedSnapshotWithoutCountDoesNotTriggerMetadataRequests()
    {
        Fixture f;
        f.seed("edge");
        auto context = f.context();
        context.readSnapshot = [&f](const HistoryArchive::Key &key) { return HistoryArchive::readSnapshot(f.db, key); };
        HistorySync task(context, f.key.peer, false, "edge");
        QSignalSpy  done(&task, &HistorySync::finished);
        task.go();
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(task.success());
        QVERIFY(task.fromCache());
        QCOMPARE(f.wire.sent.size(), 0);
        QVERIFY(!HistoryArchive::readSnapshot(f.db, f.key).observedAt.isValid());
        HistorySync again(context, f.key.peer, false, "edge");
        QSignalSpy  againDone(&again, &HistorySync::finished);
        again.go();
        QTRY_COMPARE(againDone.size(), 1);
        QCOMPARE(f.wire.sent.size(), 0);
    }

    void indexedQueryPersistsAnIslandWithoutReplacingTailCheckpoint()
    {
        Fixture f;
        f.seed();
        HistorySync::Query query { HistorySync::Query::Index, {}, 400, 9 };
        HistorySync        sync(f.context(), query, f.key.peer, false);
        QSignalSpy         done(&sync, &HistorySync::finished);
        sync.go();
        QTRY_COMPARE(f.wire.sent.size(), 1);
        const auto request = f.wire.sent.last();
        const auto rsm     = request.firstChildElement("query").firstChildElement("set");
        QCOMPARE(rsm.firstChildElement("index").text(), QString("400"));
        QCOMPARE(rsm.firstChildElement("max").text(), QString("9"));
        QVERIFY(f.reply(request, { f.result("middle", 400) }, false, 400, 1000));
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(sync.success());
        QCOMPARE(sync.resultCount(), 1000);
        QCOMPARE(sync.firstResultIndex(), 400);
        QCOMPARE(sync.archive().size(), 1);
        QVERIFY(!sync.isComplete());
        QTest::qWait(10);
        QCOMPARE(f.wire.sent.size(), 1);
        const auto tail = HistoryArchive::readPage(f.db, f.key, "", 40);
        QVERIFY(tail.found && tail.complete);
        QCOMPARE(tail.entries.last().uid, QString("seed"));
        QVERIFY(sync.eventId("middle") > 0);
    }

    void verifiedIndexedPageCanBeReopenedAtAnotherSizeWithoutNetwork()
    {
        Fixture            f;
        HistorySync::Query query { HistorySync::Query::Index, {}, 400, 9, 1000 };
        HistorySync        first(f.context(), query, f.key.peer, false);
        QSignalSpy         done(&first, &HistorySync::finished);
        first.go();
        QTRY_COMPARE(f.wire.sent.size(), 1);
        QList<QDomElement> results;
        for (int i = 0; i < 9; ++i)
            results << f.result(QString("middle-%1").arg(i), 400 + i);
        QVERIFY(f.reply(f.wire.sent.last(), results, false, 400, 1000));
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(first.success());
        query.index = 402;
        query.limit = 3;
        HistorySync cached(f.context(), query, f.key.peer, false);
        QSignalSpy  replayed(&cached, &HistorySync::finished);
        cached.go();
        QTRY_COMPARE(replayed.size(), 1);
        QVERIFY(cached.success() && cached.fromCache());
        QCOMPARE(f.wire.sent.size(), 1);
        QCOMPARE(cached.firstResultIndex(), 402);
        QCOMPARE(cached.archive().size(), 3);
        QCOMPARE(cached.archive().first().attribute("id"), QString("middle-2"));
        // The opposite edge also reuses certified UID adjacency.
        HistorySync::Query forward { HistorySync::Query::After, "middle-4", -1, 3, 1000 };
        HistorySync        next(f.context(), forward, f.key.peer, false);
        QSignalSpy         advanced(&next, &HistorySync::finished);
        next.go();
        QTRY_COMPARE(advanced.size(), 1);
        QVERIFY(next.success() && next.fromCache());
        QCOMPARE(f.wire.sent.size(), 1);
        QCOMPARE(next.archive().first().attribute("id"), QString("middle-5"));
        QVERIFY(!HistoryArchive::readIndex(f.db, f.key, 402, 3, 1001).found);
    }

    void explicitForwardPageIsBoundedAndDoesNotPretendToReachArchiveBeginning()
    {
        Fixture f;
        f.seed();
        HistorySync::Query query { HistorySync::Query::After, "seed", -1, 12 };
        HistorySync        sync(f.context(), query, f.key.peer, false);
        QSignalSpy         done(&sync, &HistorySync::finished);
        sync.go();
        QTRY_COMPARE(f.wire.sent.size(), 1);
        const auto request = f.wire.sent.last();
        QCOMPARE(request.firstChildElement("query").firstChildElement("set").firstChildElement("after").text(),
                 QString("seed"));
        QVERIFY(f.reply(request, { f.result("next", 1) }, true, 1, 2));
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(sync.success() && sync.queryComplete() && !sync.isComplete());
        QCOMPARE(sync.archive().size(), 1);
        QCOMPARE(f.wire.sent.size(), 1);
        QCOMPARE(HistoryArchive::readPage(f.db, f.key, "", 40).entries.last().uid, QString("seed"));
    }

    void reopenFetchesOnlyNewCorrectionAndThenNothing()
    {
        Fixture f;
        f.seed();
        HistorySync sync(f.context(), f.key.peer, false, {});
        QSignalSpy  done(&sync, &HistorySync::finished);
        sync.go();
        QTRY_COMPARE(f.wire.sent.size(), 1);
        const auto request = f.wire.sent.last();
        QCOMPARE(request.firstChildElement("query").firstChildElement("set").firstChildElement("after").text(),
                 QString("seed"));
        QCOMPARE(request.firstChildElement("query").firstChildElement("set").firstChildElement("max").text(),
                 QString("40"));
        QVERIFY(f.reply(
            request,
            { f.result("edit-uid", 1, "<body>corrected</body><replace xmlns='urn:xmpp:message-correct:0' id='seed'/>",
                       "edit-id"),
              f.result("receipt-uid", 2, "<received xmlns='urn:xmpp:receipts' id='edit-id'/>") }));
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(sync.success());
        QCOMPARE(sync.archive().size(), 3);
        const auto rows = HistoryArchive::localRecords(f.db, f.key.account, f.key.peer, 0, 40);
        QCOMPARE(rows.first().value("m_text").toString(), QString("corrected"));
        QCOMPARE(sync.message("receipt-uid").messageReceipt(), ReceiptReceived);
        HistorySync again(f.context(), f.key.peer, false, {});
        QSignalSpy  repeated(&again, &HistorySync::finished);
        again.go();
        QTRY_COMPARE(f.wire.sent.size(), 2);
        const auto second = f.wire.sent.last();
        QCOMPARE(second.firstChildElement("query").firstChildElement("set").firstChildElement("after").text(),
                 QString("receipt-uid"));
        QVERIFY(f.reply(second, {}));
        QTRY_COMPARE(repeated.size(), 1);
        QVERIFY(again.success());
        QCOMPARE(again.archive().size(), 3);
    }
    void largeCatchupFetchesOneDeltaThenLatestPagePreservingCoverage()
    {
        Fixture f;
        f.seed();
        HistorySync sync(f.context(), f.key.peer, false, {});
        QSignalSpy  done(&sync, &HistorySync::finished);
        sync.go();
        QTRY_COMPARE(f.wire.sent.size(), 1);
        QList<QDomElement> results;
        for (int i = 1; i <= 40; ++i)
            results.append(f.result(QString("u%1").arg(i), i));
        QVERIFY(f.reply(f.wire.sent.last(), results, false));
        QTRY_COMPARE(f.wire.sent.size(), 2);
        const auto set = f.wire.sent.last().firstChildElement("query").firstChildElement("set");
        QVERIFY(set.firstChildElement("after").isNull());
        QVERIFY(!set.firstChildElement("before").isNull());
        QList<QDomElement> latest;
        for (int i = 1001; i <= 1040; ++i)
            latest.append(f.result(QString("u%1").arg(i), i));
        QVERIFY(f.reply(f.wire.sent.last(), latest, false));
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(sync.success());
        QCOMPARE(sync.archive().size(), 40);
        QCOMPARE(sync.archive().first().attribute("id"), QString("u1001"));
        QCOMPARE(f.wire.sent.size(), 2); // Never scan the intervening 960 events.
        const auto older = HistoryArchive::readPage(f.db, f.key, "u2", 40);
        QVERIFY(older.found);
        QVERIFY(older.complete);
        QCOMPARE(older.entries.size(), 2);
        QCOMPARE(older.entries.first().uid, QString("seed"));
    }
    void expiredCursorFallsBackToOneFreshTail()
    {
        Fixture f;
        f.seed();
        HistorySync sync(f.context(), f.key.peer, false, {});
        QSignalSpy  done(&sync, &HistorySync::finished);
        sync.go();
        QTRY_COMPARE(f.wire.sent.size(), 1);
        const auto request = f.wire.sent.last();
        QVERIFY(f.client.rootTask()->take(f.xml(
            QString("<iq from='alice@example.test' type='error' id='%1'>"
                    "<error type='cancel'><item-not-found xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/></error></iq>")
                .arg(request.attribute("id")))));
        QTRY_COMPARE(f.wire.sent.size(), 2);
        QVERIFY(!f.wire.sent.last()
                     .firstChildElement("query")
                     .firstChildElement("set")
                     .firstChildElement("before")
                     .isNull());
        QVERIFY(f.reply(f.wire.sent.last(), { f.result("new-tail", 10) }));
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(sync.success());
        QCOMPARE(sync.archive().size(), 1);
    }
    void offlineCacheNeedsNoQuery()
    {
        Fixture f;
        f.seed();
        f.online = false;
        f.client.close();
        HistorySync sync(f.context(), f.key.peer, false, {});
        QSignalSpy  done(&sync, &HistorySync::finished);
        sync.go();
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(sync.success());
        QVERIFY(sync.fromCache());
        QVERIFY(f.wire.sent.isEmpty());
    }
    void ourOwnArchivedCiphertextUsesTheSendingDevicesPlaintext()
    {
        Fixture f;
        auto   *cipher = new ArchiveCipher;
        cipher->setParent(&f.client);
        QVERIFY(f.client.encryptionManager()->registerMethod(cipher));
        auto context = f.context();
        context.ownPlaintext
            = [](const HistoryArchive::Key &, const QString &id, const QString &sender, const QDateTime &) {
                  HistorySync::CachedMessage cached;
                  if (id != "own-id")
                      return cached;
                  cached.message.setId(id);
                  cached.message.setFrom(Jid(sender));
                  cached.message.setTo(Jid("bob@example.test/phone"));
                  cached.message.setType(Message::Type::Chat);
                  cached.message.setBody("sent once");
                  cached.message.setWasEncrypted(true);
                  return cached;
              };
        HistorySync sync(context, f.key.peer, false, {});
        QSignalSpy  done(&sync, &HistorySync::finished);
        sync.go();
        QTRY_COMPARE(f.wire.sent.size(), 1);
        auto raw    = f.result("own-cipher", 1, "<encrypted xmlns='urn:psi:test:cipher'/>", "own-id");
        auto stanza = raw.firstChildElement("forwarded").firstChildElement("message");
        stanza.setAttribute("from", f.client.jid().full());
        stanza.setAttribute("to", "bob@example.test/phone");
        QVERIFY(f.reply(f.wire.sent.last(), { raw }));
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(sync.success());
        QCOMPARE(cipher->decryptions, 0);
        QCOMPARE(sync.message("own-cipher").body(), QString("sent once"));
    }

    void undecodedCiphertextKeepsCoverageAndCanBeRetriedOffline()
    {
        Fixture f;
        auto   *cipher = new ArchiveCipher;
        cipher->setParent(&f.client);
        cipher->fail = true;
        QVERIFY(f.client.encryptionManager()->registerMethod(cipher));
        HistorySync sync(f.context(), f.key.peer, false, {});
        QSignalSpy  done(&sync, &HistorySync::finished);
        sync.go();
        QTRY_COMPARE(f.wire.sent.size(), 1);
        QVERIFY(f.reply(f.wire.sent.last(), { f.result("cipher", 1, "<encrypted xmlns='urn:psi:test:cipher'/>") }));
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(sync.success());
        QCOMPARE(cipher->decryptions, 1);
        QVERIFY(HistoryArchive::readPage(f.db, f.key, "", 40).found);
        QSqlQuery q(f.db);
        QVERIFY(q.exec("SELECT m_text FROM events"));
        QVERIFY(q.next());
        QVERIFY(q.value(0).toString().isEmpty());
        q.finish();
        f.online = false;
        f.client.close();
        cipher->fail = false;
        HistorySync retry(f.context(), f.key.peer, false, {});
        QSignalSpy  loaded(&retry, &HistorySync::finished);
        retry.go();
        QTRY_COMPARE(loaded.size(), 1);
        QVERIFY(retry.success());
        QCOMPARE(cipher->decryptions, 2);
        QCOMPARE(retry.message("cipher").body(), QString("decrypted"));
        QCOMPARE(f.wire.sent.size(), 1);
    }

    void peersClientIdCannotSelectOurOwnPlaintext()
    {
        Fixture f;
        auto   *cipher = new ArchiveCipher;
        cipher->fail   = true;
        cipher->setParent(&f.client);
        QVERIFY(f.client.encryptionManager()->registerMethod(cipher));
        auto context = f.context();
        int  lookups = 0;
        context.ownPlaintext
            = [&lookups](const HistoryArchive::Key &, const QString &, const QString &, const QDateTime &) {
                  ++lookups;
                  return HistorySync::CachedMessage {};
              };
        HistorySync sync(context, f.key.peer, false, {});
        QSignalSpy  done(&sync, &HistorySync::finished);
        sync.go();
        QTRY_COMPARE(f.wire.sent.size(), 1);
        QVERIFY(f.reply(f.wire.sent.last(),
                        { f.result("peer-cipher", 1, "<encrypted xmlns='urn:psi:test:cipher'/>", "own-id") }));
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(sync.success());
        QCOMPARE(lookups, 0);
        QCOMPARE(cipher->decryptions, 1);
    }

    void outgoingPlaintextCannotBeAppliedToAnotherConversation()
    {
        Fixture f;
        QVERIFY(f.logOwnMessage(false));
        auto *cipher = new ArchiveCipher;
        cipher->fail = true;
        cipher->setParent(&f.client);
        QVERIFY(f.client.encryptionManager()->registerMethod(cipher));
        HistorySync sync(f.context(), f.key.peer, false, {});
        QSignalSpy  done(&sync, &HistorySync::finished);
        sync.go();
        QTRY_COMPARE(f.wire.sent.size(), 1);
        auto raw    = f.result("wrong-peer", 1, "<encrypted xmlns='urn:psi:test:cipher'/>", "own-id");
        auto stanza = raw.firstChildElement("forwarded").firstChildElement("message");
        stanza.setAttribute("from", f.client.jid().full());
        stanza.setAttribute("to", "charlie@example.test");
        QVERIFY(f.reply(f.wire.sent.last(), { raw }));
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(!sync.success());
        QVERIFY(sync.archive().isEmpty());
        QSqlQuery q(f.db);
        QVERIFY(q.exec("SELECT count(*) FROM archive_items"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 0);
    }

    void archiveDecryptionUsesReplayAndCachedPlaintext()
    {
        Fixture f;
        auto   *cipher = new ArchiveCipher;
        cipher->setParent(&f.client);
        QVERIFY(f.client.encryptionManager()->registerMethod(cipher));
        HistorySync sync(f.context(), f.key.peer, false, {});
        QSignalSpy  done(&sync, &HistorySync::finished);
        sync.go();
        QTRY_COMPARE(f.wire.sent.size(), 1);
        QVERIFY(f.reply(f.wire.sent.last(), { f.result("cipher", 1, "<encrypted xmlns='urn:psi:test:cipher'/>") }));
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(sync.success());
        QCOMPARE(cipher->decryptions, 1);
        QVERIFY(cipher->replay);
        QCOMPARE(sync.message("cipher").body(), QString("decrypted"));
        f.online = false;
        f.client.close();
        HistorySync cached(f.context(), f.key.peer, false, {});
        QSignalSpy  loaded(&cached, &HistorySync::finished);
        cached.go();
        QTRY_COMPARE(loaded.size(), 1);
        QVERIFY(cached.success());
        QCOMPARE(cipher->decryptions, 1);
        QCOMPARE(cached.message("cipher").body(), QString("decrypted"));
    }

    void sentDisplayedCursorSurvivesRestartOutsideThePreloadedPage()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("history.db");
        {
            Fixture f(path);
            f.seed();
            QVERIFY(f.logOwnMessage(true, "seed")); // Legacy marker has no timestamp.
            QVERIFY(f.db.transaction());
            QSqlQuery q(f.db);
            q.prepare("INSERT INTO events(contact_id,type,direction,date,history_date_ms,m_text,extra_data) "
                      "VALUES(1,1,2,:date,:time,'','{}')");
            for (int i = 0; i < 200; ++i) {
                const auto time = QDateTime::fromString("2026-10-18T10:00:00Z", Qt::ISODate).addSecs(100 + i);
                q.bindValue(":date", time);
                q.bindValue(":time", time.toMSecsSinceEpoch());
                QVERIFY(q.exec());
            }
            QVERIFY(f.db.commit());
        }
        {
            Fixture f(path);
            QCOMPARE(HistoryArchive::localRecords(f.db, f.key.account, f.key.peer, 0, 40).size(), 40);
            const auto cursor = HistoryArchive::sentDisplayedCursor(f.db, f.key.account, f.key.peer);
            QCOMPARE(cursor.messageId, QString("seed"));
            QCOMPARE(cursor.time, QDateTime::fromString("2026-10-18T10:00:00Z", Qt::ISODate));
            QVERIFY(cursor.eventId > 0);
        }
    }

    void indexedPageRetainsMessageStateAfterDatabaseAndClientRestart()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("history.db");
        HistorySync::Query seek { HistorySync::Query::Index, {}, 400, 1, 1000 };
        {
            Fixture f(path);
            HistorySync original(f.context(), seek, f.key.peer, false);
            QSignalSpy originalDone(&original, &HistorySync::finished);
            original.go();
            QTRY_COMPARE(f.wire.sent.size(), 1);
            QVERIFY(f.reply(f.wire.sent.last(), { f.result("original-uid", 400, "<body>old text</body>", "original") },
                            false, 400, 1000));
            QTRY_COMPARE(originalDone.size(), 1);
            QVERIFY(original.success());
            HistorySync tail(f.context(), f.key.peer, false, {}, nullptr, 2);
            QSignalSpy tailDone(&tail, &HistorySync::finished);
            tail.go();
            QTRY_COMPARE(f.wire.sent.size(), 2);
            QVERIFY(f.reply(f.wire.sent.last(), {
                f.result("edit-uid", 415, "<body>edited text</body><replace xmlns='urn:xmpp:message-correct:0' id='original'/>", "edit"),
                f.result("retraction-uid", 420, "<retract xmlns='urn:xmpp:message-retract:1' id='original'/>", "retraction")
            }, false));
            QTRY_COMPARE(tailDone.size(), 1);
            QVERIFY(tail.success());
            QVERIFY(f.logOwnMessage(true, "original"));
        }
        {
            Fixture f(path);
            HistorySync reopened(f.context(), seek, f.key.peer, false);
            QSignalSpy done(&reopened, &HistorySync::finished);
            reopened.go();
            QTRY_COMPARE(done.size(), 1);
            QVERIFY(reopened.success() && reopened.fromCache());
            QVERIFY(f.wire.sent.isEmpty());
            QCOMPARE(reopened.archive().size(), 1); // No correction/retraction/marker on this page.
            const auto row = HistoryArchive::resolvedRecord(f.db, reopened.eventId("original-uid"));
            QCOMPARE(row.value("m_text").toString(), QString("edited text"));
            QCOMPARE(row.value("message_flags").toInt(),
                     int(MessageState::Displayed | MessageState::Edited | MessageState::Retracted));
        }
    }

    void receivedPlaintextSurvivesDatabaseAndClientRestart()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("history.db");
        {
            Fixture f(path);
            auto   *cipher = new ArchiveCipher;
            cipher->setParent(&f.client);
            QVERIFY(f.client.encryptionManager()->registerMethod(cipher));
            HistorySync sync(f.context(), f.key.peer, false, {});
            QSignalSpy  done(&sync, &HistorySync::finished);
            sync.go();
            QTRY_COMPARE(f.wire.sent.size(), 1);
            QVERIFY(f.reply(f.wire.sent.last(), { f.result("cipher", 1, "<encrypted xmlns='urn:psi:test:cipher'/>") }));
            QTRY_COMPARE(done.size(), 1);
            QVERIFY(sync.success());
            QCOMPARE(cipher->decryptions, 1);
        }
        {
            Fixture f(path);
            auto   *cipher = new ArchiveCipher;
            cipher->fail   = true; // A consumed ratchet key is no longer usable.
            cipher->setParent(&f.client);
            QVERIFY(f.client.encryptionManager()->registerMethod(cipher));
            f.online = false;
            HistorySync sync(f.context(), f.key.peer, false, {});
            QSignalSpy  done(&sync, &HistorySync::finished);
            sync.go();
            QTRY_COMPARE(done.size(), 1);
            QVERIFY(sync.success() && sync.fromCache());
            QCOMPARE(sync.message("cipher").body(), QString("decrypted"));
            QCOMPARE(cipher->decryptions, 0);
            QVERIFY(f.wire.sent.isEmpty());
        }
    }

    void outgoingPlaintextAndMarkersSurviveDatabaseAndClientRestart_data()
    {
        QTest::addColumn<bool>("marker");
        QTest::addColumn<bool>("failedCache");
        QTest::newRow("outgoing-text") << false << false;
        QTest::newRow("outgoing-displayed") << true << false;
        QTest::newRow("repair-text") << false << true;
        QTest::newRow("repair-displayed") << true << true;
    }
    void outgoingPlaintextAndMarkersSurviveDatabaseAndClientRestart()
    {
        QFETCH(bool, marker);
        QFETCH(bool, failedCache);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("history.db");
        {
            Fixture f(path);
            if (failedCache) {
                auto *cipher = new ArchiveCipher;
                cipher->fail = true;
                cipher->setParent(&f.client);
                QVERIFY(f.client.encryptionManager()->registerMethod(cipher));
                HistorySync sync(f.context(), f.key.peer, false, {});
                QSignalSpy  done(&sync, &HistorySync::finished);
                sync.go();
                QTRY_COMPARE(f.wire.sent.size(), 1);
                auto raw    = f.result("own-cipher", 1, "<encrypted xmlns='urn:psi:test:cipher'/>", "own-id");
                auto stanza = raw.firstChildElement("forwarded").firstChildElement("message");
                stanza.setAttribute("from", "alice@example.test/previous-resource");
                stanza.setAttribute("to", f.key.peer);
                QVERIFY(f.reply(f.wire.sent.last(), { raw }));
                QTRY_COMPARE(done.size(), 1);
                QVERIFY(sync.success());
            }
            // Reproduce a separately logged successful outgoing delivery,
            // alongside the existing failed archive projection when present.
            QVERIFY(f.logOwnMessage(marker));
        }
        {
            Fixture f(path);
            if (failedCache) {
                // Local preload runs before HistorySync. It must also find
                // the saved plaintext, rather than showing an error row for
                // a protocol event that the MAM replay will later recover.
                const auto records = HistoryArchive::localRecords(f.db, f.key.account, f.key.peer, 0, 40);
                QCOMPARE(records.size(), 2);
                for (const auto &record : records) {
                    const auto extra = QJsonDocument::fromJson(record.value("extra_data").toString().toUtf8()).object();
                    QVERIFY(!extra.value("psi:decryption-error").toBool());
                    if (marker)
                        QCOMPARE(extra.value("psi:events").toObject().value("displayed").toString(),
                                 QString("previous-message"));
                    else
                        QCOMPARE(record.value("m_text").toString(), QString("sent before restart"));
                }
            }
            auto *cipher = new ArchiveCipher;
            cipher->fail = true; // The sending device cannot decrypt its own ciphertext.
            cipher->setParent(&f.client);
            QVERIFY(f.client.encryptionManager()->registerMethod(cipher));
            f.online = !failedCache;
            HistorySync sync(f.context(), f.key.peer, false, {});
            QSignalSpy  done(&sync, &HistorySync::finished);
            sync.go();
            if (!failedCache) {
                QTRY_COMPARE(f.wire.sent.size(), 1);
                auto raw    = f.result("own-cipher", 1, "<encrypted xmlns='urn:psi:test:cipher'/>", "own-id");
                auto stanza = raw.firstChildElement("forwarded").firstChildElement("message");
                stanza.setAttribute("from", f.client.jid().full());
                stanza.setAttribute("to", f.key.peer);
                QVERIFY(f.reply(f.wire.sent.last(), { raw }));
            }
            QTRY_COMPARE(done.size(), 1);
            QVERIFY(sync.success());
            QCOMPARE(cipher->decryptions, 0);
            QCOMPARE(sync.protocolOnly("own-cipher"), marker);
            if (marker)
                QCOMPARE(sync.message("own-cipher").displayedMarkerId(), QString("previous-message"));
            else
                QCOMPARE(sync.message("own-cipher").body(), QString("sent before restart"));
            if (failedCache)
                QVERIFY(f.wire.sent.isEmpty());
            QSqlQuery q(f.db);
            QVERIFY(q.exec("SELECT e.extra_data FROM archive_items i JOIN events e ON e.id=i.event_id"));
            QVERIFY(q.next());
            const auto extra = QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();
            QVERIFY(extra.value("psi:decoded").toBool());
            QVERIFY(!extra.value("psi:decryption-error").toBool());
        }
    }
};
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCA::Initializer qca;
    HistorySyncTest  tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "HistorySyncTest.moc"
