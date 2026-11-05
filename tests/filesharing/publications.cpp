/* SPDX-License-Identifier: GPL-2.0-or-later */
// Publications come from what history stores and what an account chooses to share.
#include "applicationinfo.h"
#include "filedatabase.h"
#include "filesharingcatalog.h"
#include "filesharingpublicationsync.h"
#include "historyarchive.h"
#include "historymediarefs.h"
#include "historymessageextras.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>
#include <iris/xmpp_hash.h>
#include <iris/xmpp_message.h>
#include <memory>

using namespace XMPP;

// The file database needs only this much of a class that brings in the whole application.
static QString profileDirectory;
QString        ApplicationInfo::currentProfileDir(HomedirType) { return profileDirectory; }

namespace {
const QString historyConnection = QStringLiteral("publications-history");
const QString corporate         = QStringLiteral("corporate");
const QString personal          = QStringLiteral("personal");
const Jid     alice("alice@example.org/desktop");
const Jid     bob("bob@example.org/phone");
const Jid     self("me@example.org/desktop");

Hash sha256(const QByteArray &bytes)
{
    return Hash(Hash::Sha256, QCryptographicHash::hash(bytes, QCryptographicHash::Sha256));
}

MediaSharing mediaOf(const QByteArray &content, const QStringList &sources)
{
    MediaSharing media;
    media.file.setName(QString::fromLatin1(content));
    media.file.setMediaType(QStringLiteral("application/octet-stream"));
    media.file.setSize(quint64(content.size()));
    media.file.addHash(sha256(content));
    media.file.addHash(Hash::from(Hash::Sha1, content));
    media.sources = sources;
    return media;
}

Reference referenceOf(const MediaSharing &media)
{
    Reference reference(Reference::Data, media.sources.value(0));
    reference.setMediaSharing(media);
    return reference;
}

MessageAttachments::Attachment attachmentOf(const MediaSharing &media) { return { referenceOf(media), {} }; }

MessageAttachments::Attachment urlAttachment(const QString &url, const QString &description = {})
{
    return { Reference(Reference::Data, url), description };
}

Message messageWith(const Jid &from, const QList<Reference> &references, const QString &id = {})
{
    Message message;
    message.setId(id.isEmpty() ? QUuid::createUuid().toString() : id);
    message.setFrom(from);
    message.setReferences(references);
    return message;
}

QByteArray newId() { return QUuid::createUuid().toRfc4122(); }

// The connection every FileSharingCatalog in the test uses.
QStringList fileConnections()
{
    return QSqlDatabase::connectionNames().filter(QRegularExpression(QStringLiteral("^psi-files:")));
}

void closeFileDatabase()
{
    for (const auto &name : fileConnections()) {
        QSqlDatabase::database(name, false).close();
        QSqlDatabase::removeDatabase(name);
    }
}

void loseFileDatabase()
{
    closeFileDatabase();
    for (const auto &suffix : { "", "-wal", "-shm" })
        QFile::remove(profileDirectory + QStringLiteral("/files.sqlite") + QLatin1String(suffix));
}

QJsonObject legacyDescriptor(const MediaSharing &media, const QStringList &peers, const QJsonObject &local = {})
{
    QDomDocument doc;
    doc.appendChild(referenceOf(media).toXml(&doc));
    return { { "version", 1 },
             { "reference", doc.toString(-1) },
             { "peers", QJsonArray::fromStringList(peers) },
             { "local", local } };
}
}

class PublicationsTest : public QObject {
    Q_OBJECT
    std::unique_ptr<QTemporaryDir>      dir;
    std::unique_ptr<FileSharingCatalog> catalog;
    QSqlDatabase                        history;
    const HistoryArchive::Key key { corporate, QStringLiteral("me@example.org"), QStringLiteral("alice@example.org"),
                                    false };

    void createHistory()
    {
        history = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), historyConnection);
        history.setDatabaseName(dir->filePath(QStringLiteral("history.db")));
        QVERIFY(history.open());
        QSqlQuery query(history);
        QVERIFY(query.exec("PRAGMA foreign_keys=ON"));
        QVERIFY(query.exec("CREATE TABLE system(key TEXT,value TEXT)"));
        QVERIFY(query.exec("INSERT INTO system VALUES('version','0.1')"));
        QVERIFY(query.exec(
            "CREATE TABLE contacts(id INTEGER PRIMARY KEY,acc_id TEXT,type INTEGER,jid TEXT,lifetime INTEGER)"));
        QVERIFY(query.exec("INSERT INTO contacts VALUES(1,'corporate',1,'alice@example.org',-1)"));
        QVERIFY(query.exec("INSERT INTO contacts VALUES(2,'personal',1,'alice@example.org',-1)"));
        QVERIFY(query.exec("CREATE TABLE events(id INTEGER PRIMARY KEY,contact_id INTEGER NOT NULL "
                           "REFERENCES contacts(id) ON DELETE CASCADE,resource TEXT,date TEXT,type INTEGER,"
                           "direction INTEGER,subject TEXT,m_text TEXT,lang TEXT,extra_data TEXT)"));
    }

    void migrateHistory()
    {
        QVERIFY(HistoryArchive::migrate(history));
        QVERIFY(HistoryMediaRefs::migrate(history));
    }

    qint64 insertEvent(const QString &body, const QString &extra, int direction = 2, qint64 contact = 1)
    {
        const auto time = QDateTime::fromMSecsSinceEpoch(1700000000000).toUTC();
        QSqlQuery  query(history);
        query.prepare("INSERT INTO events(contact_id,date,type,direction,m_text,extra_data) "
                      "VALUES(:contact,:date,1,:direction,:body,:extra)");
        query.bindValue(":contact", contact);
        query.bindValue(":date", time);
        query.bindValue(":direction", direction);
        query.bindValue(":body", body);
        query.bindValue(":extra", extra);
        if (!query.exec())
            return 0;
        return query.lastInsertId().toLongLong();
    }

    // What the live path does when logging is on.
    qint64 store(const Message &message, qint64 contact = 1)
    {
        const auto event = insertEvent(message.body(), HistoryMessageExtras::encode(message), 2, contact);
        return event && HistoryMediaRefs::link(history, event) ? event : 0;
    }

    int linkCount() const
    {
        QSqlQuery query(history);
        return query.exec("SELECT COUNT(*) FROM message_media_refs") && query.next() ? query.value(0).toInt() : -1;
    }

    QList<QByteArray> links(qint64 event) const
    {
        QSqlQuery query(history);
        query.prepare("SELECT publication_id FROM message_media_refs WHERE event_id=:event ORDER BY ordinal");
        query.bindValue(":event", event);
        QList<QByteArray> ids;
        if (query.exec())
            while (query.next())
                ids.append(query.value(0).toByteArray());
        return ids;
    }

    int publicationCount() const
    {
        QSqlQuery query(FileDatabase::connection());
        return query.exec("SELECT COUNT(*) FROM publications") && query.next() ? query.value(0).toInt() : -1;
    }

    HistoryArchive::Entry archived(const Message &message, const QString &uid) const
    {
        HistoryArchive::Entry entry;
        entry.uid       = uid;
        entry.xml       = QStringLiteral("<result id='%1'/>").arg(uid);
        entry.timestamp = QDateTime::fromMSecsSinceEpoch(1700000000000).toUTC();
        entry.body      = message.body();
        entry.sender    = message.from().full();
        entry.messageId = message.id();
        entry.extra     = HistoryMessageExtras::encode(message);
        return entry;
    }

    bool storePage(HistoryArchive::Page &page)
    {
        const auto db = history;
        return HistoryArchive::storePage(history, key, QString(), 40, page,
                                         [db](qint64 event) { return HistoryMediaRefs::link(db, event); });
    }

    void syncEverything()
    {
        FileSharingPublicationSync sync(historyConnection, catalog.get());
        sync.reconcile();
        QTRY_VERIFY(sync.isIdle());
    }

private slots:
    void init()
    {
        dir = std::make_unique<QTemporaryDir>();
        QVERIFY(dir->isValid());
        profileDirectory = dir->path();
        catalog          = std::make_unique<FileSharingCatalog>();
        createHistory();
    }

    void cleanup()
    {
        catalog.reset();
        history.close();
        history = QSqlDatabase();
        QSqlDatabase::removeDatabase(historyConnection);
        closeFileDatabase();
    }

    // ------------------------------------------------------------------ the catalog

    void announcementsOfOneFileAreTakenTogether()
    {
        const auto first   = mediaOf("payload", { QStringLiteral("https://one.example/file") });
        const auto second  = mediaOf("payload", { QStringLiteral("https://two.example/file") });
        const auto firstId = newId();
        QVERIFY(catalog->add(firstId, corporate, alice, attachmentOf(first)));
        QVERIFY(catalog->add(newId(), corporate, bob, attachmentOf(second)));
        for (const auto &hash : first.file.computedHashes()) {
            const auto record = catalog->find(corporate, hash);
            QVERIFY(record);
            QCOMPARE(record->media.sources.size(), 2);
            QVERIFY(record->media.sources.contains(first.sources.first()));
            QVERIFY(record->media.sources.contains(second.sources.first()));
            QCOMPARE(record->peers.size(), 2);
            QCOMPARE(record->media.file.name(), QStringLiteral("payload"));
            QVERIFY(!record->urlOnly);
            QVERIFY2(!catalog->find(personal, hash), "what one account was told must not reach another");
        }

        // A deleted announcement takes its source and its peer along.
        catalog->remove({ firstId });
        const auto record = catalog->find(corporate, first.file.computedHashes().first());
        QVERIFY(record);
        QCOMPARE(record->media.sources, second.sources);
        QCOMPARE(record->peers, QStringList { bob.full() });
    }

    void announcementContradictingTheFirstIsAnotherFile()
    {
        const auto first   = mediaOf("payload", { QStringLiteral("https://one.example/file") });
        auto       claimed = mediaOf("payload", { QStringLiteral("https://evil.example/file") });
        claimed.file.setSize(99);
        QVERIFY(catalog->add(newId(), corporate, alice, attachmentOf(first)));
        QVERIFY(catalog->add(newId(), corporate, bob, attachmentOf(claimed)));
        const auto record = catalog->find(corporate, first.file.computedHashes().first());
        QVERIFY(record);
        QCOMPARE(record->media.file.size().value(), quint64(7));
        QCOMPARE(record->media.sources, first.sources);
        QCOMPARE(record->peers, QStringList { alice.full() });
    }

    void aPublicationToldAgainChangesNothing()
    {
        const auto media = mediaOf("payload", { QStringLiteral("https://one.example/file") });
        const auto id    = newId();
        QVERIFY(catalog->add(id, corporate, alice, attachmentOf(media)));
        QVERIFY(catalog->add(id, corporate, alice, attachmentOf(media)));
        QCOMPARE(publicationCount(), 1);
        QCOMPARE(catalog->find(corporate, media.file.computedHashes().first())->media.sources, media.sources);
    }

    void aUrlOnlyFileIsKnownByItsAddress()
    {
        const QString url = QStringLiteral("https://example.org/file.pdf#page=2");
        QVERIFY(catalog->add(newId(), corporate, alice, urlAttachment(url, QStringLiteral("A report"))));
        const auto identity = MessageAttachments::urlIdentity(urlAttachment(url).url());
        const auto record   = catalog->find(corporate, identity);
        QVERIFY(record);
        QVERIFY(record->urlOnly);
        QCOMPARE(record->media.sources, QStringList { url });
        QCOMPARE(record->media.file.description(), QStringLiteral("A report"));
        QCOMPARE(record->peers, QStringList { alice.full() });
        QVERIFY(!catalog->find(personal, identity));
        QVERIFY2(!catalog->isServed(corporate, { identity }), "an address is not a file to serve");
    }

    void fileIsServedOnlyByTheAccountThatSharedIt()
    {
        const auto        media  = mediaOf("payload", {});
        const auto        hashes = media.file.computedHashes();
        const QList<Hash> unknown { sha256("other") };
        QVERIFY(catalog->add(newId(), corporate, alice, attachmentOf(media)));
        QVERIFY2(!catalog->isServed(corporate, hashes), "a remembered descriptor must not be served");

        QVERIFY(catalog->serve(corporate, self, media, {}));
        QVERIFY(catalog->isServed(corporate, hashes));
        QVERIFY(catalog->isServed(corporate, { hashes.last() }));
        QVERIFY2(!catalog->isServed(personal, hashes), "serving must not cross accounts");
        QVERIFY(!catalog->isServed(corporate, unknown));
        QVERIFY(!catalog->isServed(corporate, {}));

        closeFileDatabase();
        FileSharingCatalog reopened;
        QVERIFY(reopened.isServed(corporate, hashes));
        QVERIFY(reopened.serve(personal, self, media, {}));
        reopened.stopServing(corporate, hashes);
        QVERIFY(!reopened.isServed(corporate, hashes));
        QVERIFY(reopened.isServed(personal, hashes));
        QVERIFY2(reopened.find(corporate, hashes.first()), "unsharing leaves what a message announced");
    }

    void servingAgainReplacesThePublication()
    {
        const auto hashes = mediaOf("payload", {}).file.computedHashes();
        QVERIFY(catalog->serve(corporate, self, mediaOf("payload", { QStringLiteral("https://old.example/f") }), {}));
        QVERIFY(catalog->serve(corporate, self, mediaOf("payload", { QStringLiteral("https://new.example/f") }), {}));
        QCOMPARE(publicationCount(), 1);
        QCOMPARE(catalog->find(corporate, hashes.first())->media.sources,
                 QStringList { QStringLiteral("https://new.example/f") });
        QCOMPARE(catalog->find(corporate, hashes.first())->peers, QStringList { self.full() });
    }

    void aLocalCopyIsTrustedOnlyWhileItIsUnchanged()
    {
        const auto media = mediaOf("payload", {});
        const auto path  = dir->filePath(QStringLiteral("shared"));
        QFile      file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("payload"), qint64(7));
        file.close();
        QVERIFY(catalog->serve(corporate, self, media, path));
        QCOMPARE(catalog->find(corporate, media.file.computedHashes().first())->readableLocalPath(), path);

        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("changed bytes"), qint64(13));
        file.close();
        QVERIFY(catalog->find(corporate, media.file.computedHashes().first())->readableLocalPath().isEmpty());

        QVERIFY(file.remove());
        QVERIFY(catalog->find(corporate, media.file.computedHashes().first())->readableLocalPath().isEmpty());

        QVERIFY2(catalog->serve(corporate, self, mediaOf("other size", {}), dir->filePath(QStringLiteral("none"))),
                 "a missing copy does not stop a file being shared by upload");
    }

    void urlContentSurvivesRestartWithoutBecomingAChecksum()
    {
        const auto address = sha256("psi-oob-url-v1:https://example.org/file");
        const auto actual  = sha256("bytes");
        QVERIFY(!catalog->contentForUrl(corporate, address));
        QVERIFY(catalog->rememberUrlContent(corporate, address, actual));
        QCOMPARE(catalog->contentForUrl(corporate, address).value().toString(), actual.toString());
        QVERIFY(!catalog->contentForUrl(personal, address));
        QVERIFY(!catalog->find(corporate, address));

        closeFileDatabase();
        FileSharingCatalog reopened;
        QCOMPARE(reopened.contentForUrl(corporate, address).value().toString(), actual.toString());
        QVERIFY(!reopened.contentForUrl(personal, address));
        reopened.forgetUrlContent(corporate, address);
        QVERIFY(!reopened.contentForUrl(corporate, address));
    }

    void urlContentExpires()
    {
        const auto address = sha256("url");
        QVERIFY(catalog->rememberUrlContent(corporate, address, sha256("bytes")));
        QSqlQuery query(FileDatabase::connection());
        query.prepare("UPDATE url_content SET updated=:old");
        query.bindValue(":old", QDateTime::currentSecsSinceEpoch() - FileSharingCatalog::UrlContentTtl - 1);
        QVERIFY(query.exec());
        FileSharingCatalog reopened;
        QVERIFY(!reopened.contentForUrl(corporate, address));
    }

    // ------------------------------------------------------------------ the file database

    void aDatabaseFromTheFirstSchemaIsUpgradedInPlace()
    {
        loseFileDatabase();
        const auto name = QStringLiteral("schema-one");
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
            db.setDatabaseName(profileDirectory + QStringLiteral("/files.sqlite"));
            QVERIFY(db.open());
            QSqlQuery query(db);
            QVERIFY(query.exec("CREATE TABLE files (id BLOB PRIMARY KEY, partition TEXT NOT NULL, size INTEGER NOT "
                               "NULL, created INTEGER NOT NULL, max_age INTEGER NOT NULL, metadata BLOB NOT NULL)"));
            QVERIFY(query.exec("INSERT INTO files VALUES(x'01','shares',1,1,1,x'')"));
            QVERIFY(query.exec("PRAGMA user_version=1"));
            db.close();
        }
        QSqlDatabase::removeDatabase(name);
        QSqlQuery query(FileDatabase::connection());
        QVERIFY(query.exec("SELECT COUNT(*) FROM files"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
        QVERIFY(query.exec("PRAGMA user_version"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 2);
        QVERIFY(catalog->serve(corporate, self, mediaOf("payload", {}), {}));
    }

    void aDatabaseFromANewerPsiIsLeftAlone()
    {
        loseFileDatabase();
        const auto name = QStringLiteral("schema-next");
        const auto path = profileDirectory + QStringLiteral("/files.sqlite");
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery query(db);
            QVERIFY(query.exec("PRAGMA user_version=99"));
            db.close();
        }
        QSqlDatabase::removeDatabase(name);
        QVERIFY(!FileDatabase::connection().isOpen());
        QVERIFY(!catalog->serve(corporate, self, mediaOf("payload", {}), {}));
        QVERIFY(!catalog->add(newId(), corporate, alice, attachmentOf(mediaOf("payload", {}))));
        QVERIFY(!catalog->find(corporate, sha256("payload")));
        QVERIFY(!catalog->isServed(corporate, { sha256("payload") }));
        QVERIFY(catalog->missing({ newId() }).isEmpty());
        QVERIFY(catalog->announcementsAfter({}, 10).isEmpty());
    }

    // ------------------------------------------------------------------ links in history

    void everyFileOfAStoredMessageIsLinked()
    {
        migrateHistory();
        auto message = messageWith(alice,
                                   { referenceOf(mediaOf("payload", { QStringLiteral("https://one.example/f") })),
                                     Reference(Reference::Mention, QStringLiteral("xmpp:bob@example.org")) });
        message.urlAdd(Url(QStringLiteral("https://two.example/g"), QStringLiteral("Other")));
        message.urlAdd(Url(QStringLiteral("xmpp:room@conference.example.org?join")));
        const auto event = store(message);
        QVERIFY(event);
        QCOMPARE(links(event).size(), 2);

        auto text = messageWith(alice, {});
        text.setBody(QStringLiteral("see https://three.example/h there"));
        text.urlAdd(Url(QStringLiteral("https://three.example/h")));
        QVERIFY(store(text));
        QCOMPARE(linkCount(), 2); // As shown: a URL within other text stays an ordinary link.

        auto bare = messageWith(alice, {});
        bare.setBody(QStringLiteral("https://four.example/i"));
        bare.urlAdd(Url(QStringLiteral("https://four.example/i")));
        const auto bareEvent = store(bare);
        QCOMPARE(links(bareEvent).size(), 1);

        auto plain = messageWith(alice, {});
        plain.setBody(QStringLiteral("nothing attached"));
        QCOMPARE(links(store(plain)).size(), 0);
    }

    void linkingAgainKeepsTheLinks()
    {
        migrateHistory();
        const auto event = store(
            messageWith(alice, { referenceOf(mediaOf("payload", { QStringLiteral("https://one.example/f") })) }));
        const auto before = links(event);
        QCOMPARE(before.size(), 1);
        QVERIFY(HistoryMediaRefs::link(history, event));
        QCOMPARE(links(event), before);
    }

    void aDeletedEventTakesItsLinksAlong()
    {
        migrateHistory();
        const auto event = store(
            messageWith(alice, { referenceOf(mediaOf("payload", { QStringLiteral("https://one.example/f") })) }));
        QCOMPARE(linkCount(), 1);
        QSqlQuery query(history);
        QVERIFY(query.exec(QStringLiteral("DELETE FROM events WHERE id=%1").arg(event)));
        QCOMPARE(linkCount(), 0);
    }

    void anArchivePageLinksWhatItStores()
    {
        migrateHistory();
        const auto message
            = messageWith(alice, { referenceOf(mediaOf("payload", { QStringLiteral("https://one.example/f") })) });
        HistoryArchive::Page page;
        page.stable = true;
        page.entries.append(archived(message, QStringLiteral("uid-1")));
        auto hidden     = archived(messageWith(alice, { referenceOf(mediaOf("hidden", {})) }), QStringLiteral("uid-2"));
        hidden.storable = false;
        page.entries.append(hidden);
        QVERIFY(storePage(page));
        QCOMPARE(links(page.entries.first().eventId).size(), 1);
        QCOMPARE(linkCount(), 1); // A privacy hint keeps its attachment out of storage as well.
    }

    void aMessageDeliveredTwiceHasOneSetOfLinks()
    {
        migrateHistory();
        auto message
            = messageWith(alice, { referenceOf(mediaOf("payload", { QStringLiteral("https://one.example/f") })) },
                          QStringLiteral("stanza-1"));
        message.setBody(QStringLiteral("a file"));
        const auto live = store(message);
        const auto kept = links(live);
        QCOMPARE(kept.size(), 1);

        for (int replay = 0; replay < 2; ++replay) {
            HistoryArchive::Page page;
            page.stable = true;
            page.entries.append(archived(message, QStringLiteral("uid-1")));
            QVERIFY(storePage(page));
            QCOMPARE(page.entries.first().eventId, live);
            QCOMPARE(links(live), kept);
            QCOMPARE(linkCount(), 1);
        }
    }

    void aMessageDecodedLaterIsLinkedThen()
    {
        migrateHistory();
        const auto message
            = messageWith(alice, { referenceOf(mediaOf("payload", { QStringLiteral("https://one.example/f") })) });
        auto undecoded  = archived(message, QStringLiteral("uid-1"));
        undecoded.extra = QStringLiteral("{\"psi:decoded\":false}");
        HistoryArchive::Page encrypted;
        encrypted.stable = true;
        encrypted.entries.append(undecoded);
        QVERIFY(storePage(encrypted));
        QCOMPARE(linkCount(), 0);

        HistoryArchive::Page decoded;
        decoded.stable = true;
        decoded.entries.append(archived(message, QStringLiteral("uid-1")));
        QVERIFY(storePage(decoded));
        QCOMPARE(decoded.entries.first().eventId, encrypted.entries.first().eventId);
        QCOMPARE(links(decoded.entries.first().eventId).size(), 1);
    }

    void aFailedLinkRollsThePageBack()
    {
        migrateHistory();
        HistoryArchive::Page page;
        page.stable = true;
        page.entries.append(
            archived(messageWith(alice, { referenceOf(mediaOf("payload", {})) }), QStringLiteral("uid")));
        QVERIFY(!HistoryArchive::storePage(history, key, QString(), 40, page, [](qint64) { return false; }));
        QSqlQuery query(history);
        QVERIFY(query.exec("SELECT COUNT(*) FROM events"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
        QCOMPARE(page.entries.first().eventId, qint64(0));
    }

    // ------------------------------------------------------------------ following history

    void aMessageThatIsNotStoredLeavesNothing()
    {
        migrateHistory();
        syncEverything();
        QCOMPARE(publicationCount(), 0);
    }

    void publicationsFollowStoredMessages()
    {
        migrateHistory();
        const auto                 media = mediaOf("payload", { QStringLiteral("https://one.example/f") });
        FileSharingPublicationSync sync(historyConnection, catalog.get());
        sync.reconcile();
        QTRY_VERIFY(sync.isIdle());
        QVERIFY(!catalog->find(corporate, media.file.computedHashes().first()));

        QVERIFY(store(messageWith(alice, { referenceOf(media) })));
        sync.linksAdded();
        QTRY_VERIFY(sync.isIdle());
        auto record = catalog->find(corporate, media.file.computedHashes().first());
        QVERIFY(record);
        QCOMPARE(record->media.sources, media.sources);
        QCOMPARE(record->peers, QStringList { alice.full() });
        QVERIFY2(!catalog->find(personal, media.file.computedHashes().first()),
                 "a publication belongs to the account of the conversation");

        // The same file in another message, from someone else.
        const auto other = mediaOf("payload", { QStringLiteral("https://two.example/f") });
        QVERIFY(store(messageWith(bob, { referenceOf(other) })));
        sync.linksAdded();
        QTRY_VERIFY(sync.isIdle());
        record = catalog->find(corporate, media.file.computedHashes().first());
        QCOMPARE(record->media.sources.size(), 2);
        QCOMPARE(record->peers.size(), 2);
        QVERIFY2(!catalog->isServed(corporate, media.file.computedHashes()), "receiving a file shares nothing");
    }

    void aUrlAttachmentFollowsItsMessage()
    {
        migrateHistory();
        const QString url     = QStringLiteral("https://example.org/report.pdf");
        auto          message = messageWith(alice, {});
        message.setBody(url);
        message.urlAdd(Url(url, QStringLiteral("A report")));
        QVERIFY(store(message));
        syncEverything();
        const auto record = catalog->find(corporate, MessageAttachments::urlIdentity(urlAttachment(url).url()));
        QVERIFY(record);
        QVERIFY(record->urlOnly);
        QCOMPARE(record->media.sources, QStringList { url });
        QCOMPARE(record->media.file.description(), QStringLiteral("A report"));
    }

    void aLostFileDatabaseIsToldAgainFromHistory()
    {
        migrateHistory();
        const auto media = mediaOf("payload", { QStringLiteral("https://one.example/f") });
        const auto event = store(messageWith(alice, { referenceOf(media) }));
        syncEverything();
        QVERIFY(catalog->find(corporate, media.file.computedHashes().first()));
        QVERIFY(catalog->serve(corporate, self, media, {}));

        loseFileDatabase();
        QCOMPARE(publicationCount(), 0);
        QVERIFY(!catalog->find(corporate, media.file.computedHashes().first()));

        syncEverything();
        const auto record = catalog->find(corporate, media.file.computedHashes().first());
        QVERIFY(record);
        QCOMPARE(record->media.sources, media.sources);
        QCOMPARE(record->peers, QStringList { alice.full() });
        QVERIFY2(catalog->missing(links(event)).isEmpty(), "history keeps pointing at the same publication");
        QVERIFY2(!catalog->isServed(corporate, media.file.computedHashes()),
                 "what an account shared is its own decision, and history cannot make it");
    }

    void aPublicationNothingLinksToGoesAway()
    {
        migrateHistory();
        const auto kept   = mediaOf("payload", { QStringLiteral("https://kept.example/f") });
        const auto erased = mediaOf("payload", { QStringLiteral("https://erased.example/f") });
        const auto shared = mediaOf("shared", { QStringLiteral("https://shared.example/f") });
        QVERIFY(store(messageWith(alice, { referenceOf(kept) })));
        const auto event = store(messageWith(bob, { referenceOf(erased) }));
        QVERIFY(catalog->serve(corporate, self, shared, {}));
        syncEverything();
        QCOMPARE(catalog->find(corporate, kept.file.computedHashes().first())->media.sources.size(), 2);
        QCOMPARE(publicationCount(), 3);

        QSqlQuery query(history);
        QVERIFY(query.exec(QStringLiteral("DELETE FROM events WHERE id=%1").arg(event)));
        syncEverything();
        const auto record = catalog->find(corporate, kept.file.computedHashes().first());
        QCOMPARE(record->media.sources, kept.sources);
        QCOMPARE(record->peers, QStringList { alice.full() });
        QVERIFY2(catalog->isServed(corporate, shared.file.computedHashes()), "deleting a message does not unshare");
        QCOMPARE(publicationCount(), 2);
    }

    void aHistoryThatCannotAnswerTakesNothingAway()
    {
        migrateHistory();
        const auto media = mediaOf("payload", { QStringLiteral("https://one.example/f") });
        QVERIFY(store(messageWith(alice, { referenceOf(media) })));
        syncEverything();
        QSqlQuery query(history);
        QVERIFY(query.exec("DROP TABLE message_media_refs"));
        syncEverything();
        QVERIFY(catalog->find(corporate, media.file.computedHashes().first()));
    }

    void aLongPassGoesInSteps()
    {
        migrateHistory();
        QVERIFY(history.transaction());
        QList<Hash> hashes;
        for (int i = 0; i < 250; ++i) {
            const auto media = mediaOf(QByteArray::number(i), { QStringLiteral("https://example.org/%1").arg(i) });
            hashes.append(media.file.computedHashes().first());
            QVERIFY(store(messageWith(alice, { referenceOf(media) })));
        }
        QVERIFY(history.commit());
        syncEverything();
        for (const auto &hash : hashes)
            QVERIFY(catalog->find(corporate, hash));
        QCOMPARE(publicationCount(), 250);

        QSqlQuery query(history);
        QVERIFY(query.exec("DELETE FROM events"));
        syncEverything();
        QCOMPARE(publicationCount(), 0);
    }

    void aClosedHistoryStopsTheSync()
    {
        migrateHistory();
        QVERIFY(
            store(messageWith(alice, { referenceOf(mediaOf("payload", { QStringLiteral("https://f.example/") })) })));
        FileSharingPublicationSync sync(historyConnection, catalog.get());
        sync.reconcile();
        history.close();
        QTRY_VERIFY(sync.isIdle());
        QCOMPARE(publicationCount(), 0);
    }

    // ------------------------------------------------------------------ upgrades

    void messagesFromBeforeLinksAreLinkedOnce()
    {
        QVERIFY(HistoryArchive::migrate(history));
        QVERIFY(HistoryMediaRefs::needsMigration(history));
        const auto media    = mediaOf("payload", { QStringLiteral("https://one.example/f") });
        const auto envelope = insertEvent(QStringLiteral("a file"),
                                          HistoryMessageExtras::encode(messageWith(alice, { referenceOf(media) })));
        // Old rows know only their conversation.
        const auto incoming
            = insertEvent(QString(), QStringLiteral("{\"jabber:x:oob\":[[\"https://example.org/old\",\"Old\"]]}"));
        const auto outgoing
            = insertEvent(QString(), QStringLiteral("{\"jabber:x:oob\":[[\"https://example.org/sent\"]]}"), 1);
        const auto plain   = insertEvent(QStringLiteral("hello"), QStringLiteral("{\"psi:decoded\":true}"));
        const auto noExtra = insertEvent(QStringLiteral("hello"), QString());

        qint64 reported = 0;
        QVERIFY(HistoryMediaRefs::migrate(history, [&reported](qint64 id) { reported = id; }));
        QVERIFY(reported >= outgoing);
        QVERIFY(!HistoryMediaRefs::needsMigration(history));
        QCOMPARE(links(envelope).size(), 1);
        QCOMPARE(links(incoming).size(), 1);
        QCOMPARE(links(outgoing).size(), 1);
        QVERIFY(links(plain).isEmpty());
        QVERIFY(links(noExtra).isEmpty());

        const auto kept = links(envelope);
        QVERIFY(HistoryMediaRefs::migrate(history));
        QCOMPARE(links(envelope), kept);

        syncEverything();
        QCOMPARE(catalog->find(corporate, media.file.computedHashes().first())->peers, QStringList { alice.full() });
        const auto old = catalog->find(corporate, MessageAttachments::urlIdentity(QUrl("https://example.org/old")));
        QVERIFY(old);
        QCOMPARE(old->peers, QStringList { QStringLiteral("alice@example.org") });
        const auto sent = catalog->find(corporate, MessageAttachments::urlIdentity(QUrl("https://example.org/sent")));
        QVERIFY(sent);
        QVERIFY(sent->peers.isEmpty());
    }

    void theLegacyCatalogLeavesOnlyWhatWasShared()
    {
        const auto write = [this](const QByteArray &content) {
            const auto path = dir->filePath(QString::fromLatin1(content));
            QFile      file(path);
            if (file.open(QIODevice::WriteOnly))
                file.write(content);
            file.close();
            return QJsonObject { { "path", path },
                                 { "size", QString::number(file.size()) },
                                 { "modified", QString::number(QFileInfo(path).lastModified().toMSecsSinceEpoch()) } };
        };
        const auto own      = mediaOf("own", { QStringLiteral("xmpp:me@example.org/desktop?jingle-ft") });
        const auto noCopy   = mediaOf("no-copy", { QStringLiteral("https://example.org/no-copy") });
        const auto received = mediaOf("received", { QStringLiteral("https://example.org/received") });
        const auto marked   = mediaOf("marked", { QStringLiteral("https://example.org/marked") });
        const auto unmarked = mediaOf("unmarked", { QStringLiteral("https://example.org/unmarked") });
        const auto gone     = mediaOf("gone", { QStringLiteral("https://example.org/gone") });

        const auto path = dir->filePath(QStringLiteral("shared-files.sqlite"));
        const auto name = QStringLiteral("legacy-catalog");
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery query(db);
            QVERIFY(query.exec("CREATE TABLE shared_files (account_id TEXT NOT NULL, hash TEXT NOT NULL, descriptor "
                               "TEXT NOT NULL, PRIMARY KEY(account_id, hash))"));
            QVERIFY(
                query.exec("CREATE TABLE url_content (account_id TEXT NOT NULL, url_key TEXT NOT NULL, "
                           "content_hash TEXT NOT NULL, updated INTEGER NOT NULL, PRIMARY KEY(account_id, url_key))"));
            QVERIFY(query.exec("CREATE TABLE served_files (account_id TEXT NOT NULL, hash TEXT NOT NULL, "
                               "PRIMARY KEY(account_id, hash))"));
            QVERIFY(query.exec("CREATE TABLE served_files_imported (account_id TEXT NOT NULL PRIMARY KEY)"));
            const auto share
                = [&query](const QString &account, const MediaSharing &media, const QJsonObject &descriptor) {
                      const auto json = QString::fromUtf8(QJsonDocument(descriptor).toJson(QJsonDocument::Compact));
                      for (const auto &hash : media.file.computedHashes()) {
                          query.prepare("INSERT INTO shared_files VALUES(:account,:hash,:descriptor)");
                          query.bindValue(":account", account);
                          query.bindValue(":hash", hash.toString());
                          query.bindValue(":descriptor", json);
                          QVERIFY(query.exec());
                      }
                  };
            // An account from before the list of what it serves: recognized by its JID and a copy.
            const auto myPhone = QStringLiteral("me@example.org/phone");
            share(corporate, own, legacyDescriptor(own, { myPhone, alice.full() }, write("own")));
            share(corporate, noCopy, legacyDescriptor(noCopy, { self.full() }));
            share(corporate, received, legacyDescriptor(received, { alice.full() }, write("received")));
            // An account with the list: only that counts.
            share(personal, marked, legacyDescriptor(marked, { alice.full() }));
            share(personal, unmarked, legacyDescriptor(unmarked, { self.full() }, write("unmarked")));
            QVERIFY(query.exec("INSERT INTO served_files_imported VALUES('personal')"));
            query.prepare("INSERT INTO served_files VALUES('personal',:hash)");
            query.bindValue(":hash", marked.file.computedHashes().last().toString());
            QVERIFY(query.exec());
            // An account that no longer exists.
            share(QStringLiteral("removed"), gone, legacyDescriptor(gone, { self.full() }, write("gone")));
            query.prepare("INSERT INTO url_content VALUES('corporate',:key,:content,:updated)");
            query.bindValue(":key", sha256("address").toString());
            query.bindValue(":content", sha256("bytes").toString());
            query.bindValue(":updated", QDateTime::currentSecsSinceEpoch());
            QVERIFY(query.exec());
            db.close();
        }
        QSqlDatabase::removeDatabase(name);

        const QHash<QString, Jid> accounts { { corporate, self }, { personal, self } };
        QVERIFY(catalog->importLegacy(path, accounts));

        const auto hashes = [](const MediaSharing &media) { return media.file.computedHashes(); };
        QVERIFY(catalog->isServed(corporate, hashes(own)));
        const auto record = catalog->find(corporate, hashes(own).first());
        QCOMPARE(record->media.sources, own.sources);
        QCOMPARE(record->readableLocalPath(), dir->filePath(QStringLiteral("own")));
        QVERIFY2(!catalog->find(corporate, hashes(noCopy).first()), "history brings back what was not shared");
        QVERIFY2(!catalog->find(corporate, hashes(received).first()), "a received file is merely cached");
        QVERIFY(catalog->isServed(personal, hashes(marked)));
        QVERIFY(!catalog->find(personal, hashes(unmarked).first()));
        QVERIFY(!catalog->isServed(QStringLiteral("removed"), hashes(gone)));
        QCOMPARE(publicationCount(), 2);
        QCOMPARE(catalog->contentForUrl(corporate, sha256("address")).value().toString(), sha256("bytes").toString());

        catalog->stopServing(corporate, hashes(own));
        QVERIFY(catalog->importLegacy(path, accounts));
        QVERIFY2(!catalog->isServed(corporate, hashes(own)), "the import happens once");
    }

    void mappingsWithoutTimestampsAreNotImported()
    {
        const auto path = dir->filePath(QStringLiteral("shared-files.sqlite"));
        const auto name = QStringLiteral("legacy-mappings");
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery query(db);
            QVERIFY(query.exec("CREATE TABLE url_content (account_id TEXT, url_key TEXT, content_hash TEXT)"));
            query.prepare("INSERT INTO url_content VALUES('corporate',:key,:content)");
            query.bindValue(":key", sha256("address").toString());
            query.bindValue(":content", sha256("bytes").toString());
            QVERIFY(query.exec());
            db.close();
        }
        QSqlDatabase::removeDatabase(name);
        catalog->importLegacy(path, { { corporate, self } });
        QVERIFY(!catalog->contentForUrl(corporate, sha256("address")));
    }

    void aMissingLegacyCatalogIsNothingToImport()
    {
        QVERIFY(catalog->importLegacy(dir->filePath(QStringLiteral("none.sqlite")), { { corporate, self } }));
        QCOMPARE(publicationCount(), 0);
        QVERIFY(!QFile::exists(dir->filePath(QStringLiteral("none.sqlite"))));
    }

    void withoutTheFileDatabaseTheLegacyCatalogIsKept()
    {
        loseFileDatabase();
        QVERIFY(QDir(profileDirectory).mkdir(QStringLiteral("files.sqlite"))); // Cannot be opened.
        QVERIFY(!catalog->importLegacy(dir->filePath(QStringLiteral("none.sqlite")), { { corporate, self } }));
    }
};

QTEST_GUILESS_MAIN(PublicationsTest)
#include "publications.moc"
