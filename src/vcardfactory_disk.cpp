/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "vcardfactory.h"

#include "iris/xmpp_vcard.h"
#include "iris/xmpp_vcard4.h"

#include <QDomDocument>
#include <QFile>

// Isolated translation unit so regression tests can exercise the *production*
// disk reader with real Iris parsers without linking the entire Psi application.
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
