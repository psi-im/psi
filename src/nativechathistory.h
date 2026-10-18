/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QString>
class MamChatWidget;
class PsiAccount;
class MessageView;
namespace XMPP {
class Message;
class Jid;
}

// The same content/extension projection is used for EDB and MAM replay.
MessageView nativeHistoryMessage(PsiAccount *, const XMPP::Jid &peer, const XMPP::Message &, bool local);

// Presentation adapter for the existing EDB, with a separate local keyset
// cursor. Local-only originals remain reachable after MAM reaches its start.
void prependLocalChatHistory(MamChatWidget *, PsiAccount *, const QString &peer, bool room, const QString &ownNick,
                             qint64 &before, bool &exhausted);
