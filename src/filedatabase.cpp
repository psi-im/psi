/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filedatabase.h"

#include "applicationinfo.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#else
#include <sys/stat.h>
#endif

namespace {
const int SchemaVersion = 7;

// What each schema version adds to the one before it.
const QList<QStringList> &schemaUpgrades()
{
    static const QList<QStringList> upgrades {
        { "CREATE TABLE files ("
          "id BLOB PRIMARY KEY, partition TEXT NOT NULL, size INTEGER NOT NULL, "
          "created INTEGER NOT NULL, max_age INTEGER NOT NULL, metadata BLOB NOT NULL)",
          "CREATE INDEX files_partition ON files(partition)",
          "CREATE TABLE file_hashes ("
          "file_id BLOB NOT NULL REFERENCES files(id) ON DELETE CASCADE, position INTEGER NOT NULL, "
          "algorithm TEXT NOT NULL, digest BLOB NOT NULL, PRIMARY KEY(file_id, position))",
          "CREATE INDEX file_hashes_digest ON file_hashes(algorithm, digest)",
          "CREATE TABLE file_legacy_imports (partition TEXT NOT NULL PRIMARY KEY)" },
        { "CREATE TABLE publications ("
          "id BLOB PRIMARY KEY, account TEXT NOT NULL, announcer TEXT, serving INTEGER NOT NULL, url_key TEXT, "
          "descriptor TEXT NOT NULL, local_path TEXT, local_size INTEGER, local_modified INTEGER)",
          "CREATE INDEX publications_url ON publications(account, url_key) WHERE url_key IS NOT NULL",
          "CREATE TABLE publication_hashes ("
          "publication_id BLOB NOT NULL REFERENCES publications(id) ON DELETE CASCADE, "
          "algorithm TEXT NOT NULL, digest BLOB NOT NULL, PRIMARY KEY(publication_id, algorithm, digest))",
          "CREATE INDEX publication_hashes_digest ON publication_hashes(algorithm, digest)",
          "CREATE TABLE publication_sources ("
          "publication_id BLOB NOT NULL REFERENCES publications(id) ON DELETE CASCADE, "
          "position INTEGER NOT NULL, uri TEXT NOT NULL, PRIMARY KEY(publication_id, position))",
          "CREATE TABLE url_content ("
          "account TEXT NOT NULL, url_key TEXT NOT NULL, content_hash TEXT NOT NULL, updated INTEGER NOT NULL, "
          "PRIMARY KEY(account, url_key))" },
        { "ALTER TABLE files ADD COLUMN path TEXT", "ALTER TABLE files ADD COLUMN placement INTEGER NOT NULL DEFAULT 0",
          "ALTER TABLE files ADD COLUMN external_mtime INTEGER", "DROP INDEX IF EXISTS files_partition",
          "CREATE INDEX files_partition_path ON files(partition,path)",
          "CREATE TABLE file_trash (partition TEXT NOT NULL, path TEXT NOT NULL, "
          "PRIMARY KEY(partition,path))" },
        { "ALTER TABLE files ADD COLUMN resource_key TEXT",
          "ALTER TABLE files ADD COLUMN complete INTEGER NOT NULL DEFAULT 1",
          "ALTER TABLE files ADD COLUMN payload_modified INTEGER",
          "ALTER TABLE files ADD COLUMN error TEXT NOT NULL DEFAULT ''",
          "CREATE UNIQUE INDEX files_resource_key ON files(partition,resource_key) WHERE resource_key IS NOT NULL",
          "CREATE TABLE file_ranges (file_id BLOB NOT NULL REFERENCES files(id) ON DELETE CASCADE, "
          "offset INTEGER NOT NULL CHECK(offset>=0), length INTEGER NOT NULL CHECK(length>0), "
          "PRIMARY KEY(file_id,offset))",
          "CREATE TABLE file_checksums (file_id BLOB NOT NULL REFERENCES files(id) ON DELETE CASCADE, "
          "offset INTEGER NOT NULL CHECK(offset>=0), length INTEGER NOT NULL CHECK(length>=0), "
          "algorithm TEXT NOT NULL, digest BLOB NOT NULL, verified INTEGER NOT NULL DEFAULT 0, "
          "PRIMARY KEY(file_id,offset,length,algorithm,digest))" },
        { "CREATE INDEX file_hashes_key ON file_hashes(algorithm || '+' || lower(hex(digest)))",
          "ALTER TABLE files ADD COLUMN scope_account TEXT",
          "CREATE INDEX file_checksums_digest ON file_checksums(algorithm,digest,file_id) WHERE offset=0" },
        { "DROP INDEX file_hashes_key",
          "CREATE INDEX file_hashes_key ON file_hashes(CAST(algorithm || '+' || lower(hex(digest)) AS TEXT))" },
        { "CREATE INDEX IF NOT EXISTS url_content_hash ON url_content(account,content_hash)" }
        // The media type column and metadata backfill are handled together below.
    };
    return upgrades;
}

QString markerName(const QString &profile, const QString &component = {})
{
    return QStringLiteral("imported-") + (component.isEmpty() ? QString() : component + QLatin1Char('-')) + profile;
}

bool prepare(QSqlDatabase &db)
{
    QSqlQuery query(db);
    query.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    query.finish();
    if (!query.exec(QStringLiteral("PRAGMA synchronous=NORMAL"))
        || !query.exec(QStringLiteral("PRAGMA foreign_keys=ON")) || !query.exec(QStringLiteral("PRAGMA user_version"))
        || !query.next())
        return false;
    const int version = query.value(0).toInt();
    query.finish();
    if (version == SchemaVersion)
        return true;
    if (version < 0 || version > SchemaVersion)
        return false; // Written by a newer Psi.
    if (!db.transaction())
        return false;
    for (int upgrade = version; upgrade < SchemaVersion; ++upgrade) {
        for (const auto &statement : schemaUpgrades().at(upgrade)) {
            if (!query.exec(statement)) {
                db.rollback();
                return false;
            }
        }
        if (upgrade == 6) {
            if (!query.exec("SELECT media_type FROM files LIMIT 0")
                && !query.exec("ALTER TABLE files ADD COLUMN media_type TEXT NOT NULL DEFAULT ''")) {
                db.rollback();
                return false;
            }
            query.finish();
            QSqlQuery rows(db), update(db);
            if (!rows.exec("SELECT id,metadata FROM files")) {
                db.rollback();
                return false;
            }
            update.prepare("UPDATE files SET media_type=COALESCE(:mime,'') WHERE id=:id");
            while (rows.next()) {
                update.bindValue(":id", rows.value(0));
                update.bindValue(":mime",
                                 FileDatabase::decodeMetadata(rows.value(1).toByteArray()).value("type").toString());
                if (!update.exec()) {
                    db.rollback();
                    return false;
                }
            }
            if (rows.lastError().isValid()) {
                db.rollback();
                return false;
            }
        }
    }
    if (!query.exec(QStringLiteral("PRAGMA user_version=%1").arg(SchemaVersion)) || !db.commit()) {
        db.rollback();
        return false;
    }
    return true;
}

QString connectionName(const QString &path)
{
    return QStringLiteral("psi-files:%1:%2")
        .arg(path, QString::number(reinterpret_cast<quintptr>(QThread::currentThread()), 16));
}
}

QString FileDatabase::path()
{
    return ApplicationInfo::currentProfileDir(ApplicationInfo::DataLocation) + QStringLiteral("/files.sqlite");
}

QSqlDatabase FileDatabase::connection() { return connection(path()); }

QSqlDatabase FileDatabase::connection(const QString &databasePath)
{
    if (!QCoreApplication::instance())
        return {};
    const QString path = QFileInfo(databasePath).absoluteFilePath();
    const QString name = connectionName(path);
    if (QSqlDatabase::contains(name))
        return QSqlDatabase::database(name, false);
    QDir().mkpath(QFileInfo(path).absolutePath());
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db.setDatabaseName(path);
    if (!db.open() || !prepare(db)) {
        qWarning() << "Cannot open the file database:" << db.lastError().text();
        db.close();
    }
    return db;
}

void FileDatabase::closeConnection(const QString &path)
{
    const auto name = connectionName(QFileInfo(path).absoluteFilePath());
    if (!QSqlDatabase::contains(name))
        return;
    {
        auto db = QSqlDatabase::database(name, false);
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
}

QByteArray FileDatabase::encodeMetadata(const QVariantMap &metadata)
{
    QByteArray  bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_15);
    stream << metadata;
    return bytes;
}

QVariantMap FileDatabase::decodeMetadata(const QByteArray &bytes)
{
    QVariantMap metadata;
    QDataStream stream(bytes);
    stream.setVersion(QDataStream::Qt_5_15);
    stream >> metadata;
    return metadata;
}

bool FileDatabase::safePayloadName(const QString &name)
{
    return !name.isEmpty() && name != QLatin1String(".") && name != QLatin1String("..")
        && !name.contains(QLatin1Char('/')) && !name.contains(QLatin1Char('\\')) && !name.contains(QLatin1Char(':'))
        && !name.contains(QChar(0));
}

bool FileDatabase::retireFile(QSqlDatabase &db, const QByteArray &id, const QString &partition, const QString &payload,
                              bool inTransaction)
{
    if (!inTransaction && !db.transaction())
        return false;
    QSqlQuery query(db);
    bool      ok = true;
    if (!payload.isEmpty()) {
        query.prepare(QStringLiteral("INSERT OR IGNORE INTO file_trash(partition,path) VALUES(:partition,:path)"));
        query.bindValue(QStringLiteral(":partition"), partition);
        query.bindValue(QStringLiteral(":path"), payload);
        ok = query.exec();
    }
    query.prepare(QStringLiteral("DELETE FROM files WHERE id=:id AND partition=:partition"));
    query.bindValue(QStringLiteral(":id"), id);
    query.bindValue(QStringLiteral(":partition"), partition);
    ok = ok && query.exec();
    if (!inTransaction && (!ok || !db.commit())) {
        db.rollback();
        return false;
    }
    return ok;
}

quint64 FileDatabase::allocatedBytes(const QString &path)
{
    if (QFileInfo(path).isSymLink())
        return 0;
#ifdef Q_OS_WIN
    const auto native = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
    DWORD      high   = 0;
    SetLastError(NO_ERROR);
    const auto low = GetCompressedFileSizeW(reinterpret_cast<LPCWSTR>(native.utf16()), &high);
    return low == INVALID_FILE_SIZE && GetLastError() != NO_ERROR ? 0 : (quint64(high) << 32) | low;
#else
    struct stat info;
    return ::stat(QFile::encodeName(path).constData(), &info) == 0 && info.st_blocks > 0 ? quint64(info.st_blocks) * 512
                                                                                         : 0;
#endif
}

quint64 FileDatabase::drainTrash(QSqlDatabase &db, const QString &partition, const QString &directory)
{
    if (!db.isOpen())
        return 0;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT path FROM file_trash WHERE partition=:partition"));
    query.bindValue(QStringLiteral(":partition"), partition);
    if (!query.exec())
        return 0;
    QStringList paths;
    while (query.next())
        paths.append(query.value(0).toString());
    query.finish();
    if (paths.isEmpty())
        return 0;
    query.prepare(QStringLiteral("SELECT 1 FROM files WHERE partition=:partition AND path IS NULL LIMIT 1"));
    query.bindValue(QStringLiteral(":partition"), partition);
    if (!query.exec() || query.next())
        return 0;
    query.finish();
    quint64 freed = 0;
    for (const auto &path : paths) {
        if (!safePayloadName(path))
            continue;
        query.prepare(QStringLiteral("SELECT 1 FROM files WHERE partition=:partition AND path=:path LIMIT 1"));
        query.bindValue(QStringLiteral(":partition"), partition);
        query.bindValue(QStringLiteral(":path"), path);
        if (!query.exec() || query.next())
            continue;
        query.finish();
        const auto absolute  = directory + QLatin1Char('/') + path;
        const auto allocated = allocatedBytes(absolute);
        if (QFileInfo::exists(absolute)) {
            if (!QFile::remove(absolute))
                continue;
            freed += allocated;
        }
        query.prepare(QStringLiteral("DELETE FROM file_trash WHERE partition=:partition AND path=:path"));
        query.bindValue(QStringLiteral(":partition"), partition);
        query.bindValue(QStringLiteral(":path"), path);
        query.exec();
    }
    return freed;
}

void FileDatabase::markLegacyDirectoryImported(const QString &directory, const QString &component)
{
    const auto profile = QFileInfo(ApplicationInfo::currentProfileDir(ApplicationInfo::DataLocation)).fileName();
    QFile      marker(directory + QLatin1Char('/') + markerName(profile, component));
    if (!marker.exists() && !marker.open(QIODevice::WriteOnly))
        qWarning() << "Cannot mark a legacy cache directory as imported:" << marker.errorString();
}

void FileDatabase::retireLegacyDirectory(const QString &directory, const QStringList &profiles,
                                         const QStringList &components)
{
    QDir dir(directory);
    if (!dir.exists())
        return;
    for (const auto &profile : profiles)
        for (const auto &component : components)
            if (!dir.exists(markerName(profile, component)))
                return;
    // Only a directory some profile has imported from is known to be ours to remove.
    if (dir.entryList({ markerName(QStringLiteral("*")) }, QDir::Files).isEmpty())
        return;
    if (!dir.removeRecursively())
        qWarning() << "Cannot remove the legacy cache directory" << directory;
}
