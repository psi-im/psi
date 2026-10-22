/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QList>

namespace VCardRequestPolicy {

// This is evidence from the resolved disco/caps of one *full* contact JID.
// No caps (or unresolved caps) is not evidence that vCard4 is unsupported.
struct ResourceCapabilities {
    bool known  = false;
    bool vcard4 = false;
};

enum class Protocol { VCardTemp, VCard4 };

// Explicit legacy requests and MUC vCards always use XEP-0054. Our own card
// retains its PEP-first behavior. For an ordinary roster contact, a known
// capabilities set that does not advertise vCard4 is evidence to use
// vcard-temp; a single vCard4-capable resource permits PEP vCard4. When none
// of the resources has resolved caps, preserve the previous PEP-first fallback.
inline Protocol select(bool forceTemp, bool ownCard, const QList<ResourceCapabilities> &resources)
{
    if (forceTemp)
        return Protocol::VCardTemp;
    if (ownCard)
        return Protocol::VCard4;

    bool anyKnown = false;
    for (const auto &resource : resources) {
        if (!resource.known)
            continue;
        anyKnown = true;
        if (resource.vcard4)
            return Protocol::VCard4;
    }
    return anyKnown ? Protocol::VCardTemp : Protocol::VCard4;
}

} // namespace VCardRequestPolicy
