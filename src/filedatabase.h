/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// The current profile's database of file records and publications, shared by its
// FileCache instances and FileSharingCatalog.
// Two profiles may run at once, so nothing in it is shared between them.
namespace FileDatabase {
// Not open if the database cannot be used. A cache then works for this session only.
// That includes the end of the process: Qt drops its SQL connections with the application.
QSqlDatabase connection();
QString      path(); // Capture on the profile's owner thread before starting a worker.
// Each thread gets its own connection, including when it opens the same file.
QSqlDatabase connection(const QString &path);
void         closeConnection(const QString &path); // Call on the connection's thread.

QByteArray  encodeMetadata(const QVariantMap &metadata);
QVariantMap decodeMetadata(const QByteArray &bytes);
bool        safePayloadName(const QString &name);
// Retire ownership and queue managed bytes together. An empty path is external.
bool retireFile(QSqlDatabase &db, const QByteArray &id, const QString &partition, const QString &payload,
                bool inTransaction = false);
void drainTrash(QSqlDatabase &db, const QString &partition, const QString &directory);

// Directories from before caches moved under the profile. A profile records that it
// has taken what it needs, and the last one to do so removes the directory.
void markLegacyDirectoryImported(const QString &directory, const QString &component = {});
// Every required component must finish for every profile before shared inputs go.
void retireLegacyDirectory(const QString &directory, const QStringList &profiles,
                           const QStringList &components = { QString() });
}
