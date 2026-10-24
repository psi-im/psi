/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "iris/xmpp_vcard.h"
#include "iris/xmpp_vcard4.h"

#include <QDomDocument>
#include <QFile>

// VCardFactory::vcard() uses this reader for the first on-disk cache lookup.
// Keep legacy vcard-temp XML compatibility and return the parsed value on
// *that very first lookup*, not only after it has entered the RAM cache.
namespace VCardDiskCache {
inline XMPP::VCard4::VCard read(const QString &fileName)
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
        const bool parsed = doc.setContent(&file);
#endif
        if (parsed) {
            const XMPP::VCard legacy = XMPP::VCard::fromXml(doc.documentElement());
            if (!legacy.isNull())
                card.fromVCardTemp(legacy);
        }
    }
    return card;
}
} // namespace VCardDiskCache
