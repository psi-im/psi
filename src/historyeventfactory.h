/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "psievent.h"

#include <QSqlRecord>

class PsiCon;

namespace HistoryEventFactory {
// Must be called on PsiCon's thread. Decoding can refer to PsiAccount and
// its XMPP::Client; neither QObject is part of the SQL worker state.
PsiEvent::Ptr decode(PsiCon *psi, const QSqlRecord &record);
}
