/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "historysync.h"
#include "historymessageextras.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QSqlQuery>
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
    Fixture()
    {
        client.start("example.test", "alice", {}, "desktop");
        client.connectToServer(&wire, Jid("alice@example.test/desktop"));
        db = QSqlDatabase::addDatabase("QSQLITE", "sync-test");
        db.setDatabaseName(":memory:");
        if (!db.open())
            qFatal("Cannot open test database");
        QSqlQuery q(db);
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
    void seed()
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
        if (!HistoryArchive::storePage(db, key, "", 40, page))
            qFatal("Cannot seed archive");
    }
    HistorySync::Context context()
    {
        HistorySync::Context c;
        c.accountId = key.account;
        c.owner     = client.jid();
        c.client    = &client;
        c.connected = [this] { return online; };
        c.logging   = [] { return true; };
        c.read      = [this](const HistoryArchive::Key &k, const QString &before, int limit) {
            return HistoryArchive::readPage(db, k, before, limit);
        };
        c.merge = [this](const HistoryArchive::Key &k, const QString &before, int limit, HistoryArchive::Page &p) {
            return HistoryArchive::storePage(db, k, before, limit, p);
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
        return c;
    }
    bool reply(const QDomElement &request, const QList<QDomElement> &results, bool complete = true)
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
        return client.rootTask()->take(
            xml(QString("<iq from='%1' type='result' id='%2'><fin xmlns='urn:xmpp:mam:2' complete='%3'>"
                        "<set xmlns='http://jabber.org/protocol/rsm'><first>%4</first><last>%5</last></set></fin></iq>")
                    .arg(key.archive, request.attribute("id"), complete ? "true" : "false", first, last)));
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
    void multipleForwardPagesPreserveOlderCoverage()
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
        QCOMPARE(
            f.wire.sent.last().firstChildElement("query").firstChildElement("set").firstChildElement("after").text(),
            QString("u40"));
        QVERIFY(f.reply(f.wire.sent.last(), { f.result("u41", 41) }));
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(sync.success());
        QCOMPARE(sync.archive().size(), 40);
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
};
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCA::Initializer qca;
    HistorySyncTest  tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "HistorySyncTest.moc"
