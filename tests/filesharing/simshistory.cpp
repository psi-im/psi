/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingstore.h"
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
        QCOMPARE(restored.references().size(), 1);
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
        QCOMPARE(restored.urlList().size(), 1);
        QVERIFY(restored.spooled());
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
        HistoryMessageExtras::restore(
            QStringLiteral("{\"psi:message\":{\"version\":1,\"id\":\"old-id\","
                           "\"from\":\"old@example/phone\",\"to\":\"new@example/desktop\"}}"), message);
        QCOMPARE(message.id(), QStringLiteral("old-id"));
        QCOMPARE(message.from().full(), QStringLiteral("old@example/phone"));
        QCOMPARE(message.to().full(), QStringLiteral("new@example/desktop"));
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
