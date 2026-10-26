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
#include "filesharingchunksource.h"
#include "filesharingdownloader.h"
#include "filesharingjinglesession.h"
#include "filesharingmediasave.h"
#include "filesharingstore.h"
#include "filesharingurl.h"
#include "fileutil.h"
#include "httputil.h"
#include "mediachunkcache.h"
#include "mediadownloadpolicy.h"
#include "messageattachments.h"
#include "oobmediapolicy.h"
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

#include <QDir>
#include <QCryptographicHash>
#include <QUrl>
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
    QHash<QString, QHash<XMPP::Hash, FileSharingItem *>> accountItems;
    std::unique_ptr<FileSharingStore>                    store;

    void rememberItem(FileSharingItem *item)
    {
        Q_ASSERT(item->sums().size());
        for (auto const &v : item->sums()) {
            items.insert(v, item); // TODO ensure we don't overwrite
            if (item->account())
                accountItems[item->account()->id()].insert(v, item);
        }
    }
};

FileSharingManager::FileSharingManager(QObject *parent) : QObject(parent), d(new Private)
{
    d->cache = new FileCache(cacheDir(), this);
    d->store.reset(new FileSharingStore(cacheDir() + QStringLiteral("/shared-files.sqlite")));
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
        d->chunkCache = new MediaChunkCache(cacheDir() + QStringLiteral("/media-files-v2"), this, 4 * 1024 * 1024,
                                            256 * 1024 * 1024, [](const QString &algorithm, QIODevice *device) {
                                                XMPP::Hash hash { QStringView(algorithm) };
                                                return hash.compute(device) ? hash.data() : QByteArray();
                                            });
        connect(d->chunkCache, &MediaChunkCache::integrityError, this,
                [this](const QString &source, quint64 size, const QString &) {
                    const auto path = d->chunkCache->fileState(source, size).path;
                    for (auto item : d->items) {
                        if (!item || FileSharingChunkSource::identity(item) != source)
                            continue;
                        for (const auto &hash : item->sums()) {
                            auto cached = d->cache->get(hash);
                            if (cached && cached->metadata().value(QStringLiteral("link")).toString() == path)
                                d->cache->remove(hash);
                        }
                    }
                });
    }
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
    if (link.startsWith(cacheDir() + QStringLiteral("/media-files-v2/")) && !mediaChunkCache()->isCompleteFile(link))
        return nullptr; // A restored payload is available only after worker verification.
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

std::optional<Hash> FileSharingManager::contentForUrl(const QString &account, const Hash &urlIdentity) const
{
    return d->store->contentForUrl(account, urlIdentity);
}

bool FileSharingManager::rememberUrlContent(const QString &account, const Hash &urlIdentity,
                                            const Hash &contentHash)
{
    return d->store->rememberUrlContent(account, urlIdentity, contentHash);
}

void FileSharingManager::forgetUrlContent(const QString &account, const Hash &urlIdentity)
{
    d->store->forgetUrlContent(account, urlIdentity);
}

FileSharingItem *FileSharingManager::item(const Hash &id, PsiAccount *account)
{
    if (!account)
        return d->items.value(id);
    if (auto existing = d->accountItems.value(account->id()).value(id))
        if (existing->account() == account)
            return existing;
    const auto stored = d->store->find(account->id(), id);
    if (!stored)
        return nullptr;
    const auto media = stored->reference.mediaSharing();
    const auto path  = stored->readableLocalPath();
    if (!path.isEmpty() && !cacheItem(media.file.computedHashes())) {
        QVariantMap metadata { { "type", media.file.mediaType() },
                               { "link", path },
                               { "uris", media.sources },
                               { "amplitudes", media.file.amplitudes() } };
        saveToCache(media.file.computedHashes(), {}, metadata, FileCache::Forever);
    }
    auto restored = new FileSharingItem(media, Jid(), account, this);
    for (const auto &peer : stored->peers)
        restored->mergeSources(media, Jid(peer));
    d->rememberItem(restored);
    return restored;
}

void FileSharingManager::rememberMessage(const Message &message, PsiAccount *account)
{
    if (!account)
        return;
    for (const auto &reference : message.references()) {
        if (!reference.isValid() || !reference.mediaSharing().isValid())
            continue;
        const auto hashes = reference.mediaSharing().file.computedHashes();
        if (hashes.isEmpty())
            continue;
        if (message.spooled() && d->store->find(account->id(), hashes.first()))
            continue;
        QString localPath;
        for (const auto &hash : hashes) {
            auto known = d->accountItems.value(account->id()).value(hash);
            if (known && known->account() == account && !known->localFilePath().isEmpty()) {
                localPath = known->localFilePath();
                break;
            }
        }
        const auto peer = message.from().isEmpty() ? account->jid() : message.from();
        if (!d->store->remember(account->id(), reference, peer, localPath))
            qWarning("Could not persist a shared file descriptor");
    }
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

void FileSharingManager::fillMessageView(MessageView &mv, const Message &m, PsiAccount *acc)
{
    if (!acc)
        return;
    rememberMessage(m, acc);
    // Everything a message attaches is shown in its one view: SIMS and HTTP(S)
    // OOB as share objects, other OOB URLs as links. The message is not changed.
    const auto collected   = MessageAttachments::collect(m);
    auto       attachments = collected.attachments;
    if (attachments.isEmpty() && collected.links.isEmpty())
        return;
    // Views may download attachments by themselves only up to this size.
    const quint64 autoLimit = autoDownloadLimit(m, acc);
    const QString autoAttr  = autoLimit ? QString::fromLatin1(" auto=\"%1\"").arg(autoLimit) : QString();

    QString tailReferences;
    QString desc = m.body();
    // qDebug() << "INIT TEXT:" << desc;
    QString htmlDesc;
    htmlDesc.reserve(desc.size() + attachments.count() * 64);

    // XEP-0385 senders may locate a share's body text with XEP-0428
    // fallbacks instead of reference begin/end. Assign those ranges, in
    // body order, to the media shares that have no range of their own.
    QList<QPair<int, int>> fallbackRanges; // UTF-16 [from, to)
    for (const auto &ns : { QStringLiteral("urn:xmpp:sims:1"), QStringLiteral("jabber:x:oob") }) {
        for (const auto &range : m.bodyFallbacks(ns)) {
            int from = -1, to = -1;
            if (range.toUtf16(desc, from, to) && to > from && !fallbackRanges.contains({ from, to }))
                fallbackRanges.append({ from, to });
        }
        if (!fallbackRanges.isEmpty())
            break; // SIMS ranges take precedence; OOB ranges are a fallback for them.
    }
    std::sort(fallbackRanges.begin(), fallbackRanges.end());
    for (auto &attachment : attachments) {
        if (fallbackRanges.isEmpty())
            break;
        auto &r = attachment.reference;
        if (r.begin() < 0 && r.mediaSharing().isValid()) {
            const auto range = fallbackRanges.takeFirst();
            r.setRange(range.first, range.second - 1); // Psi treats 'end' as inclusive
        }
    }

    std::sort(attachments.begin(), attachments.end(),
              [](const auto &a, const auto &b) { return a.reference.begin() < b.reference.begin(); });

    int lastEnd = 0;
    for (const auto &attachment : attachments) {
        const auto  &r  = attachment.reference;
        MediaSharing ms = r.mediaSharing();
        // qDebug() << "BEGIN:" << r.begin() << "END:" << r.end();
        // Every SIMS attachment needs a share object, not only inline media.
        // Non-media files can have an unknown size (streaming sources).
        FileSharingItem *item = nullptr;
        if (ms.isValid() && ms.file.hasComputedHashes()) {
            item = this->item(ms.file.computedHashes().first(), acc);
            if (!item)
                item = new FileSharingItem(ms, m.from(), acc, this);
            else
                item->mergeSources(ms, m.from());
            d->rememberItem(item);
        } else if (!ms.isValid() && r.type() == Reference::Data) {
            // An HTTP(S) URL without SIMS metadata, usually from OOB. Shares of
            // the same URL are one item; the item keeps the full URL to open.
            const QUrl source = OobMediaPolicy::normalizedHttpSource(r.uri());
            if (!source.isValid())
                continue;
            const QString    canonical = source.toString(QUrl::FullyEncoded);
            const QByteArray key       = QByteArrayLiteral("psi-oob-url-v1:") + canonical.toUtf8();
            const Hash       identity(Hash::Sha256, QCryptographicHash::hash(key, QCryptographicHash::Sha256));
            item = d->accountItems.value(acc->id()).value(identity, nullptr);
            if (!item || item->account() != acc || !item->isUrlOnly()) {
                item = new FileSharingItem(QUrl::fromEncoded(r.uri().toUtf8()), identity, acc, this,
                                           attachment.description);
                d->rememberItem(item);
            }
        } else {
            continue;
        }
        mv.addReference(item);

        if (r.begin() >= lastEnd && r.end() >= r.begin() && r.end() < desc.size()) {
            auto refText = desc.mid(r.begin(), r.end() - r.begin() + 1);

            QUrl url(refText.trimmed());
            if (url.isValid())
                refText = url.toString(QUrl::FullyEncoded);

            QString shareStr(
                QString::fromLatin1("<share id=\"%1\" account=\"%2\" text=\"%3\"%4/>")
                    .arg(item->sums()[0].toString(), TextUtil::escape(acc->id()), TextUtil::escape(refText), autoAttr));

            // add text before reference
            htmlDesc += TextUtil::linkify(TextUtil::plain2rich(desc.mid(lastEnd, r.begin() - lastEnd)));
            htmlDesc += shareStr; // something instead of link
            lastEnd = r.end() + 1;
        } else {
            tailReferences += QString::fromLatin1("<share id=\"%1\" account=\"%2\"%3/>")
                                  .arg(item->sums()[0].toString(), TextUtil::escape(acc->id()), autoAttr);
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
        if (!item) {
            for (const auto &hash : ft->file().computedHashes()) {
                auto known = d->items.value(hash);
                if (!known)
                    continue;
                const auto path = known->localFilePath();
                if (path.isEmpty())
                    continue;
                QVariantMap metadata { { "type", known->mimeType() }, { "link", path }, { "uris", known->uris() } };
                saveToCache(known->sums(), { }, metadata, FileCache::Session);
                item = cacheItem(known->sums(), true);
                if (item)
                    break;
            }
        }
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
    FileSharingItem *item = fsm->item(sourceId, acc);
    if (!item)
        return nullptr;

    if (item->isCached()) {
        qInfo() << "[media] opening cached file" << sourceId.toString() << "type=" << item->mimeType();
        url = QUrl::fromLocalFile(item->fileName());
        return nullptr;
    }
    // Hashless OOB shares have no verified chunk stream. They are streamed
    // through Psi's network access and its proxy like other shares, never
    // handed to the player as their remote origin.
    if (!item->isUrlOnly()) {
        FileSharingChunkSource::configure(item);
        const auto localFile = FileSharingChunkSource::localFile(item);
        if (!localFile.isEmpty()) {
            qInfo() << "[media] opening completed shared file" << sourceId.toString();
            url = QUrl::fromLocalFile(localFile);
            return nullptr;
        }
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
    qInfo() << "[media] opening HTTP stream" << sourceId.toString() << "type=" << item->mimeType();
    return nullptr;
#else
#ifndef Q_OS_WIN
    QUrl simplelUrl = item->isUrlOnly() ? QUrl() : item->simpleSource();
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
    const auto source = item->account() ? item->account()->id() + QChar(0) + hash.toString() : QString();
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
    QIODevice           *input    = nullptr;
    auto                 size     = item->fileSize();
    FileShareDownloader *download = nullptr;
    const auto           localFile = item->isCached() ? item->fileName() : FileSharingChunkSource::localFile(item);
    if (!localFile.isEmpty()) {
        auto file = new QFile(localFile);
        size      = quint64(file->size());
        input     = file;
    } else if (size && *size && !item->isUrlOnly()) {
        auto cache = FileSharingChunkSource::configure(item);
        cache->configure(FileSharingChunkSource::identity(item), *size, { },
                         { { QStringLiteral("retain-partial"), true } });
        cache->resume(FileSharingChunkSource::identity(item), *size); // Explicit save resumes a failed download.
        const QPointer<FileSharingItem> resource(item);
        const QPointer<MediaChunkCache> guard(cache);
        auto                            reader = new MediaChunkReader(
            cache, FileSharingChunkSource::identity(item), *size, 0, *size, acc->id(),
            [resource, guard](quint64 offset, quint64 length, MediaChunkCompletion done) {
                return FileSharingChunkSource::fetch(resource, offset, length, std::move(done), guard);
            });
        input = reader;
    } else {
        input = download = item->download();
    }
    if (!input) {
        const QString error = QObject::tr("No download source");
        state->setDownload(false, -1, error);
        if (completed)
            completed({}, error);
        return false;
    }
    auto save = new FileSharingMediaSave(input, size, download != nullptr || qobject_cast<MediaChunkReader *>(input),
                                         path, state, item, completed, progress);
    save->setObjectName(QStringLiteral("psi-sims-save"));
    if (download) {
        QObject::connect(download, &FileShareDownloader::transferCompleted, save, &FileSharingMediaSave::validated);
        QObject::connect(download, &FileShareDownloader::disconnected, save, &FileSharingMediaSave::sourceClosed);
        QObject::connect(download, &FileShareDownloader::failed, save,
                         [save, download] { save->fail(download->errorString()); });
    } else if (auto reader = qobject_cast<MediaChunkReader *>(input)) {
        QObject::connect(reader, &MediaChunkReader::disconnected, save, [save] {
            save->validated();
            save->sourceClosed();
        });
        QObject::connect(reader, &MediaChunkReader::failed, save,
                         [save, reader] { save->fail(reader->errorString()); });
    }
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
