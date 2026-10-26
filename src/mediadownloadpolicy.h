/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QList>
#include <QtGlobal>

// How much of a shared file Psi downloads without being asked. As in
// Conversations, chats download from contacts in the roster and from oneself;
// group chats download from everyone. Each has its own size limit, and what
// the user asks for is never limited.
namespace MediaDownloadPolicy {
struct Settings {
    bool    previews       = true; // options.ui.chat.show-previews
    quint64 chatLimit      = 0;    // options.ui.chat.auto-download-size, 0 for never
    quint64 groupchatLimit = 0;    // options.ui.chat.groupchat-auto-download-size, 0 for never
};

// The largest file the view may download by itself, 0 for none.
inline quint64 limit(const Settings &settings, bool groupchat, bool trustedSender)
{
    if (!settings.previews)
        return 0;
    if (groupchat)
        return settings.groupchatLimit;
    return trustedSender ? settings.chatLimit : 0;
}

// The limits offered in the options, in bytes; 0 means only when asked.
inline QList<quint64> choices()
{
    return { 0, 512 * 1024, 1024 * 1024, 5 * 1024 * 1024, 10 * 1024 * 1024, 50 * 1024 * 1024 };
}
} // namespace MediaDownloadPolicy
