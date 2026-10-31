/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QDateTime>
#include <QSet>
#include <QString>

namespace PsiMam {
// A client stanza ID is neither globally unique nor a verified MAM archive UID.
// Only use this conservative correlation within one already scoped conversation.
struct DeliveryIdentity {
    QString stanzaId;
    QString richText;
    bool outgoing = false;
    QDateTime timestamp;
};

inline bool sameDelivery(const DeliveryIdentity &a, const DeliveryIdentity &b)
{
    if (a.stanzaId.isEmpty() || a.stanzaId != b.stanzaId
        || a.outgoing != b.outgoing || a.richText != b.richText
        || !a.timestamp.isValid() || !b.timestamp.isValid())
        return false;
    return qAbs(a.timestamp.secsTo(b.timestamp)) <= 300;
}

// A message as a XEP-0461 reply can reference it.
struct ReplyCandidate {
    QString       sender;    // Full JID: the occupant in a room.
    QString       target;    // In a room its stanza-id, elsewhere its origin-id or id.
    QSet<QString> clientIds; // All 'id' attributes of the message and its corrections.
};

// A plain-text quote has no id: never match it to a message, whose own target
// may be just as empty in a room without stanza-ids.
inline bool isReplyTarget(const QString &id, const QString &to, bool groupChat, const ReplyCandidate &candidate)
{
    if (id.isEmpty())
        return false;
    // The XEP defines no verification, but 'to' SHOULD name the author. An
    // occupant is identified by its full JID. In a 1:1 chat a bare JID MAY be
    // used, and a full one need not name the resource that sent the message.
    const auto bare = [](const QString &jid) { return jid.section(QLatin1Char('/'), 0, 0); };
    if (!to.isEmpty() && (groupChat ? candidate.sender != to : bare(candidate.sender) != bare(to)))
        return false;
    // In a room the 'id' attribute MUST NOT be used for replies.
    return candidate.target == id || (!groupChat && candidate.clientIds.contains(id));
}

// The index of the one message a reply references, or -1. A reused id is
// ambiguous, and an answer must not be assumed to relate to any other message.
template <typename CandidateAt>
int uniqueReplyTarget(const QString &id, const QString &to, bool groupChat, int count, CandidateAt candidateAt)
{
    int match = -1;
    for (int i = 0; i < count; ++i) {
        if (!isReplyTarget(id, to, groupChat, candidateAt(i)))
            continue;
        if (match >= 0)
            return -1;
        match = i;
    }
    return match;
}
} // namespace PsiMam
