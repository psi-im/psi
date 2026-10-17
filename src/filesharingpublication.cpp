/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingpublication.h"
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <iris/httpfileupload.h>
#include <iris/jingle-ft.h>
#include <iris/jingle-pub.h>
#include <iris/jingle-session.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_pubsub.h>

bool publishSharedFile(XMPP::Client *client, const XMPP::Jingle::FileTransfer::File &file, const QString &path,
                       QObject *context, std::function<void(bool, const QString &)> announced)
{
    if (!client || !file.hasComputedHashes() || !QFileInfo(path).isReadable())
        return false;
    namespace J   = XMPP::Jingle;
    auto *manager = client->jingleManager();
    if (!manager || !manager->isRegisteredApplication(J::FileTransfer::NS))
        return false;
    J::JinglePub publication;
    publication.setFrom(client->jid());
    publication.setId(QStringLiteral("psi-file-") + file.computedHashes().first().toString());
    manager->unregisterPublishedSession(publication.id());
    QDomDocument doc;
    auto         description = doc.createElementNS(J::FileTransfer::NS, QStringLiteral("description"));
    description.appendChild(file.toXml(&doc));
    publication.addDescription(description);
    const QPointer<J::Manager> guard(manager);
    publication = manager->registerPublishedSession(publication, [guard, file, path](const XMPP::Jid &peer) {
        if (!guard || !QFileInfo(path).isReadable())
            return static_cast<J::Session *>(nullptr);
        auto *session = guard->newSession(peer);
        auto *app
            = static_cast<J::FileTransfer::Application *>(session->newContent(J::FileTransfer::NS, session->role()));
        if (!app) {
            session->deleteLater();
            return static_cast<J::Session *>(nullptr);
        }
        app->setFile(file);
        QObject::connect(app, &J::FileTransfer::Application::deviceRequested, app,
                         [app, path](quint64 offset, std::optional<quint64>) {
                             auto *device = new QFile(path, app);
                             if (!device->open(QIODevice::ReadOnly) || offset > quint64(device->size())
                                 || !device->seek(qint64(offset))) {
                                 const auto error = device->errorString();
                                 delete device;
                                 app->remove(J::Reason::FailedApplication, error);
                                 return;
                             }
                             app->setDevice(device);
                         });
        session->addContent(app);
        return session;
    });
    if (!publication.isValid())
        return false;

    // A shared private node avoids advertising file names to the entire roster.
    // Create explicitly: PubSub auto-creation is not universally available.
    const QString             node = QStringLiteral("urn:psi:shared-files:1");
    const XMPP::PubSubOptions options { { QStringLiteral("pubsub#access_model"), { QStringLiteral("whitelist") } },
                                        { QStringLiteral("pubsub#persist_items"), { QStringLiteral("1") } },
                                        { QStringLiteral("pubsub#max_items"), { QStringLiteral("1000") } },
                                        { QStringLiteral("pubsub#deliver_notifications"), { QStringLiteral("0") } } };
    auto                     *create = client->pubSubManager()->createNode(client->jid().bare(), node, options);
    QObject::connect(
        create, &XMPP::Task::finished, context,
        [guard, create, node, id = publication.id(), announced = std::move(announced), context]() {
            if (!guard)
                return;
            if (!create->success() && create->error().condition != XMPP::Stanza::Error::ErrorCond::Conflict) {
                announced(false, create->statusString());
                return;
            }
            auto *task
                = guard->publicationManager()->publishSessionAnnouncement(guard->client()->jid().bare(), node, id);
            if (!task) {
                announced(false, QStringLiteral("Jingle publication is unavailable"));
                return;
            }
            QObject::connect(task, &XMPP::Task::finished, context,
                             [task, announced]() { announced(task->success(), task->statusString()); });
            task->go(true);
        });
    create->go(true);
    return true;
}

static bool isHttpUrl(const QString &source)
{
    const QUrl url(source);
    return url.isValid() && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"));
}

FileSharingPublication::FileSharingPublication(XMPP::Client *client, const XMPP::Jingle::FileTransfer::File &file,
                                               const QString &path, const XMPP::Jid &selfJid, QObject *parent) :
    QObject(parent), _client(client), _file(file), _path(path), _selfJid(selfJid)
{
    connect(client, &QObject::destroyed, this, &QObject::deleteLater);
}

void FileSharingPublication::start(const QStringList &cachedSources, bool serveLocally)
{
    if (_started || !_client)
        return;
    _started = true;
    const bool registered
        = serveLocally && publishSharedFile(_client, _file, _path, this, [this](bool success, const QString &error) {
              emit logMessage(success ? tr("Published in the private PubSub catalog")
                                      : tr("PubSub publication failed: %1").arg(error));
              // Local hash requests still work when the private catalog is unavailable.
              _jingleFinished = true;
              checkFinished();
          });
    if (registered)
        _sources.append(QStringLiteral("xmpp:%1?jingle-ft").arg(_selfJid.full()));
    else
        _jingleFinished = true;

    checkHttpSources(cachedSources);
}

void FileSharingPublication::checkHttpSources(const QStringList &sources)
{
    QStringList candidates;
    for (const auto &source : sources) {
        if (isHttpUrl(source))
            candidates.append(source);
    }
    candidates.removeDuplicates();
    auto *nam = _client->networkAccessManager();
    if (candidates.isEmpty() || !nam) {
        upload();
        return;
    }

    _pendingHeads = candidates.size();
    for (const auto &source : std::as_const(candidates)) {
        QNetworkRequest request { QUrl(source) };
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
        request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        auto *reply = nam->head(request);
        connect(this, &QObject::destroyed, reply, &QObject::deleteLater);
        auto *timeout = new QTimer(reply);
        timeout->setSingleShot(true);
        connect(timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
        timeout->start(15000);
        connect(reply, &QNetworkReply::finished, this, [this, reply, source, timeout]() {
            timeout->stop();
            reply->deleteLater();
            if (!_client)
                return;
            const int  status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            bool       sizeOk = true;
            const auto length = reply->header(QNetworkRequest::ContentLengthHeader);
            if (length.isValid() && _file.size()) {
                bool       ok   = false;
                const auto size = length.toULongLong(&ok);
                sizeOk          = ok && size == *_file.size();
            }
            if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300 && sizeOk)
                _sources.append(source);
            if (--_pendingHeads)
                return;
            for (const auto &uri : std::as_const(_sources)) {
                if (isHttpUrl(uri)) {
                    emit logMessage(tr("Reusing an HTTP upload after checking its availability"));
                    _httpFinished = true;
                    checkFinished();
                    return;
                }
            }
            upload();
        });
    }
}

void FileSharingPublication::upload()
{
    if (!_client)
        return;
    auto *manager = _client->httpFileUploadManager();
    if (manager->discoveryStatus() == XMPP::HttpFileUploadManager::DiscoNotFound) {
        _httpFinished = true;
        checkFinished();
        return;
    }
    auto *uploader = manager->upload(_path, _file.name(), _file.mediaType());
    if (!uploader) {
        emit logMessage(tr("Could not open the file for HTTP upload"));
        _httpFinished = true;
        checkFinished();
        return;
    }
    // Keep the uploader owned by its Client so account teardown cancels it.
    connect(this, &QObject::destroyed, uploader, &QObject::deleteLater);
    connect(uploader, &XMPP::HttpFileUpload::finished, uploader, &QObject::deleteLater);
    connect(uploader, &XMPP::HttpFileUpload::progress, this, [this](qint64 bytes, qint64) { emit progress(bytes); });
    connect(uploader, &XMPP::HttpFileUpload::finished, this, [this, uploader]() {
        if (!_client)
            return;
        if (uploader->success()) {
            _sources.prepend(uploader->getHttpSlot().get.url);
            emit logMessage(tr("Published on HttpUpload service"));
        } else {
            emit logMessage(tr("Failed to publish on HttpUpload service: %1").arg(uploader->statusString()));
        }
        _httpFinished = true;
        checkFinished();
    });
}

void FileSharingPublication::checkFinished()
{
    if (!_notified && _httpFinished && _jingleFinished) {
        _notified = true;
        emit finished();
    }
}

XMPP::UrlList fileSharingHttpUrls(const QList<XMPP::Reference> &references)
{
    XMPP::UrlList urls;
    QStringList   seen;
    for (const auto &reference : references) {
        if (!reference.isValid() || !reference.mediaSharing().isValid())
            continue;
        for (const auto &source : reference.mediaSharing().sources) {
            if (isHttpUrl(source) && !seen.contains(source)) {
                urls.append(XMPP::Url(source));
                seen.append(source);
            }
        }
    }
    return urls;
}
