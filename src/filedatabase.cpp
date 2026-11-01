/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filedatabase.h"

#include "applicationinfo.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>

namespace {
const int SchemaVersion = 1;

QString markerName(const QString &profile) { return QStringLiteral("imported-") + profile; }

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
    if (version != 0)
        return false; // Written by a newer Psi.
    if (!db.transaction())
        return false;
    const char *statements[]
        = { "CREATE TABLE files ("
            "id BLOB PRIMARY KEY, partition TEXT NOT NULL, size INTEGER NOT NULL, "
            "created INTEGER NOT NULL, max_age INTEGER NOT NULL, metadata BLOB NOT NULL)",
            "CREATE INDEX files_partition ON files(partition)",
            "CREATE TABLE file_hashes ("
            "file_id BLOB NOT NULL REFERENCES files(id) ON DELETE CASCADE, position INTEGER NOT NULL, "
            "algorithm TEXT NOT NULL, digest BLOB NOT NULL, PRIMARY KEY(file_id, position))",
            "CREATE INDEX file_hashes_digest ON file_hashes(algorithm, digest)",
            "CREATE TABLE file_legacy_imports (partition TEXT NOT NULL PRIMARY KEY)" };
    for (const auto *statement : statements) {
        if (!query.exec(QString::fromLatin1(statement))) {
            db.rollback();
            return false;
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

void FileDatabase::markLegacyDirectoryImported(const QString &directory)
{
    const auto profile = QFileInfo(ApplicationInfo::currentProfileDir(ApplicationInfo::DataLocation)).fileName();
    QFile      marker(directory + QLatin1Char('/') + markerName(profile));
    if (!marker.exists() && !marker.open(QIODevice::WriteOnly))
        qWarning() << "Cannot mark a legacy cache directory as imported:" << marker.errorString();
}

void FileDatabase::retireLegacyDirectory(const QString &directory, const QStringList &profiles)
{
    QDir dir(directory);
    if (!dir.exists())
        return;
    for (const auto &profile : profiles)
        if (!dir.exists(markerName(profile)))
            return;
    // Only a directory some profile has imported from is known to be ours to remove.
    if (dir.entryList({ markerName(QStringLiteral("*")) }, QDir::Files).isEmpty())
        return;
    if (!dir.removeRecursively())
        qWarning() << "Cannot remove the legacy cache directory" << directory;
}
