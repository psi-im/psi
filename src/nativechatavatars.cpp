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
        // A group chat's JID carries our own nick; occupants stay valid
        // unless the room itself changes.
        const bool otherRoom = !peer.compare(peer_, false);
        peer_                = peer;
        if (otherRoom)
            occupants_.clear();
        return otherRoom;
    }
    if (peer.compare(peer_) && roomOccupant == roomOccupant_)
        return false;
    peer_         = peer;
    roomOccupant_ = roomOccupant;
    peerAvatar_.reset();
    return true;
}

QPixmap NativeChatAvatars::avatar(const MessageView &message)
{
    auto *avatars = account_->avatarFactory();
    if (groupChat_) {
        auto it = occupants_.constFind(message.nick());
        if (it == occupants_.cend())
            it = occupants_.insert(message.nick(), avatars->getMucAvatar(peer_.withResource(message.nick())));
        return it.value();
    }
    if (message.isLocal()) {
        if (!ownAvatar_)
            ownAvatar_ = avatars->getAvatar(account_->jid().withResource({}));
        return *ownAvatar_;
    }
    if (!peerAvatar_)
        peerAvatar_ = roomOccupant_ ? avatars->getMucAvatar(peer_) : avatars->getAvatar(peer_);
    return *peerAvatar_;
}

bool NativeChatAvatars::invalidate(const XMPP::Jid &jid)
{
    if (groupChat_) // Occupant avatars are reported for room@service/nick.
        return jid.compare(peer_, false) && occupants_.remove(jid.resource()) > 0;
    bool dropped = false;
    if (ownAvatar_ && jid.compare(account_->jid(), false)) {
        ownAvatar_.reset();
        dropped = true;
    }
    // A contact is identified by its bare JID, a room occupant by the full one.
    if (peerAvatar_ && jid.compare(peer_, roomOccupant_)) {
        peerAvatar_.reset();
        dropped = true;
    }
    return dropped;
}
