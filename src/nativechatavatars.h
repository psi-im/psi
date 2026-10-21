/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "iris/xmpp_jid.h"

#include <QHash>
#include <QPixmap>
#include <optional>

class MessageView;
class PsiAccount;

// Sender avatars of one native chat timeline, owned by its chat window. A 1:1
// chat has two senders (the peer and us), a group chat one per occupant nick.
// Each sender is resolved once, including "no avatar"; the owner drops entries
// when its existing AvatarFactory change handler reports one of these JIDs.
class NativeChatAvatars {
public:
    NativeChatAvatars(PsiAccount *account, bool groupChat);

    // roomOccupant: a 1:1 chat with room@service/nick, whose bare JID is the
    // room rather than the person. Returns true if cached avatars were dropped.
    bool    setPeer(const XMPP::Jid &peer, bool roomOccupant = false);
    QPixmap avatar(const MessageView &message);
    // Returns true if a cached sender of this timeline was dropped.
    bool invalidate(const XMPP::Jid &jid);

private:
    PsiAccount             *account_;
    bool                    groupChat_;
    XMPP::Jid               peer_;
    bool                    roomOccupant_ = false;
    std::optional<QPixmap>  peerAvatar_; // 1:1 chats
    std::optional<QPixmap>  ownAvatar_;  // 1:1 chats
    QHash<QString, QPixmap> occupants_;  // group chats, by nick
};
