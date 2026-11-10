/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "iris/xmpp_message.h"
#include <QJsonArray>

// SQLite events.extra_data remains backwards compatible with existing OOB rows.
// Store protocol data, never rendered <share> markup or runtime player state.
namespace HistoryMessageExtras {
QString encode(const XMPP::Message &message);
// The live EDB may store a localized body. Index exactly that projection.
QString encode(const XMPP::Message &message, const QString &storedBody);
void    restore(const QString &json, XMPP::Message &message);
// A compact, value-only index of eligible attachment ordinals and fingerprints.
// Build on the GUI thread, alongside the protocol snapshot. SQL workers consume
// the index without parsing JIDs hidden inside SFS/Jingle sources.
QJsonArray mediaRefs(const XMPP::Message &message);
// Legacy SIMS/OOB restoration is safe for the startup migration worker.
// SFS restoration must run on the GUI because its sources may prepare JIDs.
void restoreAttachments(const QString &json, XMPP::Message &message, bool includeSfs = true);
}
