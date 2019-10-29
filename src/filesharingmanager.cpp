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
#include "fileutil.h"
#include "httputil.h"
#include "psiaccount.h"
#include "psicon.h"
#ifndef WEBKIT
#include "qiteaudio.h"
#endif

#include "xmpp_client.h"
#include "xmpp_hash.h"
#include "xmpp_jid.h"
#include "xmpp_message.h"
#include "xmpp_reference.h"
#include "xmpp_vcard.h"
#ifdef HAVE_WEBSERVER
#include "filesharinghttpproxy.h"
#include "qhttpserverconnection.hpp"
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
    FileCache *                          cache;
    QHash<XMPP::Hash, FileSharingItem *> items;

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

FileSharingManager::~FileSharingManager() {}

QString FileSharingManager::cacheDir()
{
    QDir shares(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation) + "/shares");
    if (!shares.exists()) {
        QDir home(ApplicationInfo::homeDir(ApplicationInfo::CacheLocation));
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

QList<FileSharingItem *> FileSharingManager::fromMimeData(const QMimeData *data, PsiAccount *acc)
{
    QList<FileSharingItem *> ret;
    QStringList              files;

    QString voiceMsgMime;
    QString voiceAmplitudesMime(QLatin1String("application/x-psi-amplitudes"));

    qDebug() << "FSM items list from clipboard:" << data->formats() << " FILES:" << data->urls();

    bool hasVoice = data->hasFormat(voiceAmplitudesMime);
    if (hasVoice) {
        for (auto const &f : data->formats()) {
            if (f.startsWith("audio/") || f.startsWith("video/")) { // video container may contain just audio
                voiceMsgMime = f;
                break;
            }
        }
        hasVoice = !voiceMsgMime.isEmpty();
    }
    if (data->hasUrls()) {
        for (auto const &url : data->urls()) {
            if (!url.isLocalFile()) {
                continue;
            }
            QFileInfo fi(url.toLocalFile());
            if (fi.isFile() && fi.isReadable()) {
                files.append(fi.filePath());
            }
        }
    }
    if (files.isEmpty() && !data->hasImage() && !hasVoice) {
        return ret;
    }

    if (files.isEmpty()) { // so we have an image
        FileSharingItem *item = nullptr;
        if (hasVoice) {
            QByteArray  ba         = data->data(voiceMsgMime);
            QByteArray  amplitudes = data->data(voiceAmplitudesMime);
            QVariantMap vm;
            vm.insert("amplitudes", amplitudes);
            item = new FileSharingItem(voiceMsgMime.replace("video/", "audio/"), ba, vm, acc, this);
        } else {
            QImage img = data->imageData().value<QImage>();
            item       = new FileSharingItem(img, acc, this);
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
    foreach (const QString &file, fileList) {
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

        int lastEnd = 0;
        for (auto const &r : m.references()) {
            MediaSharing ms = r.mediaSharing();
            // qDebug() << "BEGIN:" << r.begin() << "END:" << r.end();
            // only audio and image supported for now
            if (!ms.isValid() || !ms.file.hasComputedHashes() || !ms.file.hasSize()
                || !(ms.file.mediaType().startsWith(QLatin1String("audio"))
                     || ms.file.mediaType().startsWith(QLatin1String("image")))) {
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
    QList<QPair<Jingle::FileTransfer::Application *, FileCacheItem *>> toAccept;
    // check if we can accept the session immediately w/o user interaction
    for (auto const &a : session->contentList()) {
        auto ft = static_cast<Jingle::FileTransfer::Application *>(a);
        if (a->senders() == Jingle::Origin::Initiator)
            return false;

        FileCacheItem *item = cacheItem(ft->file().computedHashes(), true);
        if (!item)
            return false;

        toAccept.append(qMakePair(ft, item));
    }

    for (auto const &p : toAccept) {
        auto ft   = p.first;
        auto item = p.second;

        connect(ft, &Jingle::FileTransfer::Application::deviceRequested, this,
                [ft, item, this](quint64 offset, quint64 /*size*/) {
                    auto    vm       = item->metadata();
                    QString fileName = vm.value(QString::fromLatin1("link")).toString();
                    if (fileName.isEmpty()) {
                        fileName = d->cache->cacheDir() + "/" + item->fileName();
                    }
                    auto f = new QFile(fileName, ft);
                    if (!f->open(QIODevice::ReadOnly)) {
                        ft->setDevice(nullptr);
                        return;
                    }
                    f->seek(qint64(offset));
                    ft->setDevice(f);
                });
    }
    session->accept();

    return true;
}

#ifdef HAVE_WEBSERVER
// returns true if request handled. false if we need to find another hander
bool FileSharingManager::downloadHttpRequest(PsiAccount *acc, const QString &sourceIdHex,
                                             qhttp::server::QHttpRequest *req, qhttp::server::QHttpResponse *res)
{
    new FileSharingHttpProxy(acc, sourceIdHex, req, res);
    return true;
}
#endif

XMPP::Hash FileSharingDeviceOpener::urlToSourceId(const QUrl &url)
{
    if (url.scheme() != QLatin1String("share"))
        return XMPP::Hash();

    QString    path = url.path();
    QStringRef sourceId(&path);
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
        url = QUrl::fromLocalFile(fsm->cacheDir() + "/" + item->cache()->fileName());
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
    url = localServerUrl;
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
    return acc->psi()->fileSharingManager()->item(XMPP::Hash::from(QStringRef(&id)));
}
