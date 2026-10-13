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
#include <QSet>

#include <iris/xmpp-im/xmpp_jinglemessage.h>
#include <iris/xmpp.h>

namespace XMPP {
class Client;
class Message;
namespace Jingle {
    class Session;
    namespace SXE {
        class Application;
    }
}
}
class PsiAccount;

/*! \brief Incoming whiteboard sessions and proposals, and whiteboard support of contacts.
 *  Whiteboards are Jingle sessions with the SVG whiteboard application type,
 *  synchronized with Shared XML Editing (XEP-0284). They are shown in the call
 *  window (CallDlg); the manager keeps incoming sessions and proposals until the
 *  user opens their event.
 *
 *  Proposals (XEP-0353) ring all resources of the contact: the one that answers
 *  with <proceed/> gets the session, whose sid is the proposal id.
 *
 *  One instance per PsiAccount should be created.
 */
class WbManager : public QObject {
    Q_OBJECT

public:
    /*! \brief An incoming invitation: a session, or a proposal that has no session yet.*/
    struct Request {
        int                                       id = 0;
        XMPP::Jid                                 peer;
        QPointer<XMPP::Jingle::SXE::Application> application;
        QString                                   proposalId;
    };

    WbManager(XMPP::Client *client, PsiAccount *pa);
    ~WbManager() override;

    /*! \brief Takes an incoming Jingle session if it is a whiteboard. Returns false otherwise.*/
    bool incomingSession(XMPP::Jingle::Session *session);
    /*! \brief Takes the incoming request with \a id. Returns a request without peer if it is gone.*/
    Request takeRequest(int id);
    /*! \brief Returns true if \a jid advertises whiteboarding over Jingle SXE.*/
    bool checkSupport(const XMPP::Jid &jid) const;

    /*! \brief Proposes a whiteboard to all resources of \a jid. Returns the proposal id or an empty string.*/
    QString propose(const XMPP::Jid &jid);
    /*! \brief Withdraws an outgoing proposal that was not answered yet.*/
    void retract(const QString &id);
    /*! \brief Accepts an incoming proposal; the session follows with proposalSession().*/
    bool proceed(const QString &id);
    /*! \brief Declines an incoming proposal.*/
    void reject(const QString &id);
    /*! \brief Ends the proposal of a session that was started with it.*/
    void finish(const QString &id, bool success);

signals:
    void wbRequest(const XMPP::Jid &peer, int id);
    /*! \brief The incoming request \a id disappeared before it was opened.*/
    void requestCancelled(int id);
    /*! \brief An outgoing proposal is ringing at the contact's devices.*/
    void proposalRinging(const QString &id);
    /*! \brief A resource accepted the outgoing proposal: start the session to \a peer with the proposal id.*/
    void proposalAccepted(const QString &id, const XMPP::Jid &peer);
    /*! \brief The outgoing proposal was declined, or an accepted incoming one was withdrawn or ended.*/
    void proposalClosed(const QString &id, const QString &message);
    /*! \brief The session of an accepted incoming proposal arrived.*/
    void proposalSession(const QString &id, XMPP::Jingle::SXE::Application *whiteboard);

private:
    struct Proposal {
        QString   id;
        XMPP::Jid peer; // bare JID before the answer, then the answering resource
        bool      incoming  = false;
        bool      accepted  = false;
        bool      finished  = false;
    };

    void       incomingMessageInitiation(const XMPP::Message                   &message,
                                         const XMPP::Jingle::MessageInitiation &initiation);
    Proposal  *proposal(const QString &id);
    void       cancelRequest(const QString &proposalId);
    bool       send(const Proposal &proposal, XMPP::Jingle::MessageInitiation::Action action,
                    const QString &condition = {}, const QString &text = {});

    XMPP::Client   *client_;
    QList<Request>  requests_;
    QList<Proposal> proposals_;
    int             nextRequestId_ = 0;
};

#endif // WBMANAGER_H
