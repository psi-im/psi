/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QHash>
#include <QString>

// Synchronous identity fence: leave/rejoin/nick reuse increments generation.
// A pending network result carries the generation captured at request start;
// a reply from an older generation can never resurrect old occupant images.
class AvatarEpochPolicy {
public:
    quint64 generation(const QString &identity) const;
    quint64 advance(const QString &identity);
    void forgetPrefix(const QString &prefix);

private:
    QHash<QString, quint64> epochs_;
};
