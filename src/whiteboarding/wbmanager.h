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

#include "wbdlg.h"

#include <QList>
#include <QPointer>

namespace XMPP {
class Client;
class Jid;
namespace Jingle {
    class Session;
}
}
class PsiAccount;
using namespace XMPP;

/*! \brief The manager for whiteboard dialogs.
 *  Whiteboards are Jingle sessions with the SVG whiteboard application type,
 *  synchronized with Shared XML Editing (XEP-0284). The manager starts
 *  outgoing sessions, presents incoming ones and creates a dialog for each.
 *
 *  One instance per PsiAccount should be created.
 *
 *  \sa WbDlg
 */
class WbManager : public QObject {
    Q_OBJECT

public:
    WbManager(XMPP::Client *client, PsiAccount *pa);
    ~WbManager();

    /*! \brief Takes an incoming Jingle session if it is a whiteboard. Returns false otherwise.*/
    bool incomingSession(XMPP::Jingle::Session *session);
    /*! \brief Asks the user about the incoming whiteboard request with \a id.*/
    void requestActivated(int id);
    /*! \brief Returns true if \a jid advertises whiteboarding over Jingle SXE.*/
    bool checkSupport(const Jid &jid) const;

public slots:
    /*! \brief Opens the existing dialog to the specified contact or starts a new session if necessary.*/
    void openWhiteboard(const Jid &target, const Jid &ownJid, bool groupChat, bool promptInitialDoc = true);

signals:
    void wbRequest(const Jid &peer, int id);

private:
    struct Request {
        int                                id = 0;
        QPointer<XMPP::Jingle::Session>    session;
        QPointer<Jingle::SXE::Application> application;
    };

    /*! \brief Returns the dialog of an active session with the contact, if any.*/
    WbDlg *findWbDlg(const Jid &target);
    /*! \brief Creates and shows a dialog for the whiteboard content.*/
    WbDlg *createWbDlg(Jingle::SXE::Application *application);

    XMPP::Client  *client_;
    PsiAccount    *pa_;
    QList<WbDlg *> dialogs_;
    QList<Request> requests_;
    int            nextRequestId_ = 0;

private slots:
    /*! \brief Removes and deletes the dialog for the given session.*/
    void removeDialog(WbDlg *dialog);
};

#endif // WBMANAGER_H
