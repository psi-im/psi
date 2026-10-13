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

#include <iris/jingle-session.h>
#include <iris/xmpp-im/jingle-sxe.h>
#include <iris/xmpp_caps.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_message.h>

#include <QUuid>

using namespace XMPP;
namespace SXE = XMPP::Jingle::SXE;
using Action  = XMPP::Jingle::MessageInitiation::Action;

WbManager::WbManager(XMPP::Client *client, PsiAccount *pa) : QObject(nullptr), client_(client)
{
    Q_UNUSED(pa)
    connect(client_->jingleManager(), &Jingle::Manager::incomingMessageInitiation, this,
            &WbManager::incomingMessageInitiation);
}

WbManager::~WbManager()
{
    // Withdraw what is still ringing; accepted proposals end with their sessions.
    for (const auto &p : std::as_const(proposals_)) {
        if (!p.finished && !p.accepted)
            send(p, p.incoming ? Action::Reject : Action::Retract, QStringLiteral("cancel"));
    }
}

bool WbManager::checkSupport(const Jid &jid) const
{
    // XEP-0284 and the whiteboard profile: the peer must support the application type and the SXE transport.
    const auto features = client_->capsManager()->features(jid);
    return features.test(QStringList { XMPP::Jingle::NS, SXE::NS, SXE::NS_TRANSPORT, SXE::NS_SVG_WHITEBOARD });
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

    // The session of an accepted proposal: its sid is the proposal id, and it comes from the resource
    // that accepted.
    if (auto p = proposal(session->sid()); p && p->incoming && p->accepted && !p->finished) {
        if (!session->peer().compare(p->peer, false)) {
            session->terminate(XMPP::Jingle::Reason::UnsupportedApplications);
            return true;
        }
        emit proposalSession(p->id, application);
        return true;
    }

    Request request;
    request.id          = nextRequestId_++;
    request.peer        = session->peer();
    request.application = application;
    requests_.append(request);
    const int id = request.id;
    connect(session, &XMPP::Jingle::Session::terminated, this, [this, id] {
        auto it = std::find_if(requests_.begin(), requests_.end(), [id](const Request &r) { return r.id == id; });
        if (it != requests_.end()) {
            requests_.erase(it);
            emit requestCancelled(id);
        }
    });
    emit wbRequest(session->peer(), request.id);
    return true;
}

WbManager::Request WbManager::takeRequest(int id)
{
    auto it = std::find_if(requests_.begin(), requests_.end(), [id](const Request &r) { return r.id == id; });
    if (it == requests_.end())
        return {};
    auto request = *it;
    requests_.erase(it);
    if (!request.proposalId.isEmpty()) {
        auto p = proposal(request.proposalId);
        if (!p || p->finished)
            return {};
        return request;
    }
    // The peer may have given up meanwhile.
    if (!request.application || request.application->pad()->session()->state() >= XMPP::Jingle::State::Finishing)
        return {};
    return request;
}

QString WbManager::propose(const Jid &jid)
{
    auto jingle = client_->jingleManager();
    if (!jingle->messageInitiationEnabled() || !jid.isValid())
        return {};
    Proposal p;
    p.id   = QUuid::createUuid().toString(QUuid::WithoutBraces);
    p.peer = jid.withResource(QString());
    XMPP::Jingle::MessageInitiation initiation(Action::Propose, p.id);
    initiation.addDescription(SXE::NS_SVG_WHITEBOARD, SXE::Proposal {});
    if (!jingle->sendMessageInitiation(p.peer, initiation))
        return {};
    proposals_.append(p);
    return p.id;
}

void WbManager::retract(const QString &id)
{
    auto p = proposal(id);
    if (!p || p->incoming || p->finished)
        return;
    if (!p->accepted)
        send(*p, Action::Retract, QStringLiteral("cancel"));
    p->finished = true;
}

bool WbManager::proceed(const QString &id)
{
    auto p = proposal(id);
    if (!p || !p->incoming || p->accepted || p->finished || !send(*p, Action::Proceed))
        return false;
    p->accepted = true;
    return true;
}

void WbManager::reject(const QString &id)
{
    auto p = proposal(id);
    if (!p || !p->incoming || p->finished)
        return;
    send(*p, p->accepted ? Action::Finish : Action::Reject, QStringLiteral("decline"));
    p->finished = true;
}

void WbManager::finish(const QString &id, bool success)
{
    auto p = proposal(id);
    if (!p || p->finished)
        return;
    if (p->accepted)
        send(*p, Action::Finish, success ? QStringLiteral("success") : QStringLiteral("expired"));
    p->finished = true;
}

void WbManager::incomingMessageInitiation(const XMPP::Message &message, const XMPP::Jingle::MessageInitiation &initiation)
{
    const bool own = message.from().compare(client_->jid(), false);
    auto       p   = proposal(initiation.id());

    switch (initiation.action()) {
    case Action::Propose: {
        if (own || p || message.spooled())
            return;
        // Only whiteboard-only proposals: calls are handled by AvCallManager; mixed proposals are not
        // supported yet.
        const auto descriptions = initiation.descriptions();
        if (descriptions.size() != 1 || descriptions.first().applicationNamespace != SXE::NS_SVG_WHITEBOARD)
            return;
        Proposal incoming;
        incoming.id       = initiation.id();
        incoming.peer     = message.from();
        incoming.incoming = true;
        proposals_.append(incoming);
        send(incoming, Action::Ringing);

        Request request;
        request.id         = nextRequestId_++;
        request.peer       = message.from();
        request.proposalId = incoming.id;
        requests_.append(request);
        emit wbRequest(message.from(), request.id);
        return;
    }
    case Action::Ringing:
        if (p && !p->incoming && !p->finished && !own && message.from().compare(p->peer, false))
            emit proposalRinging(p->id);
        return;
    case Action::Proceed:
        if (!p || p->finished)
            return;
        if (p->incoming) {
            // Another of our devices accepted.
            if (own && !p->accepted) {
                p->finished = true;
                cancelRequest(p->id);
                emit proposalClosed(p->id, tr("Answered on another device."));
            }
            return;
        }
        if (!own && !p->accepted && message.from().compare(p->peer, false)) {
            p->accepted = true;
            p->peer     = message.from();
            emit proposalAccepted(p->id, p->peer);
        }
        return;
    case Action::Reject:
        if (!p || p->finished)
            return;
        if (p->incoming) {
            if (own && !p->accepted) {
                p->finished = true;
                cancelRequest(p->id);
                emit proposalClosed(p->id, tr("Declined on another device."));
            }
            return;
        }
        // With several devices, one of them declining is enough, as for calls.
        if (!own && !p->accepted && message.from().compare(p->peer, false)) {
            p->finished = true;
            emit proposalClosed(p->id, tr("The whiteboard invitation was declined."));
        }
        return;
    case Action::Retract:
        if (p && p->incoming && !p->finished && !own && message.from().compare(p->peer, false)) {
            p->finished = true;
            cancelRequest(p->id);
            emit proposalClosed(p->id, tr("The whiteboard invitation was withdrawn."));
        }
        return;
    case Action::Finish:
        if (p && !p->finished && (own || message.from().compare(p->peer, false))) {
            p->finished = true;
            if (p->incoming)
                cancelRequest(p->id);
            emit proposalClosed(p->id, tr("The whiteboard session has ended."));
        }
        return;
    case Action::None:
        return;
    }
}

WbManager::Proposal *WbManager::proposal(const QString &id)
{
    if (id.isEmpty())
        return nullptr;
    for (auto &p : proposals_)
        if (p.id == id)
            return &p;
    return nullptr;
}

void WbManager::cancelRequest(const QString &proposalId)
{
    auto it = std::find_if(requests_.begin(), requests_.end(),
                           [&proposalId](const Request &r) { return r.proposalId == proposalId; });
    if (it == requests_.end())
        return;
    const int id = it->id;
    requests_.erase(it);
    emit requestCancelled(id);
}

bool WbManager::send(const Proposal &p, Action action, const QString &condition, const QString &text)
{
    XMPP::Jingle::MessageInitiation initiation(action, p.id);
    if (!condition.isEmpty())
        initiation.setReason(condition, text);
    return client_->jingleManager()->sendMessageInitiation(p.peer, initiation);
}
