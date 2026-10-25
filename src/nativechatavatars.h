/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "iris/xmpp_jid.h"

#include <QPixmap>

class MessageView;
class PsiAccount;

// Lightweight sender identity router for a native chat timeline.
// Positive decoded pixels and bounded negative lookups live exclusively in
// AvatarCache. No per-window QPixmap/negative memo or network requests here.
class NativeChatAvatars {
public:
    NativeChatAvatars(PsiAccount *account, bool groupChat);

    // roomOccupant: a 1:1 chat with room@service/nick, whose bare JID is the
    // room rather than the person. Returns true if cached avatars were dropped.
    bool    setPeer(const XMPP::Jid &peer, bool roomOccupant = false);
    QPixmap avatar(const MessageView &message);
    // Returns true if this sender belongs to the timeline and should repaint.
    bool invalidate(const XMPP::Jid &jid);
    // Room rejoin changes occupant identities, so refresh rendered avatars.
    bool resetRoom();

private:
    PsiAccount             *account_;
    bool                    groupChat_;
    XMPP::Jid               peer_;
    bool                    roomOccupant_ = false;
};
