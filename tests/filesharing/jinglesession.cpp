#include "filesharingjinglesession.h"
#include <iris/jingle-ft.h>
#include <iris/xmpp_client.h>
#define private public
#include <iris/jingle-ice.h>
#undef private
#include "jingle-ice-pad_p.h"
#include "sxeconnectionfixture.h"

namespace FT = J::FileTransfer;

static void queuedRanges(bool grouping, bool cancelLast = false)
{
    TcpPortReserver reserver;
    Endpoint        alice(QStringLiteral("alice")), bob(QStringLiteral("bob"));
    for (auto e : { &alice, &bob }) {
        e->client.setTcpPortReserver(&reserver);
        e->client.jingleICEManager()->setSelfAddress(QHostAddress::LocalHost);
    }
    auto features = alice.client.jingleManager()->discoFeatures();
    if (!grouping)
        features.removeAll(QStringLiteral("urn:ietf:rfc:5888"));
    setPeerFeatures(alice.client, bob.jid, features);
    setPeerFeatures(bob.client, alice.jid, features);

    const QByteArray payload = QByteArray(8192, 'a') + QByteArray(8192, 'b') + QByteArray(8192, 'c');
    FT::File         file;
    file.setName(QStringLiteral("movie.bin"));
    file.setSize(quint64(payload.size()));
    file.addHash(Hash::from(Hash::Sha256, payload));
    int                              sessions = 0, additions = 0, accepted = 0, receipts = 0, verified = 0;
    int                              removals = 0;
    QList<QPointer<J::Session>>      sourceSessions, requestSessions;
    QList<QPointer<FT::Application>> sourceParts, requestParts;
    std::function<void()>            releaseLast;
    J::ICE::IceConnection           *association = nullptr;
    onStanza                                     = [&](const Endpoint &from, QDomElement &stanza) {
        const auto jingle = stanza.firstChildElement(QStringLiteral("jingle"));
        const auto action = jingle.attribute(QStringLiteral("action"));
        if (from.jid == alice.jid && action == QLatin1String("content-add"))
            ++additions;
        if (from.jid == alice.jid && action == QLatin1String("content-remove"))
            ++removals;
        if (from.jid == bob.jid && action == QLatin1String("content-accept"))
            ++accepted;
        if (from.jid == alice.jid && !jingle.firstChildElement(QStringLiteral("received")).isNull()) {
            ++receipts;
            // Once the last receipt retires, no successor is expected.
            if (grouping && (cancelLast || receipts < 3))
                check(accepted >= receipts, "receipt preceded successor content-accept");
        }
    };
    QObject::connect(bob.client.jingleManager(), &J::Manager::incomingSession, &bob.client, [&](J::Session *s) {
        ++sessions;
        sourceSessions.append(s);
        auto accept = [&, s, payload] {
            for (auto *content : s->contentList()) {
                if (content->state() != J::State::Created)
                    continue;
                auto *a = qobject_cast<FT::Application *>(content);
                check(a && a->senders() == s->role(), "unexpected file offer");
                sourceParts.append(a);
                a->setKeepTransportUntilReceipt();
                QObject::connect(a, &J::Application::stateChanged, a, [a](J::State state) {
                    if (state == J::State::Finished)
                        a->deleteLater();
                });
                a->setAcceptFile(a->file());
                QObject::connect(a, &FT::Application::deviceRequested, a,
                                 [&, a, payload](quint64 offset, std::optional<quint64>) {
                                     auto release = [a, payload, offset] {
                                         auto *source = new QBuffer(a);
                                         source->setData(payload);
                                         source->open(QIODevice::ReadOnly);
                                         check(source->seek(qint64(offset)), "source seek failed");
                                         a->setDevice(source);
                                     };
                                     if (!cancelLast && offset == 16384)
                                         releaseLast = release;
                                     else
                                         release();
                                 });
                if (s->state() != J::State::Created)
                    a->prepare();
            }
        };
        QObject::connect(s, &J::Session::newContentReceived, s, accept);
        accept();
        s->accept();
    });
    FileSharingJingleSession pool(alice.client.jingleManager());
    QObject                  cancelledConsumer;
    bool                     cancelledStarted = false;
    auto                     cancelled
        = pool.request(bob.jid, file, &cancelledConsumer, [&](FT::Application *) { cancelledStarted = true; });
    cancelled();
    QObject                consumers[3];
    QByteArray             received[3];
    QList<MediaChunkAbort> cancel;
    for (int i = 0; i < 3; ++i) {
        auto rangeFile = file;
        rangeFile.setRange(FT::Range(quint64(i * 8192), 8192));
        cancel.append(pool.request(bob.jid, rangeFile, &consumers[i], [&, i](FT::Application *a) {
            check(a, "request setup failed");
            requestParts.append(a);
            auto *session = a->pad()->session();
            if (!requestSessions.contains(session))
                requestSessions.append(session);
            QObject::connect(a, &FT::Application::connectionReady, &consumers[i], [&, i, a] {
                if (grouping) {
                    check(a->transport()->pad()->ns() == J::ICE::NS, "unexpected media transport");
                    auto *pad   = static_cast<J::ICE::Pad *>(a->transport()->pad());
                    bool  bound = false, required = false;
                    auto *network = pad->d->groupedConnectionFor(
                        static_cast<J::ICE::Transport *>(a->transport().data()), &bound, &required);
                    check(bound && required && network && pad->d->liveAssociationCount() == 1,
                          "range did not use one physical BUNDLE association");
                    check(!association || network == association,
                          "handover replaced the physical ICE/DTLS association");
                    association = network;
                }
                if (cancelLast && i == 2) {
                    cancel[i](); // Close the player while its last prefetched range is active.
                    return;
                }
                auto connection = a->connection();
                auto read       = [&, i, connection] { received[i] += connection->readAll(); };
                QObject::connect(connection.data(), &QIODevice::readyRead, &consumers[i], read);
                read();
            });
            QObject::connect(a, &FT::Application::payloadVerified, &consumers[i], [&, i, a] {
                ++verified;
                cancel[i](); // Release the consumer, preserving the queued handover.
            });
        }));
    }
    if (!cancelLast) {
        check(pumpUntil(alice, bob, [&] { return verified == 2 && receipts == 2 && bool(releaseLast); }),
              "last range did not hold the session open");
        // Outside the network callback stack: retire posted deletions while a
        // real successor is still active, rather than only at session teardown.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        for (const auto *parts : { &sourceParts, &requestParts }) {
            check(parts->size() == 3, "missing range applications");
            check(!parts->at(0) && !parts->at(1), "finished range applications accumulated");
            check(parts->at(2) && parts->at(2)->pad()->session()->state() == J::State::Active,
                  "retiring predecessors closed the successor session");
            check(parts->at(2)->pad()->session()->contentList().size() == 1,
                  "session retained completed range contents");
        }
        std::exchange(releaseLast, {})();
    }
    const int completed = cancelLast ? 2 : 3;
    check(pumpUntil(alice, bob,
                    [&] { return verified == completed && receipts == completed && (!cancelLast || removals == 1); }),
          "queued ranges did not finish");
    for (int i = 0; i < completed; ++i)
        check(received[i] == payload.mid(i * 8192, 8192), "range payload or offset corrupted");
    check(sessions == (grouping ? 1 : 3), "wrong number of physical Jingle sessions");
    check(additions == (grouping ? 2 : 0), "ranges did not reuse content-add");
    check(!cancelledStarted, "cancelled queued request started");
    check(pumpUntil(
              alice, bob,
              [&] {
                  for (const auto &s : sourceSessions)
                      if (s && s->state() != J::State::Finished)
                          return false;
                  for (const auto &s : requestSessions)
                      if (s && s->state() != J::State::Finished)
                          return false;
                  return true;
              },
              1000),
          "completed/cancelled ranges left a Jingle session alive");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    for (const auto &s : sourceSessions)
        check(!s, "sender session was not destroyed after termination");
    for (const auto &s : requestSessions)
        check(!s, "receiver session was not destroyed after termination");
    onStanza = {};
    pumpUntil(alice, bob, [] { return false; }, 50);
}

static void deletionDuringSetup()
{
    Endpoint                           alice(QStringLiteral("setup"));
    auto                              *pool = new FileSharingJingleSession(alice.client.jingleManager());
    QPointer<FileSharingJingleSession> guard(pool);
    FT::File                           file;
    file.setName(QStringLiteral("setup.bin"));
    file.setSize(1);
    file.addHash(Hash::from(Hash::Sha256, QByteArray("x")));
    QObject consumer;
    auto    cancel = pool->request(Jid(QStringLiteral("bob@example.test/device")), file, &consumer,
                                   [pool](FT::Application *) { delete pool; });
    QCoreApplication::processEvents();
    check(!guard, "pool setup callback was not invoked");
    cancel(); // The cancellation handle may outlive the account-scoped pool.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCA::Initializer qca;
    Client           probe;
    if (!probe.jingleManager()->discoFeatures().contains(SXE::NS_CONNECTIONS)) {
        qInfo("Jingle media session test skipped: no DTLS/SCTP backend");
        return 77;
    }
    queuedRanges(true);
    queuedRanges(false);
    queuedRanges(true, true);
    deletionDuringSetup();
    qInfo("Jingle media queue and BUNDLE handover passed");
}
