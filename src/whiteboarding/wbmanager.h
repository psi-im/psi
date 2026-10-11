/*
 * wbmanager.h - Whiteboard manager
 * Copyright (C) 2007  Joonas Govenius
 *
 * Influenced by:
 * pepmanager.h - Classes for PEP
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

#ifndef WBMANAGER_H
#define WBMANAGER_H

#include <QList>
#include <QObject>
#include <QPointer>

namespace XMPP {
class Client;
class Jid;
namespace Jingle {
    class Session;
    namespace SXE {
        class Application;
    }
}
}
class PsiAccount;

/*! \brief Incoming whiteboard sessions and whiteboard support of contacts.
 *  Whiteboards are Jingle sessions with the SVG whiteboard application type,
 *  synchronized with Shared XML Editing (XEP-0284). They are shown in the call
 *  window (CallDlg); the manager keeps incoming sessions until the user opens
 *  their event.
 *
 *  One instance per PsiAccount should be created.
 */
class WbManager : public QObject {
    Q_OBJECT

public:
    WbManager(XMPP::Client *client, PsiAccount *pa);
    ~WbManager() override;

    /*! \brief Takes an incoming Jingle session if it is a whiteboard. Returns false otherwise.*/
    bool incomingSession(XMPP::Jingle::Session *session);
    /*! \brief Returns the whiteboard of the incoming request with \a id, or null if the session is gone.*/
    XMPP::Jingle::SXE::Application *takeRequest(int id);
    /*! \brief Returns true if \a jid advertises whiteboarding over Jingle SXE.*/
    bool checkSupport(const XMPP::Jid &jid) const;

signals:
    void wbRequest(const XMPP::Jid &peer, int id);

private:
    struct Request {
        int                                       id = 0;
        QPointer<XMPP::Jingle::SXE::Application> application;
    };

    XMPP::Client  *client_;
    QList<Request> requests_;
    int            nextRequestId_ = 0;
};

#endif // WBMANAGER_H
