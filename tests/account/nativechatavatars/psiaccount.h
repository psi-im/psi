/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "avatars.h"

class PsiAccount {
public:
    AvatarFactory *avatarFactory() { return &factory; }
    XMPP::Jid      jid() const { return self; }
    AvatarFactory  factory;
    XMPP::Jid      self { QStringLiteral("me@example.org/desktop") };
};
