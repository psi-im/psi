/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "iris/xmpp_reference.h"
#include "iris/xmpp_url.h"

#include <QList>

namespace XMPP {
class Message;
}

// A display-only projection. XMPP::Message is explicitly shared in Iris;
// mutating a 'copy' via setReferences()/setUrlList() would also mutate the
// received/persisted message. Keep normalization wholly outside Message.
namespace FileSharingNormalizer {
struct PreparedMedia {
    QList<XMPP::Reference> references;
    XMPP::UrlList          remainingOob;
};

PreparedMedia prepareForDisplay(const XMPP::Message &message);
}
