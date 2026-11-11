/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "chatmediaimagesource.h"

#include "filereader.h"
#include "filesharingdownloader.h"
#include "filesharingitem.h"
#include "filesharingmanager.h"
#include "psiaccount.h"
#include "psicon.h"
#include "widgets/MamChatWidget.h"

#include <QFileInfo>
#include <QImageReader>
#include <QIODevice>
#include <QMimeDatabase>
#include <QPointer>
#include <QUrl>
#include <algorithm>
#include <limits>
#include <memory>

namespace {
constexpr int PageSize = 40;
// Bound memory usage while a remote image is streamed into the viewer.
constexpr qint64 MaxImageBytes = 128 * 1024 * 1024;

QString imageName(const ChatMedia::Entry &entry)
{
    const auto sharing = entry.announcement.file.reference.mediaSharing();
    const QString name = sharing.file.name();
    return name.isEmpty() ? QFileInfo(entry.announcement.file.url().path()).fileName() : name;
}

bool isImage(const ChatMedia::Entry &entry)
{
    const QString type = entry.announcement.file.reference.mediaSharing().file.mediaType();
    if (!type.isEmpty())
        return type.startsWith(QLatin1String("image/"));
    return QMimeDatabase().mimeTypeForFile(imageName(entry), QMimeDatabase::MatchExtension)
        .name().startsWith(QLatin1String("image/"));
}
}

struct ChatMediaImageSource::Walk {
    int direction = 0;
    QList<ChatMedia::Entry> entries;
    int index = 0;
    bool hasMore = false;
    ChatMedia::Cursor next;
    Result done;
};

ChatMediaImageSource::ChatMediaImageSource(FileSharingManager *manager, PsiAccount *account,
                                         const HistoryArchive::Key &key, qint64 eventId,
                                         const QDateTime &date, int ordinal, const QString &sourceId)
    : manager_(manager), account_(account),
      cursor_ { date.isValid() ? date.toMSecsSinceEpoch() : qint64(0),
                eventId, ordinal, !date.isValid() },
      sourceId_(sourceId), hasOriginalEvent_(eventId > 0)
{
    if (manager_ && account_)
        browser_ = manager_->chatMedia(account_, key, this);
}

void ChatMediaImageSource::install(MamChatWidget *chat, PsiAccount *account,
                                   const HistoryArchive::Key &key)
{
    if (!chat || !account || !account->psi())
        return;
    const QPointer<PsiAccount> acc(account);
    const QPointer<FileSharingManager> manager(account->psi()->fileSharingManager());
    chat->setImageOpenHandler([acc, manager, key](const QImage &image, const QString &path,
                                                  qint64 eventId, const QDateTime &date, int ordinal, const QString &sourceId) {
        auto *source = acc && manager
            ? new ChatMediaImageSource(manager, acc, key, eventId, date, ordinal, sourceId)
            : nullptr;
        ImageViewer::showImage(image, path, source);
    });
}

void ChatMediaImageSource::adjacent(int direction, Result result)
{
    if (!browser_ || !manager_ || !account_ || !result || direction == 0) {
        if (result)
            result({}, {}, false);
        return;
    }
    auto walk = std::make_shared<Walk>();
    walk->direction = direction;
    walk->done = std::move(result);
    auto position = cursor_;
    if (!hasOriginalEvent_) {
        // The opened live message may not have a persisted event ID yet. Use
        // its timestamp as an exclusive boundary until a real cursor is found.
        if (!position.undated) {
            position.eventId = direction < 0 ? std::numeric_limits<qint64>::max() : qint64(1);
            position.ordinal = direction < 0 ? std::numeric_limits<int>::max() : -1;
        }
    }
    // The presentation may reorder references by their body offsets, while
    // the archive's attachment ordinals retain protocol order. Resolve the
    // clicked share identity against this one message before traversing.
    if (!anchorResolved_ && hasOriginalEvent_ && !sourceId_.isEmpty()) {
        anchorResolved_ = true;
        auto before = cursor_;
        before.ordinal = -1;
        const QPointer<ChatMediaImageSource> guard(this);
        browser_->requestPage(before, false, 200, [guard, walk, position](ChatMedia::Page page, bool ok) {
            if (!guard)
                return;
            if (ok) {
                ChatMedia::Cursor matched;
                int distance = std::numeric_limits<int>::max();
                for (const auto &entry : page.entries) {
                    if (entry.cursor.eventId != guard->cursor_.eventId)
                        break;
                    const auto &file = entry.announcement.file;
                    bool same = false;
                    for (const auto &hash : file.reference.mediaSharing().file.computedHashes())
                        same |= hash.toString() == guard->sourceId_;
                    if (!same && !file.hasContentHashes() && file.url().isValid())
                        same = MessageFileShares::urlIdentity(file.url()).toString() == guard->sourceId_;
                    const int delta = std::abs(entry.cursor.ordinal - guard->cursor_.ordinal);
                    if (same && delta < distance) {
                        matched = entry.cursor;
                        distance = delta;
                    }
                }
                if (matched.valid())
                    guard->cursor_ = matched;
            }
            guard->requestPage(walk, guard->cursor_.valid() ? guard->cursor_ : position);
        });
        return;
    }
    requestPage(walk, position);
}

void ChatMediaImageSource::requestPage(const std::shared_ptr<Walk> &walk, ChatMedia::Cursor cursor)
{
    if (!browser_) {
        walk->done({}, {}, false);
        return;
    }
    const QPointer<ChatMediaImageSource> guard(this);
    browser_->requestPage(cursor, walk->direction < 0, PageSize,
                          [guard, walk](ChatMedia::Page page, bool ok) {
        if (!guard)
            return;
        if (!ok || page.entries.isEmpty()) {
            walk->done({}, {}, false);
            return;
        }
        walk->entries = std::move(page.entries);
        walk->index = 0;
        walk->hasMore = page.hasMore;
        walk->next = page.next;
        guard->consume(walk);
    });
}

void ChatMediaImageSource::consume(const std::shared_ptr<Walk> &walk)
{
    if (!manager_ || !account_) {
        walk->done({}, {}, false);
        return;
    }
    while (walk->index < walk->entries.size()) {
        const ChatMedia::Entry entry = walk->entries.at(walk->index++);
        if (!isImage(entry))
            continue;
        auto *item = manager_->chatMediaItem(entry, account_);
        if (!item)
            continue;
        const QString path = item->localFilePath();
        if (!path.isEmpty() && QFileInfo(path).isFile()) {
            QImageReader reader(path);
            reader.setAutoTransform(true);
            const QImage image = reader.read();
            if (!image.isNull()) {
                cursor_ = entry.cursor;
                hasOriginalEvent_ = true;
                walk->done(image, path, true);
                return;
            }
        }
        load(walk, entry, item);
        return;
    }
    if (walk->hasMore && walk->next.valid())
        requestPage(walk, walk->next);
    else
        walk->done({}, {}, false);
}

void ChatMediaImageSource::load(const std::shared_ptr<Walk> &walk, const ChatMedia::Entry &entry,
                               FileSharingItem *item)
{
    if (item->fileSize() && *item->fileSize() > quint64(MaxImageBytes)) {
        consume(walk);
        return;
    }
    QIODevice *device = manager_->reader(item, this);
    FileReader *reader = qobject_cast<FileReader *>(device);
    FileShareDownloader *fallback = nullptr;
    if (!device && item->isUrlOnly()) {
        fallback = item->download();
        if (fallback) {
            fallback->setParent(this);
            fallback->setMaximumSize(quint64(MaxImageBytes));
            device = fallback;
        }
    }
    if (!device || !device->open(QIODevice::ReadOnly)) {
        if (device)
            device->deleteLater();
        consume(walk);
        return;
    }

    struct Transfer {
        QByteArray bytes;
        QPointer<QIODevice> device;
        bool finished = false;
    };
    auto transfer = std::make_shared<Transfer>();
    transfer->device = device;
    const QPointer<ChatMediaImageSource> guard(this);
    const auto finish = [guard, walk, entry, transfer](bool ok) {
        if (transfer->finished)
            return;
        transfer->finished = true;
        if (transfer->device) {
            if (auto *reader = qobject_cast<FileReader *>(transfer->device.data()))
                reader->abort();
            else if (auto *download = qobject_cast<FileShareDownloader *>(transfer->device.data()))
                download->abort();
            transfer->device->deleteLater();
        }
        if (!guard)
            return;
        QImage image;
        if (ok)
            image.loadFromData(transfer->bytes);
        if (!image.isNull()) {
            guard->cursor_ = entry.cursor;
            guard->hasOriginalEvent_ = true;
            walk->done(image, imageName(entry), true);
        } else {
            guard->consume(walk);
        }
    };
    const auto pump = [transfer, finish] {
        if (!transfer->device || transfer->finished)
            return;
        char data[64 * 1024];
        while (true) {
            const qint64 n = transfer->device->read(data, sizeof(data));
            if (n < 0) {
                finish(false);
                return;
            }
            if (n == 0)
                return;
            if (transfer->bytes.size() > MaxImageBytes - n) {
                finish(false);
                return;
            }
            transfer->bytes.append(data, int(n));
        }
    };
    connect(device, &QIODevice::readyRead, this, pump);
    if (reader) {
        connect(reader, &QIODevice::readChannelFinished, this, [pump, finish] {
            pump();
            finish(true);
        });
        connect(reader, &FileReader::failed, this, [finish] { finish(false); });
    } else if (fallback) {
        connect(fallback, &FileShareDownloader::transferCompleted, this, [pump, finish] {
            pump();
            finish(true);
        });
        connect(fallback, &FileShareDownloader::failed, this, [finish] { finish(false); });
    }
    pump();
}
