/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingcatalog.h"

#include "filedatabase.h"

#include <QDateTime>
#include <QDebug>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <algorithm>

using XMPP::Jingle::FileTransfer::File;

namespace {
const QString legacyImportMark = QStringLiteral("publications");

struct LocalCopy {
    QString path;
    qint64  size = 0, modified = 0;
};

// A file is only as good as it was when it was published.
LocalCopy localCopyOf(const QString &path, const File &file)
{
    const QFileInfo local(path);
    if (path.isEmpty() || !local.isFile() || !local.isReadable() || !file.size()
        || quint64(local.size()) != *file.size())
        return {};
    return { local.absoluteFilePath(), local.size(), local.lastModified().toMSecsSinceEpoch() };
}

bool parseXml(QDomDocument &doc, const QString &xml)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return bool(doc.setContent(xml, QDomDocument::ParseOption::UseNamespaceProcessing));
#else
    return doc.setContent(xml, true);
#endif
}

QString fileXml(const File &file)
{
    QDomDocument doc;
    // The Jingle form requires hashes, which a URL-only file does not have.
    doc.appendChild(file.hasComputedHashes() ? file.toXml(&doc) : file.toMetadataXml(&doc));
    return doc.toString(-1);
}

bool insert(QSqlDatabase db, const FileSharingCatalog::PublicationId &id, const QString &account,
            const XMPP::Jid &announcer, bool serving, const QString &urlKey, const XMPP::MediaSharing &media,
            const LocalCopy &local)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO publications(id,account,announcer,serving,url_key,descriptor,local_path,local_size,"
        "local_modified) VALUES(:id,:account,:announcer,:serving,:url_key,:descriptor,:path,:size,:modified)"));
    query.bindValue(QStringLiteral(":id"), id);
    query.bindValue(QStringLiteral(":account"), account);
    query.bindValue(QStringLiteral(":announcer"), announcer.full());
    query.bindValue(QStringLiteral(":serving"), serving);
    query.bindValue(QStringLiteral(":url_key"), urlKey);
    query.bindValue(QStringLiteral(":descriptor"), fileXml(media.file));
    query.bindValue(QStringLiteral(":path"), local.path);
    query.bindValue(QStringLiteral(":size"), local.size);
    query.bindValue(QStringLiteral(":modified"), local.modified);
    if (!query.exec())
        return false;
    if (query.numRowsAffected() < 1)
        return true; // Told again, and known already.
    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO publication_hashes(publication_id,algorithm,digest) VALUES(:id,:algorithm,:digest)"));
    for (const auto &hash : media.file.computedHashes()) {
        query.bindValue(QStringLiteral(":id"), id);
        query.bindValue(QStringLiteral(":algorithm"), hash.stringType());
        query.bindValue(QStringLiteral(":digest"), hash.data());
        if (!query.exec())
            return false;
    }
    query.prepare(
        QStringLiteral("INSERT INTO publication_sources(publication_id,position,uri) VALUES(:id,:position,:uri)"));
    for (int position = 0; position < media.sources.size(); ++position) {
        query.bindValue(QStringLiteral(":id"), id);
        query.bindValue(QStringLiteral(":position"), position);
        query.bindValue(QStringLiteral(":uri"), media.sources.at(position));
        if (!query.exec())
            return false;
    }
    return true;
}

bool removeServing(QSqlDatabase db, const QString &account, const QList<XMPP::Hash> &hashes)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM publications WHERE serving=1 AND account=:account AND id IN ("
                                 "SELECT publication_id FROM publication_hashes "
                                 "WHERE algorithm=:algorithm AND digest=:digest)"));
    for (const auto &hash : hashes) {
        query.bindValue(QStringLiteral(":account"), account);
        query.bindValue(QStringLiteral(":algorithm"), hash.stringType());
        query.bindValue(QStringLiteral(":digest"), hash.data());
        if (!query.exec())
            return false;
    }
    return true;
}

// Takes the rows of one file's publications, the first ones decisive: a later one that
// contradicts them is about another file, whatever it claims.
std::optional<FileSharingCatalog::Record> merged(QSqlDatabase db, QSqlQuery &rows)
{
    std::optional<FileSharingCatalog::Record> record;
    QSqlQuery                                 sources(db);
    sources.prepare(QStringLiteral("SELECT uri FROM publication_sources WHERE publication_id=:id ORDER BY position"));
    while (rows.next()) {
        QDomDocument doc;
        if (!parseXml(doc, rows.value(2).toString()))
            continue;
        const File file(doc.documentElement());
        if (!file.isValid())
            continue;
        if (!record) {
            record             = FileSharingCatalog::Record {};
            record->media.file = file;
        } else {
            const auto size = record->media.file.size();
            auto       both = record->media.file;
            if ((size && file.size() && *size != *file.size()) || !both.merge(file))
                continue;
            record->media.file = both;
        }
        sources.bindValue(QStringLiteral(":id"), rows.value(0));
        if (sources.exec()) {
            QStringList uris;
            while (sources.next())
                uris.append(sources.value(0).toString());
            record->media.sources = uris + record->media.sources; // The newest come first.
        }
        const auto announcer = rows.value(1).toString();
        if (!announcer.isEmpty() && !record->peers.contains(announcer))
            record->peers.append(announcer);
        if (record->localPath.isEmpty() && !rows.value(3).toString().isEmpty()) {
            record->localPath     = rows.value(3).toString();
            record->localSize     = rows.value(4).toLongLong();
            record->localModified = rows.value(5).toLongLong();
        }
    }
    if (record)
        record->media.sources.removeDuplicates();
    return record;
}

// What the catalog before publications shared a file with.
struct LegacyDescriptor {
    XMPP::MediaSharing media;
    QStringList        peers;
    LocalCopy          local;
};

std::optional<LegacyDescriptor> parseLegacyDescriptor(const QString &descriptor)
{
    const auto      json = QJsonDocument::fromJson(descriptor.toUtf8()).object();
    QDomDocument    doc;
    XMPP::Reference reference;
    if (json.value(QStringLiteral("version")).toInt() != 1
        || !parseXml(doc, json.value(QStringLiteral("reference")).toString())
        || doc.documentElement().namespaceURI() != XMPP::REFERENCE_NS || !reference.fromXml(doc.documentElement())
        || !reference.mediaSharing().isValid())
        return {};
    LegacyDescriptor result;
    result.media = reference.mediaSharing();
    for (const auto &peer : json.value(QStringLiteral("peers")).toArray())
        result.peers.append(peer.toString());
    const auto local      = json.value(QStringLiteral("local")).toObject();
    result.local.path     = local.value(QStringLiteral("path")).toString();
    result.local.size     = local.value(QStringLiteral("size")).toString().toLongLong();
    result.local.modified = local.value(QStringLiteral("modified")).toString().toLongLong();
    return result;
}

bool importSharedFiles(QSqlDatabase legacy, QSqlDatabase db, const QHash<QString, XMPP::Jid> &accounts)
{
    QSqlQuery     read(legacy);
    QSet<QString> decided, served;
    if (read.exec(QStringLiteral("SELECT account_id FROM served_files_imported")))
        while (read.next())
            decided.insert(read.value(0).toString());
    if (read.exec(QStringLiteral("SELECT account_id,hash FROM served_files")))
        while (read.next())
            served.insert(read.value(0).toString() + QLatin1Char('\n') + read.value(1).toString());
    if (!read.exec(QStringLiteral("SELECT account_id,hash,descriptor FROM shared_files")))
        return true;
    QSet<QString> imported; // One descriptor was stored per hash of its file.
    while (read.next()) {
        const auto account = read.value(0).toString();
        const auto key     = account + QLatin1Char('\n') + read.value(2).toString();
        if (!accounts.contains(account) || imported.contains(key))
            continue;
        const auto descriptor = parseLegacyDescriptor(read.value(2).toString());
        if (!descriptor)
            continue;
        const auto self = accounts.value(account);
        // What an account shared was recognized by its own JID and a local copy,
        // until the catalog began to record it.
        const bool own = decided.contains(account)
            ? served.contains(account + QLatin1Char('\n') + read.value(1).toString())
            : !descriptor->local.path.isEmpty()
                && std::any_of(descriptor->peers.cbegin(), descriptor->peers.cend(),
                               [&self](const QString &peer) { return XMPP::Jid(peer).compare(self, false); });
        if (!own)
            continue;
        imported.insert(key);
        if (!insert(db, QUuid::createUuid().toRfc4122(), account, self, true, {}, descriptor->media, descriptor->local))
            return false;
    }
    return true;
}

bool importUrlContent(QSqlDatabase legacy, QSqlDatabase db)
{
    // The first version of these mappings had no timestamps. They are only a cache.
    QSqlQuery read(legacy);
    if (!read.exec(QStringLiteral("SELECT account_id,url_key,content_hash,updated FROM url_content")))
        return true;
    QSqlQuery write(db);
    write.prepare(QStringLiteral("INSERT OR IGNORE INTO url_content(account,url_key,content_hash,updated) "
                                 "VALUES(:account,:key,:content_hash,:updated)"));
    while (read.next()) {
        write.bindValue(QStringLiteral(":account"), read.value(0));
        write.bindValue(QStringLiteral(":key"), read.value(1));
        write.bindValue(QStringLiteral(":content_hash"), read.value(2));
        write.bindValue(QStringLiteral(":updated"), read.value(3));
        if (!write.exec())
            return false;
    }
    return true;
}
}

QString FileSharingCatalog::Record::readableLocalPath() const
{
    const QFileInfo file(localPath);
    if (localPath.isEmpty() || !file.isFile() || !file.isReadable() || file.size() != localSize
        || file.lastModified().toMSecsSinceEpoch() != localModified)
        return {};
    return localPath;
}

FileSharingCatalog::FileSharingCatalog()
{
    const auto db = FileDatabase::connection();
    if (!db.isOpen())
        return;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM url_content WHERE updated < :cutoff"));
    query.bindValue(QStringLiteral(":cutoff"), QDateTime::currentSecsSinceEpoch() - UrlContentTtl);
    query.exec();
}

bool FileSharingCatalog::add(const PublicationId &id, const QString &account, const XMPP::Jid &announcer,
                             const MessageAttachments::Attachment &attachment)
{
    auto db = FileDatabase::connection();
    if (!db.isOpen() || account.isEmpty() || !db.transaction())
        return false;
    bool ok = false;
    if (attachment.isFile()) {
        ok = insert(db, id, account, announcer, false, {}, attachment.reference.mediaSharing(), {});
    } else if (const auto url = attachment.url(); url.isValid()) {
        XMPP::MediaSharing media;
        media.file.setDescription(attachment.description);
        media.sources.append(attachment.reference.uri());
        ok = insert(db, id, account, announcer, false, MessageAttachments::urlIdentity(url).toString(), media, {});
    }
    if (ok && db.commit())
        return true;
    db.rollback();
    return false;
}

void FileSharingCatalog::remove(const QList<PublicationId> &ids)
{
    auto db = FileDatabase::connection();
    if (!db.isOpen() || !db.transaction())
        return;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM publications WHERE id=:id"));
    for (const auto &id : ids) {
        query.bindValue(QStringLiteral(":id"), id);
        query.exec();
    }
    query.finish();
    if (!db.commit())
        db.rollback();
}

QList<FileSharingCatalog::PublicationId> FileSharingCatalog::missing(const QList<PublicationId> &ids) const
{
    const auto db = FileDatabase::connection();
    if (!db.isOpen())
        return {};
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT 1 FROM publications WHERE id=:id"));
    QList<PublicationId> result;
    for (const auto &id : ids) {
        query.bindValue(QStringLiteral(":id"), id);
        if (!query.exec())
            return {};
        if (!query.next())
            result.append(id);
        query.finish();
    }
    return result;
}

QList<FileSharingCatalog::PublicationId> FileSharingCatalog::announcementsAfter(const PublicationId &id,
                                                                                int                  limit) const
{
    const auto db = FileDatabase::connection();
    if (!db.isOpen())
        return {};
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT id FROM publications WHERE id>:id AND serving=0 ORDER BY id LIMIT :limit"));
    query.bindValue(QStringLiteral(":id"), id.isEmpty() ? QByteArray(1, '\0') : id); // That is below any ID.
    query.bindValue(QStringLiteral(":limit"), limit);
    QList<PublicationId> result;
    if (query.exec())
        while (query.next())
            result.append(query.value(0).toByteArray());
    return result;
}

std::optional<FileSharingCatalog::Record> FileSharingCatalog::find(const QString &account, const XMPP::Hash &hash) const
{
    const auto db = FileDatabase::connection();
    if (!db.isOpen() || account.isEmpty() || !hash.isValid() || hash.data().isEmpty())
        return {};
    QSqlQuery rows(db);
    rows.prepare(QStringLiteral(
        "SELECT p.id,p.announcer,p.descriptor,p.local_path,p.local_size,p.local_modified FROM publication_hashes h "
        "JOIN publications p ON p.id=h.publication_id "
        "WHERE h.algorithm=:algorithm AND h.digest=:digest AND p.account=:account ORDER BY p.serving DESC,p.rowid"));
    rows.bindValue(QStringLiteral(":algorithm"), hash.stringType());
    rows.bindValue(QStringLiteral(":digest"), hash.data());
    rows.bindValue(QStringLiteral(":account"), account);
    if (rows.exec())
        if (auto record = merged(db, rows))
            return record;
    rows.prepare(QStringLiteral("SELECT id,announcer,descriptor,local_path,local_size,local_modified "
                                "FROM publications WHERE account=:account AND url_key=:key ORDER BY rowid"));
    rows.bindValue(QStringLiteral(":account"), account);
    rows.bindValue(QStringLiteral(":key"), hash.toString());
    if (!rows.exec())
        return {};
    auto record = merged(db, rows);
    if (record)
        record->urlOnly = true;
    return record;
}

bool FileSharingCatalog::serve(const QString &account, const XMPP::Jid &self, const XMPP::MediaSharing &media,
                               const QString &localPath)
{
    auto db = FileDatabase::connection();
    if (!db.isOpen() || account.isEmpty() || !media.file.hasComputedHashes() || !db.transaction())
        return false;
    if (removeServing(db, account, media.file.computedHashes())
        && insert(db, QUuid::createUuid().toRfc4122(), account, self, true, {}, media,
                  localCopyOf(localPath, media.file))
        && db.commit())
        return true;
    db.rollback();
    return false;
}

void FileSharingCatalog::stopServing(const QString &account, const QList<XMPP::Hash> &hashes)
{
    const auto db = FileDatabase::connection();
    if (db.isOpen())
        removeServing(db, account, hashes);
}

bool FileSharingCatalog::isServed(const QString &account, const QList<XMPP::Hash> &hashes) const
{
    const auto db = FileDatabase::connection();
    if (!db.isOpen() || account.isEmpty())
        return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT 1 FROM publication_hashes h JOIN publications p ON p.id=h.publication_id "
                                 "WHERE h.algorithm=:algorithm AND h.digest=:digest AND p.account=:account "
                                 "AND p.serving=1 LIMIT 1"));
    for (const auto &hash : hashes) {
        if (!hash.isValid() || hash.data().isEmpty())
            continue;
        query.bindValue(QStringLiteral(":algorithm"), hash.stringType());
        query.bindValue(QStringLiteral(":digest"), hash.data());
        query.bindValue(QStringLiteral(":account"), account);
        if (query.exec() && query.next())
            return true;
    }
    return false;
}

std::optional<XMPP::Hash> FileSharingCatalog::contentForUrl(const QString &account, const XMPP::Hash &urlIdentity) const
{
    const auto db = FileDatabase::connection();
    if (!db.isOpen() || account.isEmpty() || !urlIdentity.isValid())
        return {};
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT content_hash FROM url_content WHERE account=:account AND url_key=:key"));
    query.bindValue(QStringLiteral(":account"), account);
    query.bindValue(QStringLiteral(":key"), urlIdentity.toString());
    if (!query.exec() || !query.next())
        return {};
    const auto hash = XMPP::Hash::from(query.value(0).toString());
    return hash.isValid() ? std::optional<XMPP::Hash> { hash } : std::nullopt;
}

bool FileSharingCatalog::rememberUrlContent(const QString &account, const XMPP::Hash &urlIdentity,
                                            const XMPP::Hash &contentHash)
{
    const auto db = FileDatabase::connection();
    if (!db.isOpen() || account.isEmpty() || !urlIdentity.isValid() || !contentHash.isValid())
        return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO url_content(account,url_key,content_hash,updated) "
                                 "VALUES(:account,:key,:content_hash,:updated)"));
    query.bindValue(QStringLiteral(":account"), account);
    query.bindValue(QStringLiteral(":key"), urlIdentity.toString());
    query.bindValue(QStringLiteral(":content_hash"), contentHash.toString());
    query.bindValue(QStringLiteral(":updated"), QDateTime::currentSecsSinceEpoch());
    return query.exec();
}

void FileSharingCatalog::forgetUrlContent(const QString &account, const XMPP::Hash &urlIdentity)
{
    const auto db = FileDatabase::connection();
    if (!db.isOpen() || account.isEmpty() || !urlIdentity.isValid())
        return;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM url_content WHERE account=:account AND url_key=:key"));
    query.bindValue(QStringLiteral(":account"), account);
    query.bindValue(QStringLiteral(":key"), urlIdentity.toString());
    query.exec();
}

bool FileSharingCatalog::importLegacy(const QString &path, const QHash<QString, XMPP::Jid> &accounts)
{
    auto db = FileDatabase::connection();
    if (!db.isOpen() || !db.transaction())
        return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO file_legacy_imports(partition) VALUES(:mark)"));
    query.bindValue(QStringLiteral(":mark"), legacyImportMark);
    if (!query.exec()) {
        db.rollback();
        return false;
    }
    if (query.numRowsAffected() < 1) { // Imported earlier.
        db.rollback();
        return true;
    }
    query.finish();

    bool ok = true;
    if (QFile::exists(path)) {
        const auto connection = QStringLiteral("psi-legacy-shared-files");
        {
            auto legacy = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
            legacy.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
            legacy.setDatabaseName(path);
            ok = !legacy.open() || (importSharedFiles(legacy, db, accounts) && importUrlContent(legacy, db));
            legacy.close();
        }
        QSqlDatabase::removeDatabase(connection);
    }
    if (ok && db.commit())
        return true;
    qWarning() << "Cannot import the legacy shared file catalog:" << db.lastError().text();
    db.rollback();
    return false;
}
