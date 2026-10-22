#include "filesharingchunksource.h"
#include "filesharingdownloader.h"
#include "filesharingitem.h"
#include "filesharingmanager.h"
#include "filesourceselector.h"
#include "mediachunkcache.h"
#include "mediachunktransfer.h"
#include "psiaccount.h"
#include "psicon.h"
#include <QDomDocument>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <algorithm>
#include <utility>

namespace {
class Sources : public QObject {
public:
    using QObject::QObject;
    FileSourceSelector selector;
};
Sources *sources(FileSharingItem *item)
{
    const auto name = QStringLiteral("shared-file-sources");
    if (auto object = item->findChild<QObject *>(name, Qt::FindDirectChildrenOnly))
        return static_cast<Sources *>(object);
    auto object = new Sources(item);
    object->setObjectName(name);
    return object;
}

// Servers without Range support use one shared sequential transfer. Its bytes
// still enter the same result file, with the same metadata and hash validation.
class SequentialDownload : public QObject {
    struct Waiter {
        quint64              offset, length;
        MediaChunkCompletion done;
    };

public:
    SequentialDownload(FileSharingItem *item, const QString &source) :
        QObject(item), item_(item), cache_(FileSharingChunkSource::configure(item)),
        identity_(FileSharingChunkSource::identity(item)), size_(*item->fileSize())
    {
        setObjectName(QStringLiteral("shared-file-sequential"));
        download_ = item->download(FileSharingItem::Range { 0, 0 }, source);
        download_->setParent(this);
        stall_.setSingleShot(true);
        stall_.setInterval(60000);
        connect(&stall_, &QTimer::timeout, this, [this] { finish(QStringLiteral("Shared file transfer stalled")); });
        connect(download_, &FileShareDownloader::readyRead, this, [this] { pump(); });
        connect(download_, &FileShareDownloader::metaDataChanged, this, [this] {
            const auto size  = download_->responseSize();
            const auto range = download_->responseRange();
            if ((size && *size != size_) || (range && (range->start || (range->size && range->size != size_)))) {
                finish(QStringLiteral("Unexpected sequential shared file response"));
                return;
            }
            pump();
        });
        connect(download_, &FileShareDownloader::fileMetadataReceived, this,
                [this](const XMPP::Jingle::FileTransfer::File &file) {
                    if (item_)
                        FileSharingChunkSource::addMetadata(item_, file);
                });
        connect(download_, &FileShareDownloader::transferCompleted, this, [this] {
            accepted_ = true;
            pump();
        });
        connect(download_, &FileShareDownloader::disconnected, this, [this] {
            if (!accepted_)
                finish(QStringLiteral("Shared file ended before validation"));
            else
                pump();
        });
        connect(download_, &FileShareDownloader::failed, this, [this] {
            if (download_->hasIntegrityError() && cache_) {
                cache_->invalidate(identity_, size_, 0, size_, download_->errorString());
                finish({ MediaChunkError::IntegrityError, download_->errorString() });
            } else
                finish(download_->errorString());
        });
        connect(cache_, &QObject::destroyed, this, [this] { finish(QStringLiteral("Shared file storage closed")); });
        connect(item->account(), &QObject::destroyed, this,
                [this] { finish(QStringLiteral("Shared file account removed")); });
        QTimer::singleShot(0, this, [this] {
            if (!stopped_) {
                stall_.start();
                download_->open();
            }
        });
    }
    ~SequentialDownload() override
    {
        download_->abort();
        const auto waiters = std::exchange(waiters_, { });
        for (const auto &waiter : waiters)
            waiter.done({ }, { MediaChunkError::Cancelled, QStringLiteral("Shared file source removed") });
    }
    MediaChunkAbort add(quint64 offset, quint64 length, MediaChunkCompletion done)
    {
        const auto id = ++next_;
        waiters_.insert(id, { offset, length, std::move(done) });
        const QPointer<SequentialDownload> guard(this);
        return [guard, id] {
            if (!guard || guard->stopped_)
                return;
            auto waiter = guard->waiters_.take(id);
            if (waiter.done)
                waiter.done({ }, { MediaChunkError::Cancelled, QStringLiteral("Shared file request cancelled") });
            if (guard && guard->waiters_.isEmpty())
                guard->finish({ MediaChunkError::Cancelled, QStringLiteral("Shared file download cancelled") });
        };
    }

private:
    void pump()
    {
        if (stopped_ || writing_)
            return;
        if (!cache_) {
            finish(QStringLiteral("Shared file storage closed"));
            return;
        }
        if (download_->bytesAvailable()) {
            if (position_ >= size_) {
                finish(QStringLiteral("Shared file exceeds advertised size"));
                return;
            }
            const auto count = std::min<quint64>(MediaChunkMaxSize,
                                                 std::min<quint64>(download_->bytesAvailable(), size_ - position_));
            // A final read can synchronously emit success/disconnected.
            writing_         = true;
            const auto bytes = download_->read(qint64(count));
            if (stopped_)
                return;
            if (bytes.isEmpty()) {
                finish(QStringLiteral("Cannot read sequential shared file"));
                return;
            }
            const QPointer<SequentialDownload> guard(this);
            cache_->storeBytes(identity_, size_, position_, bytes,
                               [guard, count = quint64(bytes.size())](QByteArray, MediaChunkError error) {
                                   if (!guard || guard->stopped_)
                                       return;
                                   guard->writing_ = false;
                                   if (!error.isEmpty()) {
                                       guard->finish(error);
                                       return;
                                   }
                                   guard->position_ += count;
                                   guard->stall_.start();
                                   guard->pump();
                               });
        } else if (accepted_) {
            if (position_ != size_)
                finish(QStringLiteral("Incomplete sequential shared file"));
            else
                finish({ });
        }
    }
    void finish(MediaChunkError error)
    {
        if (stopped_)
            return;
        stopped_ = true;
        stall_.stop();
        setObjectName({ });
        download_->disconnect(this);
        download_->abort();
        const auto waiters  = std::exchange(waiters_, { });
        const auto cache    = cache_;
        const auto identity = identity_;
        const auto size     = size_;
        deleteLater();
        for (const auto &waiter : waiters) {
            if (error.isEmpty() && cache)
                cache->readStored(identity, size, waiter.offset, waiter.length, waiter.done);
            else
                waiter.done({ },
                            error.isEmpty() ? MediaChunkError(QStringLiteral("Shared file storage closed")) : error);
        }
    }
    QPointer<FileSharingItem> item_;
    QPointer<MediaChunkCache> cache_;
    QString                   identity_;
    quint64                   size_, position_ = 0, next_ = 0;
    FileShareDownloader      *download_;
    QTimer                    stall_;
    QHash<quint64, Waiter>    waiters_;
    bool                      stopped_ = false, writing_ = false, accepted_ = false;
};
MediaChunkAbort sequential(FileSharingItem *item, const QString &source, quint64 offset, quint64 length,
                           MediaChunkCompletion done)
{
    auto object   = item->findChild<QObject *>(QStringLiteral("shared-file-sequential"), Qt::FindDirectChildrenOnly);
    auto download = object ? static_cast<SequentialDownload *>(object) : new SequentialDownload(item, source);
    return download->add(offset, length, std::move(done));
}
}

QString FileSharingChunkSource::identity(FileSharingItem *item)
{
    return item && item->account() ? item->account()->id() + QChar(0) + item->sums().value(0).toString() : QString();
}
void FileSharingChunkSource::addMetadata(FileSharingItem *item, const XMPP::Jingle::FileTransfer::File &file)
{
    if (!item || !item->account() || !item->fileSize())
        return;
    const auto               size = *item->fileSize();
    QList<MediaFileChecksum> hashes;
    for (const auto &h : item->sums())
        if (!h.data().isEmpty())
            hashes.append({ 0, size, h.stringType(), h.data() });
    for (const auto &h : file.computedHashes())
        hashes.append({ 0, size, h.stringType(), h.data() });
    const auto range = file.range();
    if (range.offset < size) {
        const auto length = range.length ? range.length : size - range.offset;
        for (const auto &h : range.hashes)
            if (!h.data().isEmpty())
                hashes.append({ range.offset, length, h.stringType(), h.data() });
    }
    QDomDocument document;
    document.appendChild(item->fileMetadata().toXml(&document));
    QVariantMap metadata = item->metaData();
    metadata.insert(QStringLiteral("file-xml"), document.toString(-1));
    metadata.insert(QStringLiteral("sources"), item->uris());
    metadata.insert(QStringLiteral("media-type"), item->mimeType());
    metadata.insert(QStringLiteral("name"), item->displayName());
    metadata.insert(QStringLiteral("identity"), identity(item));
    auto cache = item->account()->psi()->fileSharingManager()->mediaChunkCache();
    cache->configure(identity(item), size, hashes, metadata);
}
MediaChunkCache *FileSharingChunkSource::configure(FileSharingItem *item)
{
    if (!item || !item->account() || !item->fileSize())
        return nullptr;
    addMetadata(item, item->fileMetadata());
    return item->account()->psi()->fileSharingManager()->mediaChunkCache();
}
QString FileSharingChunkSource::localFile(FileSharingItem *item)
{
    if (!item || !item->account() || !item->fileSize())
        return { };
    auto       cache = item->account()->psi()->fileSharingManager()->mediaChunkCache();
    const auto state = cache->fileState(identity(item), *item->fileSize());
    return state.complete && state.error.isEmpty() && QFileInfo(state.path).isReadable() ? state.path : QString();
}

QString FileSharingChunkSource::error(FileSharingItem *item)
{
    if (!item || !item->account() || !item->fileSize())
        return { };
    auto cache = item->account()->psi()->fileSharingManager()->mediaChunkCache();
    return cache->fileState(identity(item), *item->fileSize()).error;
}

MediaChunkAbort FileSharingChunkSource::fetch(FileSharingItem *item, quint64 offset, quint64 length,
                                              MediaChunkCompletion done, QObject *owner)
{
    if (!item || !item->account() || !item->fileSize()) {
        QTimer::singleShot(0, owner, [done] { done({ }, QStringLiteral("Media source unavailable")); });
        return { };
    }
    configure(item);
    auto provider   = sources(item);
    auto candidates = FileSharingItem::sortSourcesByPriority(item->uris());
    std::reverse(candidates.begin(), candidates.end());
    candidates.removeDuplicates();
    const auto source = provider->selector.acquire(candidates);
    auto       timer  = std::make_shared<QElapsedTimer>();
    timer->start();
    const QPointer<Sources> providerGuard(provider);
    auto deliver = [done = std::move(done), providerGuard, source, timer](QByteArray bytes, MediaChunkError error) {
        if (providerGuard) {
            providerGuard->selector.release(source, quint64(bytes.size()), timer->elapsed(), error.isEmpty(),
                                            error.code() == MediaChunkError::Cancelled);
        }
        done(std::move(bytes), error);
    };
    struct Request {
        bool            cancelled = false;
        MediaChunkAbort fallback;
    };
    auto                            request = std::make_shared<Request>();
    const QPointer<FileSharingItem> resource(item);
    auto                            complete
        = [deliver, resource, source, offset, length, request](QByteArray bytes, MediaChunkError error) mutable {
              if (error.code() == MediaChunkError::RangeUnsupported && resource && !request->cancelled) {
                  request->fallback = sequential(resource, source, offset, length, std::move(deliver));
              } else
                  deliver(std::move(bytes), error);
          };
    auto download = item->download(FileSharingItem::Range { offset, length }, source);
    download->setJingleSessionPool(item->account()->psi()->fileSharingManager()->jingleMediaSessions(item->account()));
    auto transfer = new MediaChunkTransfer(
        download, *item->fileSize(), offset, length, std::move(complete), [download] { download->abort(); }, owner);
    auto checksumSpan = std::make_shared<QPair<quint64, quint64>>(offset, length);
    QObject::connect(download, &FileShareDownloader::fileMetadataReceived, item,
                     [item, checksumSpan](const XMPP::Jingle::FileTransfer::File &file) {
                         addMetadata(item, file);
                         const auto range = file.range();
                         if (!range.hashes.isEmpty() && range.offset < *item->fileSize())
                             *checksumSpan
                                 = { range.offset, range.length ? range.length : *item->fileSize() - range.offset };
                         else if (!file.computedHashes().isEmpty())
                             *checksumSpan = { 0, *item->fileSize() };
                     });
    QObject::connect(item, &QObject::destroyed, transfer,
                     [transfer] { transfer->failed(QStringLiteral("Media source removed")); });
    QObject::connect(item->account(), &QObject::destroyed, transfer,
                     [transfer] { transfer->failed(QStringLiteral("Media account removed")); });
    QObject::connect(download, &FileShareDownloader::metaDataChanged, transfer, [transfer, download] {
        std::optional<MediaChunkTransfer::Range> range;
        if (const auto r = download->responseRange())
            range = MediaChunkTransfer::Range { r->start, r->size };
        transfer->metadata(download->responseSize(), range);
    });
    QObject::connect(download, &FileShareDownloader::transferCompleted, transfer, &MediaChunkTransfer::succeeded);
    QObject::connect(download, &FileShareDownloader::disconnected, transfer, &MediaChunkTransfer::disconnected);
    QObject::connect(download, &FileShareDownloader::failed, transfer, [transfer, download, resource, checksumSpan] {
        if (download->hasIntegrityError() && resource && resource->fileSize()) {
            if (auto cache = configure(resource))
                cache->invalidate(identity(resource), *resource->fileSize(), checksumSpan->first, checksumSpan->second,
                                  download->errorString());
            transfer->integrityFailed(download->errorString());
        } else
            transfer->failed(download->errorString());
    });
    const QPointer<MediaChunkTransfer> guard(transfer);
    download->open();
    return [guard, request] {
        request->cancelled = true;
        if (request->fallback) {
            auto abort = std::exchange(request->fallback, { });
            abort();
        } else if (guard)
            guard->cancel();
    };
}
