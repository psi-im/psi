/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef CHATREPLY_H
#define CHATREPLY_H
#include <QMetaType>
#include <QString>

namespace XMPP {
class Message;
}

// UI-independent context shared by the composer and native history.
struct ChatReply {
    QString id;
    QString to;
    QString author;
    QString text;
    bool    isValid() const { return !id.isEmpty(); }
};
Q_DECLARE_METATYPE(ChatReply)

ChatReply messageReply(const XMPP::Message &);
QString   messageReplyBody(const XMPP::Message &);
void      applyMessageReply(XMPP::Message &, const ChatReply &);
#endif
