/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <functional>
#include <iris/jingle-ft.h>
class QObject;
namespace XMPP {
class Client;
}

// Registers an owner-side Jingle factory and announces it in the owner's private
// PEP catalog. Local SIMS hash requests remain usable if PEP is unsupported.
bool publishSharedFile(XMPP::Client *client, const XMPP::Jingle::FileTransfer::File &file, const QString &path,
                       QObject *context, std::function<void(bool, const QString &)> announced);
