/*
 * wbmanager.cpp - Whiteboard manager
 * Copyright (C) 2007  Joonas Govenius
 *
 * Influenced by:
 * pepmanager.cpp - Classes for PEP
 * Copyright (C) 2006  Remko Troncon
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

#include "wbmanager.h"

#include "psiaccount.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <iris/jingle-session.h>
#include <iris/xmpp_caps.h>
#include <iris/xmpp_client.h>

using namespace XMPP;
namespace SXE = XMPP::Jingle::SXE;

WbManager::WbManager(XMPP::Client *client, PsiAccount *pa) : client_(client), pa_(pa) { }

WbManager::~WbManager() { qDeleteAll(dialogs_); }

bool WbManager::checkSupport(const Jid &jid) const
{
    // XEP-0284 and the whiteboard profile: the peer must support the application type and the SXE transport.
    const auto features = client_->capsManager()->features(jid);
    return features.test(QStringList { XMPP::Jingle::NS, SXE::NS, SXE::NS_TRANSPORT, SXE::NS_SVG_WHITEBOARD });
}

void WbManager::openWhiteboard(const Jid &target, const Jid &ownJid, bool groupChat, bool promptInitialDoc)
{
    Q_UNUSED(ownJid)
    if (groupChat) {
        QMessageBox::information(nullptr, tr("Unsupported"), tr("Group chat whiteboards are not supported yet."));
        return;
    }
    if (!checkSupport(target)) {
        QMessageBox::information(nullptr, tr("Unsupported"), tr("The contact does not support whiteboarding."));
        return;
    }

    if (auto dialog = findWbDlg(target)) {
        bringToFront(dialog);
        return;
    }

    QDomDocument doc;
    if (promptInitialDoc
        && QMessageBox::question(nullptr, tr("Open Existing SVG?"),
                                 tr("Would you like to open an existing SVG document in the whiteboard?"),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            == QMessageBox::Yes) {
        const QString fileName = QFileDialog::getOpenFileName(nullptr, tr("Initial SVG Document for the Whiteboard"),
                                                              QDir::homePath(), tr("Scalable Vector Graphics (*.svg)"));
        QFile         file(fileName);
        if (file.open(QIODevice::ReadOnly)) {
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
            doc.setContent(&file, true);
#else
            doc.setContent(&file, QDomDocument::ParseOption::UseNamespaceProcessing);
#endif
        }
    }

    auto session     = client_->jingleManager()->newSession(target);
    auto application = qobject_cast<SXE::Application *>(session->newContent(SXE::NS_SVG_WHITEBOARD));
    if (!application) {
        session->terminate(XMPP::Jingle::Reason::FailedApplication);
        return;
    }
    // Without a user document the application starts from the empty whiteboard of the profile.
    if (doc.documentElement().localName() == QLatin1String("svg"))
        application->setInitialDocument(doc);
    session->addContent(application);
    createWbDlg(application);
    session->initiate();
}

bool WbManager::incomingSession(XMPP::Jingle::Session *session)
{
    if (!session || session->preferredApplication() != SXE::NS_SVG_WHITEBOARD)
        return false;

    SXE::Application *application = nullptr;
    for (auto content : session->contentList()) {
        auto whiteboard = qobject_cast<SXE::Application *>(content);
        if (!whiteboard || application) {
            session->terminate(XMPP::Jingle::Reason::UnsupportedApplications,
                               QStringLiteral("a whiteboard session has exactly one whiteboard content"));
            return true;
        }
        application = whiteboard;
    }
    if (!application) {
        session->terminate(XMPP::Jingle::Reason::UnsupportedApplications);
        return true;
    }

    Request request;
    request.id          = nextRequestId_++;
    request.session     = session;
    request.application = application;
    requests_.append(request);
    emit wbRequest(session->peer(), request.id);
    return true;
}

void WbManager::requestActivated(int id)
{
    auto it = std::find_if(requests_.begin(), requests_.end(), [id](const Request &r) { return r.id == id; });
    if (it == requests_.end())
        return;
    const auto request = *it;
    requests_.erase(it);
    if (!request.session || !request.application || request.session->state() >= XMPP::Jingle::State::Finishing)
        return; // the peer gave up meanwhile

    const auto answer
        = QMessageBox::question(nullptr, tr("Whiteboarding Invitation?"),
                                tr("%1 has invited you to a whiteboarding session. Would you like to join?")
                                    .arg(request.session->peer().full()),
                                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    // The modal question runs an event loop: the session may have ended meanwhile.
    if (!request.session || !request.application || request.session->state() >= XMPP::Jingle::State::Finishing)
        return;
    if (answer != QMessageBox::Yes) {
        request.session->terminate(XMPP::Jingle::Reason::Decline);
        return;
    }
    createWbDlg(request.application);
    request.session->accept();
}

void WbManager::removeDialog(WbDlg *dialog)
{
    dialogs_.removeAll(dialog);
    dialog->deleteLater();
}

WbDlg *WbManager::findWbDlg(const Jid &jid)
{
    for (WbDlg *w : std::as_const(dialogs_)) {
        if (w->application() && w->peer().compare(jid))
            return w;
    }
    return nullptr;
}

WbDlg *WbManager::createWbDlg(SXE::Application *application)
{
    auto w = new WbDlg(application, pa_);
    connect(w, &WbDlg::sessionEnded, this, &WbManager::removeDialog);
    dialogs_.append(w);
    bringToFront(w);
    return w;
}
