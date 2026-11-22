// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>
#include <QVector>
#include <functional>

class QObject;

// Best-effort, bounded, asynchronous poster extraction for outgoing videos.
// The decoder runs on a single worker thread; file publication can continue
// even when no compatible multimedia backend is installed.
class VideoThumbnailGenerator {
public:
    using Done = std::function<void(QByteArray jpeg)>;
    static void start(const QString &path, QObject *lifetime, Done done);
    static QVector<qint64> candidatePositions(qint64 durationMs);
    static QByteArray encodeTinyJpeg(const QImage &image);
};
