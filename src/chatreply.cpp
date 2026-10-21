/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "chatreply.h"
#include "iris/xmpp_message.h"
#include <algorithm>

namespace {
const QString          replyNamespace = QStringLiteral("urn:xmpp:reply:0");
QList<QPair<int, int>> fallbackRanges(const XMPP::Message &message)
{
    QList<QPair<int, int>> result;
    for (const auto &range : message.bodyFallbacks(replyNamespace)) {
        int first, last;
        if (range.toUtf16(message.body(), first, last))
            result.append({ first, last });
    }
    std::sort(result.begin(), result.end());
    QList<QPair<int, int>> merged;
    for (const auto &range : result) {
        if (!merged.isEmpty() && range.first <= merged.last().second)
            merged.last().second = qMax(merged.last().second, range.second);
        else
            merged.append(range);
    }
    return merged;
}
}

ChatReply messageReply(const XMPP::Message &message)
{
    const auto reply = message.reply();
    if (!reply.isValid())
        return {};
    ChatReply result {
        reply.id, reply.to.full(), reply.to.resource().isEmpty() ? reply.to.bare() : reply.to.resource(), {}
    };
    QStringList lines;
    for (const auto &range : fallbackRanges(message))
        lines.append(message.body().mid(range.first, range.second - range.first).split(QLatin1Char('\n')));
    for (auto &line : lines) {
        if (line.startsWith(QLatin1Char('>'))) {
            line.remove(0, 1);
            if (line.startsWith(QLatin1Char(' ')))
                line.remove(0, 1);
        }
    }
    result.text = lines.join(QLatin1Char('\n')).trimmed();
    return result;
}

QString messageReplyBody(const XMPP::Message &message)
{
    QString body = message.body();
    if (!message.reply().isValid())
        return body;
    const auto ranges = fallbackRanges(message);
    for (auto it = ranges.crbegin(); it != ranges.crend(); ++it)
        body.remove(it->first, it->second - it->first);
    return body;
}

void applyMessageReply(XMPP::Message &message, const ChatReply &reply)
{
    if (!reply.isValid())
        return;
    const XMPP::Message::Reply reference { XMPP::Jid(reply.to), reply.id };
    if (!reference.isValid())
        return;
    message.setReply(reference);
    // The fallback is optional. Do not move existing reference/fallback ranges
    // in media messages by prepending a second feature's compatibility text.
    if (!message.references().isEmpty() || !message.fallbacks().isEmpty())
        return;
    QString fallback;
    if (!reply.author.isEmpty())
        fallback = QStringLiteral("> %1:\n").arg(reply.author);
    for (const auto &line : reply.text.split(QLatin1Char('\n')))
        fallback += QStringLiteral("> %1\n").arg(line);
    auto fallbacks = message.fallbacks();
    fallbacks.append({ replyNamespace, { { 0, int(fallback.toUcs4().size()) } }, {} });
    message.setFallbacks(fallbacks);
    message.setBody(fallback + message.body());
}
