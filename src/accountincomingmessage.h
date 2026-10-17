// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <iris/xmpp_encryption.h>
#include <iris/xmpp_message.h>
#include <optional>

namespace XMPP {
class Client;
}

// Retain authenticated encryption context while a message waits behind an
// asynchronous OpenPGP decryption. EME labels alone do not establish this context.
struct AccountIncomingMessage {
    explicit AccountIncomingMessage(const XMPP::Message &message, const XMPP::EncryptionMetadata *metadata = nullptr);

    // The caller applies the account's receipt/subscription policy first.
    void sendReceipt(XMPP::Client *client) const;

    XMPP::Message                           message;
    std::optional<XMPP::EncryptionMetadata> encryption;
};
