/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QString>

// SQLite schema upgrades only; legacy flat-file history import is retired.
namespace HistoryUpgrade {
bool upgradeDatabase(const QString &path);
}
