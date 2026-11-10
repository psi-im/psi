/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filedatabase.h"

#include "applicationinfo.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>

namespace {
const int SchemaVersion = 2;

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
          "PRIMARY KEY(account, url_key))" }
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
    }
    if (!query.exec(QStringLiteral("PRAGMA user_version=%1").arg(SchemaVersion)) || !db.commit()) {
        db.rollback();
        return false;
    }
    return true;
}
}

QSqlDatabase FileDatabase::connection()
{
    if (!QCoreApplication::instance())
        return {};
    const QString directory = ApplicationInfo::currentProfileDir(ApplicationInfo::DataLocation);
    const QString path      = directory + QStringLiteral("/files.sqlite");
    const QString name      = QStringLiteral("psi-files:") + path;
    if (QSqlDatabase::contains(name))
        return QSqlDatabase::database(name, false);
    QDir().mkpath(directory);
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db.setDatabaseName(path);
    if (!db.open() || !prepare(db)) {
        qWarning() << "Cannot open the file database:" << db.lastError().text();
        db.close();
    }
    return db;
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
