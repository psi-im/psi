/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QStringList>

// The current profile's database of file records and publications, shared by its
// FileCache instances and FileSharingCatalog.
// Two profiles may run at once, so nothing in it is shared between them.
namespace FileDatabase {
// Not open if the database cannot be used. A cache then works for this session only.
// That includes the end of the process: Qt drops its SQL connections with the application.
QSqlDatabase connection();

// Directories from before caches moved under the profile. A profile records that it
// has taken what it needs, and the last one to do so removes the directory.
void markLegacyDirectoryImported(const QString &directory, const QString &component = {});
// Every required component must finish for every profile before shared inputs go.
void retireLegacyDirectory(const QString &directory, const QStringList &profiles,
                           const QStringList &components = { QString() });
}
