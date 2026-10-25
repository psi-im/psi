/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QtGlobal>
#include <QString>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <limits>

// The AvatarCache's negative PHOTO resolution is a *session-only* hint, not
// an authoritative XMPP absence. It is invalidated by source changes and
// bounded independently from the FileCache containing actual image bytes.
//
// Packed stamp: high 32 bits = monotonic seconds since cache construction,
// plus one (zero means "not negative"); low 32 bits = Q16.16 heat.
//
// Distinct viewing sessions increase heat, while old hits lose weight with
// a one-day half-life. Repeated paint/layout lookups within ten minutes do
// not artificially make an entry popular.
struct AvatarNegativePolicy {
    static constexpr quint32 HalfLifeSeconds = 24 * 60 * 60;
    static constexpr quint32 MinHitIntervalSeconds = 10 * 60;
    static constexpr quint32 HeatUnit = 1U << 16;
    static constexpr int MaxEntries = 2048;
    static constexpr int TrimToEntries = 1536;

    static int trimCount(int count)
    {
        return count > MaxEntries ? count - TrimToEntries : 0;
    }

    static quint32 lastAccess(quint64 stamp) { return quint32(stamp >> 32); }

    static double heat(quint64 stamp, quint32 nowSeconds)
    {
        if (!stamp)
            return 0;
        const quint32 prior = lastAccess(stamp) - 1;
        const quint32 age = nowSeconds >= prior ? nowSeconds - prior : 0;
        return (double(quint32(stamp)) / HeatUnit) * std::exp2(-double(age) / HalfLifeSeconds);
    }

    static quint64 touch(quint64 stamp, quint32 nowSeconds)
    {
        // The clock is monotonic and the stamp never persists across sessions.
        const quint32 current = nowSeconds + 1;
        if (stamp && current >= lastAccess(stamp)
            && current - lastAccess(stamp) < MinHitIntervalSeconds)
            return stamp;

        const double newHeat = (heat(stamp, nowSeconds) + 1.0) * HeatUnit;
        const auto boundedHeat = quint32(newHeat >= double(std::numeric_limits<quint32>::max())
                                        ? std::numeric_limits<quint32>::max()
                                        : newHeat);
        return (quint64(current) << 32) | boundedHeat;
    }

    struct Candidate {
        QString jid;
        quint64 stamp;
        double decayedHeat = 0;
    };

    static void sortByUsefulness(QVector<Candidate> &items, quint32 nowSeconds)
    {
        // Evaluate the exponential once per candidate, not during every
        // comparison in std::sort: a 2048-entry eviction runs on the GUI
        // thread and should not repeatedly calculate an expensive score.
        for (auto &candidate : items)
            candidate.decayedHeat = heat(candidate.stamp, nowSeconds);
        std::sort(items.begin(), items.end(), [](const Candidate &a, const Candidate &b) {
            if (a.decayedHeat != b.decayedHeat)
                return a.decayedHeat < b.decayedHeat;
            if (lastAccess(a.stamp) != lastAccess(b.stamp))
                return lastAccess(a.stamp) < lastAccess(b.stamp);
            return a.jid < b.jid;
        });
    }
};
