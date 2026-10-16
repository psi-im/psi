/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingpublication.h"
#include <QFile>
#include <QFileInfo>
#include <QPointer>
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
