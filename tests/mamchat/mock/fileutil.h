/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QString>
// The native chat tests replace only the filesystem/open-folder boundary.
class FileUtil {
public:
    static void openFolder(const QString &) { }
};
