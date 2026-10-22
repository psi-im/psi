/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "iris/xmpp_vcard4.h"

// vCard4 (XEP-0292) and vcard-temp (XEP-0054) are independent stores.
// Modern values win when present, while legacy fields can fill gaps.
namespace VCardRequestMerge {
inline void supplement(VCard4::VCard &modern, const VCard4::VCard &legacy)
{
    if (!legacy)
        return;
    if (!modern || modern.isEmpty()) {
        modern = legacy;
        return;
    }
    if (modern.fullName().isEmpty()) modern.setFullName(legacy.fullName());
    if (modern.names().data.isEmpty()) modern.setNames(legacy.names());
    if (modern.nickName().isEmpty()) modern.setNickName(legacy.nickName());
    if (modern.photo().isEmpty()) modern.setPhoto(legacy.photo());
    if (modern.emails().isEmpty()) modern.setEmails(legacy.emails());
    if (modern.phones().isEmpty()) modern.setPhones(legacy.phones());
    if (modern.addresses().isEmpty()) modern.setAddresses(legacy.addresses());
    if (modern.org().isEmpty()) modern.setOrg(legacy.org());
    if (modern.title().isEmpty()) modern.setTitle(legacy.title());
    if (modern.role().isEmpty()) modern.setRole(legacy.role());
    if (modern.note().isEmpty()) modern.setNote(legacy.note());
    if (modern.urls().isEmpty()) modern.setUrls(legacy.urls());
    if (modern.bday().isEmpty()) modern.setBday(legacy.bday());
    if (modern.categories().isEmpty()) modern.setCategories(legacy.categories());
    if (modern.logo().isEmpty()) modern.setLogo(legacy.logo());
    if (modern.impp().isEmpty()) modern.setImpp(legacy.impp());
    if (modern.timeZone().isEmpty()) modern.setTimeZone(legacy.timeZone());
}

// vcard-temp can return the entire contact card even for an avatar refresh.
// Only the PHOTO field may change during such refreshes.
inline bool applyLegacyPhoto(VCard4::VCard &cached, const VCard4::VCard &legacy)
{
    const auto photo = legacy.photo();
    if (photo.isEmpty())
        return false;
    cached.setPhoto(photo);
    return true;
}
} // namespace VCardRequestMerge
