/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QString>
#include <functional>
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

#include "messageview.h"
#include <QStringList>
class HistorySync;
struct NativeHistoryPage {
    QList<QPair<QString, MessageView>> messages;
    QStringList                        uids;
    int                                visibleMessages = 0, protocolEvents = 0;
};
// Canonical replay projection shared by cursor pages and random-seek islands.
NativeHistoryPage nativeArchivePage(PsiAccount *, HistorySync *, const XMPP::Jid &peer, bool room,
                                    const QString &ownNick, const std::function<QString(bool)> &nickname = {});
