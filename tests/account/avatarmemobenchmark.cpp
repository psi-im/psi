/* SPDX-License-Identifier: GPL-2.0-or-later */
// Reproducible synthetic lookup microbenchmark, NOT a whole-timeline benchmark.
// No network, disk I/O, image decoding, compositing, text or avatar painting.
#include "avatarnegativememo.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QHash>
#include <QPixmap>
#include <QTest>
#include <QVector>

#include <algorithm>
#include <functional>

class AvatarCacheBenchmark final : public QObject {
    Q_OBJECT
private:
    using Work = std::function<void()>;
    static double medianMicros(const Work &work, int lookups)
    {
        QVector<qint64> samples;
        samples.reserve(7);
        for (int i = 0; i < 7; ++i) {
            QElapsedTimer timer;
            timer.start();
            work();
            samples.append(timer.nsecsElapsed());
        }
        std::sort(samples.begin(), samples.end());
        return double(samples.at(3)) / (double(lookups) * 1000.0);
    }
private slots:
    void warmLookupScenario()
    {
        constexpr int senders = 1000;
        constexpr int passes = 32;
        QStringList identities;
        identities.reserve(senders);
        QHash<QString, QPixmap> windowMemo;
        QHash<QString, QPixmap> positiveIconset;
        AvatarNegativeMemo negative(4096, 30000);
        QPixmap picture(96, 96);
        picture.fill(Qt::blue);
        for (int i = 0; i < senders; ++i) {
            const QString id = QStringLiteral("account/test/contact/") + QString::number(i);
            identities.append(id);
            const bool hasAvatar = (i % 5) == 0;
            windowMemo.insert(id, hasAvatar ? picture : QPixmap());
            if (hasAvatar)
                positiveIconset.insert(id, picture);
            else
                negative.record(id, AvatarNegativeMemo::State::Unknown, 0);
        }

        volatile qint64 checksum = 0; // Force all warm lookups to be observed.
        const auto perWindow = [&]() {
            for (int p = 0; p < passes; ++p)
                for (const auto &id : identities)
                    checksum += !windowMemo.value(id).isNull();
        };
        const auto onlyPositive = [&]() {
            for (int p = 0; p < passes; ++p)
                for (const auto &id : identities)
                    checksum += !positiveIconset.value(id).isNull();
        };
        const auto centralNegative = [&]() {
            for (int p = 0; p < passes; ++p) {
                for (const auto &id : identities) {
                    auto it = positiveIconset.constFind(id);
                    if (it != positiveIconset.cend()) {
                        checksum += !it.value().isNull();
                    } else if (negative.lookup(id, 1)) {
                        checksum += 0;
                    } else {
                        QFAIL("A prepared negative entry must not miss");
                    }
                }
            }
        };
        perWindow();
        onlyPositive();
        centralNegative();

        const int lookups = senders * passes;
        const double a = medianMicros(perWindow, lookups);
        const double b = medianMicros(onlyPositive, lookups);
        const double c = medianMicros(centralNegative, lookups);
        qInfo().noquote() << QStringLiteral(
            "SYNTHETIC avatar lookup: %1 senders, %2 passes, 20%% positive; "
            "Qt %3; median of 7; microseconds/lookup; "
            "A per-window=%4, B positive-only optimistic bound=%5, "
            "C central negative=%6; checksum=%7")
            .arg(senders).arg(passes).arg(QLatin1String(QT_VERSION_STR))
            .arg(a, 0, 'f', 4).arg(b, 0, 'f', 4).arg(c, 0, 'f', 4)
            .arg(qint64(checksum));
        QVERIFY(checksum > 0);
        // This is an observation, never a CI speed gate: runner scheduling
        // noise and whole-chat costs dominate microbenchmark deltas.
    }
};

QTEST_MAIN(AvatarCacheBenchmark)
#include "avatarmemobenchmark.moc"
