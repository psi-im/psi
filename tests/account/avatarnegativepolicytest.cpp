/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "avatarnegativepolicy.h"

#include <QtTest/QtTest>

class AvatarNegativePolicyTest : public QObject {
    Q_OBJECT
private slots:
    void noHintIsDifferentFromKnownNoPhoto()
    {
        QCOMPARE(AvatarNegativePolicy::heat(0, 0), 0.0);
        const auto stamp = AvatarNegativePolicy::touch(0, 0);
        QVERIFY(stamp != 0);
        QCOMPARE(AvatarNegativePolicy::heat(stamp, 0), 1.0);
    }

    void repaintStormDoesNotBoostUsage()
    {
        const auto once = AvatarNegativePolicy::touch(0, 1000);
        for (quint32 i = 0; i < AvatarNegativePolicy::MinHitIntervalSeconds; ++i)
            QCOMPARE(AvatarNegativePolicy::touch(once, 1000 + i), once);
        const auto nextSession = AvatarNegativePolicy::touch(once, 1000 + AvatarNegativePolicy::MinHitIntervalSeconds);
        QVERIFY(AvatarNegativePolicy::heat(nextSession, 1600) > 1.0);
    }

    void popularityDecaysWithElapsedTime()
    {
        auto stamp = AvatarNegativePolicy::touch(0, 0);
        QCOMPARE(AvatarNegativePolicy::heat(stamp, AvatarNegativePolicy::HalfLifeSeconds), 0.5);
        QCOMPARE(AvatarNegativePolicy::heat(stamp, 2 * AvatarNegativePolicy::HalfLifeSeconds), 0.25);
        // An older popular contact eventually loses to a new occasional contact.
        for (quint32 hour = 1; hour <= 8; ++hour)
            stamp = AvatarNegativePolicy::touch(stamp, hour * 3600);
        auto recent = AvatarNegativePolicy::touch(0, 4 * AvatarNegativePolicy::HalfLifeSeconds);
        QVERIFY(AvatarNegativePolicy::heat(stamp, 4 * AvatarNegativePolicy::HalfLifeSeconds)
                < AvatarNegativePolicy::heat(recent, 4 * AvatarNegativePolicy::HalfLifeSeconds));
    }

    void periodicUseOutranksOneOffScroll()
    {
        auto frequent = AvatarNegativePolicy::touch(0, 0);
        const auto oneOff = AvatarNegativePolicy::touch(0, 0);
        for (quint32 hour = 1; hour <= 12; ++hour)
            frequent = AvatarNegativePolicy::touch(frequent, hour * 3600);
        const auto now = quint32(13 * 3600);
        QVERIFY(AvatarNegativePolicy::heat(frequent, now) > AvatarNegativePolicy::heat(oneOff, now));
    }

    void evictLeastUsefulByDecayedHeatAndRecency()
    {
        using Policy = AvatarNegativePolicy;
        QVector<Policy::Candidate> candidates {
            { QStringLiteral("old-and-frequent"), Policy::touch(0, 0) },
            { QStringLiteral("fresh"), Policy::touch(0, 4 * Policy::HalfLifeSeconds) },
            { QStringLiteral("old"), Policy::touch(0, 0) }
        };
        for (quint32 hour = 1; hour <= 8; ++hour)
            candidates[0].stamp = Policy::touch(candidates[0].stamp, hour * 3600);
        Policy::sortByUsefulness(candidates, 4 * Policy::HalfLifeSeconds);
        QCOMPARE(candidates[0].jid, QStringLiteral("old"));
        QCOMPARE(candidates[1].jid, QStringLiteral("old-and-frequent"));
        QCOMPARE(candidates[2].jid, QStringLiteral("fresh"));
    }

    void capacityAndBatchedVictims()
    {
        using Policy = AvatarNegativePolicy;
        QCOMPARE(Policy::trimCount(Policy::MaxEntries), 0);
        QCOMPARE(Policy::trimCount(Policy::MaxEntries + 1),
                 Policy::MaxEntries + 1 - Policy::TrimToEntries);

        QVector<Policy::Candidate> entries;
        entries.reserve(Policy::MaxEntries + 1);
        for (int i = 0; i < Policy::MaxEntries + 1; ++i) {
            const auto stamp = Policy::touch(0, quint32(i * 600));
            entries.push_back({ QString::number(i), stamp });
        }
        const quint32 now = quint32((Policy::MaxEntries + 1) * 600);
        Policy::sortByUsefulness(entries, now);
        const int evicted = Policy::trimCount(entries.size());
        QCOMPARE(entries.size() - evicted, Policy::TrimToEntries);
        QCOMPARE(entries.front().jid, QStringLiteral("0"));
        QCOMPARE(entries.at(evicted - 1).jid, QString::number(evicted - 1));
        QCOMPARE(entries.back().jid, QString::number(Policy::MaxEntries));
    }

    void scoreSaturatesWithoutWraparound()
    {
        // A near-full Q16.16 score remains valid after decay and another
        // credited hit; it must never wrap to zero.
        const quint64 nearSaturated = (quint64(1) << 32) | quint32(0xffff0000U);
        const auto result = AvatarNegativePolicy::touch(nearSaturated, 600);
        QVERIFY(quint32(result) > 0);
        QCOMPARE(AvatarNegativePolicy::lastAccess(result), quint32(601));
    }
};

QTEST_GUILESS_MAIN(AvatarNegativePolicyTest)
#include "avatarnegativepolicytest.moc"
