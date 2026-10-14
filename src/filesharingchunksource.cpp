#include "filesharingchunksource.h"

#include "filesharingdownloader.h"
#include "filesharingitem.h"
#include "iris/jingle-ft.h"
#include "psiaccount.h"

#include <QPointer>
#include <QTimer>
#include <algorithm>
#include <utility>

namespace {
class ChunkFetch : public QObject {
    QPointer<FileSharingItem>   item_;
    FileShareDownloader        *download_ = nullptr;
    quint64                     offset_, length_, size_;
    MediaChunkCache::Completion done_;
    QByteArray                  bytes_;
    QTimer                      timer_;
    bool                        metadata_ = false, completed_ = false, finished_ = false, reading_ = false;

public:
    ChunkFetch(FileSharingItem *item, quint64 offset, quint64 length, MediaChunkCache::Completion done,
               QObject *owner) :
        QObject(owner), item_(item), offset_(offset), length_(length),
        size_(item && item->fileSize() ? *item->fileSize() : 0), done_(std::move(done))
    {
        timer_.setSingleShot(true);
        timer_.setInterval(60000);
        connect(&timer_, &QTimer::timeout, this, [this] { finish(QStringLiteral("Media chunk transfer stalled")); });
    }

    ~ChunkFetch() override { stopDownload(); }

    void start()
    {
        if (!item_ || !item_->account() || !length_ || length_ > 4 * 1024 * 1024 || offset_ >= size_
            || length_ > size_ - offset_) {
            finish(QStringLiteral("Media source unavailable"));
            return;
        }
        connect(item_, &QObject::destroyed, this, [this] { finish(QStringLiteral("Media source removed")); });
        connect(item_->account(), &QObject::destroyed, this,
                [this] { finish(QStringLiteral("Media account removed")); });
        download_ = item_->download(FileSharingItem::Range { offset_, length_ });
        download_->setParent(this);
        connect(download_, &FileShareDownloader::metaDataChanged, this, [this] {
            const auto range      = download_->responseRange();
            const auto actualSize = download_->jingleFile().size();
            if (!actualSize || *actualSize != size_) {
                finish(QStringLiteral("Media source size changed"));
                return;
            }
            if (range && range->start >= size_) {
                finish(FileSharingChunkSource::rangeUnsupported);
                return;
            }
            const auto rangeLength = range && !range->size ? size_ - range->start : range ? range->size : size_;
            if ((range && (range->start != offset_ || rangeLength != length_))
                || (!range && (offset_ || length_ != size_))) {
                finish(FileSharingChunkSource::rangeUnsupported);
                return;
            }
            metadata_ = true;
            timer_.start();
            pump();
        });
        connect(download_, &FileShareDownloader::readyRead, this, [this] { pump(); });
        connect(download_, &FileShareDownloader::failed, this, [this] { finish(download_->errorString()); });
        connect(download_, &FileShareDownloader::transferCompleted, this, [this] {
            completed_ = true;
            pump();
        });
        connect(download_, &FileShareDownloader::disconnected, this, [this] {
            pump();
            if (!finished_ && quint64(bytes_.size()) != length_)
                finish(QStringLiteral("Media chunk ended early"));
        });
        timer_.start();
        download_->open();
    }

    void pump()
    {
        if (finished_ || reading_ || !metadata_)
            return;
        reading_ = true;
        while (!finished_ && download_->bytesAvailable() && quint64(bytes_.size()) < length_) {
            const auto bytes = download_->read(qint64(std::min<quint64>(65536, length_ - quint64(bytes_.size()))));
            if (bytes.isEmpty())
                break;
            bytes_ += bytes;
            timer_.start();
        }
        reading_ = false;
        if (!finished_ && completed_) {
            if (quint64(bytes_.size()) == length_)
                finish({});
            else
                finish(QStringLiteral("Incomplete media chunk"));
        }
    }

    void stopDownload()
    {
        if (!download_)
            return;
        download_->disconnect(this);
        download_->abort();
        delete std::exchange(download_, nullptr);
    }

    void finish(QString error)
    {
        if (finished_)
            return;
        finished_ = true;
        timer_.stop();
        // Reading the final bytes can synchronously emit a transport callback.
        // Destroy the transport outside its read stack, before releasing admission.
        QTimer::singleShot(0, this, [this, error] {
            stopDownload();
            auto done = std::exchange(done_, {});
            if (done)
                done(error.isEmpty() ? std::move(bytes_) : QByteArray(), error);
            deleteLater();
        });
    }
};
}

MediaChunkCache::Abort FileSharingChunkSource::fetch(FileSharingItem *item, quint64 offset, quint64 length,
                                                     MediaChunkCache::Completion done, QObject *owner)
{
    auto                       op = new ChunkFetch(item, offset, length, std::move(done), owner);
    const QPointer<ChunkFetch> guard(op);
    op->start();
    return [guard] {
        if (guard)
            guard->finish(QStringLiteral("Media chunk cancelled"));
    };
}
