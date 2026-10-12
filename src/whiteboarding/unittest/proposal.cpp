// SPDX-License-Identifier: GPL-2.0-or-later
// Whiteboard proposals (XEP-0353) between two WbManagers over looped-back client streams.
#include "../wbmanager.h"

#include <iris/jingle-session.h>
#include <iris/xmpp-im/jingle-sxe.h>
#include <iris/xmpp.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_clientstream.h>

#include <QCoreApplication>
#include <QPointer>
#include <QtCrypto>

using namespace XMPP;
namespace J   = XMPP::Jingle;
namespace SXE = XMPP::Jingle::SXE;

static void check(bool ok, const char *message)
{
    if (!ok)
        qFatal("%s", message);
}

class NoNetworkConnector final : public Connector {
public:
    void        setOptHostPort(const QString &, quint16) override { }
    void        connectToServer(const QString &) override { }
    ByteStream *stream() const override { return nullptr; }
    void        done() override { }
};

class RecordingStream final : public ClientStream {
public:
    explicit RecordingStream(Connector *connector) : ClientStream(connector) { }
    void write(const Stanza &stanza) override { written.append(stanza.element().cloneNode(true).toElement()); }

    QList<QDomElement> written;
};

struct Endpoint {
    NoNetworkConnector connector;
    RecordingStream    stream { &connector };
    Client             client;
    Jid                jid;
    WbManager         *wb = nullptr;

    explicit Endpoint(const QString &user)
    {
        jid = Jid(user + QStringLiteral("@example.test/device"));
        client.start(QStringLiteral("example.test"), user, QString(), QStringLiteral("device"));
        client.connectToServer(&stream, jid);
        client.jingleManager()->setMessageInitiationEnabled(true);
        wb = new WbManager(&client, nullptr);
        QObject::connect(client.jingleManager(), &J::Manager::incomingSession, wb,
                         [this](J::Session *session) { wb->incomingSession(session); });
    }
    ~Endpoint() { delete wb; }
};

// Delivers everything written by one endpoint to the other until both are quiet.
static void pump(Endpoint &a, Endpoint &b)
{
    for (int round = 0; round < 200; ++round) {
        bool moved = false;
        for (auto [from, to] : { std::pair<Endpoint *, Endpoint *> { &a, &b }, { &b, &a } }) {
            auto out = std::move(from->stream.written);
            from->stream.written.clear();
            for (auto stanza : out) {
                moved = true;
                stanza.setAttribute(QStringLiteral("from"), from->jid.full());
                stanza.setAttribute(QStringLiteral("to"), to->jid.full());
                to->client.rootTask()->take(stanza);
            }
        }
        for (int i = 0; i < 4; ++i)
            QCoreApplication::processEvents();
        if (!moved && a.stream.written.isEmpty() && b.stream.written.isEmpty())
            return;
    }
    qFatal("stanza exchange did not settle");
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCA::Initializer qca;

    Endpoint alice(QStringLiteral("alice"));
    Endpoint bob(QStringLiteral("bob"));

    int     request = -1;
    QString ringing, accepted, closed, sessionFor;
    Jid     acceptedBy;
    QList<int>                  cancelled;
    QPointer<SXE::Application> bobWhiteboard;
    QObject::connect(bob.wb, &WbManager::wbRequest, &app, [&](const Jid &, int id) { request = id; });
    QObject::connect(bob.wb, &WbManager::requestCancelled, &app, [&](int id) { cancelled.append(id); });
    QObject::connect(bob.wb, &WbManager::proposalSession, &app, [&](const QString &id, SXE::Application *wb) {
        sessionFor    = id;
        bobWhiteboard = wb;
    });
    QObject::connect(alice.wb, &WbManager::proposalRinging, &app, [&](const QString &id) { ringing = id; });
    QObject::connect(alice.wb, &WbManager::proposalAccepted, &app, [&](const QString &id, const Jid &peer) {
        accepted   = id;
        acceptedBy = peer;
    });
    QObject::connect(alice.wb, &WbManager::proposalClosed, &app,
                     [&](const QString &id, const QString &) { closed = id; });

    // Accepted proposal: the session that follows has the proposal id as sid and goes to the proposal.
    const auto id = alice.wb->propose(bob.jid.withResource(QString()));
    check(!id.isEmpty(), "the proposal was not sent");
    pump(alice, bob);
    check(request >= 0 && ringing == id, "the proposal did not ring");
    const auto incoming = bob.wb->takeRequest(request);
    check(incoming.proposalId == id && incoming.peer.compare(alice.jid) && !incoming.application,
          "the request does not describe the proposal");
    check(bob.wb->proceed(id), "the proposal could not be accepted");
    pump(alice, bob);
    check(accepted == id && acceptedBy.compare(bob.jid), "the proposer did not see the acceptance");

    request      = -1;
    auto session = alice.client.jingleManager()->newSession(acceptedBy, id);
    auto content = qobject_cast<SXE::Application *>(session->newContent(SXE::NS_SVG_WHITEBOARD));
    check(content, "whiteboard content was not created");
    session->addContent(content);
    session->initiate();
    pump(alice, bob);
    check(sessionFor == id && bobWhiteboard && request < 0,
          "the session of an accepted proposal was presented as a new invitation");
    bobWhiteboard->pad()->session()->accept();
    pump(alice, bob);
    check(session->state() == J::State::Active, "the proposed session did not start");
    session->terminate(J::Reason::Success);
    alice.wb->finish(id, true);
    pump(alice, bob);

    // Withdrawn proposal: the invitation disappears before it is opened.
    const auto withdrawn = alice.wb->propose(bob.jid.withResource(QString()));
    pump(alice, bob);
    const int withdrawnRequest = request;
    alice.wb->retract(withdrawn);
    pump(alice, bob);
    check(cancelled.contains(withdrawnRequest), "a withdrawn proposal stayed in the event queue");
    check(!bob.wb->takeRequest(withdrawnRequest).peer.isValid(), "a withdrawn proposal can still be opened");

    // Declined proposal.
    const auto declined = alice.wb->propose(bob.jid.withResource(QString()));
    pump(alice, bob);
    check(bob.wb->takeRequest(request).proposalId == declined, "the third proposal was not presented");
    bob.wb->reject(declined);
    pump(alice, bob);
    check(closed == declined, "the proposer did not see the decline");

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    qInfo("whiteboard proposal tests passed");
    return 0;
}
