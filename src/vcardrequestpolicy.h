/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

namespace VCardRequestPolicy {

// The two vCard stores belong to the account's server, not to individual
// online resources. Ordinary full-card requests query both stores; the modern
// PEP card takes precedence while legacy data can fill missing fields.
// vcard-temp is the only source for legacy avatar hashes (XEP-0153).
struct Plan {
    bool fetchModern = false;
    bool fetchLegacy = true;
};

inline Plan plan(bool photoOnly, bool forceLegacy, bool muc)
{
    return { !(photoOnly || forceLegacy || muc), true };
}

} // namespace VCardRequestPolicy
