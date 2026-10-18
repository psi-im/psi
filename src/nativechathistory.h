/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QString>
class MamChatWidget;
class PsiAccount;

// Presentation adapter for the existing EDB, with a separate local keyset
// cursor. Local-only originals remain reachable after MAM reaches its start.
void prependLocalChatHistory(MamChatWidget *, PsiAccount *, const QString &peer, bool room, const QString &ownNick,
                             qint64 &before, bool &exhausted);
