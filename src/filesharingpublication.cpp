/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingpublication.h"
#include "aesgcmurl.h"
#include "filesourceselector.h"
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <QUuid>
#include <iris/httpfileupload.h>
#include <iris/jingle-ft.h>
#include <iris/jingle-pub.h>
#include <iris/jingle-session.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_pubsub.h>

bool publishSharedFile(XMPP::Client *client, const XMPP::Jingle::FileTransfer::File &file,
                       const FileSharingResponder::Lookup &lookup,
                       QObject *context, std::function<void(bool, const QString &)> announced)
{
    if (!client || !file.hasComputedHashes() || !lookup)
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
    publication = manager->registerPublishedSession(publication, [guard, file, lookup](const XMPP::Jid &peer) {
        if (!guard)
            return static_cast<J::Session *>(nullptr);
        const auto grant = lookup(peer, file.computedHashes());
        if (!grant || !QFileInfo(grant->path).isReadable())
            return static_cast<J::Session *>(nullptr);
        auto *session = guard->newSession(peer);
        if (!FileSharingResponder::applyProtectionPolicy(session, grant->encryption)) {
            session->deleteLater();
            return static_cast<J::Session *>(nullptr);
        }
        auto *app
            = static_cast<J::FileTransfer::Application *>(session->newContent(J::FileTransfer::NS, session->role()));
        if (!app) {
            session->deleteLater();
            return static_cast<J::Session *>(nullptr);
        }
        app->setFile(file);
        FileSharingResponder::attachSource(app, peer, *grant, lookup);
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
                                               const QString &path, const XMPP::Jid &selfJid, QObject *parent,
                                               Mode mode) :
    QObject(parent), _client(client), _file(file), _path(path), _selfJid(selfJid), _mode(mode)
{
    connect(client, &QObject::destroyed, this, &QObject::deleteLater);
}

void FileSharingPublication::start(const QStringList &cachedSources, const FileSharingResponder::Lookup &lookup)
{
    if (_started || !_client)
        return;
    _started = true;
    // Legacy Jingle and cached HTTP sources have no protected publication
    // identity. Until scoped Jingle sources are negotiated, exclude them
    // from an encrypted publication.
    const bool legacySourcesAllowed = _mode == Mode::Plain;
    const bool registered
        = legacySourcesAllowed && lookup && publishSharedFile(_client, _file, lookup, this, [this](bool success, const QString &error) {
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

    checkHttpSources(legacySourcesAllowed ? cachedSources : QStringList {});
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
    namespace S = XMPP::StatelessFileSharing;
    const auto complete = [this](const QString &error = QString()) {
        _errorString = error;
        if (!error.isEmpty())
            emit logMessage(error);
        _httpFinished = true;
        checkFinished();
    };
    if (!_client) {
        complete(tr("The account is unavailable"));
        return;
    }
    auto *manager = _client->httpFileUploadManager();
    if (manager->discoveryStatus() == XMPP::HttpFileUploadManager::DiscoNotFound) {
        complete(tr("HTTP upload service is unavailable"));
        return;
    }

    XMPP::HttpFileUpload *uploader = nullptr;
    if (_mode == Mode::Aes256Gcm) {
        if (!_file.size() || !S::cipherSupported(S::Cipher::Aes256Gcm)) {
            complete(tr("Encrypted file publication is unavailable"));
            return;
        }
        const auto wireSize = S::encryptedSize(S::Cipher::Aes256Gcm, *_file.size());
        if (!wireSize) {
            complete(tr("The file is too large for encrypted HTTP upload"));
            return;
        }
        _plaintext = new QFile(_path, this);
        _ciphertext = new S::EncryptingDevice(_plaintext, S::Cipher::Aes256Gcm, this);
        if (!_plaintext->open(QIODevice::ReadOnly) || !_ciphertext->open(QIODevice::ReadOnly)) {
            complete(tr("Cannot read the file for encrypted upload"));
            return;
        }
        // The slot request must not reveal the original name or media type.
        const QString name = QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".bin");
        uploader = manager->upload(_ciphertext, *wireSize, name, QStringLiteral("application/octet-stream"));
    } else {
        uploader = manager->upload(_path, _file.name(), _file.mediaType());
    }

    if (!uploader) {
        complete(tr("Could not open the file for HTTP upload"));
        return;
    }
    connect(this, &QObject::destroyed, uploader, &QObject::deleteLater);
    connect(uploader, &XMPP::HttpFileUpload::finished, uploader, &QObject::deleteLater);
    connect(uploader, &XMPP::HttpFileUpload::progress, this, [this](qint64 bytes, qint64) { emit progress(bytes); });
    connect(uploader, &XMPP::HttpFileUpload::finished, this, [this, uploader, complete]() {
        if (!_client)
            return;
        if (!uploader->success() || (_ciphertext && !_ciphertext->finished())) {
            const auto status = uploader->statusString();
            complete(status.isEmpty() ? tr("HTTP upload did not finish") : tr("HTTP upload failed: %1").arg(status));
            return;
        }
        QString source = uploader->getHttpSlot().get.url;
        if (_ciphertext) {
            QUrl url(source);
            if (!url.isValid() || url.scheme() != QLatin1String("https") || url.host().isEmpty()) {
                complete(tr("Encrypted file sharing requires an HTTPS download URL"));
                return;
            }
            url.setScheme(QStringLiteral("aesgcm"));
            url.setFragment(QString::fromLatin1((_ciphertext->iv() + _ciphertext->key()).toHex()));
            if (!AesGcmUrl::parse(url)) {
                complete(tr("The encrypted HTTP URL is invalid"));
                return;
            }
            source = url.toString(QUrl::FullyEncoded);
        }
        _sources.prepend(source);
        emit logMessage(tr("Published on HttpUpload service"));
        complete();
    });
}

XMPP::Reference fileSharingReference(const XMPP::Jingle::FileTransfer::File &file, const QStringList &sources)
{
    auto uris = FileSourceSelector::prioritized(sources);
    uris.removeDuplicates();
    std::reverse(uris.begin(), uris.end());
    if (uris.isEmpty())
        return {};
    XMPP::Reference reference(XMPP::Reference::Data, uris.first());
    XMPP::MediaSharing sharing;
    sharing.file = file;
    sharing.sources = uris;
    reference.setMediaSharing(sharing);
    return reference;
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
