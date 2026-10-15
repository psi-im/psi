#include "filesharingchunksource.h"
#include "filesharingdownloader.h"
#include "filesharingitem.h"
#include "mediachunktransfer.h"
#include "psiaccount.h"
#include <QPointer>
#include <QTimer>
#include <utility>

MediaChunkAbort FileSharingChunkSource::fetch(FileSharingItem *item, quint64 offset, quint64 length,
                                              MediaChunkCompletion done, QObject *owner)
{
    if (!item || !item->account() || !item->fileSize()) {
        QTimer::singleShot(0, owner, [done] { done({}, QStringLiteral("Media source unavailable")); });
        return {};
    }
    auto download = item->download(FileSharingItem::Range { offset, length });
    auto transfer = new MediaChunkTransfer(
        download, *item->fileSize(), offset, length, std::move(done), [download] { download->abort(); }, owner);
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
    QObject::connect(download, &FileShareDownloader::failed, transfer,
                     [transfer, download] { transfer->failed(download->errorString()); });
    const QPointer<MediaChunkTransfer> guard(transfer);
    download->open();
    return [guard] {
        if (guard)
            guard->cancel();
    };
}
