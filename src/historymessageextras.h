/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "iris/xmpp_message.h"

// SQLite events.extra_data remains backwards compatible with existing OOB rows.
// Store protocol data, never rendered <share> markup or runtime player state.
namespace HistoryMessageExtras {
QString encode(const XMPP::Message &message);
void    restore(const QString &json, XMPP::Message &message);
}
