/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingstore.h"
#include "historyarchive.h"
#include "historymessageextras.h"
#include <QCryptographicHash>
#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

using namespace XMPP;
class SimsHistoryTest : public QObject {
    Q_OBJECT
    Reference sample(quint64 size = 2343036928ULL)
    {
        MediaSharing media;
        media.file.setName(QString::fromUtf8("Фильм.webm"));
        media.file.setMediaType("video/webm");
        media.file.setSize(size);
        media.file.addHash(Hash(Hash::Sha256, QCryptographicHash::hash("payload", QCryptographicHash::Sha256)));
        media.file.addHash(Hash(Hash::Sha1, QCryptographicHash::hash("payload", QCryptographicHash::Sha1)));
        media.sources << "xmpp:alice@example.org/desktop?jingle-ft";
        Reference reference(Reference::Data, media.sources.first());
        reference.setMediaSharing(media);
        reference.setRange(3, 17);
        reference.setAnchor("urn:example:anchor");
        return reference;
    }
private slots:
    void protocolEventsAndArchiveIdentitiesRoundTrip()
    {
        Message original;
        original.setId("receipt-id");
        original.setFrom(Jid("alice@example.org/phone"));
        original.setMessageReceipt(ReceiptReceived);
        original.setMessageReceiptId("delivered-message");
        original.setReactions({ "target", { QString::fromUtf8("👍") } });
        original.setRetraction("another-target");
        original.setReplaceId("original-id");
        original.setOriginId("origin-id");
        original.addStanzaId({ Jid("alice@example.org"), "server-id" });
        original.setWasEncrypted(true);
        Message restored;
        HistoryMessageExtras::restore(HistoryMessageExtras::encode(original), restored);
        QCOMPARE(restored.messageReceipt(), ReceiptReceived);
        QCOMPARE(restored.messageReceiptId(), QString("delivered-message"));
        QCOMPARE(restored.reactions().targetId, QString("target"));
        QCOMPARE(restored.reactions().reactions, original.reactions().reactions);
        QCOMPARE(restored.retraction(), QString("another-target"));
        QCOMPARE(restored.replaceId(), QString("original-id"));
        QCOMPARE(restored.originId(), QString("origin-id"));
        QCOMPARE(restored.stanzaId(Jid("alice@example.org")).id, QString("server-id"));
        QVERIFY(restored.wasEncrypted());
    }
    void roundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Message original;
        original.setId("message-id");
        original.setFrom(Jid("alice@example.org/desktop"));
        original.setTo(Jid("bob@example.org/laptop"));
        original.setReferences({ sample(9007199254740993ULL) });
        original.urlAdd(Url("https://example.org/movie", "movie"));
        const auto path = dir.filePath("history.db");
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", "history-test");
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery query(db);
            QVERIFY(query.exec("CREATE TABLE events(extra_data TEXT)"));
            query.prepare("INSERT INTO events VALUES(?)");
            query.addBindValue(HistoryMessageExtras::encode(original));
            QVERIFY(query.exec());
        }
        QSqlDatabase::removeDatabase("history-test");
        Message restored;
        restored.setBody("unchanged body");
        restored.setSpooled(true);
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", "history-test");
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery query(db);
            QVERIFY(query.exec("SELECT extra_data FROM events"));
            QVERIFY(query.next());
            HistoryMessageExtras::restore(query.value(0).toString(), restored);
        }
        QSqlDatabase::removeDatabase("history-test");
        QCOMPARE(restored.references().size(), 2);
        const auto reference = restored.references().first();
        QCOMPARE(reference.mediaSharing().file.size().value(), 9007199254740993ULL);
        QCOMPARE(reference.begin(), 3);
        QCOMPARE(reference.end(), 17);
        QCOMPARE(reference.anchor(), QString("urn:example:anchor"));
        QCOMPARE(reference.mediaSharing().file.name(), QString::fromUtf8("Фильм.webm"));
        QCOMPARE(restored.from().full(), original.from().full());
        QCOMPARE(restored.to().full(), original.to().full());
        QCOMPARE(restored.id(), original.id());
        QCOMPARE(restored.body(), QString("unchanged body"));
        QVERIFY(restored.urlList().isEmpty());
        QCOMPARE(restored.references().last().uri(), QString("https://example.org/movie"));
        QCOMPARE(restored.references().last().anchor(), QString("movie"));
        QVERIFY(restored.spooled());
    }
    void edbMigrationPreservesSimsAndLegacyOob()
    {
        QTemporaryDir dir;
        const auto    name = QStringLiteral("sims-edb-migration");
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", name);
            db.setDatabaseName(dir.filePath("history.db"));
            QVERIFY(db.open());
            QSqlQuery query(db);
            QVERIFY(query.exec("PRAGMA foreign_keys=ON"));
            QVERIFY(query.exec("CREATE TABLE system(key TEXT,value TEXT)"));
            QVERIFY(query.exec("INSERT INTO system VALUES('version','0.1')"));
            QVERIFY(query.exec(
                "CREATE TABLE contacts(id INTEGER PRIMARY KEY,acc_id TEXT,type INTEGER,jid TEXT,lifetime INTEGER)"));
            QVERIFY(query.exec("INSERT INTO contacts VALUES(1,'account',1,'alice@example.org',-1)"));
            QVERIFY(query.exec("CREATE TABLE events(id INTEGER PRIMARY KEY,contact_id INTEGER REFERENCES contacts(id),"
                               "resource TEXT,date TEXT,type INTEGER,direction INTEGER,subject TEXT,m_text TEXT,lang "
                               "TEXT,extra_data TEXT)"));
            Message original;
            original.setId("sims-before-migration");
            original.setFrom(Jid("alice@example.org/desktop"));
            original.setTo(Jid("bob@example.org/laptop"));
            original.setReferences({ sample(9007199254740993ULL) });
            original.urlAdd(Url("https://example.org/movie", "movie"));
            const QString encoded = HistoryMessageExtras::encode(original);
            const QString legacy  = QStringLiteral("{\"jabber:x:oob\":[[\"https://example.org/old\",\"Old\"]]}");
            query.prepare("INSERT INTO events(id,contact_id,date,type,direction,m_text,extra_data) "
                          "VALUES(:id,1,'2026-10-16T13:00:00+03:00',1,2,'original body',:extra)");
            query.bindValue(":id", 42);
            query.bindValue(":extra", encoded);
            QVERIFY(query.exec());
            query.bindValue(":id", 43);
            query.bindValue(":extra", legacy);
            QVERIFY(query.exec());
            QVERIFY(HistoryArchive::migrate(db));
            QVERIFY(HistoryArchive::migrate(db));
            QVERIFY(query.exec("SELECT id,m_text,extra_data,history_state FROM events ORDER BY id"));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 42);
            QCOMPARE(query.value(1).toString(), QString("original body"));
            QCOMPARE(query.value(2).toString(), encoded);
            QCOMPARE(query.value(3).toInt(), int(HistoryArchive::Unverified));
            Message restored;
            HistoryMessageExtras::restore(query.value(2).toString(), restored);
            QCOMPARE(restored.id(), original.id());
            QCOMPARE(restored.from().full(), original.from().full());
            QCOMPARE(restored.to().full(), original.to().full());
            QCOMPARE(restored.references().size(), 2);
            QCOMPARE(restored.references().first().mediaSharing().file.size().value(), quint64(9007199254740993ULL));
            QCOMPARE(restored.references().first().mediaSharing().sources,
                     original.references().first().mediaSharing().sources);
            QVERIFY(restored.urlList().isEmpty());
            QCOMPARE(restored.references().last().uri(), QString("https://example.org/movie"));
            QCOMPARE(restored.references().last().anchor(), QString("movie"));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 43);
            QCOMPARE(query.value(2).toString(), legacy);
            Message oldMessage;
            HistoryMessageExtras::restore(query.value(2).toString(), oldMessage);
            QCOMPARE(oldMessage.urlList().size(), 1);
            QVERIFY(!query.next());
        }
        QSqlDatabase::removeDatabase(name);
    }
    void newHistoryNormalizesOobWithoutModifyingIncomingMessage()
    {
        Message incoming;
        const QString url = QStringLiteral("https://example.org/photo.png?size=2");
        incoming.setBody(QStringLiteral("View: ") + url);
        incoming.urlAdd(Url(url + QStringLiteral("#fragment"), QStringLiteral("Original image")));
        incoming.urlAdd(Url(url, QStringLiteral("Duplicate")));
        incoming.urlAdd(Url(QStringLiteral("ftp://example.org/archive"), QStringLiteral("FTP description")));
        const QString encoded = HistoryMessageExtras::encode(incoming);
        QCOMPARE(incoming.urlList().size(), 3);
        QVERIFY(incoming.references().isEmpty());

        Message restored;
        restored.setBody(incoming.body());
        HistoryMessageExtras::restore(encoded, restored);
        QCOMPARE(restored.references().size(), 1);
        QCOMPARE(restored.references().first().uri(), url);
        QCOMPARE(restored.references().first().anchor(), QStringLiteral("Original image"));
        QCOMPARE(restored.references().first().begin(), incoming.body().indexOf(url));
        QCOMPARE(restored.urlList().size(), 1);
        QCOMPARE(restored.urlList().first().url(), QStringLiteral("ftp://example.org/archive"));
        QCOMPARE(restored.urlList().first().desc(), QStringLiteral("FTP description"));
        // Re-encoding already normalized SQLite history never creates a second attachment.
        const QString again = HistoryMessageExtras::encode(restored);
        Message twice;
        twice.setBody(restored.body());
        HistoryMessageExtras::restore(again, twice);
        QCOMPARE(twice.references().size(), 1);
        QCOMPARE(twice.urlList().size(), 1);
    }

    void redundantSimsOobRetainsDescriptionInSavedReference()
    {
        auto reference = sample(7);
        auto media = reference.mediaSharing();
        media.sources = { QStringLiteral("https://example.org/image.png") };
        reference.setMediaSharing(media);
        reference.setAnchor(QString());
        Message incoming;
        incoming.setReferences({ reference });
        incoming.urlAdd(Url(QStringLiteral("https://example.org/image.png#fragment"),
                            QStringLiteral("A useful description")));
        Message restored;
        HistoryMessageExtras::restore(HistoryMessageExtras::encode(incoming), restored);
        QCOMPARE(restored.references().size(), 1);
        QCOMPARE(restored.references().first().anchor(), QStringLiteral("A useful description"));
        QVERIFY(restored.urlList().isEmpty());
        QCOMPARE(incoming.references().first().anchor(), QString()); // shared Message must remain untouched
    }

    void plainMessageIdentity()
    {
        Message original;
        original.setId(QStringLiteral("client-message-42"));
        original.setFrom(Jid(QStringLiteral("room@conference.example/alice")));
        original.setTo(Jid(QStringLiteral("juliet@example/desktop")));

        // Unlike media-bearing messages, ordinary chat messages previously
        // lost their stanza IDs and original full JIDs on SQLite round trips.
        const QString encoded = HistoryMessageExtras::encode(original);
        QVERIFY(!encoded.isEmpty());
        Message restored;
        restored.setBody(QStringLiteral("a plain message"));
        HistoryMessageExtras::restore(encoded, restored);
        QCOMPARE(restored.id(), original.id());
        QCOMPARE(restored.from().full(), original.from().full());
        QCOMPARE(restored.to().full(), original.to().full());
        QCOMPARE(restored.body(), QStringLiteral("a plain message"));
        QVERIFY(restored.references().isEmpty());

        Message empty;
        QVERIFY(HistoryMessageExtras::encode(empty).isEmpty());
    }
    void legacyAndMalformed()
    {
        Message message;
        message.setBody("old message");
        HistoryMessageExtras::restore("{\"jabber:x:oob\":[[\"https://example.org/a\",\"A\"]]}", message);
        QCOMPARE(message.urlList().size(), 1);
        QVERIFY(message.references().isEmpty());
        HistoryMessageExtras::restore("broken json", message);
        HistoryMessageExtras::restore("{\"urn:xmpp:reference:0\":[\"<reference/>\"]}", message);
        QCOMPARE(message.body(), QString("old message"));
        QVERIFY(message.references().isEmpty());

        // The v1 envelope from older media-bearing rows must still load,
        // regardless of which valid references survived parsing.
        HistoryMessageExtras::restore(QStringLiteral("{\"psi:message\":{\"version\":1,\"id\":\"old-id\","
                                                     "\"from\":\"old@example/phone\",\"to\":\"new@example/desktop\"}}"),
                                      message);
        QCOMPARE(message.id(), QStringLiteral("old-id"));
        QCOMPARE(message.from().full(), QStringLiteral("old@example/phone"));
        QCOMPARE(message.to().full(), QStringLiteral("new@example/desktop"));
    }
    void uriLessShareAndFallbackSurviveHistory()
    {
        // XEP-0385 form: no outer reference URI; the body URL is located by
        // XEP-0428 fallbacks instead of reference begin/end.
        auto media = sample(7).mediaSharing();
        media.sources = QStringList { "https://example.org/whiteboard.png" };
        Reference share(Reference::Data, QString());
        share.setMediaSharing(media);
        Message original;
        original.setBody(QString::fromUtf8("Переслано:\n> https://example.org/whiteboard.png\n"));
        original.setReferences({ share });
        original.setFallbacks({ { "urn:xmpp:sims:1", { { 13, 47 } }, {} }, { "urn:xmpp:reply:0", { {} }, {} } });

        Message restored;
        HistoryMessageExtras::restore(HistoryMessageExtras::encode(original), restored);
        QCOMPARE(restored.references().size(), 1);
        QVERIFY(restored.references().first().uri().isEmpty());
        QCOMPARE(restored.references().first().mediaSharing().sources, media.sources);
        const auto sims = restored.bodyFallbacks("urn:xmpp:sims:1");
        QCOMPARE(sims.size(), 1);
        QCOMPARE(sims.first().start, 13);
        QCOMPARE(sims.first().end, 47);
        const auto reply = restored.bodyFallbacks("urn:xmpp:reply:0");
        QCOMPARE(reply.size(), 1);
        QVERIFY(reply.first().isWhole());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FileSharingStore store(dir.filePath("shares.sqlite"));
        QVERIFY(store.remember("alice-account", share, Jid("alice@example.org/desktop")));
        const auto record = store.find("alice-account", media.file.computedHashes().first());
        QVERIFY(record);
        QCOMPARE(record->reference.uri(), media.sources.first());
    }
    void urlContentMappingSurvivesRestartWithoutBecomingContentChecksum()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("shared-files.sqlite"));
        const Hash address(Hash::Sha256,
                           QCryptographicHash::hash("psi-oob-url-v1:https://example.org/file",
                                                    QCryptographicHash::Sha256));
        const Hash actual(Hash::Sha256, QCryptographicHash::hash("bytes", QCryptographicHash::Sha256));
        QVERIFY(!(address == actual));
        {
            FileSharingStore store(path);
            QVERIFY(!store.contentForUrl(QStringLiteral("account-a"), address).has_value());
            QVERIFY(store.rememberUrlContent(QStringLiteral("account-a"), address, actual));
            QCOMPARE(store.contentForUrl(QStringLiteral("account-a"), address).value(), actual);
            QVERIFY(!store.contentForUrl(QStringLiteral("account-b"), address));
            QVERIFY(!store.find(QStringLiteral("account-a"), address));
        }
        {
            FileSharingStore reopened(path);
            QCOMPARE(reopened.contentForUrl(QStringLiteral("account-a"), address).value(), actual);
            QVERIFY(!reopened.contentForUrl(QStringLiteral("account-b"), address));
            QVERIFY(!reopened.find(QStringLiteral("account-a"), address));
        }
    }

    void catalogReopen()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto ref     = sample(7);
        const auto hashes  = ref.mediaSharing().file.computedHashes();
        const auto path    = dir.filePath("shares.sqlite");
        const auto payload = dir.filePath("movie");
        QFile      file(payload);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("payload"), qint64(7));
        file.close();
        {
            FileSharingStore store(path);
            QVERIFY(store.remember("alice-account", ref, Jid("alice@example.org/desktop"), payload));
            auto second   = ref;
            auto media    = second.mediaSharing();
            media.sources = QStringList { "https://example.org/movie" };
            second.setMediaSharing(media);
            QVERIFY(store.remember("alice-account", second, Jid("alice@example.org/mobile")));
        }
        {
            FileSharingStore store(path);
            for (const auto &hash : hashes) {
                const auto record = store.find("alice-account", hash);
                QVERIFY(record);
                QCOMPARE(record->reference.mediaSharing().sources.size(), 2);
                QCOMPARE(record->peers.size(), 2);
                QCOMPARE(record->readableLocalPath(), payload);
                QVERIFY(!store.find("other-account", hash));
            }
            auto conflict = sample(8);
            QVERIFY(!store.remember("alice-account", conflict, Jid()));
            QVERIFY(QFile::remove(payload));
            auto record = store.find("alice-account", hashes.first());
            QVERIFY(record);
            QVERIFY(record->readableLocalPath().isEmpty());
            QCOMPARE(record->reference.mediaSharing().file.size().value(), quint64(7));
        }
    }
};
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    SimsHistoryTest  test;
    return QTest::qExec(&test, argc, argv);
}
#include "simshistory.moc"
