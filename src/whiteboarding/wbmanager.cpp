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

using namespace XMPP;
namespace SXE = XMPP::Jingle::SXE;

WbManager::WbManager(XMPP::Client *client, PsiAccount *pa) : QObject(nullptr), client_(client) { Q_UNUSED(pa) }

WbManager::~WbManager() = default;

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

    Request request;
    request.id          = nextRequestId_++;
    request.application = application;
    requests_.append(request);
    emit wbRequest(session->peer(), request.id);
    return true;
}

SXE::Application *WbManager::takeRequest(int id)
{
    auto it = std::find_if(requests_.begin(), requests_.end(), [id](const Request &r) { return r.id == id; });
    if (it == requests_.end())
        return nullptr;
    const auto application = it->application;
    requests_.erase(it);
    // The peer may have given up meanwhile.
    if (!application || application->pad()->session()->state() >= XMPP::Jingle::State::Finishing)
        return nullptr;
    return application;
}
