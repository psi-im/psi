/* SPDX-License-Identifier: GPL-2.0-or-later */
// Publications come from what history stores and what an account chooses to share.
#include "applicationinfo.h"
#include "chatmedia.h"
#include "filedatabase.h"
#include "fileidentity.h"
#include "filesharingcatalog.h"
#include "filesharingpublicationsync.h"
#include "historyarchive.h"
#include "historymediarefs.h"
#include "historymessageextras.h"
#include "historysqlreadworker.h"
#include <QSignalSpy>
#include <QThread>
#include <iris/jingle-pub.h>
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

QJsonArray fingerprintProjection(const QJsonArray &index)
{
    QJsonArray result;
    for (const auto &value : index) {
        auto item = value.toObject();
        item.remove("resource");
        result.append(item);
    }
    return result;
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

MessageFileShares::FileShare attachmentOf(const MediaSharing &media) { return { referenceOf(media), {} }; }

MessageFileShares::FileShare urlAttachment(const QString &url, const QString &description = {})
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
        history.setConnectOptions("QSQLITE_OPEN_URI");
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

    QByteArray storedFile(const MediaSharing &media, bool complete, const QString &account = corporate,
                          quint64 bytes = 1)
    {
        const auto id = QUuid::createUuid().toRfc4122();
        QSqlQuery  q(FileDatabase::connection());
        q.prepare(
            "INSERT INTO files(id,partition,size,created,max_age,metadata,path,resource_key,complete,scope_account) "
            "VALUES(:id,:partition,:size,0,4294967295,:meta,:path,:key,:complete,:account)");
        q.bindValue(":id", id);
        q.bindValue(":partition", complete ? "shares" : "shares:ranges");
        q.bindValue(":size", qlonglong(*media.file.size()));
        q.bindValue(":meta", FileDatabase::encodeMetadata({}));
        q.bindValue(":path", QString::fromLatin1(id.toHex()) + ".data");
        q.bindValue(":key",
                    complete ? QString()
                             : FileIdentity::rangeKey(account, media.file.computedHashes().first().toString(),
                                                      *media.file.size()));
        q.bindValue(":complete", complete);
        q.bindValue(":account", complete ? QString() : account);
        if (!q.exec())
            return {};
        if (complete) {
            int position = 0;
            for (const auto &hash : media.file.computedHashes()) {
                q.prepare("INSERT INTO file_hashes VALUES(:id,:position,:algorithm,:digest)");
                q.bindValue(":id", id);
                q.bindValue(":position", position++);
                q.bindValue(":algorithm", hash.stringType());
                q.bindValue(":digest", hash.data());
                if (!q.exec())
                    return {};
            }
        } else {
            q.prepare("INSERT INTO file_ranges VALUES(:id,0,:bytes)");
            q.bindValue(":id", id);
            q.bindValue(":bytes", qlonglong(bytes));
            if (!q.exec())
                return {};
            for (const auto &hash : media.file.computedHashes()) {
                q.prepare("INSERT INTO file_checksums VALUES(:id,0,:size,:algorithm,:digest,0)");
                q.bindValue(":id", id);
                q.bindValue(":size", qlonglong(*media.file.size()));
                q.bindValue(":algorithm", hash.stringType());
                q.bindValue(":digest", hash.data());
                if (!q.exec())
                    return {};
            }
        }
        return id;
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

    FileSharingPublicationSync::HistoryAccess historyAccess()
    {
        FileSharingPublicationSync::HistoryAccess access;
        access.readLinks = [this](qint64 position, int limit, QObject *receiver,
                                  std::function<void(QList<HistoryMediaRefs::Link>, bool)> done) {
            QTimer::singleShot(0, receiver, [this, position, limit, done] {
                QSqlError  error;
                const auto links = HistoryMediaRefs::linksAfter(history, position, limit, &error);
                done(links, !error.isValid());
            });
        };
        access.readLinked = [this](const QList<QByteArray> &ids, QObject *receiver,
                                   std::function<void(QSet<QByteArray>, bool)> done) {
            QTimer::singleShot(0, receiver, [this, ids, done] {
                const auto linked = HistoryMediaRefs::linked(history, ids);
                done(linked.value_or(QSet<QByteArray>()), bool(linked));
            });
        };
        return access;
    }

    void syncEverything()
    {
        FileSharingPublicationSync sync(historyAccess(), catalog.get());
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

    void versionOneHistoryIndexKeepsOrdinalsAndFingerprints()
    {
        auto      media = mediaOf("payload", { "https://example.org/file" });
        Reference mention(Reference::Mention, "xmpp:alice@example.org");
        mention.setRange(0, 4);
        auto message = messageWith(alice, { mention, referenceOf(media) });
        message.setBody("Read https://example.org/article please");
        StatelessFileSharing::FileSharing sharing;
        sharing.setFile(media.file);
        sharing.setDisposition(StatelessFileSharing::Disposition::Attachment);
        StatelessFileSharing::Sources sources;
        sources.add(StatelessFileSharing::Source::fromUrl(QUrl("https://example.org/mirror")));
        sharing.setSources(sources);
        message.addFileSharing(sharing);
        message.urlAdd(Url("https://example.org/file", "caption"));
        message.urlAdd(Url("https://example.org/article"));
        message.urlAdd(Url("https://example.org/tail#fragment", "tail"));

        // Captured from the version-1 implementation before this refactor.
        const auto oldIndex = QJsonDocument::fromJson(R"([
            {"fingerprint":"14ec5a5e48ddc7352bb61716d6f01b9dfaee7c8057aae25e04bc215ea42cdb38","ordinal":1},
            {"fingerprint":"16bb6010ac0594d41bc0676572e1c58efd11bbe1fecc7566ab2d39107900e92d","ordinal":2}
        ])")
                                  .array();
        QCOMPARE(fingerprintProjection(HistoryMessageExtras::mediaRefs(message, 1)), oldIndex);
        auto extra = QJsonDocument::fromJson(HistoryMessageExtras::encode(message).toUtf8()).object();
        extra.insert("psi:media-refs", QJsonObject { { "version", 1 }, { "items", oldIndex } });
        const auto json  = QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact));
        const auto event = insertEvent(message.body(), json);
        QVERIFY(event);
        migrateHistory();
        QCOMPARE(linkCount(), 2);
        const auto ids = links(event);
        QVERIFY(HistoryMediaRefs::link(history, event));
        QCOMPARE(links(event), ids); // No ownership churn on a replay of the same stored projection.
        syncEverything();
        const auto record = catalog->find(corporate, media.file.computedHashes().first());
        QVERIFY(record);
        QCOMPARE(record->media.sources.size(), 2);
        QVERIFY(catalog->find(corporate, MessageFileShares::urlIdentity(QUrl("https://example.org/tail"))));
        QVERIFY(!catalog->find(corporate, MessageFileShares::urlIdentity(QUrl("https://example.org/article"))));
    }

    void versionOneStandaloneOobKeepsInferredRangeFingerprint()
    {
        Message message;
        message.setBody("https://example.org/photo.png");
        message.urlAdd(Url("https://example.org/photo.png#fragment", "photo"));
        const auto oldIndex = QJsonDocument::fromJson(R"([
            {"fingerprint":"cd2e96d8b1fcf8102675e6531122ee0fad5cf2917f670a1736b59b323282bac2","ordinal":0}
        ])")
                                  .array();
        QCOMPARE(fingerprintProjection(HistoryMessageExtras::mediaRefs(message, 1)), oldIndex);
        const auto files = HistoryMessageExtras::indexedFiles(message, 1);
        QCOMPARE(files.size(), 1);
        QCOMPARE(files.first().reference.begin(), 0);
        QCOMPARE(files.first().reference.end(), message.body().size() - 1);
    }

    void versionOneUnsupportedSimsKeepsDescriptionDistribution()
    {
        const QString url = "https://example.org/file";
        MediaSharing  media;
        media.file.setName("file");
        media.file.setSize(7);
        media.file.addHash(sha256("payload"));
        media.sources = QStringList { url };
        MediaSharing unsupported;
        unsupported.file.setName("legacy");
        unsupported.sources = QStringList { url };
        auto message        = messageWith(alice, { referenceOf(unsupported), referenceOf(media) });
        message.urlAdd(Url(url, "caption"));
        message.urlAdd(Url("https://example.org/tail#fragment", "tail"));
        message.urlAdd(Url("https://example.org/tail#copy", "duplicate"));
        const auto oldIndex = QJsonDocument::fromJson(R"([
            {"fingerprint":"1088da59be3df4b5c615e752c85971b39bb3c59a655fae17458f8cf80e5c3d73","ordinal":1},
            {"fingerprint":"16bb6010ac0594d41bc0676572e1c58efd11bbe1fecc7566ab2d39107900e92d","ordinal":2}
        ])")
                                  .array();
        QCOMPARE(fingerprintProjection(HistoryMessageExtras::mediaRefs(message, 1)), oldIndex);
        QCOMPARE(message.urlList().size(), 3);
        const auto normalized = MessageFileShares::collect(message).files;
        QCOMPARE(normalized.size(), 2);
        QCOMPARE(normalized.first().description, QString("caption"));
    }

    void fileOwnershipDoesNotDependOnOobPresentation()
    {
        migrateHistory();
        auto          message = messageWith(alice, {});
        const QString url     = "https://example.org/photo.png";
        message.setBody("Caption: " + url);
        message.urlAdd(Url(url, "photo"));
        const auto extra = QJsonDocument::fromJson(HistoryMessageExtras::encode(message).toUtf8()).object();
        QCOMPARE(extra.value("psi:media-refs").toObject().value("version").toInt(), 2);
        const auto before = HistoryMessageExtras::mediaRefs(message);
        QVERIFY(!before.isEmpty());
        QVERIFY(MessageFileShares::forDisplay(message).files.isEmpty());
        const auto event = store(message);
        QVERIFY(event);
        const auto ids = links(event);
        QCOMPARE(ids.size(), 1);
        syncEverything();
        QVERIFY(catalog->find(corporate, MessageFileShares::urlIdentity(QUrl(url))));

        message.setBody(url);
        QCOMPARE(HistoryMessageExtras::mediaRefs(message), before);
        const auto display = MessageFileShares::forDisplay(message).files;
        QCOMPARE(display.size(), 1);
        QCOMPARE(display.first().reference.begin(), 0);
        QSqlQuery update(history);
        update.prepare("UPDATE events SET m_text=:body,extra_data=:extra WHERE id=:event");
        update.bindValue(":body", message.body());
        update.bindValue(":extra", HistoryMessageExtras::encode(message));
        update.bindValue(":event", event);
        QVERIFY(update.exec());
        QVERIFY(HistoryMediaRefs::link(history, event));
        QCOMPARE(links(event), ids);
    }

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
        const auto identity = MessageFileShares::urlIdentity(urlAttachment(url).url());
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

    void oneServedHashCannotAuthorizeAnUnrelatedCacheLookup()
    {
        const auto publicFile  = mediaOf("public", {});
        const auto privateFile = mediaOf("private", {});
        QVERIFY(catalog->serve(personal, self, publicFile, {}));
        QVERIFY(catalog->serve(corporate, self, privateFile, {}));
        const auto publicHash  = publicFile.file.computedHashes().first();
        const auto privateHash = privateFile.file.computedHashes().last();
        QVERIFY(!catalog->isServed(personal, { privateHash, publicHash }));
        QVERIFY(!catalog->servedRecord(personal, { privateHash, publicHash }));
        const auto grant = catalog->servedRecord(personal, { publicHash });
        QVERIFY(grant);
        QCOMPARE(grant->media.file.computedHashes(), publicFile.file.computedHashes());
        QVERIFY(!catalog->servedRecord(personal, { privateHash }));
    }

    void receivedHashAliasesDoNotExpandServingAuthority()
    {
        auto       own  = mediaOf("public", {});
        const auto sha1 = own.file.computedHashes().last();
        own.file.setHashes({ sha1 }); // Legacy own files may have only this hash.
        QVERIFY(catalog->serve(personal, self, own, {}));
        auto       forged  = mediaOf("secret", { "https://example.org/forged" });
        const auto foreign = forged.file.computedHashes().first();
        forged.file.setHashes({ sha1, foreign });
        QVERIFY(catalog->add(newId(), personal, alice, attachmentOf(forged)));
        const auto grant = catalog->servedRecord(personal, { sha1 });
        QVERIFY(grant);
        QCOMPARE(grant->media.file.computedHashes(), QList<Hash> { sha1 });
        QVERIFY(!grant->media.file.computedHashes().contains(foreign));
        QVERIFY(!catalog->servedRecord(personal, { sha1, foreign }));
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
            QVERIFY(query.exec("CREATE TABLE file_hashes(file_id BLOB NOT NULL REFERENCES files(id) ON DELETE CASCADE,"
                               "position INTEGER NOT NULL,algorithm TEXT NOT NULL,digest BLOB NOT NULL,"
                               "PRIMARY KEY(file_id,position))"));
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
        QCOMPARE(query.value(0).toInt(), 6);
        QVERIFY(catalog->serve(corporate, self, mediaOf("payload", {}), {}));
    }

    void untypedHashIndexUpgradesWithoutReplacingFileRecords()
    {
        const auto id = storedFile(mediaOf("preserved-index", {}), true);
        QVERIFY(!id.isEmpty());
        closeFileDatabase();
        const auto name = QStringLiteral("schema-five");
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", name);
            db.setDatabaseName(FileDatabase::path());
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec("DROP INDEX file_hashes_key"));
            QVERIFY(q.exec("CREATE INDEX file_hashes_key ON file_hashes(algorithm || '+' || lower(hex(digest)))"));
            QVERIFY(q.exec("PRAGMA user_version=5"));
            db.close();
        }
        QSqlDatabase::removeDatabase(name);
        QSqlQuery q(FileDatabase::connection());
        q.prepare("SELECT 1 FROM files WHERE id=:id"); q.bindValue(":id", id);
        QVERIFY(q.exec() && q.next()); q.finish();
        QVERIFY(q.exec("SELECT sql FROM sqlite_master WHERE name='file_hashes_key'")); QVERIFY(q.next());
        QVERIFY(q.value(0).toString().contains("CAST("));
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

    void chatMediaPagesUseChronologyAndIsolateChats()
    {
        migrateHistory();
        const auto message = messageWith(alice,
                                         { referenceOf(mediaOf("one", { "https://example.org/1" })),
                                           referenceOf(mediaOf("two", { "https://example.org/2" })) });
        const auto first   = store(message);
        const auto second  = store(message);
        const auto foreign = store(message, 2);
        QVERIFY(first && second && foreign);
        QSqlQuery query(history);
        QVERIFY(query.exec("INSERT INTO contacts VALUES(3,'corporate',3,'alice@example.org',-1)"));
        QVERIFY(store(message, 3));
        // Archive pages may arrive after newer live messages.
        QVERIFY(
            query.exec(QString("UPDATE events SET history_date_ms=CASE WHEN id=%1 THEN 200 ELSE 100 END").arg(first)));
        QSqlError error;
        auto      page = HistoryMediaRefs::chatLinks(history, key, {}, false, 1, &error);
        QVERIFY(!error.isValid());
        QCOMPARE(page.size(), 1);
        QCOMPARE(page.first().cursor.eventId, second);
        QCOMPARE(page.first().ordinal, 0);
        const auto cursor = page.first().cursor;
        // A deleted cursor row must not break keyset pagination.
        QVERIFY(query.exec(QString("DELETE FROM message_media_refs WHERE event_id=%1 AND ordinal=0").arg(second)));
        page = HistoryMediaRefs::chatLinks(history, key, cursor, false, 10, &error);
        QCOMPARE(page.size(), 3);
        QCOMPARE(page.first().cursor.eventId, second);
        QCOMPARE(page.first().ordinal, 1);
        QCOMPARE(page.last().cursor.eventId, first);
        page = HistoryMediaRefs::chatLinks(history, key, {}, true, 10, &error);
        QCOMPARE(page.size(), 3);
        QCOMPARE(page.first().cursor.eventId, first);
        QCOMPARE(page.first().ordinal, 1);
        page = HistoryMediaRefs::chatLinks(history, key, page.last().cursor, true, 10, &error);
        QVERIFY(page.isEmpty());
        QVERIFY(!error.isValid());
        QVERIFY(query.exec(QString("UPDATE events SET history_date_ms=NULL WHERE id=%1").arg(second)));
        page = HistoryMediaRefs::chatLinks(history, key, {}, false, 1, &error);
        QVERIFY(page.first().cursor.undated);
        page = HistoryMediaRefs::chatLinks(history, key, page.first().cursor, false, 1, &error);
        QVERIFY(!page.first().cursor.undated);
        QCOMPARE(page.first().cursor.eventId, first);
        page = HistoryMediaRefs::chatLinks(history, key, page.first().cursor, true, 1, &error);
        QVERIFY(page.first().cursor.undated);
        QCOMPARE(page.first().cursor.eventId, second);
        QVERIFY(!error.isValid());
    }

    void chatMediaUsesVisibleRevisionAndHidesDeletedOrRetractedMessages()
    {
        migrateHistory();
        const auto message = messageWith(alice, { referenceOf(mediaOf("one", { "https://example.org/1" })) });
        const auto root    = store(message);
        const auto edit    = store(messageWith(alice, {}));
        const auto visible = store(message);
        QVERIFY(root && edit && visible);
        QSqlQuery q(history);
        QVERIFY(q.exec(QString("INSERT INTO message_revisions VALUES(%1,1,1,2,'alice','original','',%1)").arg(root)));
        QVERIFY(q.exec(QString("INSERT INTO message_revisions VALUES(%1,1,1,2,'alice','edit','original',%2)")
                           .arg(edit)
                           .arg(root)));
        QSqlError error;
        auto      page = HistoryMediaRefs::chatLinks(history, key, {}, false, 10, &error);
        QVERIFY(!error.isValid());
        QCOMPARE(page.size(), 1); // The edit removed the original attachment.
        QCOMPARE(page.first().cursor.eventId, visible);
        QVERIFY(q.exec(QString("UPDATE events SET message_flags=8 WHERE id=%1").arg(visible)));
        QVERIFY(HistoryMediaRefs::chatLinks(history, key, {}, false, 10, &error).isEmpty());
        QVERIFY(q.exec(QString("UPDATE events SET message_flags=16 WHERE id=%1").arg(visible)));
        QVERIFY(HistoryMediaRefs::chatLinks(history, key, {}, false, 10, &error).isEmpty());
        QVERIFY(!error.isValid());
    }

    void chatMediaStatisticsUseOneAggregateAndDoNotResolveAnnouncements()
    {
        migrateHistory();
        const QList<MediaSharing> media { mediaOf("one", { "https://example.org/1" }),
                                          mediaOf("two", { "https://example.org/2" }),
                                          mediaOf("three", { "https://example.org/3" }) };
        for (int i = 0; i < 205; ++i)
            QVERIFY(store(messageWith(alice, { referenceOf(media.at(i % 3)) })));
        QVERIFY(!storedFile(media.at(0), true).isEmpty());
        QVERIFY(!storedFile(media.at(1), false).isEmpty());
        QSqlError error;
        QVERIFY2(HistoryMediaRefs::attachFileDatabase(history, FileDatabase::path(), &error), qPrintable(error.text()));
        // Aggregate availability does not decode event payloads.
        QSqlQuery damaged(history);
        QVERIFY(damaged.exec("UPDATE events SET extra_data='not JSON' WHERE id=1"));
        int       reads = 0, counts = 0, completed = 0;
        ChatMedia browser(
            [&](ChatMedia::Cursor cursor, bool older, int limit, QObject *receiver, ChatMedia::LinksDone done) {
                ++reads;
                QTimer::singleShot(0, receiver, [&, cursor, older, limit, done] {
                    QSqlError  e;
                    const auto links = HistoryMediaRefs::chatLinks(history, key, cursor, older, limit, &e, true);
                    done(links, !e.isValid());
                });
            },
            [&](QObject *receiver, ChatMedia::StatisticsDone done) {
                ++counts;
                QTimer::singleShot(0, receiver, [&, done] {
                    QSqlError  e;
                    const auto stats = HistoryMediaRefs::chatStatistics(history, key, &e);
                    QVERIFY2(!e.isValid(), qPrintable(e.text()));
                    done(stats, !e.isValid());
                });
            });
        browser.requestStatistics([&](ChatMedia::Statistics stats, bool ok) {
            QVERIFY(ok);
            QCOMPARE(stats.complete, quint64(69));
            QCOMPARE(stats.partial, quint64(68));
            QCOMPARE(stats.remote, quint64(68));
            QVERIFY(stats.stale);
            ++completed;
        });
        browser.invalidate();
        QTRY_COMPARE(completed, 1);
        QCOMPARE(reads, 0);
        QCOMPARE(counts, 1);
        browser.requestPage({}, true, 2, [&](ChatMedia::Page page, bool ok) {
            QVERIFY(ok);
            QCOMPARE(page.entries.size(), 2);
            QVERIFY(page.hasMore);
            QVERIFY(page.next.valid());
            QCOMPARE(page.entries.first().storage.availability, ChatMedia::Availability::Complete);
            QCOMPARE(page.entries.last().storage.availability, ChatMedia::Availability::Remote);
            ++completed;
        });
        QTRY_COMPARE(completed, 2);
        QCOMPARE(reads, 1);
    }

    void chatMediaAggregateFollowsStorageAndScopesRangesAndUrls()
    {
        migrateHistory();
        const auto full    = mediaOf("full", { "https://example.org/full" });
        const auto partial = mediaOf("partial", { "https://example.org/partial" });
        QVERIFY(store(messageWith(alice, { referenceOf(full), referenceOf(partial) })));
        QVERIFY(store(messageWith(alice, { referenceOf(full) })));
        QVERIFY(store(messageWith(alice, { referenceOf(full) }), 2));
        QSqlQuery q(history);
        QVERIFY(q.exec("INSERT INTO contacts VALUES(3,'corporate',3,'alice@example.org',-1)"));
        QVERIFY(store(messageWith(alice, { referenceOf(full) }), 3));
        const auto fullId = storedFile(full, true);
        QVERIFY(!fullId.isEmpty());
        QVERIFY(!storedFile(partial, false, personal, 2).isEmpty());
        QSqlError error;
        QVERIFY2(HistoryMediaRefs::attachFileDatabase(history, FileDatabase::path(), &error), qPrintable(error.text()));
        auto stats = HistoryMediaRefs::chatStatistics(history, key, &error);
        QVERIFY2(!error.isValid(), qPrintable(error.text()));
        QCOMPARE(stats.complete, quint64(2)); // Two occurrences, not four hash aliases.
        QCOMPARE(stats.partial, quint64(0));  // Another account's partial ranges are inaccessible.
        QCOMPARE(stats.remote, quint64(1));
        const auto partId = storedFile(partial, false, corporate, 3);
        QVERIFY(!partId.isEmpty());
        stats = HistoryMediaRefs::chatStatistics(history, key, &error);
        QCOMPARE(stats.complete, quint64(2));
        QCOMPARE(stats.partial, quint64(1));
        QCOMPARE(stats.remote, quint64(0));
        auto page = HistoryMediaRefs::chatLinks(history, key, {}, false, 10, &error, true);
        QCOMPARE(page.size(), 3);
        QCOMPARE(page.at(1).storage.downloadedBytes, quint64(3));
        QCOMPARE(page.at(1).storage.availability, ChatMedia::Availability::Partial);
        auto alias = partial;
        alias.file.setHashes({ partial.file.computedHashes().at(1) });
        QVERIFY(store(messageWith(alice, { referenceOf(alias) })));
        stats = HistoryMediaRefs::chatStatistics(history, key, &error);
        QCOMPARE(stats.partial, quint64(2)); // The later announcement knows only the secondary hash.
        auto url = messageWith(alice, {});
        url.urlAdd(Url("https://example.org/address-only"));
        QVERIFY(store(url));
        const auto address = MessageFileShares::urlIdentity(QUrl("https://example.org/address-only"));
        QVERIFY(catalog->rememberUrlContent(personal, address, full.file.computedHashes().first()));
        stats = HistoryMediaRefs::chatStatistics(history, key, &error);
        QCOMPARE(stats.remote, quint64(1));
        QVERIFY(catalog->rememberUrlContent(corporate, address, full.file.computedHashes().first()));
        stats = HistoryMediaRefs::chatStatistics(history, key, &error);
        QCOMPARE(stats.complete, quint64(3));
        QCOMPARE(stats.partial, quint64(2));
        // Physical ownership changes drive counts; there is no counter to maintain.
        QSqlQuery files(FileDatabase::connection());
        files.prepare("UPDATE files SET max_age=1,created=0 WHERE id=:id");
        files.bindValue(":id", fullId);
        QVERIFY(files.exec());
        stats = HistoryMediaRefs::chatStatistics(history, key, &error);
        QCOMPARE(stats.complete, quint64(0));
        QCOMPARE(stats.remote, quint64(3));
        files.prepare("UPDATE files SET max_age=4294967295 WHERE id=:id");
        files.bindValue(":id", fullId);
        QVERIFY(files.exec());
        stats = HistoryMediaRefs::chatStatistics(history, key, &error);
        QCOMPARE(stats.complete, quint64(3));
        files.prepare("DELETE FROM files WHERE id=:id");
        files.bindValue(":id", fullId);
        QVERIFY(files.exec());
        stats = HistoryMediaRefs::chatStatistics(history, key, &error);
        QCOMPARE(stats.complete, quint64(0));
        QCOMPARE(stats.remote, quint64(3));
        files.prepare("DELETE FROM file_ranges WHERE file_id=:id");
        files.bindValue(":id", partId);
        QVERIFY(files.exec());
        stats = HistoryMediaRefs::chatStatistics(history, key, &error);
        QCOMPARE(stats.partial, quint64(0));
        QCOMPARE(stats.remote, quint64(5));
        QVERIFY(!error.isValid());
        QVERIFY(!q.exec("DELETE FROM media_files.files")); // Attachment is read-only.
        QVERIFY(insertEvent("history still writable", "{}"));
        QVERIFY(q.exec("EXPLAIN QUERY PLAN SELECT file_id FROM media_files.file_hashes "
                       "WHERE CAST(algorithm || '+' || lower(hex(digest)) AS TEXT)='sha-256+00'"));
        bool indexed = false;
        while (q.next())
            indexed |= q.value(3).toString().contains("file_hashes_key");
        QVERIFY(indexed);
    }

    void resourceMigrationPreservesPublicationIdsAndScanPositions()
    {
        migrateHistory();
        const auto media   = mediaOf("preserved", { "https://example.org/old" });
        const auto message = messageWith(alice, { referenceOf(media) });
        const auto event   = store(message);
        QVERIFY(event);
        const auto ids = links(event);
        QSqlQuery  q(history);
        QVERIFY(q.exec("SELECT position FROM message_media_refs"));
        QVERIFY(q.next());
        const auto position = q.value(0).toLongLong();
        q.finish();
        QVERIFY(q.exec("DROP TABLE message_media_hashes"));
        QVERIFY(q.exec("DROP TABLE message_media_resources"));
        auto extra = QJsonDocument::fromJson(HistoryMessageExtras::encode(message).toUtf8()).object();
        auto index = extra.value("psi:media-refs").toObject();
        index.insert("items", fingerprintProjection(index.value("items").toArray()));
        extra.insert("psi:media-refs", index);
        extra.remove("psi:media-resources");
        q.prepare("UPDATE events SET extra_data=:extra WHERE id=:id");
        q.bindValue(":extra", QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact)));
        q.bindValue(":id", event);
        QVERIFY(q.exec());
        QVERIFY(HistoryMediaRefs::needsMigration(history));
        QVERIFY(HistoryMediaRefs::migrate(history));
        QVERIFY(!HistoryMediaRefs::needsMigration(history));
        QCOMPARE(links(event), ids);
        QVERIFY(q.exec("SELECT position FROM message_media_refs"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toLongLong(), position);
        q.finish();
        QVERIFY(q.exec("SELECT range_key FROM message_media_hashes WHERE algorithm='sha-256'"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(),
                 FileIdentity::rangeKey(corporate, media.file.computedHashes().first().toString(), *media.file.size()));
        q.finish();
        QVERIFY(history.transaction());
        QVERIFY(q.exec("CREATE TRIGGER reject_resource BEFORE INSERT ON message_media_hashes BEGIN "
                       "SELECT RAISE(ABORT,'resource failure'); END"));
        const auto next = insertEvent(message.body(), HistoryMessageExtras::encode(message));
        QVERIFY(next);
        QVERIFY(!HistoryMediaRefs::link(history, next));
        QVERIFY(history.rollback());
        QCOMPARE(linkCount(), 1);
    }

    void fileRegistryAttachmentAcceptsProfileSymlinks()
    {
#ifdef Q_OS_WIN
        QSKIP("Creating directory symlinks requires a Windows privilege");
#else
        const auto alias = dir->filePath("profile alias");
        QVERIFY(QFile::link(profileDirectory, alias));
        QSqlError  error;
        const auto filePath = alias + "/files.sqlite";
        QVERIFY2(HistoryMediaRefs::attachFileDatabase(history, filePath, &error), qPrintable(error.text()));
        QVERIFY2(HistoryMediaRefs::attachFileDatabase(history, filePath, &error), qPrintable(error.text()));
        QVERIFY2(HistoryMediaRefs::attachFileDatabase(history, FileDatabase::path(), &error), qPrintable(error.text()));
#endif
    }

    void completeCopyDeterminesReportedByteCount()
    {
        migrateHistory();
        const auto   full = mediaOf("data", {});
        MediaSharing metadata;
        metadata.file.setName(full.file.name());
        metadata.file.setMediaType(full.file.mediaType());
        metadata.file.setHashes(full.file.computedHashes());
        metadata.sources = QStringList { "https://example.org/data" };
        QVERIFY(store(messageWith(alice, { referenceOf(metadata) })));
        QVERIFY(!storedFile(full, true).isEmpty());
        auto conflicting = full;
        conflicting.file.setSize(20);
        QVERIFY(!storedFile(conflicting, false, corporate, 12).isEmpty());
        QSqlError error;
        QVERIFY(HistoryMediaRefs::attachFileDatabase(history, FileDatabase::path(), &error));
        const auto page = HistoryMediaRefs::chatLinks(history, key, {}, false, 10, &error, true);
        QVERIFY2(!error.isValid(), qPrintable(error.text()));
        QCOMPARE(page.size(), 1);
        QCOMPARE(page.first().storage.availability, ChatMedia::Availability::Complete);
        QCOMPARE(page.first().storage.downloadedBytes, quint64(4));
    }

    void chatMediaCancellationAndQueryErrorsDoNotLookLikeEmptyResults()
    {
        int  completed = 0;
        auto read      = [](ChatMedia::Cursor, bool, int, QObject *receiver, ChatMedia::LinksDone done) {
            QTimer::singleShot(0, receiver, [done] { done({}, false); });
        };
        auto count = [](QObject *receiver, ChatMedia::StatisticsDone done) {
            QTimer::singleShot(0, receiver, [done] { done({}, false); });
        };
        auto browser = new ChatMedia(read, count);
        browser->requestStatistics([&](ChatMedia::Statistics, bool) { ++completed; });
        delete browser;
        QTest::qWait(10);
        QCOMPARE(completed, 0);
        ChatMedia live(read, count);
        live.requestStatistics([&](ChatMedia::Statistics, bool ok) {
            QVERIFY(!ok);
            ++completed;
        });
        QTRY_COMPARE(completed, 1);
        live.requestPage({}, false, 201, [&](ChatMedia::Page, bool ok) {
            QVERIFY(!ok);
            ++completed;
        });
        QTRY_COMPARE(completed, 2);
    }

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
        QCOMPARE(linkCount(), 3); // Ownership includes OOB files that the view leaves as links in prose.

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
        FileSharingPublicationSync sync(historyAccess(), catalog.get());
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
        const auto record = catalog->find(corporate, MessageFileShares::urlIdentity(urlAttachment(url).url()));
        QVERIFY(record);
        QVERIFY(record->urlOnly);
        QCOMPARE(record->media.sources, QStringList { url });
        QCOMPARE(record->media.file.description(), QStringLiteral("A report"));
    }

    void aFileSharingFollowsItsMessage()
    {
        migrateHistory();
        const auto    sims = mediaOf("payload", { QStringLiteral("https://sims.example/f") });
        const QString url  = QStringLiteral("https://sfs.example/f");
        auto          file = mediaOf("payload", {}).file;
        file.setDescription(QStringLiteral("Described"));
        StatelessFileSharing::Sources sources;
        sources.add(StatelessFileSharing::Source::fromUrl(QUrl(url)));
        StatelessFileSharing::FileSharing sharing;
        sharing.setFile(file);
        sharing.setSources(sources);
        auto message = messageWith(bob, {});
        message.addFileSharing(sharing);
        const auto event = store(message);
        QCOMPARE(links(event).size(), 1);
        syncEverything();
        auto record = catalog->find(corporate, file.computedHashes().first());
        QVERIFY(record);
        QVERIFY(!record->urlOnly);
        QCOMPARE(record->media.sources, QStringList { url });
        QCOMPARE(record->media.file.name(), QStringLiteral("payload"));
        QCOMPARE(record->media.file.description(), QStringLiteral("Described"));
        QCOMPARE(record->peers, QStringList { bob.full() });

        // Whatever the protocol, it is one file.
        QVERIFY(store(messageWith(alice, { referenceOf(sims) })));
        syncEverything();
        record = catalog->find(corporate, file.computedHashes().first());
        QCOMPARE(record->media.sources.size(), 2);
        QCOMPARE(record->peers.size(), 2);

        HistoryArchive::Page page;
        page.stable = true;
        auto later  = messageWith(alice, {});
        later.addFileSharing(sharing);
        page.entries.append(archived(later, QStringLiteral("uid-1")));
        QVERIFY(storePage(page));
        QCOMPARE(links(page.entries.first().eventId).size(), 1);
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
        QVERIFY(query.exec("ALTER TABLE message_media_refs RENAME TO unavailable_refs"));
        FileSharingPublicationSync sync(historyAccess(), catalog.get());
        sync.reconcile();
        QTest::qWait(100);
        QVERIFY(catalog->find(corporate, media.file.computedHashes().first()));
        QVERIFY(query.exec("ALTER TABLE unavailable_refs RENAME TO message_media_refs"));
        QTRY_VERIFY(sync.isIdle());
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
        FileSharingPublicationSync sync(historyAccess(), catalog.get());
        sync.reconcile();
        history.close();
        QTest::qWait(100);
        QCOMPARE(publicationCount(), 0);
        QVERIFY(history.open());
        QTRY_VERIFY(sync.isIdle());
        QCOMPARE(publicationCount(), 1);
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
        const auto old = catalog->find(corporate, MessageFileShares::urlIdentity(QUrl("https://example.org/old")));
        QVERIFY(old);
        QCOMPARE(old->peers, QStringList { QStringLiteral("alice@example.org") });
        const auto sent = catalog->find(corporate, MessageFileShares::urlIdentity(QUrl("https://example.org/sent")));
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

    void linkScanPositionsAreNotReusedAfterDeletion()
    {
        migrateHistory();
        auto       message = messageWith(alice, { referenceOf(mediaOf("first", {})) });
        const auto event   = store(message);
        const auto before  = HistoryMediaRefs::linksAfter(history, 0, 100);
        QCOMPARE(before.size(), 1);
        QSqlQuery erase(history);
        QVERIFY(erase.exec(QStringLiteral("DELETE FROM events WHERE id=%1").arg(event)));
        QVERIFY(store(messageWith(alice, { referenceOf(mediaOf("second", {})) })));
        const auto after = HistoryMediaRefs::linksAfter(history, before.first().position, 100);
        QCOMPARE(after.size(), 1);
        QVERIFY(after.first().position > before.first().position);
    }

    void removingAttachmentsRetiresTheirLinks()
    {
        migrateHistory();
        const auto event = store(messageWith(alice, { referenceOf(mediaOf("first", {})) }));
        QSqlQuery  change(history);
        change.prepare("UPDATE events SET extra_data=:extra WHERE id=:id");
        change.bindValue(":extra", HistoryMessageExtras::encode(messageWith(alice, {})));
        change.bindValue(":id", event);
        QVERIFY(change.exec());
        bool removed = false;
        QVERIFY(HistoryMediaRefs::link(history, event, &removed));
        QVERIFY(removed);
        QVERIFY(links(event).isEmpty());
    }

    void localizedBodyDoesNotChangeFileProjection()
    {
        migrateHistory();
        Message       message;
        const QString url = "https://example.org/report";
        message.setBody("See " + url + " in this text");
        message.urlAdd(Url(url));
        const auto id = insertEvent(url, HistoryMessageExtras::encode(message));
        QVERIFY(HistoryMediaRefs::link(history, id));
        QCOMPARE(links(id).size(), 1);
        syncEverything();
        QVERIFY(catalog->find(corporate, MessageFileShares::urlIdentity(QUrl(url))));
    }

    void pendingSfsFilesHaveDistinctStableFingerprints()
    {
        migrateHistory();
        StatelessFileSharing::FileSharing first, second;
        first.setFile(mediaOf("pending-first", {}).file);
        second.setFile(mediaOf("pending-second", {}).file);
        auto before = messageWith(alice, {});
        before.addFileSharing(first);
        auto after = messageWith(alice, {});
        after.addFileSharing(second);
        const auto event    = store(before);
        const auto original = links(event);
        QCOMPARE(original.size(), 1);
        QVERIFY(HistoryMediaRefs::link(history, event));
        QCOMPARE(links(event), original);
        QSqlQuery change(history);
        change.prepare("UPDATE events SET extra_data=:extra WHERE id=:id");
        change.bindValue(":id", event);
        change.bindValue(":extra", HistoryMessageExtras::encode(after));
        QVERIFY(change.exec());
        QVERIFY(HistoryMediaRefs::link(history, event));
        QVERIFY(links(event) != original);
    }

    void aFailedCatalogBatchRollsBackEveryAnnouncement()
    {
        const auto first = mediaOf("first", {}), second = mediaOf("second", {});
        QSqlQuery  fail(FileDatabase::connection());
        QVERIFY(fail.exec("CREATE TRIGGER reject_second BEFORE INSERT ON publications "
                          "WHEN (SELECT count(*) FROM publications)>0 "
                          "BEGIN SELECT RAISE(ABORT,'test batch failure'); END"));
        QVERIFY(!catalog->addBatch(
            { { newId(), corporate, alice, attachmentOf(first) }, { newId(), corporate, bob, attachmentOf(second) } }));
        QCOMPARE(publicationCount(), 0);
    }

    void sweepingRetriesFailedDeletes()
    {
        migrateHistory();
        const auto media = mediaOf("payload", {});
        const auto id    = newId();
        QVERIFY(catalog->add(id, corporate, alice, attachmentOf(media)));
        QSqlQuery fail(FileDatabase::connection());
        QVERIFY(fail.exec("CREATE TRIGGER reject_delete BEFORE DELETE ON publications "
                          "BEGIN SELECT RAISE(ABORT,'test delete failure'); END"));
        FileSharingPublicationSync sync(historyAccess(), catalog.get());
        sync.reconcile();
        QTest::qWait(100);
        QCOMPARE(publicationCount(), 1);
        QVERIFY(fail.exec("DROP TRIGGER reject_delete"));
        QTRY_COMPARE(publicationCount(), 0);
        QTRY_VERIFY(sync.isIdle());
    }

    void staleLinkSnapshotCannotRecreateAnErasedPublication()
    {
        migrateHistory();
        const auto media = mediaOf("payload", {});
        QVERIFY(store(messageWith(alice, { referenceOf(media) })));
        auto                                                     access     = historyAccess();
        auto                                                     normalRead = access.readLinks;
        std::function<void(QList<HistoryMediaRefs::Link>, bool)> pending;
        QList<HistoryMediaRefs::Link>                            old;
        access.readLinks = [normalRead, &pending, &old](qint64 position, int limit, QObject *receiver,
                                                        std::function<void(QList<HistoryMediaRefs::Link>, bool)> done) {
            normalRead(position, limit, receiver, [&pending, &old, done](QList<HistoryMediaRefs::Link> links, bool ok) {
                if (pending) {
                    done(std::move(links), ok);
                } else {
                    old     = std::move(links);
                    pending = done;
                }
            });
        };
        FileSharingPublicationSync sync(std::move(access), catalog.get());
        sync.reconcile();
        QTRY_VERIFY(bool(pending));
        QSqlQuery erase(history);
        QVERIFY(erase.exec("DELETE FROM events"));
        sync.reconcile();
        pending(old, true);
        QTRY_VERIFY(sync.isIdle());
        QCOMPARE(publicationCount(), 0);
    }

    void oldDevelopmentSfsIsPreparedBeforeThreadedMigration()
    {
        QVERIFY(HistoryArchive::migrate(history));
        Jingle::JinglePub publication;
        publication.setFrom(alice);
        publication.setId("legacy");
        publication.addDescription(Jingle::FileTransfer::NS);
        StatelessFileSharing::Sources sources;
        sources.add(StatelessFileSharing::Source::fromJinglePub(publication));
        StatelessFileSharing::FileSharing sharing;
        sharing.setFile(mediaOf("payload", {}).file);
        sharing.setSources(sources);
        auto message = messageWith(alice, {});
        message.addFileSharing(sharing);
        auto extra = QJsonDocument::fromJson(HistoryMessageExtras::encode(message).toUtf8()).object();
        extra.remove("psi:media-refs");
        QVERIFY(insertEvent({}, QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact))));
        const auto migrate = [this] {
            bool       success = false;
            const auto path    = history.databaseName();
            auto      *thread  = QThread::create([path, &success] {
                const auto name = QStringLiteral("legacy-media-worker");
                {
                    auto db = QSqlDatabase::addDatabase("QSQLITE", name);
                    db.setDatabaseName(path);
                    success = db.open() && HistoryMediaRefs::migrate(db);
                    db.close();
                }
                QSqlDatabase::removeDatabase(name);
            });
            thread->start();
            if (!thread->wait(5000))
                qFatal("Migration worker did not stop");
            delete thread;
            return success;
        };
        QVERIFY(!migrate()); // Never prepare a legacy Jingle JID on the worker.
        QVERIFY(HistoryMediaRefs::prepareLegacySfs(history));
        QVERIFY(migrate());
        QCOMPARE(linkCount(), 1);
    }

    void sfsJingleIsIndexedBeforeEnteringTheWorker()
    {
        migrateHistory();
        Jingle::JinglePub publication;
        publication.setFrom(alice);
        publication.setId("source");
        publication.addDescription(Jingle::FileTransfer::NS);
        StatelessFileSharing::Sources sources;
        sources.add(StatelessFileSharing::Source::fromJinglePub(publication));
        StatelessFileSharing::FileSharing sharing;
        sharing.setFile(mediaOf("payload", {}).file);
        sharing.setSources(sources);
        Message message = messageWith(alice, {});
        message.addFileSharing(sharing);
        const auto extra = HistoryMessageExtras::encode(message);
        QVERIFY(QJsonDocument::fromJson(extra.toUtf8()).object().value("psi:media-refs").isObject());

        struct Worker {
            QThread               thread;
            HistorySqlReadWorker *db;
            explicit Worker(const QString &path) : db(new HistorySqlReadWorker(path))
            {
                db->setEventStoredHook([](QSqlDatabase connection, qint64 id, bool *removed) {
                    return HistoryMediaRefs::link(connection, id, removed);
                });
                db->moveToThread(&thread);
                QObject::connect(&thread, &QThread::started, db, &HistorySqlReadWorker::open);
                QObject::connect(db, &HistorySqlReadWorker::stopped, &thread, &QThread::quit, Qt::DirectConnection);
                QObject::connect(&thread, &QThread::finished, db, &QObject::deleteLater);
                thread.start();
            }
            ~Worker()
            {
                QMetaObject::invokeMethod(db, "close", Qt::QueuedConnection);
                if (!thread.wait(5000))
                    qFatal("History worker did not stop");
            }
        } worker(history.databaseName());
        QSignalSpy read(worker.db, &HistorySqlReadWorker::completed);
        QSignalSpy wrote(worker.db, &HistorySqlReadWorker::writeCompleted);
        qRegisterMetaType<HistoryReadRequest>("HistoryReadRequest");
        qRegisterMetaType<HistoryWriteRequest>("HistoryWriteRequest");
        HistoryWriteRequest append;
        append.account       = corporate;
        append.contactPeer   = key.peer;
        append.jidType       = 1;
        append.encoded.valid = append.encoded.isMessage = true;
        append.encoded.type                             = 1;
        append.encoded.direction                        = 2;
        append.encoded.timestamp                        = QDateTime::currentDateTimeUtc();
        append.encoded.extras                           = extra;
        append.encoded.sender                           = alice.full();
        QVERIFY(
            QMetaObject::invokeMethod(worker.db, "write", Qt::QueuedConnection, Q_ARG(HistoryWriteRequest, append)));
        HistoryReadRequest request;
        request.type = HistoryReadRequest::MediaLinks;
        QVERIFY(QMetaObject::invokeMethod(worker.db, "read", Qt::QueuedConnection, Q_ARG(HistoryReadRequest, request)));
        QTRY_COMPARE(read.count(), 1);
        QCOMPARE(wrote.count(), 1);
        QVERIFY(qvariant_cast<HistoryWriteResult>(wrote.first().first()).success);
        const auto result = qvariant_cast<HistoryReadResult>(read.takeFirst().first());
        QVERIFY(result.success);
        QCOMPARE(result.mediaLinks.size(), 1);
        const auto told = HistoryMediaRefs::announcement(result.mediaLinks.first());
        QVERIFY(told);
        QCOMPARE(told->file.reference.mediaSharing().sources,
                 QStringList { "xmpp:alice@example.org/desktop?jingle-ft" });
        request.archiveKey = key;
        request.type = HistoryReadRequest::ChatMediaLinks;
        request.direction = 1;
        QVERIFY(QMetaObject::invokeMethod(worker.db, "read", Qt::QueuedConnection, Q_ARG(HistoryReadRequest, request)));
        QTRY_COMPARE(read.count(), 1);
        const auto chatResult = qvariant_cast<HistoryReadResult>(read.takeFirst().first());
        QVERIFY(chatResult.success);
        QCOMPARE(chatResult.mediaLinks.size(), 1);
        QVERIFY(chatResult.mediaLinks.first().cursor.valid());
        QCOMPARE(chatResult.mediaLinks.first().account, corporate);
        request.type = HistoryReadRequest::ChatMediaStatistics;
        request.fileDatabasePath = FileDatabase::path();
        QVERIFY(QMetaObject::invokeMethod(worker.db, "read", Qt::QueuedConnection, Q_ARG(HistoryReadRequest, request)));
        QTRY_COMPARE(read.count(), 1);
        const auto statsResult = qvariant_cast<HistoryReadResult>(read.takeFirst().first());
        QVERIFY(statsResult.success);
        QCOMPARE(statsResult.mediaStatistics.remote, quint64(1));
        QSqlQuery damage(history);
        QVERIFY(damage.exec("SELECT sql FROM sqlite_master WHERE name='message_media_hashes'")); QVERIFY(damage.next());
        const auto repair = damage.value(0).toString(); damage.finish();
        QVERIFY(damage.exec("DROP TABLE message_media_hashes"));
        QVERIFY(QMetaObject::invokeMethod(worker.db, "read", Qt::QueuedConnection, Q_ARG(HistoryReadRequest, request)));
        QTRY_COMPARE(read.count(), 1);
        QVERIFY(!qvariant_cast<HistoryReadResult>(read.takeFirst().first()).success);
        QVERIFY(damage.exec(repair));
        QVERIFY(QMetaObject::invokeMethod(worker.db, "read", Qt::QueuedConnection, Q_ARG(HistoryReadRequest, request)));
        QTRY_COMPARE(read.count(), 1);
        QVERIFY(qvariant_cast<HistoryReadResult>(read.takeFirst().first()).success);
        request.fileDatabasePath.clear();
        request.archiveKey = {};
        request.type           = HistoryReadRequest::MediaLinked;
        request.publicationIds = { result.mediaLinks.first().publicationId };
        QVERIFY(QMetaObject::invokeMethod(worker.db, "read", Qt::QueuedConnection, Q_ARG(HistoryReadRequest, request)));
        QTRY_COMPARE(read.count(), 1);
        const auto linked = qvariant_cast<HistoryReadResult>(read.takeFirst().first());
        QVERIFY(linked.success);
        QVERIFY(linked.linkedPublications.contains(request.publicationIds.first()));
    }

    void legacyDirectoryWaitsForBothPayloadsAndPublications()
    {
        const auto legacy = dir->filePath(QStringLiteral("legacy"));
        QVERIFY(QDir().mkpath(legacy));
        const QStringList profiles { QFileInfo(profileDirectory).fileName() };
        const QStringList components { QString(), QStringLiteral("publications") };
        FileDatabase::markLegacyDirectoryImported(legacy);
        FileDatabase::retireLegacyDirectory(legacy, profiles, components);
        QVERIFY(QDir(legacy).exists()); // A payload import alone cannot retire the only catalog.
        FileDatabase::markLegacyDirectoryImported(legacy, QStringLiteral("publications"));
        FileDatabase::retireLegacyDirectory(legacy, profiles, components);
        QVERIFY(!QDir(legacy).exists());
    }

    void unreadableLegacyCatalogCanBeRetried()
    {
        const auto path = dir->filePath(QStringLiteral("broken.sqlite"));
        QFile      file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("not a SQLite database"), qint64(21));
        file.close();
        QVERIFY(!catalog->importLegacy(path, { { corporate, self } }));
        QSqlQuery check(FileDatabase::connection());
        QVERIFY(check.exec("SELECT count(*) FROM file_legacy_imports WHERE partition='publications'") && check.next());
        QCOMPARE(check.value(0).toInt(), 0);
    }

    void aChangedStoredAttachmentReplacesItsPublication()
    {
        migrateHistory();
        const auto old   = mediaOf("first", { QStringLiteral("https://example.org/old") });
        const auto fresh = mediaOf("second", { QStringLiteral("https://example.org/new") });
        const auto event = store(messageWith(alice, { referenceOf(old) }));
        syncEverything();
        const auto before = links(event);
        QSqlQuery  change(history);
        change.prepare("UPDATE events SET extra_data=:extra WHERE id=:id");
        change.bindValue(":extra", HistoryMessageExtras::encode(messageWith(alice, { referenceOf(fresh) })));
        change.bindValue(":id", event);
        QVERIFY(change.exec());
        QVERIFY(HistoryMediaRefs::link(history, event));
        QVERIFY(links(event) != before);
        syncEverything();
        QVERIFY(!catalog->find(corporate, old.file.computedHashes().first()));
        QVERIFY(catalog->find(corporate, fresh.file.computedHashes().first()));
    }

    void publicationWriteFailureIsRetriedWithoutRestart()
    {
        migrateHistory();
        FileSharingPublicationSync sync(historyAccess(), catalog.get());
        sync.reconcile();
        QTRY_VERIFY(sync.isIdle());
        QSqlQuery fail(FileDatabase::connection());
        QVERIFY(fail.exec("CREATE TRIGGER reject_publication BEFORE INSERT ON publications "
                          "BEGIN SELECT RAISE(ABORT,'test write failure'); END"));
        const auto media = mediaOf("payload", { QStringLiteral("https://example.org/file") });
        QVERIFY(store(messageWith(alice, { referenceOf(media) })));
        sync.linksAdded();
        QTest::qWait(100);
        QVERIFY(!catalog->find(corporate, media.file.computedHashes().first()));
        QVERIFY(fail.exec("DROP TRIGGER reject_publication"));
        QTRY_VERIFY(catalog->find(corporate, media.file.computedHashes().first()));
    }

    void legacyServingIsNotReimportedAfterFileDatabaseLoss()
    {
        const auto path  = dir->filePath(QStringLiteral("legacy-serving.sqlite"));
        const auto media = mediaOf("shared", { QStringLiteral("https://example.org/shared") });
        const auto name  = QStringLiteral("legacy-serving");
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", name);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery query(db);
            QVERIFY(query.exec("CREATE TABLE shared_files(account_id TEXT,hash TEXT,descriptor TEXT)"));
            QVERIFY(query.exec("CREATE TABLE served_files(account_id TEXT,hash TEXT)"));
            QVERIFY(query.exec("CREATE TABLE served_files_imported(account_id TEXT)"));
            QVERIFY(query.exec("INSERT INTO served_files_imported VALUES('corporate')"));
            query.prepare("INSERT INTO shared_files VALUES('corporate',:hash,:descriptor)");
            query.bindValue(":hash", media.file.computedHashes().first().toString());
            query.bindValue(
                ":descriptor",
                QString::fromUtf8(
                    QJsonDocument(legacyDescriptor(media, { self.full() })).toJson(QJsonDocument::Compact)));
            QVERIFY(query.exec());
            query.prepare("INSERT INTO served_files VALUES('corporate',:hash)");
            query.bindValue(":hash", media.file.computedHashes().first().toString());
            QVERIFY(query.exec());
            db.close();
        }
        QSqlDatabase::removeDatabase(name);
        QVERIFY(catalog->importLegacy(path, { { corporate, self } }));
        QVERIFY(catalog->isServed(corporate, media.file.computedHashes()));
        loseFileDatabase();
        QVERIFY(QFile::exists(path)); // Another profile may still need this input.
        QVERIFY(catalog->importLegacy(path, { { corporate, self } }));
        QVERIFY(!catalog->isServed(corporate, media.file.computedHashes()));
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
