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
    // A reply normally references the quoted message by id (XEP-0461). In
    // rooms that do not assign stanza-ids (e.g. old ejabberd without MAM) a
    // quote may have no usable id; it is then sent as plain quoted body text.
    bool hasReference() const { return !id.isEmpty(); }
    bool isValid() const { return hasReference() || !text.isEmpty(); }
};
Q_DECLARE_METATYPE(ChatReply)

ChatReply messageReply(const XMPP::Message &);
QString   messageReplyBody(const XMPP::Message &);
void      applyMessageReply(XMPP::Message &, const ChatReply &);
// Recognizes a leading plain-text quote ("> Nick:" then "> line"...), as sent
// for rooms without stanza-ids or by clients without XEP-0461. It must be
// followed by the actual answer. Returns the quote's UTF-16 length including
// its last newline, or 0; fills an id-less reply when one is given.
int plainQuotePrefix(const QString &body, ChatReply *reply = nullptr);
#endif
