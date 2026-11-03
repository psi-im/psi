/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingresponder.h"

#include "iris/jingle-ft.h"
#include "iris/jingle-session.h"

#include <QFile>
#include <QPair>
#include <QVector>

using namespace XMPP;

namespace FileSharingResponder {
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
            auto file = lookup(ft->file().computedHashes());
            if (!file)
                return false;

            toAccept.append(qMakePair(ft, *file));
        }

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

            // TODO hashes for ranges
            // The path was taken before waiting for the transport. Cache
            // maintenance may retire the cache item while a content-add waits.
            QObject::connect(ft, &Jingle::FileTransfer::Application::deviceRequested, ft,
                             [ft, fileName = p.second.path](quint64 offset, std::optional<quint64> /*size*/) {
                                 auto f = new QFile(fileName, ft);
                                 if (!f->open(QIODevice::ReadOnly)) {
                                     const auto error = f->errorString();
                                     delete f;
                                     ft->remove(Jingle::Reason::FailedApplication, error);
                                     return;
                                 }
                                 if (offset > quint64(f->size()) || !f->seek(qint64(offset))) {
                                     delete f;
                                     ft->remove(Jingle::Reason::FailedApplication,
                                                QStringLiteral("Shared file range unavailable"));
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
