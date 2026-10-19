// SPDX-License-Identifier: GPL-2.0-or-later
#include "accountincomingmessage.h"

#include <iris/xmpp_client.h>
#include <iris/xmpp_forwarding.h>

AccountIncomingMessage::AccountIncomingMessage(const XMPP::Message &message, const XMPP::EncryptionMetadata *metadata) :
    message(message)
{
    if (metadata && !metadata->methodId.isEmpty())
        encryption = *metadata;
}

bool AccountIncomingMessage::isPlaintextDirectMessage() const
{
    const auto display = message.displayMessage();
    if (encryption || message.wasEncrypted() || display.wasEncrypted() || display.spooled())
        return false;
    const auto forwarding = message.forwarded().type();
    if (forwarding != XMPP::Forwarding::ForwardedNone
        && forwarding != XMPP::Forwarding::ForwardedCarbonsReceived)
        return false;
    if (display.type() != XMPP::Message::Type::Chat
        && display.type() != XMPP::Message::Type::Normal)
        return false;
    // Do not change session policy based on chat states, receipts, forms,
    // or a message with only a subject/URL and no ordinary user text.
    return !display.body().isEmpty();
}

void AccountIncomingMessage::sendReceipt(XMPP::Client *client) const
{
    const auto request = message.displayMessage();
    if (!client || !client->hasStream() || request.messageReceipt() != XMPP::ReceiptRequest || request.id().isEmpty())
        return;

    XMPP::Message receipt(request.from());
    receipt.setId(client->genUniqueId());
    receipt.setMessageReceiptId(request.id());
    receipt.setMessageReceipt(XMPP::ReceiptReceived);
    // A protocol reply follows this request, independently of the user's
    // per-contact preference for newly composed messages.
    if (!encryption) {
        client->sendMessage(receipt);
        return;
    }
    auto *job = client->replyEncrypted(receipt.toStanza(&client->stream()).element(), *encryption);
    if (job->isFinished())
        job->deleteLater();
    else
        QObject::connect(job, &XMPP::EncryptionJob::finished, job, &QObject::deleteLater);
}
