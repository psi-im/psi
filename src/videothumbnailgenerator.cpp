// SPDX-License-Identifier: GPL-2.0-or-later
#include "videothumbnailgenerator.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QMediaPlayer>
#include <QPointer>
#include <QQueue>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QVideoSink>
#else
#include <QAbstractVideoBuffer>
#include <QVideoProbe>
#endif
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {

double frameQuality(const QImage &source)
{
    if (source.isNull())
        return -std::numeric_limits<double>::infinity();
    const QImage sample = source.scaled(QSize(96, 54), Qt::KeepAspectRatio, Qt::FastTransformation)
                              .convertToFormat(QImage::Format_RGB32);
    if (sample.isNull())
        return -std::numeric_limits<double>::infinity();
    double sum = 0.0, squares = 0.0, edges = 0.0;
    const int pixels = sample.width() * sample.height();
    for (int y = 0; y < sample.height(); ++y) {
        for (int x = 0; x < sample.width(); ++x) {
            const int gray = qGray(sample.pixel(x, y));
            sum += gray;
            squares += gray * gray;
            if (x)
                edges += std::abs(gray - qGray(sample.pixel(x - 1, y)));
        }
    }
    const double mean = sum / pixels;
    return std::max(0.0, squares / pixels - mean * mean) + edges / pixels * 8.0
        - (mean < 18.0 || mean > 245.0 ? 1000.0 : 0.0);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
QImage frameImage(const QVideoFrame &frame)
{
    QVideoFrame mapped(frame);
    if (!mapped.map(QAbstractVideoBuffer::ReadOnly))
        return {};
    const auto format = QVideoFrame::imageFormatFromPixelFormat(mapped.pixelFormat());
    QImage image;
    if (format != QImage::Format_Invalid) {
        const QImage view(mapped.bits(), mapped.width(), mapped.height(), mapped.bytesPerLine(), format);
        image = view.copy();
    }
    mapped.unmap();
    return image;
}
#else
QImage frameImage(const QVideoFrame &frame) { return frame.toImage(); }
#endif

class ThumbnailJob final : public QObject {
public:
    ThumbnailJob(QString path, std::function<void(QByteArray)> done, QObject *parent) :
        QObject(parent), path_(std::move(path)), done_(std::move(done))
    {
        step_.setSingleShot(true);
        deadline_.setSingleShot(true);
        connect(&step_, &QTimer::timeout, this, [this] { nextCandidate(); });
        connect(&deadline_, &QTimer::timeout, this, [this] { finish(); });
        connect(&player_, &QMediaPlayer::durationChanged, this, [this](qint64) { maybeSample(); });
        connect(&player_, &QMediaPlayer::seekableChanged, this, [this](bool) { maybeSample(); });
        connect(&player_, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::InvalidMedia || status == QMediaPlayer::NoMedia) {
                finish();
                return;
            }
            if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia)
                maybeSample();
        });
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        connect(&sink_, &QVideoSink::videoFrameChanged, this,
                [this](const QVideoFrame &frame) { capture(frame); });
#else
        connect(&probe_, &QVideoProbe::videoFrameProbed, this,
                [this](const QVideoFrame &frame) { capture(frame); });
#endif
    }

    ~ThumbnailJob() override
    {
        finished_ = true;
        step_.stop();
        deadline_.stop();
        player_.stop();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        player_.setVideoSink(nullptr);
#endif
    }

    void begin()
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        player_.setVideoSink(&sink_);
        player_.setSource(QUrl::fromLocalFile(path_));
#else
        if (!probe_.setSource(&player_)) {
            finish();
            return;
        }
        player_.setMedia(QUrl::fromLocalFile(path_));
#endif
        deadline_.start(9000); // Bound a missing/unsupported decoder.
    }

private:
    void maybeSample()
    {
        if (finished_ || sampling_)
            return;
        const auto status = player_.mediaStatus();
        if (status != QMediaPlayer::LoadedMedia && status != QMediaPlayer::BufferedMedia)
            return;
        if (player_.duration() > 0 && player_.isSeekable()) {
            beginSampling();
        } else if (!metadataFallbackScheduled_) {
            metadataFallbackScheduled_ = true;
            QTimer::singleShot(700, this, [this] {
                if (!finished_ && !sampling_)
                    beginSampling();
            });
        }
    }

    void beginSampling()
    {
        if (finished_ || sampling_)
            return;
        candidates_ = player_.duration() > 0 && player_.isSeekable()
            ? VideoThumbnailGenerator::candidatePositions(player_.duration()) : QVector<qint64> { 0 };
        sampling_ = true;
        index_ = -1;
        nextCandidate();
    }

    void nextCandidate()
    {
        if (finished_ || !sampling_)
            return;
        if (++index_ >= candidates_.size()) {
            finish();
            return;
        }
        captured_ = false;
        player_.setPosition(candidates_.at(index_));
        player_.play();
        step_.start(1800);
    }

    void capture(const QVideoFrame &frame)
    {
        if (finished_ || !sampling_ || captured_ || !frame.isValid())
            return;
        if (frame.width() > 4096 || frame.height() > 4096)
            return;
        const qint64 position = frame.startTime() >= 0 ? frame.startTime() / 1000 : player_.position();
        const qint64 target = candidates_.at(index_);
        if (target > 0 && std::abs(position - target) > 1250)
            return;
        if (position < 0 || (player_.duration() > 0 && position > player_.duration() / 2))
            return;
        QImage image = frameImage(frame);
        if (image.isNull() || image.width() > 4096 || image.height() > 4096)
            return;
        const double quality = frameQuality(image);
        if (quality > bestQuality_) {
            bestQuality_ = quality;
            bestImage_ = std::move(image);
        }
        captured_ = true;
        step_.stop();
        player_.pause();
        QTimer::singleShot(0, this, [this] { nextCandidate(); });
    }

    void finish()
    {
        if (finished_)
            return;
        finished_ = true;
        step_.stop();
        deadline_.stop();
        player_.stop();
        if (done_)
            done_(VideoThumbnailGenerator::encodeTinyJpeg(bestImage_));
        deleteLater();
    }

    QString path_;
    std::function<void(QByteArray)> done_;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QVideoSink sink_;
#else
    QVideoProbe probe_;
#endif
    QMediaPlayer player_;
    QTimer step_, deadline_;
    QVector<qint64> candidates_;
    QImage bestImage_;
    double bestQuality_ = -std::numeric_limits<double>::infinity();
    int index_ = -1;
    bool sampling_ = false, metadataFallbackScheduled_ = false, captured_ = false, finished_ = false;
};

class ThumbnailQueue final : public QObject {
public:
    explicit ThumbnailQueue(QObject *parent) : QObject(parent)
    {
        worker_ = new QObject;
        worker_->moveToThread(&thread_);
        connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
        thread_.start();
    }
    ~ThumbnailQueue() override
    {
        thread_.quit();
        thread_.wait();
    }
    void enqueue(QString path, QObject *lifetime, VideoThumbnailGenerator::Done done)
    {
        requests_.enqueue({ std::move(path), QPointer<QObject>(lifetime), std::move(done) });
        pump();
    }

private:
    struct Request {
        QString path;
        QPointer<QObject> lifetime;
        VideoThumbnailGenerator::Done done;
    };
    void pump()
    {
        if (busy_)
            return;
        while (!requests_.isEmpty()) {
            auto request = requests_.dequeue();
            if (!request.lifetime)
                continue;
            busy_ = true;
            const QPointer<ThumbnailQueue> guard(this);
            auto completion = [guard, lifetime = request.lifetime, done = std::move(request.done)](
                                  QByteArray jpeg) mutable {
                if (!guard)
                    return;
                QMetaObject::invokeMethod(guard.data(), [guard, lifetime, done = std::move(done),
                                                         jpeg = std::move(jpeg)]() mutable {
                    if (!guard)
                        return;
                    guard->busy_ = false;
                    if (lifetime)
                        done(std::move(jpeg));
                    guard->pump();
                }, Qt::QueuedConnection);
            };
            const QString path = std::move(request.path);
            QMetaObject::invokeMethod(worker_, [worker = worker_, path, done = std::move(completion)]() mutable {
                auto *job = new ThumbnailJob(path, std::move(done), worker);
                job->begin();
            }, Qt::QueuedConnection);
            return;
        }
    }
    QThread thread_;
    QObject *worker_ = nullptr;
    QQueue<Request> requests_;
    bool busy_ = false;
};

} // namespace

QVector<qint64> VideoThumbnailGenerator::candidatePositions(qint64 durationMs)
{
    if (durationMs <= 0)
        return { 0 };
    const qint64 maximum = durationMs / 2;
    QVector<qint64> positions;
    for (const qint64 preferred : { qint64(2000), qint64(4000), qint64(500), qint64(5000) }) {
        const qint64 position = std::min(preferred, maximum);
        if (!positions.contains(position))
            positions.append(position);
    }
    return positions;
}

QByteArray VideoThumbnailGenerator::encodeTinyJpeg(const QImage &image)
{
    if (image.isNull())
        return {};
    // XEP-0264 / BoB should carry a useful tiny poster, not the generic file
    // icon. Cap both dimensions and bytes; publication works if extraction fails.
    for (const int width : { 240, 200, 160, 120, 96 }) {
        const auto scaled = image.scaled(image.size().boundedTo(QSize(width, width)),
                                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
        for (const int quality : { 75, 60, 45, 30 }) {
            QByteArray jpeg;
            QBuffer buffer(&jpeg);
            if (!buffer.open(QIODevice::WriteOnly) || !scaled.save(&buffer, "JPEG", quality))
                return {};
            if (jpeg.size() <= 50 * 1024)
                return jpeg;
        }
    }
    return {};
}

void VideoThumbnailGenerator::start(const QString &path, QObject *lifetime, Done done)
{
    if (!lifetime || path.isEmpty() || !done)
        return;
    static auto *queue = new ThumbnailQueue(QCoreApplication::instance());
    queue->enqueue(path, lifetime, std::move(done));
}
