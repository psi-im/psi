// SPDX-License-Identifier: GPL-2.0-or-later
#include "accountincomingmessage.h"

#include <iris/xmpp_client.h>

AccountIncomingMessage::AccountIncomingMessage(const XMPP::Message &message, const XMPP::EncryptionMetadata *metadata) :
    message(message)
{
    if (metadata && !metadata->methodId.isEmpty())
        encryption = *metadata;
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
