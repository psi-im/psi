/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingresponder.h"

#include "iris/jingle-ft.h"
#include "iris/jingle-session.h"

#include <QFile>
#include <QPair>
#include <QVector>

using namespace XMPP;

namespace FileSharingResponder {

bool applyProtectionPolicy(Jingle::Session *session, Jingle::EncryptionPolicy policy)
{
    using Policy = Jingle::EncryptionPolicy;
    const auto current = session->encryptionPolicy();
    if (policy == Policy::Disabled || current == Policy::Required || current == policy)
        return true;
    if (session->state() != Jingle::State::Created)
        return false;
    session->setEncryptionPolicy(policy);
    return session->encryptionPolicy() == policy;
}

void attachSource(Jingle::FileTransfer::Application *app, const Jid &peer,
                  const ServedFile &file, const Lookup &lookup)
{
    QObject::connect(app, &Jingle::FileTransfer::Application::deviceRequested, app,
                     [app, peer, file, lookup](quint64 offset, std::optional<quint64>) {
                         const auto current = lookup(peer, file.hashes);
                         if (!current || current->encryption != file.encryption) {
                             app->remove(Jingle::Reason::Decline, QStringLiteral("Shared file unavailable"));
                             return;
                         }
                         auto *device = new QFile(current->path, app);
                         if (!device->open(QIODevice::ReadOnly) || offset > quint64(device->size())
                             || !device->seek(qint64(offset))) {
                             delete device;
                             app->remove(Jingle::Reason::FailedApplication,
                                         QStringLiteral("Shared file range unavailable"));
                             return;
                         }
                         app->setDevice(device);
                     });
}

namespace {
    bool acceptContents(Jingle::Session *session, const Lookup &lookup)
    {
        QVector<QPair<Jingle::FileTransfer::Application *, ServedFile>> toAccept;
        // check if we can accept the session immediately w/o user interaction
        for (auto const &a : session->contentList()) {
            if (!a->isRemote() || a->state() != Jingle::State::Created)
                continue;
            if (a->pad()->ns() != Jingle::FileTransfer::NS || a->senders() != session->role())
                return false;
            auto ft   = static_cast<Jingle::FileTransfer::Application *>(a);
            auto file = lookup(session->peer(), ft->file().computedHashes());
            if (!file)
                return false;

            toAccept.append(qMakePair(ft, *file));
        }

        for (const auto &p : toAccept)
            if (!applyProtectionPolicy(session, p.second.encryption))
                return false;

        for (auto const &p : toAccept) {
            auto ft = p.first; // jingle-ft app

            ft->setKeepTransportUntilReceipt();
            QObject::connect(ft, &Jingle::Application::stateChanged, ft, [ft](Jingle::State state) {
                if (state == Jingle::State::Finished)
                    ft->deleteLater();
            });

            auto acceptFile = ft->file();
            acceptFile.setHashes(p.second.hashes);
            ft->setAcceptFile(acceptFile);

            // Range hashes are negotiated by Iris. The catalog grant is
            // resolved again when the transport is ready to request bytes.
            attachSource(ft, session->peer(), p.second, lookup);
        }
        if (session->state() != Jingle::State::Created)
            for (const auto &p : toAccept)
                p.first->prepare();

        return true;
    }
}

bool answer(Jingle::Session *session, const Lookup &lookup)
{
    if (acceptContents(session, lookup)) {
        QObject::connect(session, &Jingle::Session::newContentReceived, session, [session, lookup] {
            if (acceptContents(session, lookup))
                return;
            // Only reject the new proposals. Existing accepted transfers continue.
            for (auto *app : session->contentList())
                if (app->isRemote() && app->state() == Jingle::State::Created)
                    app->remove(Jingle::Reason::Decline, QStringLiteral("Shared file unavailable"));
        });
        session->accept();
        return true;
    }
    // make sure there are only file offers
    for (auto const &a : session->contentList()) {
        if (a->senders() != Jingle::Origin::Initiator) {
            session->terminate(Jingle::Reason::FailedApplication, "file request failed, so expected only file offer");
            return true;
        }
    }
    return false;
}
}
