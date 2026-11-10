/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "iris/xmpp_message.h"

// SQLite events.extra_data remains backwards compatible with existing OOB rows.
// Store protocol data, never rendered <share> markup or runtime player state.
namespace HistoryMessageExtras {
QString encode(const XMPP::Message &message);
void    restore(const QString &json, XMPP::Message &message);
// Only what the message attaches. The history upgrade calls this off the GUI thread, where
// it must not touch a JID, as Iris prepares those through a cache without locks. Only a
// XEP-0447 Jingle source would, and messages stored with those came after that upgrade.
void restoreAttachments(const QString &json, XMPP::Message &message);
}
