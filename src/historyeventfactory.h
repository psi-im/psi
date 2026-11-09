/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "psievent.h"
#include "historyeventpayload.h"

#include <QSqlRecord>
#include <QDateTime>
#include <QString>

class PsiCon;

namespace HistoryEventFactory {
Encoded encode(const PsiEvent::Ptr &event);

// Must be called on PsiCon's thread. Decoding can refer to PsiAccount and
// its XMPP::Client; neither QObject is part of the SQL worker state.
PsiEvent::Ptr decode(PsiCon *psi, const QSqlRecord &record);
}
