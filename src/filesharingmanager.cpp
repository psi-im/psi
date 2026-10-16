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

#include "applicationinfo.h"
#include "filecache.h"
#include "filesharingdownloader.h"
#include "filesharingjinglesession.h"
#include "filesharingmediasave.h"
#include "filesharingurl.h"
#include "fileutil.h"
#include "httputil.h"
#include "mediachunkcache.h"
#include "psiaccount.h"
#include "psicon.h"
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

#include <QDir>
#include <QMimeData>

// ======================================================================
// FileSharingManager
// ======================================================================
class FileSharingManager::Private {
public:
    FileCache                                      *cache;
    MediaChunkCache                                *chunkCache = nullptr;
    QHash<PsiAccount *, FileSharingJingleSession *> mediaSessions;
    QHash<XMPP::Hash, FileSharingItem *>            items;

    void rememberItem(FileSharingItem *item)
    {
        Q_ASSERT(item->sums().size());
        for (auto const &v : item->sums())
            items.insert(v, item); // TODO ensure we don't overwrite
    }
};

FileSharingManager::FileSharingManager(QObject *parent) : QObject(parent), d(new Private)
{
    d->cache = new FileCache(cacheDir(), this);
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
    if (!d->chunkCache)
        d->chunkCache = new MediaChunkCache(cacheDir() + QStringLiteral("/media-chunks-v1"), this);
    return d->chunkCache;
}

QString FileSharingManager::cacheDir()
{
    QDir shares(ApplicationInfo::homeDir(ApplicationInfo::DataLocation) + "/shares");
    if (!shares.exists()) {
        QDir home(ApplicationInfo::homeDir(ApplicationInfo::DataLocation));
        home.mkdir("shares");
    }
    return shares.path();
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

    QString link     = item->metadata().value(QLatin1String("link")).toString();
    QString fileName = link.size() ? link : d->cache->cacheDir() + "/" + item->fileName();
    if (fileName.isEmpty() || !QFileInfo(fileName).isReadable()) {
        d->cache->remove(id);
        return nullptr;
    }
    if (fileName_out)
        *fileName_out = fileName;
    return item;
}

FileCacheItem *FileSharingManager::saveToCache(const QList<XMPP::Hash> &sums, const QByteArray &data,
                                               const QVariantMap &metadata, unsigned int maxAge)
{
    return d->cache->append(sums, data, metadata, maxAge);
}

FileCacheItem *FileSharingManager::moveToCache(const QList<XMPP::Hash> &sums, const QFileInfo &file,
                                               const QVariantMap &metadata, unsigned int maxAge)
{
    return d->cache->moveToCache(sums, file, metadata, maxAge);
}

FileSharingItem *FileSharingManager::item(const Hash &id) { return d->items.value(id); }

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

void FileSharingManager::fillMessageView(MessageView &mv, const Message &m, PsiAccount *acc)
{
    auto refs = m.references();
    if (refs.count()) {
        QString tailReferences;
        QString desc = m.body();
        // qDebug() << "INIT TEXT:" << desc;
        QString htmlDesc;
        htmlDesc.reserve(desc.size() + refs.count() * 64);

        std::sort(refs.begin(), refs.end(), [](auto &a, auto &b) { return a.begin() < b.begin(); });

        int         lastEnd = 0;
        const auto &refs    = m.references();
        for (auto const &r : refs) {
            MediaSharing ms = r.mediaSharing();
            // qDebug() << "BEGIN:" << r.begin() << "END:" << r.end();
            // only audio and image supported for now
            if (!ms.isValid() || !ms.file.hasComputedHashes() || !ms.file.size().has_value()
                || !(ms.file.mediaType().startsWith(QLatin1String("audio"))
                     || ms.file.mediaType().startsWith(QLatin1String("image"))
                     || ms.file.mediaType().startsWith(QLatin1String("video")))) {
                continue;
            }
            auto item = new FileSharingItem(ms, m.from(), acc, this);
            d->rememberItem(item);
            mv.addReference(item);

            if (r.begin() != -1 && r.begin() >= lastEnd) {
                auto refText = desc.mid(r.begin(), r.end() - r.begin() + 1);

                QUrl url(refText.trimmed());
                if (url.isValid())
                    refText = url.toString(QUrl::FullyEncoded);
                else
                    refText = TextUtil::escape(refText);

                QString shareStr(
                    QString::fromLatin1("<share id=\"%1\" text=\"%2\"/>").arg(item->sums()[0].toString(), refText));

                // add text before reference
                htmlDesc += TextUtil::linkify(TextUtil::plain2rich(desc.mid(lastEnd, r.begin() - lastEnd)));
                htmlDesc += shareStr; // something instead of link
                lastEnd = r.end() + 1;
            } else {
                tailReferences += QString::fromLatin1("<share id=\"%1\"/>").arg(item->sums()[0].toString());
            }
        }
        if (lastEnd < desc.size()) {
            htmlDesc += TextUtil::linkify(TextUtil::plain2rich(desc.mid(lastEnd, desc.size() - lastEnd)));
        }
        htmlDesc += tailReferences;
        // qDebug() << "HTML:" << htmlDesc;
        mv.setHtml(htmlDesc);
    }
}

bool FileSharingManager::jingleAutoAcceptIncomingDownloadRequest(Jingle::Session *session)
{
    if (!acceptIncomingDownloadContents(session))
        return false;
    connect(session, &Jingle::Session::newContentReceived, this, [this, session] {
        if (acceptIncomingDownloadContents(session))
            return;
        // Only reject the new proposals. Existing accepted transfers continue.
        for (auto *app : session->contentList())
            if (app->isRemote() && app->state() == Jingle::State::Created)
                app->remove(Jingle::Reason::Decline, QStringLiteral("Shared file unavailable"));
    });
    session->accept();
    return true;
}

bool FileSharingManager::acceptIncomingDownloadContents(Jingle::Session *session)
{
    QVector<QPair<Jingle::FileTransfer::Application *, FileCacheItem *>> toAccept;
    // check if we can accept the session immediately w/o user interaction
    for (auto const &a : session->contentList()) {
        if (!a->isRemote() || a->state() != Jingle::State::Created)
            continue;
        if (a->pad()->ns() != Jingle::FileTransfer::NS || a->senders() != session->role())
            return false;
        auto ft = static_cast<Jingle::FileTransfer::Application *>(a);

        FileCacheItem *item = cacheItem(ft->file().computedHashes(), true);
        if (!item)
            return false;

        toAccept.append(qMakePair(ft, item));
    }

    for (auto const &p : toAccept) {
        auto ft   = p.first;  // jingle-ft app
        auto item = p.second; // FileCacheItem

        ft->setKeepTransportUntilReceipt();
        connect(ft, &Jingle::Application::stateChanged, ft, [ft](Jingle::State state) {
            if (state == Jingle::State::Finished)
                ft->deleteLater();
        });

        auto acceptFile = ft->file();
        acceptFile.setHashes(item->sums());
        ft->setAcceptFile(acceptFile);

        // TODO hashes for ranges
        // Snapshot the source path before waiting for the transport. Cache
        // maintenance may retire the FileCacheItem while a content-add waits.
        QString fileName = item->metadata().value(QStringLiteral("link")).toString();
        if (fileName.isEmpty())
            fileName = d->cache->cacheDir() + "/" + item->fileName();
        connect(ft, &Jingle::FileTransfer::Application::deviceRequested, ft,
                [ft, fileName](quint64 offset, std::optional<quint64> /*size*/) {
                    auto f = new QFile(fileName, ft);
                    if (!f->open(QIODevice::ReadOnly)) {
                        const auto error = f->errorString();
                        delete f;
                        ft->remove(Jingle::Reason::FailedApplication, error);
                        return;
                    }
                    if (offset > quint64(f->size()) || !f->seek(qint64(offset))) {
                        delete f;
                        ft->remove(Jingle::Reason::FailedApplication, QStringLiteral("Shared file range unavailable"));
                        return;
                    }
                    ft->setDevice(f);
                });
    }
    if (session->state() != Jingle::State::Created)
        for (const auto &p : toAccept)
            p.first->prepare();

    return true;
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

QIODevice *FileSharingDeviceOpener::open(QUrl &url)
{
    auto sourceId = urlToSourceId(url);
    if (!sourceId.isValid())
        return nullptr;

    auto             fsm  = acc->psi()->fileSharingManager();
    FileSharingItem *item = fsm->item(sourceId);
    if (!item)
        return nullptr;

    if (item->isCached()) {
        url = QUrl::fromLocalFile(item->fileName());
        return nullptr;
    }

#ifdef HAVE_WEBSERVER
    QUrl    localServerUrl = acc->psi()->webServer()->serverUrl();
    QString path           = localServerUrl.path();
    path.reserve(128);
    if (!path.endsWith('/'))
        path += '/';
    path += QString("psi/account/%1/sharedfile/%2").arg(acc->id(), sourceId.toString());
    localServerUrl.setPath(path);
    url = FileSharingUrl::withMode(localServerUrl, FileSharingUrl::Mode::Stream);
    return nullptr;
#else
#ifndef Q_OS_WIN
    QUrl simplelUrl = item->simpleSource();
    if (simplelUrl.isValid()) {
        url = simplelUrl;
        return nullptr;
    }
#endif
    // return
    // acc->psi()->networkAccessManager()->get(QNetworkRequest(QUrl("https://jabber.ru/upload/98354d3264f6584ef9520cc98641462d6906288f/mW6JnUCmCwOXPch1M3YeqSQUMzqjH9NjmeYuNIzz/file_example_OOG_1MG.ogg")));
    FileShareDownloader *downloader = item->download();
    if (!downloader)
        return nullptr;

    downloader->open(QIODevice::ReadOnly);
    return downloader;
#endif
}

void FileSharingDeviceOpener::close(QIODevice *dev)
{
    dev->close();
    dev->deleteLater();
}

QVariant FileSharingDeviceOpener::metadata(const QUrl &url)
{
    auto sourceId = urlToSourceId(url);
    auto item     = acc->psi()->fileSharingManager()->item(sourceId);
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
    return acc->psi()->fileSharingManager()->item(XMPP::Hash::from(id));
}

ITEMediaState *FileSharingDeviceOpener::mediaState(const QUrl &url)
{
    auto item = acc->psi()->fileSharingManager()->item(urlToSourceId(url));
    if (!item)
        return nullptr;
    const auto name = QStringLiteral("qite-media-state");
    if (auto existing = item->findChild<ITEMediaState *>(name, Qt::FindDirectChildrenOnly))
        return existing;
    auto state = new ITEMediaState(item);
    state->setObjectName(name);
    auto       cache  = acc->psi()->fileSharingManager()->mediaChunkCache();
    const auto size   = item->fileSize();
    const auto hash   = item->sums().value(0);
    const auto source = item->account() ? item->account()->id() + QChar(0) + hash.toString() : QString();
    auto       update = [item, cache, state, source, size] {
        ITEMediaState::Ranges ranges;
        if (item->isCached()) {
            ranges.append({ 0, 1 });
        } else if (size && *size && !source.isEmpty()) {
            for (const auto &range : cache->cachedRanges(source, *size))
                ranges.append({ qreal(range.first) / *size, qreal(range.first + range.second) / *size });
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

void FileSharingDeviceOpener::saveAs(const QUrl &url, QWidget *parent)
{
    QPointer<FileSharingItem> item  = acc->psi()->fileSharingManager()->item(urlToSourceId(url));
    QPointer<ITEMediaState>   state = mediaState(url);
    if (!item || !state || state->downloading())
        return;
    const auto path = FileUtil::getSaveFileName(parent, QObject::tr("Save media"), item->displayName());
    // The dialog runs a nested event loop. Recheck lifetime and concurrent saves.
    if (path.isEmpty() || !item || !state || state->downloading())
        return;
    QIODevice           *input    = nullptr;
    auto                 size     = item->fileSize();
    FileShareDownloader *download = nullptr;
    if (item->isCached()) {
        auto file = new QFile(item->fileName());
        size      = quint64(file->size());
        input     = file;
    } else {
        input = download = item->download(); // Full transfer, not playback chunks.
    }
    if (!input) {
        state->setDownload(false, -1, QObject::tr("No download source"));
        return;
    }
    auto save = new FileSharingMediaSave(input, size, download != nullptr, path, state, item);
    if (download) {
        QObject::connect(download, &FileShareDownloader::transferCompleted, save, &FileSharingMediaSave::validated);
        QObject::connect(download, &FileShareDownloader::disconnected, save, &FileSharingMediaSave::sourceClosed);
        QObject::connect(download, &FileShareDownloader::failed, save,
                         [save, download] { save->fail(download->errorString()); });
    }
}
