/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QDateTime>
#include <QString>

namespace PsiMam {
// A client stanza ID is neither globally unique nor a verified MAM archive UID.
// Only use this conservative correlation within one already scoped conversation.
struct DeliveryIdentity {
    QString stanzaId;
    QString richText;
    bool outgoing = false;
    QDateTime timestamp;
};

inline bool sameDelivery(const DeliveryIdentity &a, const DeliveryIdentity &b)
{
    if (a.stanzaId.isEmpty() || a.stanzaId != b.stanzaId
        || a.outgoing != b.outgoing || a.richText != b.richText
        || !a.timestamp.isValid() || !b.timestamp.isValid())
        return false;
    return qAbs(a.timestamp.secsTo(b.timestamp)) <= 300;
}
} // namespace PsiMam
