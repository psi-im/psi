/*
 * messageview.cpp - message data for chatview
 * Copyright (C) 2010-2017  Sergey Ilinykh
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "messageview.h"
#include <QTextDocument>
#include <QTextCursor>
#include <algorithm>

#include "common.h"
#include "psioptions.h"
#include "textutil.h"

#include <QTextDocument>

static const QString me_cmd = "/me ";

// ======================================================================
// MessageView
// ======================================================================
MessageView::MessageView(Type t) : _type(t), _status(0), _statusPriority(0), _dateTime(QDateTime::currentDateTime()) { }

MessageView MessageView::fromPlainText(const QString &text, Type type)
{
    MessageView mv(type);
    mv.setPlainText(text);
    return mv;
}

MessageView MessageView::fromHtml(const QString &text, Type type)
{
    MessageView mv(type);
    mv.setHtml(text);
    return mv;
}

MessageView MessageView::subjectMessage(const QString &subject, const QString &prefix)
{
    MessageView mv(Subject);
    mv._text     = TextUtil::escape(prefix);
    mv._userText = subject;
    return mv;
}

MessageView MessageView::mucJoinMessage(const QString &nick, int status, const QString &message,
                                        const QString &statusText, int priority)
{
    MessageView mv = MessageView::fromPlainText(message, MUCJoin);
    mv.setNick(nick);
    mv.setStatus(status);
    mv.setStatusPriority(priority);
    mv.setUserText(statusText);
    return mv;
}

MessageView MessageView::mucPartMessage(const QString &nick, const QString &message, const QString &statusText)
{
    MessageView mv = MessageView::fromPlainText(message, MUCPart);
    mv.setNick(nick);
    mv.setUserText(statusText);
    return mv;
}

MessageView MessageView::nickChangeMessage(const QString &nick, const QString &newNick)
{
    MessageView mv = MessageView::fromPlainText(QObject::tr("%1 is now known as %2").arg(nick, newNick), NickChange);
    mv.setNick(nick);
    mv.setUserText(newNick);
    return mv;
}

MessageView MessageView::reactionsMessage(const QString &nick, const QString &targetMessageId,
                                          const QSet<QString> &reactions)
{
    MessageView mv(Reactions);
    mv.setNick(nick);
    mv.setReactionsId(targetMessageId);
    mv.setReactions(reactions);
    return mv;
}

MessageView MessageView::retractionMessage(const QString &targetMessageId)
{
    MessageView mv(MessageRetraction);
    mv.setRetractionId(targetMessageId);
    return mv;
}

QString MessageView::reactionTarget(const XMPP::Message &message)
{
    if (message.type() == XMPP::Message::Type::Groupchat)
        return message.stanzaId(XMPP::Jid(message.from().bare())).id;
    return message.originId().isEmpty() ? message.id() : message.originId();
}

void MessageView::setReplyMetadata(const XMPP::Message &message)
{
    setStylingDisabled(message.stylingDisabled());
    // XEP-0461 replies mark their compatibility quote with XEP-0428 ranges.
    // Without <reply/> (rooms without stanza-ids, older clients) recognize a
    // leading plain-text quote instead; it simply has no reference id.
    ChatReply              parsed = messageReply(message);
    QList<QPair<int, int>> ranges;
    if (message.reply().isValid()) {
        for (const auto &range : message.bodyFallbacks(QStringLiteral("urn:xmpp:reply:0"))) {
            int first, last;
            if (range.toUtf16(message.body(), first, last))
                ranges.append({ first, last });
        }
    } else if (const int quoteEnd = plainQuotePrefix(message.body(), &parsed)) {
        ranges.append({ 0, quoteEnd });
    }
    setReply(parsed);
    if (!reply().isValid())
        return;
    _replyData->hasFallback = message.reply().isValid() ? messageReplyBody(message) != message.body()
                                                        : !ranges.isEmpty();
    _replyData->formatted = formattedText();
    if (_replyData->hasFallback) {
        // Preserve XHTML and plugin-provided rendering. Only apply the wire
        // offsets when the rendered text still contains that exact body.
        // Try the original rich prefix before emoticonify() can replace text
        // with inline objects. This also preserves SIMS/share objects in the
        // rest of the message instead of round-tripping them through Qt HTML.
        QString rich = _text;
        int prefixEnd = 0;
        for (const auto &range : ranges) {
            if (range.first == 0)
                prefixEnd = qMax(prefixEnd, range.second);
        }
        const QString prefix = TextUtil::linkify(TextUtil::plain2rich(message.body().left(prefixEnd)));
        if (prefixEnd > 0 && rich.startsWith(prefix)) {
            rich.remove(0, prefix.size());
            MessageView presentation = *this;
            presentation._text = rich;
            _replyData->formatted = presentation.formattedText();
            return;
        }
        QTextDocument document;
        document.setHtml(_text);
        if (document.toPlainText().startsWith(message.body())) {
            std::sort(ranges.begin(), ranges.end());
            QList<QPair<int, int>> merged;
            for (const auto &range : ranges) {
                if (!merged.isEmpty() && range.first <= merged.last().second)
                    merged.last().second = qMax(merged.last().second, range.second);
                else
                    merged.append(range);
            }
            for (auto it = merged.crbegin(); it != merged.crend(); ++it) {
                QTextCursor cursor(&document);
                cursor.setPosition(it->first);
                cursor.setPosition(it->second, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
            }
            MessageView presentation = *this;
            presentation._text = document.toHtml();
            _replyData->formatted = presentation.formattedText();
        }
    }
}

const ChatReply &MessageView::reply() const
{
    static const ChatReply empty;
    return _replyData ? _replyData->reply : empty;
}

void MessageView::setReply(const ChatReply &reply)
{
    if (!reply.isValid()) {
        _replyData = nullptr;
        return;
    }
    if (!_replyData)
        _replyData = new ReplyData;
    _replyData->reply = reply;
}

const QString &MessageView::replyFormattedText() const
{
    static const QString empty;
    return _replyData ? _replyData->formatted : empty;
}

bool MessageView::hasReplyFallback() const { return _replyData && _replyData->hasFallback; }

QString MessageView::retractionTarget(const XMPP::Message &message)
{
    if (message.type() != XMPP::Message::Type::Groupchat)
        return message.id();
    const auto roomId = message.stanzaId(XMPP::Jid(message.from().bare())).id;
    return roomId.isEmpty() ? message.originId() : roomId;
}

bool MessageView::protocolMessage(const XMPP::Message &message, bool local, MessageView &view)
{
    if (!message.retraction().isEmpty())
        view = retractionMessage(message.retraction());
    else if (!message.reactions().targetId.isEmpty())
        view = reactionsMessage(message.from().resource(), message.reactions().targetId, message.reactions().reactions);
    else if (!message.displayedMarkerId().isEmpty())
        view = displayedMarkerMessage(message.displayedMarkerId());
    else if (message.messageReceipt() == XMPP::ReceiptReceived)
        view = receiptMessage(message.messageReceiptId());
    else
        return false;
    view.setLocal(local);
    view.setNick(message.from().resource());
    view.setUserId(message.from().full());
    view.setDateTime(message.timeStamp());
    view.setSpooled(message.spooled());
    return true;
}

MessageView MessageView::statusMessage(const QString &nick, int status, const QString &statusText, int priority)
{
    QString message = QObject::tr("%1 is now %2").arg(nick, status2txt(status));

    MessageView mv = MessageView::fromPlainText(message, Status);
    mv.setNick(nick);
    mv.setStatus(status);
    mv.setStatusPriority(priority);
    mv.setUserText(statusText);
    return mv;
}

// getters and setters

void MessageView::setPlainText(const QString &text)
{
    if (!text.isEmpty()) {
        if (_type == Message) {
            setEmote(text.startsWith(me_cmd));
        }
        _text = TextUtil::plain2rich(text);
        if (_type == Message) {
            _text = TextUtil::linkify(_text);
        }
    }
}

void MessageView::setHtml(const QString &text)
{
    if (_type == Message) {
        QString str = TextUtil::rich2plain(text).trimmed();
        setEmote(str.startsWith(me_cmd));
        if (isEmote()) {
            setPlainText(str);
            return;
        }
    }
    _text = text;
}

QString MessageView::formattedText() const
{
    QString txt = _text;

    if (isEmote() && _type == Message) {
        int cmd = txt.indexOf(me_cmd);
        txt     = txt.remove(cmd, me_cmd.length());
    }
    if (PsiOptions::instance()->getOption("options.ui.emoticons.use-emoticons").toBool())
        txt = TextUtil::emoticonify(txt);
    if (!_stylingDisabled && PsiOptions::instance()->getOption("options.ui.chat.legacy-formatting").toBool())
        txt = TextUtil::legacyFormat(txt);

    return txt;
}

QString MessageView::formattedUserText() const
{
    if (!_userText.isEmpty()) {
        QString text = TextUtil::plain2rich(_userText);
        text         = TextUtil::linkify(text);
        if (PsiOptions::instance()->getOption("options.ui.emoticons.use-emoticons").toBool())
            text = TextUtil::emoticonify(text);
        if (!_stylingDisabled && PsiOptions::instance()->getOption("options.ui.chat.legacy-formatting").toBool())
            text = TextUtil::legacyFormat(text);
        return text;
    }
    return "";
}

bool MessageView::hasStatus() const { return _type == Status || _type == MUCJoin; }
