/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QDateTime>
#include <QString>

// Plain values only. May cross queued connections without taking ownership
// of a PsiEvent, PsiAccount, QSqlDatabase or any GUI-side QObject.
namespace HistoryEventFactory {
struct Encoded {
    bool valid = false, isMessage = false;
    int type = 0, direction = 2;
    QDateTime timestamp;
    QString subject, body, language, extras;
    QString matchSubject, matchBody, sender, messageId, replaceId;
};
}
