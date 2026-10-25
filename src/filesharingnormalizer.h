/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "iris/xmpp_reference.h"
#include "iris/xmpp_url.h"

#include <QList>

namespace XMPP {
class Message;
}

// Canonical attachment projection shared by live rendering and SQLite
// serialization (including MAM). Iris Message uses explicitly shared data:
// mutating a shallow copy changes the received stanza. Never modify it here.
// Encoding persists the projected references/remaining links instead.
namespace FileSharingNormalizer {
struct PreparedMedia {
    QList<XMPP::Reference> references;
    XMPP::UrlList          remainingOob;
};

PreparedMedia prepareForDisplay(const XMPP::Message &message);
}
