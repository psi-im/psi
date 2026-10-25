/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "vcardfactory.h"

#include "iris/xmpp_vcard.h"
#include "iris/xmpp_vcard4.h"

#include <QDomDocument>
#include <QFile>

// Keep cache read and vCard field merging in one translation unit. Tests use
// these production private methods with real Iris without linking all of Psi.
// Only full, non-MUC requests consult the account's modern PEP vCard store.
// Caps advertised by a client's resources say nothing about that store.
bool VCardFactory::shouldFetchModern(Flags flags)
{
    return !(flags & (PhotoOnly | ForceVCardTemp | MucRoom | MucUser));
}

XMPP::VCard4::VCard VCardFactory::readCachedVCard(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    XMPP::VCard4::VCard card = XMPP::VCard4::VCard::fromDevice(&file);
    if (!card) {
        file.seek(0);
        QDomDocument doc;
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
        const bool parsed = doc.setContent(&file, false);
#else
        const auto parsed = doc.setContent(&file);
#endif
        if (parsed) {
            const XMPP::VCard legacy = XMPP::VCard::fromXml(doc.documentElement());
            if (!legacy.isNull())
                card.fromVCardTemp(legacy);
        }
    }
    return card;
}

// XEP-0292 and XEP-0054 expose separate account-level stores. Modern
// values take precedence; legacy values supplement missing modern fields.
void VCardFactory::supplementFromLegacy(XMPP::VCard4::VCard &modern, const XMPP::VCard4::VCard &legacy)
{
    if (!legacy)
        return;
    if (!modern || modern.isEmpty()) {
        modern = legacy;
        return;
    }
    // Iris vCard4 uses explicitly shared data: its setters do not detach.
    // Never mutate an unrelated cached copy when supplementing a card.
    modern.detach();
    if (modern.fullName().isEmpty()) modern.setFullName(legacy.fullName());
    if (modern.names().data.isEmpty()) modern.setNames(legacy.names());
    if (modern.nickName().isEmpty()) modern.setNickName(legacy.nickName());
    // Prefer a directly usable legacy PHOTO over a modern URI-only image.
    if (modern.photo().isEmpty()
        || (QByteArray(modern.photo()).isEmpty() && !QByteArray(legacy.photo()).isEmpty()))
        modern.setPhoto(legacy.photo());
    if (modern.emails().isEmpty()) modern.setEmails(legacy.emails());
    if (modern.phones().isEmpty()) modern.setPhones(legacy.phones());
    if (modern.addresses().isEmpty()) modern.setAddresses(legacy.addresses());
    if (modern.org().isEmpty()) modern.setOrg(legacy.org());
    if (modern.title().isEmpty()) modern.setTitle(legacy.title());
    if (modern.role().isEmpty()) modern.setRole(legacy.role());
    if (modern.note().isEmpty()) modern.setNote(legacy.note());
    if (modern.urls().isEmpty()) modern.setUrls(legacy.urls());
    if (QString(modern.bday()).isEmpty()) modern.setBday(legacy.bday());
    if (modern.categories().isEmpty()) modern.setCategories(legacy.categories());
    if (modern.logo().isEmpty()) modern.setLogo(legacy.logo());
    if (modern.impp().isEmpty()) modern.setImpp(legacy.impp());
    if (modern.timeZone().isEmpty()) modern.setTimeZone(legacy.timeZone());
}

// vcard-temp can return the entire contact card even for an avatar refresh.
// Only the PHOTO field may change during such refreshes.
bool VCardFactory::applyLegacyPhoto(XMPP::VCard4::VCard &cached, const XMPP::VCard4::VCard &legacy)
{
    const auto photo = legacy.photo();
    if (photo.isEmpty())
        return false;
    if (cached)
        cached.detach();
    cached.setPhoto(photo);
    return true;
}
