/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "applicationinfo.h"
#include <QtGlobal>

// These standalone fixtures supply their database path explicitly. Never use a
// real profile or silently fall back to a database in the working directory.
QString ApplicationInfo::currentProfileDir(HomedirType) { qFatal("Range fixtures must use an explicit database path"); }
