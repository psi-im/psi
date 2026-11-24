/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "iris/xmpp_hash.h"
#include "iris/xmpp_jid.h"
#include "iris/jingle-session.h"

#include <QList>
#include <QString>
#include <functional>
#include <optional>

namespace XMPP::Jingle {
namespace FileTransfer { class Application; }
}

// Answers XEP-0234 requests for a file on behalf of one account.
namespace FileSharingResponder {
struct ServedFile {
    QString           path;
    QList<XMPP::Hash> hashes;
    XMPP::Jingle::EncryptionPolicy encryption = XMPP::Jingle::EncryptionPolicy::Disabled;
};

// A local copy of the requested file, if the account serves it. An unknown file and
// one this account does not serve both give nothing, so a requester cannot tell them
// apart. A copy on disk alone is no reason to serve: bytes are shared between the
// accounts of a profile, the decision to serve them is not.
using Lookup = std::function<std::optional<ServedFile>(const XMPP::Jid &, const QList<XMPP::Hash> &)>;

// Shared by JinglePub and receiver-initiated requests. Never weaken a
// session's protection; an active session cannot be upgraded in place.
bool applyProtectionPolicy(XMPP::Jingle::Session *session, XMPP::Jingle::EncryptionPolicy policy);

// Resolve the grant again immediately before opening the file. No factory or
// pending content may retain serving authority independently of the catalog.
void attachSource(XMPP::Jingle::FileTransfer::Application *app, const XMPP::Jid &peer,
                  const ServedFile &file, const Lookup &lookup);

// True when the session was a request and has been answered, accepted or refused.
// False for anything else, such as a file offer for the user to accept.
bool answer(XMPP::Jingle::Session *session, const Lookup &lookup);
}
