/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "iris/xmpp_hash.h"

#include <QString>
#include <functional>

class QObject;

// Hash a completed, uniquely owned download off the UI thread. The temporary
// file is removed after the callback unless the caller moved it into FileCache.
// If the receiver dies before the callback, the worker retains cleanup ownership
// until its last read finishes. Never block on the hashing worker in a destructor.
namespace FileSharingHashJob {
using Completion = std::function<void(XMPP::Hash)>;

void sha256TemporaryFile(const QString &path, QObject *receiver, Completion complete);
}
