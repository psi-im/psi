/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingstore.h"
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace {
bool parseReferenceXml(QDomDocument &doc, const QString &xml)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return bool(doc.setContent(xml, QDomDocument::ParseOption::UseNamespaceProcessing));
#else
    return doc.setContent(xml, true);
#endif
}
}

QString FileSharingStore::Record::readableLocalPath() const
{
    const QFileInfo file(localPath);
    if (localPath.isEmpty() || !file.isFile() || !file.isReadable() || QString::number(file.size()) != localSize
        || QString::number(file.lastModified().toMSecsSinceEpoch()) != localModified)
        return {};
    return localPath;
}

FileSharingStore::FileSharingStore(const QString &path) :
    name_(QStringLiteral("shared-files-") + QUuid::createUuid().toString()),
    db_(QSqlDatabase::addDatabase("QSQLITE", name_))
{
    db_.setDatabaseName(path);
    if (db_.open()) {
        QSqlQuery query(db_);
        ready_ = query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS shared_files ("
                                           "account_id TEXT NOT NULL, hash TEXT NOT NULL, descriptor TEXT NOT NULL, "
                                           "PRIMARY KEY(account_id, hash))"));
        if (ready_)
            ready_ = query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS url_content ("
                                               "account_id TEXT NOT NULL, url_key TEXT NOT NULL, "
                                               "content_hash TEXT NOT NULL, "
                                               "PRIMARY KEY(account_id, url_key))"));
    }
    if (!ready_)
        qWarning() << "Cannot open shared file catalog:" << db_.lastError().text();
}

FileSharingStore::~FileSharingStore()
{
    db_.close();
    db_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(name_);
}

std::optional<FileSharingStore::Record> FileSharingStore::find(const QString &account, const XMPP::Hash &hash) const
{
    if (!ready_ || account.isEmpty() || !hash.isValid())
        return {};
    QSqlQuery query(db_);
    query.prepare("SELECT descriptor FROM shared_files WHERE account_id=:account AND hash=:hash");
    query.bindValue(":account", account);
    query.bindValue(":hash", hash.toString());
    if (!query.exec() || !query.next())
        return {};
    const auto   json = QJsonDocument::fromJson(query.value(0).toString().toUtf8()).object();
    QDomDocument doc;
    Record       record;
    if (json.value("version").toInt() != 1 || !parseReferenceXml(doc, json.value("reference").toString())
        || doc.documentElement().namespaceURI() != XMPP::REFERENCE_NS
        || !record.reference.fromXml(doc.documentElement()) || !record.reference.mediaSharing().isValid()
        || !record.reference.mediaSharing().file.computedHashes().contains(hash))
        return {};
    for (const auto &peer : json.value("peers").toArray()) {
        const XMPP::Jid jid(peer.toString());
        if (jid.isValid() && !jid.isEmpty())
            record.peers.append(jid.full());
    }
    const auto local     = json.value("local").toObject();
    record.localPath     = local.value("path").toString();
    record.localSize     = local.value("size").toString();
    record.localModified = local.value("modified").toString();
    return record;
}

std::optional<XMPP::Hash> FileSharingStore::contentForUrl(const QString &account,
                                                         const XMPP::Hash &urlIdentity) const
{
    if (!ready_ || account.isEmpty() || !urlIdentity.isValid())
        return {};
    QSqlQuery query(db_);
    query.prepare("SELECT content_hash FROM url_content WHERE account_id=:account AND url_key=:url_key");
    query.bindValue(":account", account);
    query.bindValue(":url_key", urlIdentity.toString());
    if (!query.exec() || !query.next())
        return {};
    const auto hash = XMPP::Hash::from(query.value(0).toString());
    return hash.isValid() ? std::optional<XMPP::Hash> { hash } : std::nullopt;
}

bool FileSharingStore::rememberUrlContent(const QString &account, const XMPP::Hash &urlIdentity,
                                          const XMPP::Hash &contentHash)
{
    if (!ready_ || account.isEmpty() || !urlIdentity.isValid() || !contentHash.isValid())
        return false;
    QSqlQuery query(db_);
    query.prepare("INSERT OR REPLACE INTO url_content(account_id,url_key,content_hash) "
                  "VALUES(:account,:url_key,:content_hash)");
    query.bindValue(":account", account);
    query.bindValue(":url_key", urlIdentity.toString());
    query.bindValue(":content_hash", contentHash.toString());
    return query.exec();
}

bool FileSharingStore::remember(const QString &account, const XMPP::Reference &reference, const XMPP::Jid &peer,
                                const QString &trustedLocalPath)
{
    if (!ready_ || account.isEmpty() || !reference.isValid() || !reference.mediaSharing().isValid()
        || !reference.mediaSharing().file.hasComputedHashes() || reference.mediaSharing().sources.isEmpty())
        return false;
    // XEP-0385 shares may omit the outer reference URI; use the first source.
    const QString uri = !reference.uri().isEmpty() ? reference.uri() : reference.mediaSharing().sources.first();
    Record record;
    auto   media = reference.mediaSharing();
    for (const auto &hash : media.file.computedHashes()) {
        const auto previous = find(account, hash);
        if (!previous)
            continue;
        if (previous->reference.mediaSharing().file.size() != media.file.size())
            return false; // One identity must not silently change its file size.
        record = *previous;
        media.sources.append(record.reference.mediaSharing().sources);
        auto hashes = media.file.hashes();
        for (const auto &oldHash : record.reference.mediaSharing().file.computedHashes())
            if (!hashes.contains(oldHash))
                hashes.append(oldHash);
        media.file.setHashes(hashes);
        break;
    }
    media.sources.removeDuplicates();
    record.reference = XMPP::Reference(XMPP::Reference::Data, uri);
    record.reference.setMediaSharing(media);
    if (peer.isValid() && !peer.isEmpty() && !record.peers.contains(peer.full()))
        record.peers.append(peer.full());
    const QFileInfo local(trustedLocalPath);
    if (!trustedLocalPath.isEmpty() && local.isFile() && local.isReadable() && media.file.size()
        && quint64(local.size()) == *media.file.size()) {
        record.localPath     = local.absoluteFilePath();
        record.localSize     = QString::number(local.size());
        record.localModified = QString::number(local.lastModified().toMSecsSinceEpoch());
    }
    QDomDocument doc;
    doc.appendChild(record.reference.toXml(&doc));
    const QJsonObject json { { "version", 1 },
                             { "reference", doc.toString(-1) },
                             { "peers", QJsonArray::fromStringList(record.peers) },
                             { "local",
                               QJsonObject { { "path", record.localPath },
                                             { "size", record.localSize },
                                             { "modified", record.localModified } } } };
    const auto        descriptor = QString::fromUtf8(QJsonDocument(json).toJson(QJsonDocument::Compact));
    if (!db_.transaction())
        return false;
    for (const auto &hash : media.file.computedHashes()) {
        QSqlQuery query(db_);
        query.prepare(
            "INSERT OR REPLACE INTO shared_files(account_id,hash,descriptor) VALUES(:account,:hash,:descriptor)");
        query.bindValue(":account", account);
        query.bindValue(":hash", hash.toString());
        query.bindValue(":descriptor", descriptor);
        if (!query.exec()) {
            db_.rollback();
            return false;
        }
    }
    if (db_.commit())
        return true;
    db_.rollback();
    return false;
}
