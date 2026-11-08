#pragma once

#include <cstdint>

namespace Mattermost {

// Only one badge is drawn over the tray icon. Personal attention has priority
// over ordinary unread channel activity.
enum class TrayBadgeKind {
    None,
    Attention,
    ChannelUnread,
};

inline TrayBadgeKind trayBadgeKind(std::uint32_t attentionCount,
                                  bool hasUnreadChannel)
{
    if (attentionCount != 0) {
        return TrayBadgeKind::Attention;
    }
    return hasUnreadChannel ? TrayBadgeKind::ChannelUnread : TrayBadgeKind::None;
}

// Channel-only unread is unrelated to DM/GM or CRT thread attention. Ignore
// muted and untracked channels; their notification policy is separate.
inline bool isTrayUnreadChannel(bool publicOrPrivate, bool tracked,
                                bool muted, bool unread)
{
    return publicOrPrivate && tracked && !muted && unread;
}

} // namespace Mattermost
