/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
// Test double: counts lookups and returns configured pixmaps.
#include "iris/xmpp_jid.h"
#include <QHash>
#include <QPixmap>

class AvatarFactory {
public:
    QPixmap getAvatar(const XMPP::Jid &jid)
    {
        ++contactLookups;
        return contacts.value(jid.bare());
    }
    QPixmap getMucAvatar(const XMPP::Jid &jid)
    {
        ++occupantLookups;
        return occupants.value(jid.full());
    }
    QHash<QString, QPixmap> contacts, occupants;
    int                     contactLookups = 0, occupantLookups = 0;
};
