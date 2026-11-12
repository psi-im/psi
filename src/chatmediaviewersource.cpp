/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "chatmediaviewersource.h"
#include "filesharingchunksource.h"
#include "filedownload.h"
#include "filesourceselector.h"
#include "aesgcmurl.h"
#include "filesharingitem.h"
#include "filesharingmanager.h"
#include "psiaccount.h"
#include "psicon.h"
#include "qitevideo.h"
#include "widgets/MamChatWidget.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QTimer>
#include <QThreadPool>
#include <QUrl>
#include <QtConcurrentRun>
#include <atomic>
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <utility>

namespace {
constexpr quint64 MaxImageBytes  = 128 * 1024 * 1024;
constexpr quint64 MaxImagePixels = 32 * 1024 * 1024;
using ImageDone                  = std::function<void(QImage, MediaChunkError)>;

// A rapid gallery scroll can cancel several image decodes in succession.
// Keeping a small, dedicated pool prevents large JPEG/PNG decoders from
// exhausting the global worker pool or allocating many huge images at once.
QThreadPool &imageDecodePool()
{
    static QThreadPool pool;
    static const bool configured = [] {
        pool.setMaxThreadCount(2);
        return true;
    }();
    Q_UNUSED(configured);
    return pool;
}

MediaChunkAbort decodeFile(QString path, QObject *receiver, ImageDone done)
{
    // Only complete local files reach the codec. A blocking QIODevice that
    // waits for GUI-driven network chunks can deadlock Qt's image plugin mutex
    // against icon rendering on the GUI thread.
    struct Result {
        QImage image;
        MediaChunkError error;
    };
    auto *watcher = new QFutureWatcher<Result>(QCoreApplication::instance());
    const QPointer<QObject> guard(receiver);
    const auto cancelled = std::make_shared<std::atomic<bool>>(false);
    auto cancel = [cancelled] { cancelled->store(true); };
    QObject::connect(receiver, &QObject::destroyed, watcher, cancel);
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, watcher, [watcher, cancel] {
        cancel();
        watcher->waitForFinished();
    });
    QObject::connect(watcher, &QFutureWatcher<Result>::finished, watcher,
                     [watcher, guard, cancelled, done = std::move(done)] {
        auto result = watcher->result();
        watcher->deleteLater();
        if (guard && !cancelled->load())
            done(std::move(result.image), std::move(result.error));
    });
    watcher->setFuture(QtConcurrent::run(&imageDecodePool(), [path = std::move(path), cancelled] {
        if (cancelled->load())
            return Result { {}, { MediaChunkError::Cancelled, QStringLiteral("Image load cancelled") } };
        const QFileInfo file(path);
        if (!file.isFile() || file.size() < 0 || quint64(file.size()) > MaxImageBytes)
            return Result { {}, { MediaChunkError::Unavailable, QObject::tr("Local image is missing or too large") } };
        QImage image;
        if (!image.load(path))
            return Result { {}, { MediaChunkError::Failed, QObject::tr("Cannot decode image") } };
        if (quint64(image.width()) * quint64(image.height()) > MaxImagePixels)
            return Result { {}, { MediaChunkError::Unavailable, QObject::tr("Unsupported image dimensions") } };
        return Result { std::move(image), {} };
    }));
    return cancel;
}
}

ChatMediaViewerSource::ChatMediaViewerSource(FileSharingManager *manager, PsiAccount *account,
                                             const HistoryArchive::Key &key, QString source, QImage preview,
                                             QString path, qint64 position) :
    manager_(manager), account_(account), source_(std::move(source)), initialPosition_(position)
{
    view_.key   = "initial";
    view_.image = std::move(preview);
    if (QFileInfo(path).isAbsolute())
        view_.path = path;
    view_.name     = QFileInfo(path).fileName();
    view_.revision = 1;
    if (manager && account)
        browser_ = manager->chatMedia(account, key, this);
    if (browser_)
        connect(browser_, &ChatMedia::changed, this, [this] {
            if (!started_ || refreshPending_)
                return;
            refreshPending_ = true;
            QTimer::singleShot(750, this, [this] {
                refreshPending_ = false;
                refresh();
            });
        });
    if (account)
        connect(account, &QObject::destroyed, this, [this] {
            cancelContent();
            view_.navigating = false;
            view_.state      = State::Failed;
            view_.error      = tr("Account closed");
            publish();
        });
    if (browser_)
        connect(browser_, &QObject::destroyed, this, [this] {
            view_.navigating = false;
            view_.error      = tr("Chat media history unavailable");
            publish();
        });
}
ChatMediaViewerSource::~ChatMediaViewerSource() { cancelContent(); }
void ChatMediaViewerSource::cancelContent()
{
    ++contentEpoch_;
    disconnect(integrity_);
    auto opening = std::exchange(opening_, {}), decoding = std::exchange(decoding_, {});
    if (opening)
        opening();
    if (decoding)
        decoding();
}
void ChatMediaViewerSource::publish() { emit changed(view_); }
void ChatMediaViewerSource::start()
{
    started_ = true;
    publish();
    refresh();
}
void ChatMediaViewerSource::setFilter(MediaFilter filter)
{
    ++queryEpoch_; // Invalidate any query made with the previous filter.
    navigationActive_ = false;
    queuedSteps_ = 0;
    view_.navigating = false;
    if (browser_)
        browser_->setFilter(filter);
    if (started_)
        refresh();
}
void ChatMediaViewerSource::refresh()
{
    if (!browser_ || !account_)
        return;
    if (view_.navigating) {
        if (!refreshPending_) {
            refreshPending_ = true;
            QTimer::singleShot(200, this, [this] {
                refreshPending_ = false;
                refresh();
            });
        }
        return;
    }
    const auto                            epoch = ++queryEpoch_;
    const QPointer<ChatMediaViewerSource> guard(this);
    browser_->requestEntry(source_, [guard, epoch](ChatMedia::Page page, bool ok) {
        if (!guard || guard->queryEpoch_ != epoch)
            return;
        if (!ok) {
            guard->view_.navigating = false;
            guard->view_.error      = tr("Cannot read chat media");
            guard->publish();
            return;
        }
        if (!page.entries.isEmpty()) {
            if (!guard->entry_ || guard->entry_->type != page.entries.first().type)
                guard->select(page.entries.first());
            else {
                guard->entry_      = page.entries.first();
                guard->view_.index = guard->entry_->index;
                guard->view_.total = guard->entry_->total;
                guard->view_.path
                    = guard->entry_->storage.availability == ChatMedia::Availability::Complete && guard->item_
                    ? guard->item_->localFilePath()
                    : QString();
                guard->publish();
            }
        } else if (guard->entry_) {
            guard->browser_->requestAdjacent(*guard->entry_, false, [guard, epoch](ChatMedia::Page page, bool ok) {
                if (!guard || guard->queryEpoch_ != epoch)
                    return;
                if (!ok || !page.entries.isEmpty()) {
                    guard->choose(std::move(page), ok, 1, true);
                    return;
                }
                guard->browser_->requestAdjacent(*guard->entry_, true, [guard, epoch](ChatMedia::Page page, bool ok) {
                    if (guard && guard->queryEpoch_ == epoch)
                        guard->choose(std::move(page), ok, -1, true);
                });
            });
        } else {
            guard->browser_->requestStatistics([guard, epoch](ChatMedia::Statistics stats, bool ok) {
                if (!guard || guard->queryEpoch_ != epoch)
                    return;
                guard->view_.index = 0;
                guard->view_.total = ok ? stats.total() : 0;
                guard->publish();
            });
        }
    });
}
bool ChatMediaViewerSource::enqueueAdjacent(int direction)
{
    if (!navigationActive_)
        return false;
    // Multiple wheel events are one logical navigation operation: never open
    // or decode images that the user merely scrolls past.
    queuedSteps_ = qBound(-200, queuedSteps_ + direction, 200);
    return true;
}

void ChatMediaViewerSource::adjacent(int direction)
{
    if (!browser_ || !account_ || !direction) {
        view_.navigating = false;
        publish();
        return;
    }
    if (navigationActive_) {
        enqueueAdjacent(direction);
        return;
    }
    direction_        = direction < 0 ? -1 : 1;
    skips_            = 0;
    navigationActive_ = true;
    queuedSteps_      = 0;
    const auto                            epoch = ++queryEpoch_;
    const QPointer<ChatMediaViewerSource> guard(this);
    view_.navigating = true;
    publish();

    // A detented wheel emits separate events even for one quick gesture.
    // Collect the first burst before submitting SQL, so rapid browsing does
    // not spawn many short-lived decoders and canceled network readers.
    QTimer::singleShot(40, this, [guard, epoch, direction] {
        if (!guard || guard->queryEpoch_ != epoch || !guard->navigationActive_)
            return;
        const int steps = qBound(-400, direction + std::exchange(guard->queuedSteps_, 0), 400);
        if (!steps) {
            guard->completeNavigation(guard->entry_, epoch, true);
            return;
        }
        if (guard->entry_) {
            guard->requestNavigationPage(steps, *guard->entry_, epoch);
            return;
        }
        // On first open, resolve the clicked resource even if it is excluded
        // by the filter, instead of starting at an unrelated end of the chat.
        guard->browser_->requestAdjacent(guard->source_, steps < 0,
            [guard, epoch, steps](ChatMedia::Page page, bool ok) {
                if (!guard || guard->queryEpoch_ != epoch || !guard->navigationActive_)
                    return;
                if (!ok || page.entries.isEmpty()) {
                    guard->completeNavigation({}, epoch, ok);
                    return;
                }
                const ChatMedia::Entry anchor = page.entries.first();
                const int remaining = steps - (steps < 0 ? -1 : 1);
                if (remaining)
                    guard->requestNavigationPage(remaining, anchor, epoch);
                else
                    guard->completeNavigation(anchor, epoch, true);
            });
    });
}

void ChatMediaViewerSource::requestNavigationPage(int steps, const ChatMedia::Entry &anchor, quint64 epoch)
{
    if (!browser_ || queryEpoch_ != epoch || !navigationActive_)
        return;
    const int count = qMin(std::abs(steps), 200);
    if (!count) {
        completeNavigation(anchor, epoch, true);
        return;
    }
    const QPointer<ChatMediaViewerSource> guard(this);
    browser_->requestPage(anchor.cursor, steps < 0, count,
                          [guard, anchor, epoch, steps, count](ChatMedia::Page page, bool ok) {
        if (!guard || guard->queryEpoch_ != epoch || !guard->navigationActive_)
            return;
        if (!ok) {
            guard->completeNavigation({}, epoch, false);
            return;
        }
        auto candidate = page.entries.isEmpty() ? anchor : page.entries.last();
        if (std::abs(steps) > count && !page.entries.isEmpty()) {
            guard->requestNavigationPage(steps - (steps < 0 ? -count : count), candidate, epoch);
            return;
        }
        guard->completeNavigation(candidate, epoch, true);
    });
}

void ChatMediaViewerSource::completeNavigation(std::optional<ChatMedia::Entry> candidate, quint64 epoch, bool ok)
{
    if (queryEpoch_ != epoch || !navigationActive_)
        return;
    if (!ok) {
        navigationActive_ = false;
        queuedSteps_      = 0;
        view_.navigating  = false;
        view_.error       = tr("Cannot read chat media");
        publish();
        return;
    }
    const int further = std::exchange(queuedSteps_, 0);
    if (further && candidate) {
        requestNavigationPage(further, *candidate, epoch);
        return;
    }
    navigationActive_ = false;
    view_.navigating  = false;
    if (candidate) {
        if (entry_ && candidate->cursor.assetId != entry_->cursor.assetId)
            direction_ = candidate->cursor.assetId < entry_->cursor.assetId ? -1 : 1;
        if (!entry_ || entry_->cursor.assetId != candidate->cursor.assetId) {
            select(*candidate);
            return;
        }
    }
    publish();
}

void ChatMediaViewerSource::choose(ChatMedia::Page page, bool ok, int direction, bool replace)
{
    view_.navigating = false;
    if (!ok) {
        view_.state = State::Failed;
        view_.error = tr("Cannot read chat media");
        publish();
        return;
    }
    if (page.entries.isEmpty()) {
        if (replace) {
            cancelContent();
            entry_.reset();
            item_       = nullptr;
            view_       = {};
            view_.state = State::Empty;
            view_.key   = QString::number(contentEpoch_);
        }
        publish();
        return;
    }
    direction_ = direction;
    select(page.entries.first());
}
void ChatMediaViewerSource::select(const ChatMedia::Entry &entry)
{
    const bool initial = !entry_;
    auto       preview = initial ? view_.image : QImage();
    cancelContent();
    entry_         = entry;
    source_        = entry.sourceId;
    view_          = {};
    view_.key      = QString::number(contentEpoch_);
    view_.revision = 1;
    view_.name     = entry.name;
    view_.index    = entry.index;
    view_.total    = entry.total;
    view_.image    = preview;
    item_          = manager_ && account_ ? manager_->chatMediaItem(entry, account_) : nullptr;
    if (!item_) {
        fail({ MediaChunkError::Unavailable, tr("Media source unavailable") });
        return;
    }
    if (entry.storage.availability == ChatMedia::Availability::Complete)
        view_.path = item_->localFilePath();
    if (entry.type == HistoryMediaRefs::Video) {
        const QUrl                              url(QStringLiteral("share:%1").arg(item_->sums().first().toString()));
        const QPointer<FileSharingDeviceOpener> opener(account_->fileSharingDeviceOpener());
        const auto                              position = std::exchange(initialPosition_, 0);
        const bool                              audio = entry.mime.startsWith(QLatin1String("audio/"));
        view_.video = [url, opener, position, audio](QWidget *parent, std::function<void()> fullscreen) {
            return opener ? ITEVideoController::createPlayer(url, opener, parent, std::move(fullscreen), position, audio)
                          : nullptr;
        };
        view_.state = State::Ready;
        publish();
        return;
    }
    if (entry.type != HistoryMediaRefs::Image && preview.isNull()) {
        view_.state = State::Ready;
        publish();
        return;
    }
    view_.state = State::Loading;
    view_.progressPercent = -1;
    publish();
    const auto                            epoch = contentEpoch_;
    const QPointer<ChatMediaViewerSource> guard(this);
    const auto onReady = [guard, epoch](QString path, MediaChunkError error) {
        if (!guard || guard->contentEpoch_ != epoch)
            return;
        guard->opening_ = {};
        if (!error.isEmpty() || path.isEmpty()) {
            guard->fail(error.isEmpty()
                            ? MediaChunkError(MediaChunkError::Unavailable, QObject::tr("Image file unavailable"))
                            : std::move(error));
            return;
        }
        // The verified payload is now complete. Image decoding is local-only.
        guard->view_.progressPercent = -1;
        guard->publish();
        guard->decoding_ = decodeFile(path, guard, [guard, epoch, path](QImage image, MediaChunkError error) {
            if (!guard || guard->contentEpoch_ != epoch)
                return;
            guard->decoding_ = {};
            if (!error.isEmpty()) {
                guard->fail(error);
                return;
            }
            guard->view_.state = State::Ready;
            guard->view_.error.clear();
            guard->view_.image = std::move(image);
            ++guard->view_.revision;
            guard->view_.path = path;
            guard->publish();
        });
    };

    // Complete cached payloads can be opened directly. Known-length, ranged
    // sources reuse existing verified chunks, filling only missing ranges.
    // Encrypted whole-file and unknown-length sources retain the shared
    // whole-download fallback. No decoder consumes a live network device.
    const QString local = item_->localFilePath();
    if (!local.isEmpty()) {
        QTimer::singleShot(0, this, [onReady, local] { onReady(local, {}); });
        return;
    }
    if (item_->fileSize() && *item_->fileSize() > MaxImageBytes) {
        fail({ MediaChunkError::Unavailable, tr("Image exceeds the requested size limit") });
        return;
    }
    const auto sources = FileSourceSelector::prioritized(item_->uris());
    const bool authenticatedWhole = !sources.isEmpty()
        && std::all_of(sources.cbegin(), sources.cend(),
                       [](const QString &uri) { return bool(AesGcmUrl::parse(QUrl(uri))); });
    if (!item_->isUrlOnly() && item_->fileSize() && !authenticatedWhole) {
        if (auto *download = manager_->download(item_)) {
            const QPointer<FileDownload> controller(download);
            const quint64 goal = download->downloadAll(
                [guard, epoch, onReady](const QString &path, MediaChunkError error) {
                    if (guard && guard->contentEpoch_ == epoch)
                        onReady(path, std::move(error));
                },
                [guard, epoch](qreal fraction) {
                    if (!guard || guard->contentEpoch_ != epoch)
                        return;
                    const int percent = qBound(0, qRound(fraction * 100), 100);
                    if (guard->view_.progressPercent != percent) {
                        guard->view_.progressPercent = percent;
                        guard->publish();
                    }
                });
            if (goal) {
                opening_ = [controller, goal] {
                    if (controller)
                        controller->cancelGoal(goal);
                };
                return;
            }
        }
    }
    opening_ = FileSharingChunkSource::openWhole(item_, this, MaxImageBytes, onReady);
}
void ChatMediaViewerSource::fail(MediaChunkError error)
{
    view_.state = State::Failed;
    view_.error = error.message();
    publish();
    if (error.code() != MediaChunkError::Unavailable || ++skips_ >= 8 || !browser_)
        return;
    const QPointer<ChatMediaViewerSource> guard(this);
    const auto                            query = ++queryEpoch_;
    browser_->requestAdjacent(*entry_, direction_ < 0, [guard, query](ChatMedia::Page page, bool ok) {
        if (guard && guard->queryEpoch_ == query)
            guard->choose(std::move(page), ok, guard->direction_, false);
    });
}
void ChatMediaViewerSource::retry()
{
    if (entry_) {
        if (account_ && item_)
            account_->fileSharingDeviceOpener()->retry(
                QUrl(QStringLiteral("share:%1").arg(item_->sums().first().toString())));
        skips_ = 0;
        select(*entry_);
    } else
        refresh();
}
bool ChatMediaViewerSource::saveAs(QWidget *parent)
{
    if (!account_ || !item_)
        return false;
    const QUrl url(QStringLiteral("share:%1").arg(item_->sums().first().toString()));
    account_->fileSharingDeviceOpener()->saveAs(url, parent);
    return true;
}
void ChatMediaViewerSource::install(MamChatWidget *chat, PsiAccount *account, const HistoryArchive::Key &key)
{
    if (!chat || !account || !account->psi())
        return;
    const QPointer<PsiAccount>         acc(account);
    const QPointer<FileSharingManager> manager(account->psi()->fileSharingManager());
    chat->setMediaOpenHandler([acc, manager, key](const QImage &image, const QString &path, const QString &source,
                                                  qint64 position, bool fullscreen) {
        auto viewer = acc && manager && !source.isEmpty()
            ? new ChatMediaViewerSource(manager, acc, key, source, image, path, position)
            : nullptr;
        MediaViewer::showImage(image, path, viewer, fullscreen);
    });
}
