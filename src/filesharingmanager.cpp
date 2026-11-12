/*
 * filesharingmanager.cpp - file sharing manager
 * Copyright (C) 2019  Sergey Ilinykh
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "filesharingmanager.h"
#include "filesourceselector.h"

#include "applicationinfo.h"
#include "edbsqlite.h"
#include "filecache.h"
#include "filedatabase.h"
#include "filedownload.h"
#include "fileidentity.h"
#include "filereader.h"
#include "filesharingcatalog.h"
#include "filesharingchunksource.h"
#include "filesharingdownloader.h"
#include "filesharingjinglesession.h"
#include "filesharingmediasave.h"
#include "filesharingpublicationsync.h"
#include "filesharingresponder.h"
#include "filesharingurl.h"
#include "fileutil.h"
#include "httputil.h"
#include "mediachunkcache.h"
#include "mediadownloadpolicy.h"
#include "mediaplaybackpolicy.h"
#include "messagefileshares.h"
#include "psiaccount.h"
#include "psicon.h"
#include "psicontact.h"
#include "psicontactlist.h"
#include "psioptions.h"
#include "qitemedia.h"
#include <QFile>

#include "iris/jingle-session.h"
#include "iris/xmpp_client.h"
#include "iris/xmpp_hash.h"
#include "iris/xmpp_jid.h"
#include "iris/xmpp_message.h"
#include "iris/xmpp_reference.h"
#include "iris/xmpp_vcard.h"
#ifdef HAVE_WEBSERVER
#include "webserver.h"
#endif
#include "messageview.h"
#include "textutil.h"

#include <QDateTime>
#include <QDir>
#include <QMimeData>
#include <QTimer>
#include <QUrl>
#include <algorithm>
#include <utility>

static QString legacyDir()
{
    return ApplicationInfo::homeDir(ApplicationInfo::DataLocation) + QStringLiteral("/shares");
}

static bool obsoleteRangePayload(const QString &path)
{
    const auto suffix = QStringLiteral("/media-files-v2/");
    return path.startsWith(legacyDir() + suffix) || path.startsWith(FileSharingManager::cacheDir() + suffix);
}

// ======================================================================
// FileSharingManager
// ======================================================================
class FileSharingManager::Private {
public:
    QPointer<EDBSqLite>                                  history;
    FileCache                                           *cache;
    MediaChunkCache                                     *chunkCache = nullptr;
    QHash<FileSharingItem *, FileDownload *>             downloads;
    QHash<QString, FileSharingItem *>                    rangeItems;
    QHash<PsiAccount *, FileSharingJingleSession *>      mediaSessions;
    QHash<XMPP::Hash, FileSharingItem *>                 items;
    QHash<QString, QHash<XMPP::Hash, FileSharingItem *>> accountItems;
    std::unique_ptr<FileSharingCatalog>                  catalog;

    void rememberItem(FileSharingItem *item)
    {
        Q_ASSERT(item->sums().size());
        if (!item->isUrlOnly() && item->account() && item->fileSize())
            rangeItems.insert(FileIdentity::rangeKey(FileSharingChunkSource::identity(item), *item->fileSize()), item);
        for (auto const &v : item->sums()) {
            items.insert(v, item); // TODO ensure we don't overwrite
            if (item->account())
                accountItems[item->account()->id()].insert(v, item);
        }
    }
};

FileSharingManager::FileSharingManager(QObject *parent) : QObject(parent), d(new Private)
{
    // Complete legacy payloads are imported per profile. Unreleased JSON range
    // sidecars and media-files-v2 payloads are deliberately ignored.
    const auto legacy = legacyDir();

    d->cache = new FileCache(QStringLiteral("shares"), cacheDir(), this);
    d->cache->setFileCacheSize(256 * 1024 * 1024);
    d->cache->setMemoryCacheSize(16 * 1024 * 1024);
    d->cache->importLegacy(legacy);
    d->catalog.reset(new FileSharingCatalog);
    connect(d->cache, &FileCache::contentsChanged, this, &FileSharingManager::chatMediaChanged, Qt::QueuedConnection);
    connect(d->cache->rangeStore(), &MediaChunkStore::fileChanged, this, &FileSharingManager::chatMediaChanged);
    const auto expire = [this] {
        const auto days = std::clamp(
            PsiOptions::instance()->getOption(QStringLiteral("options.media.incomplete-max-age-days")).toInt(), 1,
            3650);
        d->cache->rangeStore()->removeIncomplete(QDateTime::currentSecsSinceEpoch() - qint64(days) * 86400);
    };
    auto timer = new QTimer(this);
    timer->setInterval(60 * 60 * 1000);
    connect(timer, &QTimer::timeout, this, expire);
    timer->start();
    QTimer::singleShot(0, this, expire);
    connect(PsiOptions::instance(), &PsiOptions::optionChanged, this, [expire](const QString &option) {
        if (option == QLatin1String("options.media.incomplete-max-age-days"))
            expire();
    });
}

FileSharingManager::~FileSharingManager() { delete d->chunkCache; }

FileSharingJingleSession *FileSharingManager::jingleMediaSessions(PsiAccount *account)
{
    auto &sessions = d->mediaSessions[account];
    if (!sessions) {
        sessions = new FileSharingJingleSession(account->client()->jingleManager(), account);
        connect(account, &QObject::destroyed, this, [this, account] { d->mediaSessions.remove(account); });
    }
    return sessions;
}

MediaChunkCache *FileSharingManager::mediaChunkCache()
{
    if (!d->chunkCache) {
        d->chunkCache = new MediaChunkCache(d->cache->rangeStore(), this);
        connect(d->chunkCache, &MediaChunkCache::integrityError, this,
                [this](const QString &source, quint64 size, const QString &) {
                    const auto path = d->chunkCache->fileState(source, size).path;
                    for (auto item : d->items) {
                        if (!item || FileSharingChunkSource::identity(item) != source)
                            continue;
                        for (const auto &hash : item->sums()) {
                            auto cached = d->cache->get(hash);
                            if (cached && cached->isExternal() && cached->filePath() == path)
                                d->cache->remove(hash);
                        }
                    }
                });
    }
    return d->chunkCache;
}

void FileSharingManager::removeIncomplete(const std::function<void(quint64, const QString &)> &completed)
{
    d->cache->rangeStore()->removeIncomplete(0, completed);
}

FileDownload *FileSharingManager::download(FileSharingItem *item)
{
    if (!item || !item->account() || item->isUrlOnly() || !item->fileSize() || !*item->fileSize())
        return nullptr;
    if (auto existing = d->downloads.value(item))
        return existing;
    auto                            cache = FileSharingChunkSource::configure(item);
    d->rememberItem(item);
    const QPointer<FileSharingItem> resource(item);
    const QPointer<MediaChunkCache> storage(cache);
    auto                            download = new FileDownload(
        cache, FileSharingChunkSource::identity(item), *item->fileSize(), item->account()->id(),
        [resource, storage](quint64 offset, quint64 length, FileDownload::FullDownloadAllowed allowWhole,
                            MediaChunkCompletion done) {
            return FileSharingChunkSource::fetch(resource, offset, length, std::move(done), storage, allowWhole);
        },
        item);
    d->downloads.insert(item, download);
    download->setLocalFileResolver([resource] { return resource ? resource->localFilePath() : QString(); });
    connect(download, &FileDownload::fullDownloadChanged, this, [resource](bool active) {
        if (!active)
            FileSharingChunkSource::cancelFullDownloadFallback(resource);
    });
    connect(download, &QObject::destroyed, this, [this, item] { d->downloads.remove(item); });
    connect(item->account(), &QObject::destroyed, download, &FileDownload::shutdown);
    return download;
}

FileReader *FileSharingManager::reader(FileSharingItem *item, QObject *parent, quint64 offset,
                                       std::optional<quint64> length)
{
    if (!item)
        return nullptr;
    if (item->isCached()) {
        const auto path = item->localFilePath();
        return path.isEmpty() ? nullptr : new FileReader(path, offset, length, parent);
    }
    FileSharingChunkSource::configure(item);
    const auto local = FileSharingChunkSource::localFile(item);
    if (!local.isEmpty()) {
        auto reader = new FileReader(local, offset, length, parent);
        if (auto source = download(item)) {
            source->pin(true);
            connect(reader, &QObject::destroyed, source, [source] { source->pin(false); });
        }
        return reader;
    }
    auto source = download(item);
    return source ? new FileReader(source, offset, length, parent) : nullptr;
}

MediaChunkAbort FileSharingManager::open(FileSharingItem *item, QObject *owner, OpenDone done, quint64 maximumBytes)
{
    if (!owner || !done)
        return {};
    struct Pending {
        bool            finished = false;
        MediaChunkAbort abort;
        OpenDone        done;
    };
    auto pending                               = std::make_shared<Pending>();
    pending->done                              = std::move(done);
    auto                               request = new QObject(owner);
    const QPointer<QObject>            guard(request);
    const QPointer<FileSharingItem>    resource(item);
    const QPointer<FileSharingManager> manager(this);
    auto                               finish = [pending, guard](FileReader *reader, MediaChunkError error) {
        if (!guard || pending->finished) {
            if (reader)
                reader->deleteLater();
            return;
        }
        pending->finished    = true;
        const auto completed = std::exchange(pending->done, {});
        guard->deleteLater();
        completed(reader, std::move(error));
    };
    connect(request, &QObject::destroyed, this, [pending] {
        pending->finished = true;
        if (pending->abort)
            pending->abort();
    });
    connect(this, &QObject::destroyed, request,
            [finish] { finish(nullptr, { MediaChunkError::Unavailable, QStringLiteral("File manager closed") }); });
    if (item)
        connect(item, &QObject::destroyed, request, [finish] {
            finish(nullptr, { MediaChunkError::Unavailable, QStringLiteral("Media source disappeared") });
        });
    QTimer::singleShot(0, request, [manager, resource, owner, guard, pending, finish, maximumBytes] {
        if (pending->finished)
            return;
        if (!manager || !resource || !resource->account()) {
            finish(nullptr, { MediaChunkError::Unavailable, QStringLiteral("Media source unavailable") });
            return;
        }
        if (maximumBytes && resource->fileSize() && *resource->fileSize() > maximumBytes) {
            finish(nullptr, { MediaChunkError::Unavailable, QStringLiteral("Media exceeds the requested size limit") });
            return;
        }
        if (!resource->isUrlOnly()) {
            resource->_uris += manager->knownSources(resource->account(), resource->sums());
            resource->_uris.removeDuplicates();
        }
        const auto uris               = FileSourceSelector::prioritized(resource->uris());
        const auto local              = resource->localFilePath();
        const bool authenticatedWhole = !uris.isEmpty()
            && std::all_of(uris.cbegin(), uris.cend(),
                           [](const QString &uri) { return bool(AesGcmUrl::parse(QUrl(uri))); });
        if (!local.isEmpty() || (!resource->isUrlOnly() && resource->fileSize() && !authenticatedWhole)) {
            auto reader = manager->reader(resource, owner);
            if (reader && reader->open()) {
                finish(reader, {});
                return;
            }
            if (reader)
                reader->deleteLater();
        }
        if (uris.isEmpty()) {
            finish(nullptr, { MediaChunkError::Unavailable, QStringLiteral("No usable download source") });
            return;
        }
        pending->abort = FileSharingChunkSource::openWhole(
            resource, guard, maximumBytes,
            [manager, resource, owner, finish, maximumBytes](QString path, MediaChunkError error) {
                if (!error.isEmpty()) {
                    finish(nullptr, std::move(error));
                    return;
                }
                if (!manager || !resource || path.isEmpty()) {
                    finish(nullptr, { MediaChunkError::Unavailable, QStringLiteral("Media source unavailable") });
                    return;
                }
                auto reader = manager->reader(resource, owner);
                if (!reader || !reader->open()) {
                    const auto error = reader ? reader->errorString() : QStringLiteral("Cannot open downloaded media");
                    if (reader)
                        reader->deleteLater();
                    finish(nullptr, error);
                    return;
                }
                if (maximumBytes && quint64(reader->size()) > maximumBytes) {
                    reader->deleteLater();
                    finish(nullptr,
                           { MediaChunkError::Unavailable, QStringLiteral("Media exceeds the requested size limit") });
                    return;
                }
                finish(reader, {});
            });
    });
    return [pending, finish] {
        if (pending->finished)
            return;
        finish(nullptr, { MediaChunkError::Cancelled, QStringLiteral("Content open cancelled") });
        if (pending->abort)
            pending->abort();
    };
}

QString FileSharingManager::cacheDir()
{
    return ApplicationInfo::makeSubprofilePath(QStringLiteral("shares"), ApplicationInfo::DataLocation);
}

FileCacheItem *FileSharingManager::cacheItem(const QList<Hash> &hashes, bool reborn, QString *fileName)
{
    for (auto const &h : hashes) {
        auto c = cacheItem(h, reborn, fileName);
        if (c)
            return c;
    }
    return nullptr;
}

FileCacheItem *FileSharingManager::cacheItem(const XMPP::Hash &id, bool reborn, QString *fileName_out)
{
    auto item = d->cache->get(id, reborn);
    if (!item)
        return nullptr;

    const auto fileName = item->filePath();
    if (obsoleteRangePayload(fileName)
        || (fileName.startsWith(cacheDir() + QStringLiteral("/ranges/"))
            && !mediaChunkCache()->isCompleteFile(fileName)))
        return nullptr; // A restored payload is available only after worker verification.
    if (fileName.isEmpty() || !QFileInfo(fileName).isReadable()) {
        d->cache->remove(id);
        return nullptr;
    }
    if (fileName_out)
        *fileName_out = fileName;
    return item;
}

FileCacheItem *FileSharingManager::moveToCache(const QList<XMPP::Hash> &sums, const QFileInfo &file,
                                               const QVariantMap &metadata, unsigned int maxAge)
{
    return d->cache->moveToCache(sums, file, metadata, maxAge);
}

FileCacheItem *FileSharingManager::trackExternal(const QList<XMPP::Hash> &sums, const QFileInfo &file,
                                                 const QVariantMap &metadata, unsigned int maxAge)
{
    if (obsoleteRangePayload(file.absoluteFilePath()))
        return nullptr;
    return d->cache->trackExternal(sums, file, metadata, maxAge);
}

ITEMediaOpener::CancelOpen FileSharingDeviceOpener::openAsync(QUrl url, QObject *receiver, OpenDone done)
{
    const QPointer<FileSharingDeviceOpener> guard(this);
    const QPointer<FileSharingItem>         resource(acc->psi()->fileSharingManager()->item(urlToSourceId(url), acc));
    if (resource && resource->isUrlOnly() && !resource->isCached() && !AesGcmUrl::parse(resource->simpleSource())) {
        // Keep streaming address-only HTTP through the existing backend URL
        // adapter. Bounded image reads use open(); AES must authenticate first.
        return ITEMediaOpener::openAsync(url, receiver, std::move(done));
    }
    return acc->psi()->fileSharingManager()->open(
        resource, receiver,
        [guard, resource, url, done = std::move(done)](FileReader *reader, MediaChunkError error) mutable {
            if (!guard || !resource) {
                if (reader)
                    reader->deleteLater();
                done(nullptr, url, QStringLiteral("Media source unavailable"));
                return;
            }
            if (!error.isEmpty()) {
                done(nullptr, url, error.message());
                return;
            }
            if (!MediaPlaybackPolicy::deviceStreaming() && !resource->isCached()) {
                // Keep the existing GStreamer compatibility adapter over the
                // same scheduler; authenticated URL sources are already local.
                reader->abort();
                reader->deleteLater();
                auto source = url;
                auto input  = guard->open(source);
                done(input, source, {});
            } else
                done(reader, url, {});
        });
}

std::optional<Hash> FileSharingManager::contentForUrl(const QString &account, const Hash &urlIdentity) const
{
    return d->catalog->contentForUrl(account, urlIdentity);
}

bool FileSharingManager::rememberUrlContent(const QString &account, const Hash &urlIdentity, const Hash &contentHash)
{
    const bool ok = d->catalog->rememberUrlContent(account, urlIdentity, contentHash);
    if (ok)
        emit chatMediaChanged();
    return ok;
}

void FileSharingManager::forgetUrlContent(const QString &account, const Hash &urlIdentity)
{
    d->catalog->forgetUrlContent(account, urlIdentity);
    emit chatMediaChanged();
}

FileSharingItem *FileSharingManager::item(const Hash &id, PsiAccount *account)
{
    if (!account)
        return d->items.value(id);
    if (auto existing = d->accountItems.value(account->id()).value(id))
        if (existing->account() == account)
            return existing;
    const auto stored = d->catalog->find(account->id(), id);
    if (!stored)
        return nullptr;
    const auto media = stored->media;
    if (stored->urlOnly) {
        auto restored = new FileSharingItem(QUrl::fromEncoded(media.sources.value(0).toUtf8()), id, account, this,
                                            media.file.description());
        d->rememberItem(restored);
        return restored;
    }
    if (auto existing = rangeItem(media, account)) {
        if (stored->peers.isEmpty())
            existing->mergeSources(media, Jid());
        for (const auto &peer : stored->peers)
            existing->mergeSources(media, Jid(peer));
        d->rememberItem(existing);
        return existing;
    }
    const auto path = stored->readableLocalPath();
    if (!path.isEmpty() && !cacheItem(media.file.computedHashes())) {
        QVariantMap metadata { { "type", media.file.mediaType() },
                               { "name", media.file.name() },
                               { "amplitudes", media.file.amplitudes() } };
        trackExternal(media.file.computedHashes(), QFileInfo(path), metadata, FileCache::Forever);
    }
    auto restored = new FileSharingItem(media, Jid(), account, this);
    for (const auto &peer : stored->peers)
        restored->mergeSources(media, Jid(peer));
    d->rememberItem(restored);
    return restored;
}

QList<FileSharingItem *> FileSharingManager::createFromMimeData(const QMimeData *data, PsiAccount *acc)
{
    QStringList files;

    QString voiceMsgMime;
    QString voiceAmplitudesMime(QLatin1String("application/x-psi-amplitudes"));

    qDebug() << "FSM items list from clipboard:" << data->formats() << " FILES:" << data->urls();

    QImage img(qvariant_cast<QImage>(data->imageData()));
    bool   hasVoice = data->hasFormat(voiceAmplitudesMime);
    if (hasVoice) {
        const auto &formats = data->formats();
        for (auto const &f : formats) {
            if (f.startsWith("audio/") || f.startsWith("video/")) { // video container may contain just audio
                voiceMsgMime = f;
                break;
            }
        }
        hasVoice = !voiceMsgMime.isEmpty();
    }
    if (data->hasUrls()) {
        const auto &urls = data->urls();
        for (auto const &url : urls) {
            if (!url.isLocalFile()) {
                continue;
            }
            QFileInfo fi(url.toLocalFile());
            if (fi.isFile() && fi.isReadable()) {
                files.append(fi.filePath());
            }
        }
    }
    if (files.isEmpty() && img.isNull() && !hasVoice) {
        return {};
    }

    QList<FileSharingItem *> ret;
    if (files.isEmpty()) { // so we have an image
        FileSharingItem *item = nullptr;
        if (hasVoice) {
            QByteArray  ba         = data->data(voiceMsgMime);
            QByteArray  amplitudes = data->data(voiceAmplitudesMime);
            QVariantMap vm;
            vm.insert("amplitudes", amplitudes);
            item = new FileSharingItem(voiceMsgMime.replace("video/", "audio/"), ba, vm, acc, this);
        } else if (!img.isNull()) {
            qDebug() << img;
            item = new FileSharingItem(img, acc, this);
        }
        if (item) {
            d->rememberItem(item);
            ret.append(item);
        }
    } else {
        for (auto const &f : files) {
            auto item = new FileSharingItem(f, acc, this);
            if (!item->sums().count())
                continue; // failed to calculate checksum. permissions problem?
            d->rememberItem(item);
            ret.append(item);
        }
    }

    return ret;
}

QList<FileSharingItem *> FileSharingManager::fromFilesList(const QStringList &fileList, PsiAccount *acc)
{
    QList<FileSharingItem *> ret;
    if (fileList.isEmpty())
        return ret;
    for (const QString &file : fileList) {
        QFileInfo fi(file);
        if (fi.isFile() && fi.isReadable()) {
            auto item = new FileSharingItem(file, acc, this);
            d->rememberItem(item);
            ret << item;
        }
    }
    return ret;
}

FileSharingItem *FileSharingManager::rangeItem(const MediaSharing &media, PsiAccount *account)
{
    if (!account || !media.file.size() || media.file.computedHashes().isEmpty())
        return nullptr;
    const auto source   = FileSharingChunkSource::sourceFor(account, media.file);
    const auto resource = d->rangeItems.value(FileIdentity::rangeKey(source, *media.file.size()));
    return resource && resource->account() == account && resource->fileSize() == media.file.size()
            && FileSharingChunkSource::identity(resource) == source
        ? resource
        : nullptr;
}

FileSharingItem *FileSharingManager::fromShare(const MessageFileShares::FileShare &file, const Jid &from,
                                               PsiAccount *acc)
{
    if (!acc)
        return nullptr;
    const auto       ms       = file.reference.mediaSharing();
    FileSharingItem *resource = nullptr;
    if (file.hasContentHashes()) {
        resource = rangeItem(ms, acc);
        if (!resource)
            resource = item(ms.file.computedHashes().first(), acc);
        if (!resource)
            resource = new FileSharingItem(ms, from, acc, this);
        else
            resource->mergeSources(ms, from);
    } else if (const auto url = file.url(); url.isValid()) {
        const auto identity = MessageFileShares::urlIdentity(url);
        resource            = d->accountItems.value(acc->id()).value(identity);
        if (!resource || resource->account() != acc || !resource->isUrlOnly())
            resource = new FileSharingItem(url, identity, acc, this, file.description);
    }
    if (resource)
        d->rememberItem(resource);
    return resource;
}

ChatMedia *FileSharingManager::chatMedia(PsiAccount *account, const HistoryArchive::Key &key, QObject *parent)
{
    if (!account || !parent || key.account != account->id() || key.peer.isEmpty() || !d->history)
        return nullptr;
    const QPointer<EDBSqLite> history(d->history);
    const auto filesPath = FileDatabase::path();
    auto                      browser   = new ChatMedia(
        [history, key, filesPath](ChatMedia::Query query, QObject *receiver, ChatMedia::LinksDone done) {
            if (history)
                history->requestChatMediaLinks(key, query, receiver, std::move(done), filesPath);
            else
                QTimer::singleShot(0, receiver, [done] { done({}, false); });
        },
        [history, key, filesPath](ChatMedia::Filter filter, QObject *receiver, ChatMedia::StatisticsDone done) {
            if (history)
                history->requestChatMediaStatistics(key, filesPath, receiver, std::move(done), filter);
            else
                QTimer::singleShot(0, receiver, [done] { done({}, false); });
        },
        parent);
    connect(this, &FileSharingManager::chatMediaChanged, browser, &ChatMedia::invalidate);
    const auto close = [browser] { delete browser; };
    connect(account, &QObject::destroyed, browser, close);
    connect(this, &QObject::destroyed, browser, close);
    connect(d->history, &QObject::destroyed, browser, close);
    return browser;
}

FileSharingItem *FileSharingManager::chatMediaItem(const ChatMedia::Entry &entry, PsiAccount *account)
{
    if (!account || entry.announcement.account != account->id())
        return nullptr;
    return fromShare(entry.announcement.file, entry.announcement.announcer, account);
}

void FileSharingManager::fillMessageView(MessageView &mv, const Message &m, PsiAccount *acc)
{
    if (!acc)
        return;
    // Render the presentation projection; the normalized announcements used
    // by history do not depend on these body replacements.
    const auto collected = MessageFileShares::forDisplay(m);
    auto       files     = collected.files;
    if (files.isEmpty() && collected.links.isEmpty())
        return;
    // Views may download files by themselves only up to this size.
    const quint64 autoLimit = autoDownloadLimit(m, acc);
    const QString autoAttr  = autoLimit ? QString::fromLatin1(" auto=\"%1\"").arg(autoLimit) : QString();

    QString tailReferences;
    QString desc = m.body();
    // qDebug() << "INIT TEXT:" << desc;
    QString htmlDesc;
    htmlDesc.reserve(desc.size() + files.count() * 64);

    std::sort(files.begin(), files.end(),
              [](const auto &a, const auto &b) { return a.reference.begin() < b.reference.begin(); });

    int lastEnd = 0;
    for (const auto &file : files) {
        const auto &r = file.reference;
        // qDebug() << "BEGIN:" << r.begin() << "END:" << r.end();
        // Every SIMS file needs a share object, not only inline media.
        // Non-media files can have an unknown size (streaming sources).
        auto item = fromShare(file, m.from(), acc);
        if (!item)
            continue;
        mv.addReference(item);
        const QString itemAuto = file.manualFetch ? QString() : autoAttr;

        if (r.begin() >= lastEnd && r.end() >= r.begin() && r.end() < desc.size()) {
            auto refText = desc.mid(r.begin(), r.end() - r.begin() + 1);

            QUrl url(refText.trimmed());
            if (url.isValid())
                refText = url.toString(QUrl::FullyEncoded);

            QString shareStr(
                QString::fromLatin1("<share id=\"%1\" account=\"%2\" text=\"%3\"%4/>")
                    .arg(item->sums()[0].toString(), TextUtil::escape(acc->id()), TextUtil::escape(refText), itemAuto));

            // add text before reference
            htmlDesc += TextUtil::linkify(TextUtil::plain2rich(desc.mid(lastEnd, r.begin() - lastEnd)));
            htmlDesc += shareStr; // something instead of link
            lastEnd = r.end() + 1;
        } else {
            tailReferences += QString::fromLatin1("<share id=\"%1\" account=\"%2\"%3/>")
                                  .arg(item->sums()[0].toString(), TextUtil::escape(acc->id()), itemAuto);
        }
    }
    if (lastEnd < desc.size()) {
        htmlDesc += TextUtil::linkify(TextUtil::plain2rich(desc.mid(lastEnd, desc.size() - lastEnd)));
    }
    htmlDesc += tailReferences;
    // OOB links that are not files (xmpp:, ftp: ...). Only known schemes become clickable.
    for (const auto &link : collected.links) {
        if (!htmlDesc.isEmpty())
            htmlDesc += QLatin1String("<br/>");
        htmlDesc += TextUtil::linkify(TextUtil::escape(link.url()));
        if (!link.desc().isEmpty())
            htmlDesc += QLatin1String(" &mdash; ") + TextUtil::escape(link.desc());
    }
    // qDebug() << "HTML:" << htmlDesc;
    mv.setHtml(htmlDesc);
}

quint64 FileSharingManager::autoDownloadLimit(const Message &m, PsiAccount *acc) const
{
    if (!acc)
        return 0;
    auto                         *o = PsiOptions::instance();
    MediaDownloadPolicy::Settings settings;
    settings.previews  = o->getOption("options.ui.chat.show-previews").toBool();
    settings.chatLimit = quint64(qMax<qlonglong>(0, o->getOption("options.ui.chat.auto-download-size").toLongLong()));
    settings.groupchatLimit
        = quint64(qMax<qlonglong>(0, o->getOption("options.ui.chat.groupchat-auto-download-size").toLongLong()));

    const bool groupchat = m.type() == Message::Type::Groupchat;
    // Our own messages, sent from here or another device, and roster contacts.
    bool trusted = m.from().isEmpty() || m.from().bare() == acc->jid().bare();
    if (!trusted && !groupchat) {
        const PsiContact *contact = acc->findContact(m.from().withResource({}));
        trusted                   = contact && contact->inList() && !contact->isPrivate();
    }
    return MediaDownloadPolicy::limit(settings, groupchat, trusted);
}

bool FileSharingManager::answerFileRequest(Jingle::Session *session, PsiAccount *account)
{
    const QPointer<PsiAccount> guard(account);
    return FileSharingResponder::answer(
        session, [this, guard](const QList<Hash> &hashes) -> std::optional<FileSharingResponder::ServedFile> {
            if (!guard)
                return {};
            const auto publication = d->catalog->servedRecord(guard->id(), hashes);
            if (!publication)
                return {};
            const auto authorized = publication->media.file.computedHashes();
            QString    path;
            if (auto cached = cacheItem(authorized, true, &path))
                return FileSharingResponder::ServedFile { path, cached->sums() };
            // Runtime items and received source unions are not authorization
            // records. Only the unchanged local copy of this own publication
            // can supply a fallback path.
            path = publication->readableLocalPath();
            if (!path.isEmpty()) {
                if (obsoleteRangePayload(path)
                    || (path.startsWith(cacheDir() + QStringLiteral("/ranges/"))
                        && !mediaChunkCache()->isCompleteFile(path)))
                    return {};
                return FileSharingResponder::ServedFile { path, authorized };
            }
            return {};
        });
}

bool FileSharingManager::serve(PsiAccount *account, const MediaSharing &media, const QString &localPath)
{
    // A publication can outlive a crash immediately after this call. Its
    // managed payload must already have an owner in the file registry.
    for (const auto &hash : media.file.computedHashes()) {
        auto cached = d->cache->get(hash);
        if (cached && !cached->isExternal() && cached->filePath() == localPath) {
            if (!d->cache->persist(hash))
                return false;
            break;
        }
    }
    const bool stored = d->catalog->serve(account->id(), account->jid(), media, localPath);
    if (!stored)
        qWarning("Could not persist a served file");
    return stored;
}

void FileSharingManager::stopServing(PsiAccount *account, const QList<Hash> &hashes)
{
    d->catalog->stopServing(account->id(), hashes);
}

QStringList FileSharingManager::knownSources(PsiAccount *account, const QList<Hash> &hashes) const
{
    QStringList sources;
    for (const auto &hash : hashes)
        if (const auto stored = d->catalog->find(account->id(), hash))
            sources += stored->media.sources;
    sources.removeDuplicates();
    return sources;
}

void FileSharingManager::importLegacyCatalog(const QList<PsiAccount *> &accounts)
{
    QHash<QString, Jid> own;
    for (const auto *account : accounts)
        own.insert(account->id(), account->jid());
    // A profile has had its own copy since caches moved under the profile.
    const auto copy = cacheDir() + QStringLiteral("/shared-files.sqlite");
    if (d->catalog->importLegacy(QFile::exists(copy) ? copy : legacyDir() + QStringLiteral("/shared-files.sqlite"),
                                 own)) {
        if (QDir(legacyDir()).exists())
            FileDatabase::markLegacyDirectoryImported(legacyDir(), QStringLiteral("publications"));
        QFile::remove(copy);
    }
}

void FileSharingManager::followHistory(EDBSqLite *history)
{
    d->history = history;
    connect(history, &EDBSqLite::mediaLinksAdded, this, &FileSharingManager::chatMediaChanged);
    connect(history, &EDBSqLite::mediaLinksRemoved, this, &FileSharingManager::chatMediaChanged);
    const QPointer<EDBSqLite>                 database(history);
    FileSharingPublicationSync::HistoryAccess access;
    access.readLinks = [database](qint64 position, int limit, QObject *receiver,
                                  std::function<void(QList<HistoryMediaRefs::Link>, bool)> finished) {
        if (database)
            database->requestMediaLinks(position, limit, receiver, std::move(finished));
    };
    access.readLinked = [database](const QList<QByteArray> &ids, QObject *receiver,
                                   std::function<void(QSet<QByteArray>, bool)> finished) {
        if (database)
            database->requestLinkedMedia(ids, receiver, std::move(finished));
    };
    auto *sync = new FileSharingPublicationSync(std::move(access), d->catalog.get(), this);
    connect(history, &EDBSqLite::mediaLinksAdded, sync, &FileSharingPublicationSync::linksAdded);
    connect(history, &EDBSqLite::mediaLinksRemoved, sync, &FileSharingPublicationSync::reconcile);
    sync->reconcile();
}

XMPP::Hash FileSharingDeviceOpener::urlToSourceId(const QUrl &url)
{
    if (url.scheme() != QLatin1String("share"))
        return XMPP::Hash();

    QString path     = url.path();
    auto    sourceId = QStringView { path };
    if (sourceId.startsWith('/'))
        sourceId = sourceId.mid(1);
    return XMPP::Hash::from(sourceId);
}

FileSharingDeviceOpener::~FileSharingDeviceOpener()
{
    // Wake decoder reads before QObject::destroyed tears down the players.
    for (auto reader : findChildren<FileReader *>())
        reader->abort();
}

QIODevice *FileSharingDeviceOpener::open(QUrl &url)
{
    const auto sourceId = urlToSourceId(url);
    if (!sourceId.isValid())
        return nullptr;
    auto manager = acc->psi()->fileSharingManager();
    auto item    = manager->item(sourceId, acc);
    if (!item)
        return nullptr;
    const bool cached = item->isCached();
    if (!cached)
        FileSharingChunkSource::configure(item);
    const bool complete = cached || !FileSharingChunkSource::localFile(item).isEmpty();
    if (complete || MediaPlaybackPolicy::deviceStreaming()) {
        if (auto reader = manager->reader(item, this)) {
            if (reader->open())
                return reader;
            reader->deleteLater();
            return nullptr;
        }
    }
    // GStreamer cannot suspend a seekable GUI-thread read. The loopback proxy
    // remains a thin adapter over the same FileReader and download controller.
    // Address-only shares also stay on Psi's network path until identity/size is known.
#ifdef HAVE_WEBSERVER
    auto local = acc->psi()->webServer()->serverUrl();
    auto path  = local.path();
    if (!path.endsWith('/'))
        path += '/';
    path += QStringLiteral("psi/account/%1/sharedfile/%2").arg(acc->id(), sourceId.toString());
    local.setPath(path);
    url = FileSharingUrl::withMode(local, FileSharingUrl::Mode::Stream);
    return nullptr;
#else
    if (!item->isUrlOnly() && item->fileSize() && *item->fileSize()) {
        if (auto state = mediaState(url))
            state->setSourceError(tr("Download the complete file before playing it."));
        return nullptr;
    }
    auto downloader = item->download();
    if (downloader) {
        downloader->setParent(this);
        downloader->open(QIODevice::ReadOnly);
    }
    return downloader;
#endif
}

void FileSharingDeviceOpener::abort(QIODevice *dev)
{
    if (auto reader = qobject_cast<FileReader *>(dev))
        reader->abort();
    else if (auto downloader = qobject_cast<FileShareDownloader *>(dev))
        downloader->abort();
}

void FileSharingDeviceOpener::close(QIODevice *dev)
{
    abort(dev);
    dev->close();
    dev->deleteLater();
}

void FileSharingDeviceOpener::retry(const QUrl &url)
{
    auto item = sharedItem(urlToSourceId(url).toString());
    if (item && item->fileSize())
        if (auto cache = FileSharingChunkSource::configure(item))
            cache->resume(FileSharingChunkSource::identity(item), *item->fileSize());
}

QVariant FileSharingDeviceOpener::metadata(const QUrl &url)
{
    auto sourceId = urlToSourceId(url);
    auto item     = acc->psi()->fileSharingManager()->item(sourceId, acc);
    if (!item)
        return QVariant();
    auto       vm   = item->metaData();
    QByteArray ampl = vm.value(QLatin1String("amplitudes")).toByteArray();
    if (ampl.size()) {
        QList<float> fampl;
        std::transform(ampl.cbegin(), ampl.cend(), std::back_inserter(fampl),
                       [](const char b) { return float(quint8(b)) / 255.1f; });
        vm.insert(QLatin1String("amplitudes"), QVariant::fromValue<QList<float>>(fampl));
    }
    return vm;
}

FileSharingItem *FileSharingDeviceOpener::sharedItem(const QString &id)
{
    return acc->psi()->fileSharingManager()->item(XMPP::Hash::from(id), acc);
}

FileSharingDeviceOpener *FileSharingDeviceOpener::forAccount(const QString &id) const
{
    if (id.isEmpty() || id == acc->id())
        return const_cast<FileSharingDeviceOpener *>(this);
    auto account = acc->psi()->contactList()->getAccount(id);
    return account ? account->fileSharingDeviceOpener() : nullptr;
}

ITEMediaState *FileSharingDeviceOpener::mediaState(const QUrl &url)
{
    auto item = acc->psi()->fileSharingManager()->item(urlToSourceId(url), acc);
    if (!item)
        return nullptr;
    const auto name = QStringLiteral("qite-media-state");
    if (auto existing = item->findChild<ITEMediaState *>(name, Qt::FindDirectChildrenOnly))
        return existing;
    auto state = new ITEMediaState(item);
    state->setObjectName(name);
    auto cache = FileSharingChunkSource::configure(item);
    if (!cache)
        cache = acc->psi()->fileSharingManager()->mediaChunkCache();
    const auto size   = item->fileSize();
    const auto hash   = item->sums().value(0);
    const auto source = FileSharingChunkSource::identity(item);
    auto       update = [item, cache, state, source, size] {
        ITEMediaState::Ranges ranges;
        if (item->isCached()) {
            ranges.append({ 0, 1 });
        } else if (size && *size && !source.isEmpty()) {
            for (const auto &range : cache->cachedRanges(source, *size))
                ranges.append({ qreal(range.first) / *size, qreal(range.first + range.second) / *size });
        }
        if (size && !source.isEmpty()) {
            const auto error = cache->fileState(source, *size).error;
            state->setSourceError(error);
        }
        state->setCachedRanges(std::move(ranges));
    };
    QObject::connect(cache, &MediaChunkCache::rangesChanged, state,
                     [update, source, size](const QString &changed, quint64 changedSize) {
                         if (size && changed == source && changedSize == *size)
                             update();
                     });
    QObject::connect(item, &FileSharingItem::downloadFinished, state, update);
    update();
    return state;
}

bool FileSharingDeviceOpener::saveAs(const QUrl &url, QWidget *parent, const SaveCallback &completed,
                                     const ProgressCallback &progress)
{
    QPointer<FileSharingItem> item  = acc->psi()->fileSharingManager()->item(urlToSourceId(url), acc);
    QPointer<ITEMediaState>   state = mediaState(url);
    if (!item || !state || state->downloading())
        return false;
    const auto path = FileUtil::getSaveFileName(parent, QObject::tr("Save file"), item->displayName());
    // The dialog runs a nested event loop. Recheck lifetime and concurrent saves.
    if (path.isEmpty() || !item || !state || state->downloading())
        return false;
    return saveTo(url, path, completed, progress);
}

bool FileSharingDeviceOpener::saveTo(const QUrl &url, const QString &path, const SaveCallback &completed,
                                     const ProgressCallback &progress)
{
    QPointer<FileSharingItem> item  = acc->psi()->fileSharingManager()->item(urlToSourceId(url), acc);
    QPointer<ITEMediaState>   state = mediaState(url);
    if (path.isEmpty() || !item || !state || state->downloading())
        return false;
    auto                  manager = acc->psi()->fileSharingManager();
    const auto            local   = item->isCached() ? item->fileName() : FileSharingChunkSource::localFile(item);
    FileSharingMediaSave *save;
    if (local.isEmpty()) {
        if (auto source = manager->download(item)) {
            save = new FileSharingMediaSave(source, path, state, item, completed, progress);
            save->setObjectName(QStringLiteral("psi-sims-save"));
            return true;
        }
    }
    const QPointer<FileSharingManager> storage(manager);
    save = new FileSharingMediaSave(
        [storage, item](QObject *owner, FileSharingMediaSave::OpenReady ready) -> MediaChunkAbort {
            if (!storage || !item) {
                QTimer::singleShot(0, owner,
                                   [ready] { ready(nullptr, 0, QStringLiteral("Media source unavailable")); });
                return {};
            }
            return storage->open(item, owner, [ready = std::move(ready)](FileReader *reader, MediaChunkError error) {
                ready(reader, reader ? quint64(reader->size()) : 0, error.message());
            });
        },
        path, state, item, completed, progress);
    save->setObjectName(QStringLiteral("psi-sims-save"));
    return true;
}

void FileSharingDeviceOpener::cancelSave(const QUrl &url)
{
    auto item = acc->psi()->fileSharingManager()->item(urlToSourceId(url), acc);
    if (!item)
        return;
    // FileSharingMediaSave has no Q_OBJECT, so findChild<FileSharingMediaSave *>()
    // fails the Qt 6 compile-time meta-object check. The object name is
    // assigned only to FileSharingMediaSave instances created by saveTo().
    if (auto save = item->findChild<QObject *>(QStringLiteral("psi-sims-save"), Qt::FindDirectChildrenOnly))
        static_cast<FileSharingMediaSave *>(save)->cancel();
}
