/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "nativechatavatars.h"

#include "avatars.h"
#include "messageview.h"
#include "psiaccount.h"

NativeChatAvatars::NativeChatAvatars(PsiAccount *account, bool groupChat) : account_(account), groupChat_(groupChat)
{
}

bool NativeChatAvatars::setPeer(const XMPP::Jid &peer, bool roomOccupant)
{
    if (groupChat_) {
        const bool otherRoom = !peer.compare(peer_, false);
        peer_ = peer;
        return otherRoom;
    }
    if (peer.compare(peer_) && roomOccupant == roomOccupant_)
        return false;
    peer_ = peer;
    roomOccupant_ = roomOccupant;
    return true;
}

QPixmap NativeChatAvatars::avatar(const MessageView &message)
{
    auto *avatars = account_->avatarFactory();
    // AvatarCache owns both the positive iconset and bounded negative memo.
    // Routing is local, constant time, and deliberately never fetches a vCard.
    if (groupChat_)
        return avatars->getMucAvatar(peer_.withResource(message.nick()));
    if (message.isLocal())
        return avatars->getAvatar(account_->jid().withResource({}));
    return roomOccupant_ ? avatars->getMucAvatar(peer_) : avatars->getAvatar(peer_);
}

bool NativeChatAvatars::resetRoom()
{
    // Called on rejoin even when the room JID is unchanged. MUC epochs are
    // invalidated by AvatarFactory first; the view only needs a repaint.
    return groupChat_;
}

bool NativeChatAvatars::invalidate(const XMPP::Jid &jid)
{
    if (groupChat_)
        return !jid.resource().isEmpty() && jid.compare(peer_, false);
    if (jid.compare(account_->jid(), false))
        return true;
    return jid.compare(peer_, roomOccupant_);
}
