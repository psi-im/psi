/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "psievent.h"

#include <QSqlRecord>
#include <QDateTime>
#include <QString>

class PsiCon;

namespace HistoryEventFactory {
// A value-only, immutable SQL insertion request. Encode it before crossing
// the GUI/EDB boundary: a worker must never dereference a PsiEvent or PsiAccount.
struct Encoded {
    bool valid = false, isMessage = false;
    int type = 0, direction = 2;
    QDateTime timestamp;
    QString subject, body, language, extras;
    QString matchSubject, matchBody, sender, messageId, replaceId;
};
Encoded encode(const PsiEvent::Ptr &event);

// Must be called on PsiCon's thread. Decoding can refer to PsiAccount and
// its XMPP::Client; neither QObject is part of the SQL worker state.
PsiEvent::Ptr decode(PsiCon *psi, const QSqlRecord &record);
}
