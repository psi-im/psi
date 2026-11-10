/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "historyarchivevalues.h"
#include "iris/xmpp_message.h"
#include "messagefileshares.h"
#include <QJsonArray>

// SQLite events.extra_data remains backwards compatible with existing OOB rows.
// Store protocol data, never rendered <share> markup or runtime player state.
namespace HistoryMessageExtras {
inline constexpr int mediaRefsVersion = 2;
// Identity-only projection for local suppression, before decrypt/display.
HistoryArchive::Entry identity(const XMPP::Message &, bool local, const QString &archiveUid = {});
QString encode(const XMPP::Message &message);
void    restore(const QString &json, XMPP::Message &message);
// A compact, value-only index of eligible attachment ordinals and fingerprints.
// Build on the GUI thread, alongside the protocol snapshot. SQL workers consume
// the index without parsing JIDs hidden inside SFS/Jingle sources.
QJsonArray mediaRefs(const XMPP::Message &message, int version = mediaRefsVersion);
// Decode the ordinal space of an existing index. Version 1 included non-file
// reference slots and applied the historical OOB display heuristic.
QList<MessageFileShares::FileShare> indexedFiles(const XMPP::Message &message, int version = mediaRefsVersion);
// Legacy SIMS/OOB restoration is safe for the startup migration worker.
// SFS restoration must run on the GUI because its sources may prepare JIDs.
void restoreFileSharing(const QString &json, XMPP::Message &message, bool includeSfs = true);
}
