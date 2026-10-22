#include "mediachunktransfer.h"
#include <QPointer>
#include <algorithm>
#include <utility>

MediaChunkTransfer::MediaChunkTransfer(QIODevice *device, quint64 size, quint64 offset, quint64 length,
                                       MediaChunkCompletion done, MediaChunkAbort stop, QObject *parent) :
    QObject(parent), device_(device), size_(size), offset_(offset), length_(length), done_(std::move(done)),
    stop_(std::move(stop))
{
    device_->setParent(this);
    timer_.setSingleShot(true);
    timer_.setInterval(60000);
    connect(&timer_, &QTimer::timeout, this, [this] { failed(QStringLiteral("Media chunk transfer stalled")); });
    connect(device_, &QIODevice::readyRead, this, [this] { pump(); });
    timer_.start();
}

MediaChunkTransfer::~MediaChunkTransfer()
{
    if (device_)
        device_->disconnect(this);
    if (stop_)
        stop_();
}

void MediaChunkTransfer::metadata(std::optional<quint64> size, std::optional<Range> range)
{
    if (finished_)
        return;
    if ((size && *size != size_) || !length_ || length_ > MediaChunkMaxSize || offset_ >= size_
        || length_ > size_ - offset_) {
        failed(QStringLiteral("Invalid media source size or request"));
        return;
    }
    // A server may legally ignore Range. A malformed partial response is a
    // different failure and must never be retried as an unrestricted response.
    if (!range && (offset_ || length_ != size_)) {
        finish({ MediaChunkError::RangeUnsupported, QStringLiteral("Source ignored byte range") });
        return;
    }
    if (range
        && (range->offset >= size_ || range->offset != offset_
            || (range->length ? range->length : size_ - range->offset) != length_)) {
        failed(QStringLiteral("Unexpected media response range"));
        return;
    }
    metadata_ = true;
    timer_.start();
    pump();
}

void MediaChunkTransfer::succeeded()
{
    completed_ = true;
    pump();
}
void MediaChunkTransfer::failed(QString message)
{
    finish(message.isEmpty() ? QStringLiteral("Media chunk transfer failed") : std::move(message));
}
void MediaChunkTransfer::integrityFailed(QString message)
{
    finish({ MediaChunkError::IntegrityError, std::move(message) });
}
void MediaChunkTransfer::cancel() { finish({ MediaChunkError::Cancelled, QStringLiteral("Media chunk cancelled") }); }
void MediaChunkTransfer::disconnected()
{
    disconnected_ = true;
    pump();
    if (!finished_ && !reading_ && !metadata_)
        failed(QStringLiteral("Media chunk ended without successful completion"));
}

void MediaChunkTransfer::pump()
{
    if (finished_ || reading_ || !metadata_)
        return;
    reading_ = true;
    while (!finished_ && device_->bytesAvailable() && quint64(bytes_.size()) < length_) {
        const auto bytes = device_->read(qint64(std::min<quint64>(65536, length_ - quint64(bytes_.size()))));
        if (bytes.isEmpty())
            break;
        bytes_ += bytes;
        timer_.start();
    }
    reading_ = false;
    if (!finished_ && completed_) {
        if (quint64(bytes_.size()) == length_ && device_->bytesAvailable() == 0)
            finish({ });
        else
            failed(QStringLiteral("Unexpected media chunk payload length"));
    } else if (!finished_ && disconnected_)
        failed(QStringLiteral("Media chunk ended without successful completion"));
}

void MediaChunkTransfer::finish(MediaChunkError error)
{
    if (finished_)
        return;
    finished_ = true;
    timer_.stop();
    // Never destroy a QIODevice within its readData()/signal stack.
    QTimer::singleShot(0, this, [this, error] {
        const QPointer<MediaChunkTransfer> guard(this);
        device_->disconnect(this);
        auto stop = std::exchange(stop_, { });
        if (stop)
            stop();
        if (!guard)
            return;
        delete std::exchange(device_, nullptr);
        if (!guard)
            return;
        auto done = std::exchange(done_, { });
        if (done)
            done(error.isEmpty() ? std::move(bytes_) : QByteArray(), error);
        if (guard)
            deleteLater();
    });
}
