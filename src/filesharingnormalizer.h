/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

namespace XMPP {
class Message;
}

// Normalize an application-owned, decoded message in place, before it enters
// the event/history/view pipeline. In Iris Message is explicitly shared, so
// changing displayMessage() also changes its shared backing data (including
// the inner forwarded message of a Carbon). Never call this on a wire message
// before it has been processed/sent.
namespace FileSharingNormalizer {
void normalize(XMPP::Message &message);
}
