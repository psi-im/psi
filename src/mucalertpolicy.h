/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MUCALERTPOLICY_H
#define MUCALERTPOLICY_H

#include "chatreply.h"

// Shared MUC highlight policy: a verified reply reference is authoritative;
// id-less plain-text quotes can only be matched by the quoted nickname.
namespace MucAlertPolicy {
inline bool quotesSelf(const ChatReply &reply, const QString &ownNick, bool addressedToSelf,
                       bool referencesOwnMessage)
{
    if (ownNick.isEmpty() || !reply.isValid())
        return false;
    if (referencesOwnMessage || (reply.hasReference() && addressedToSelf))
        return true;
    // Never trust the displayed author when a structured reference points
    // elsewhere: that is not evidence that the message quotes us.
    return !reply.hasReference() && reply.to.isEmpty() && !reply.text.isEmpty()
        && reply.author.compare(ownNick, Qt::CaseInsensitive) == 0;
}
}
#endif // MUCALERTPOLICY_H
