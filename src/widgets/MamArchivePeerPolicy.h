/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QString>

namespace PsiMam {
// An account archive may contain traffic beyond the requested peer. Accept
// only an actual 1:1 direction (including the self-chat case), never a message
// involving one unrelated third party merely because it mentions the peer.
inline bool belongsToConversation(const QString &owner, const QString &peer,
                                  const QString &from, const QString &to)
{
    return !owner.isEmpty() && !peer.isEmpty()
        && ((from == owner && to == peer) || (from == peer && to == owner));
}
} // namespace PsiMam
