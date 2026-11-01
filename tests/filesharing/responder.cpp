/* SPDX-License-Identifier: GPL-2.0-or-later */
// Remote file requests over real Jingle sessions, against two accounts of one
// profile that hold the same bytes on disk.
#include "filesharingjinglesession.h"
#include "filesharingresponder.h"
#include "filesharingstore.h"
#include <QFile>
#include <QTemporaryDir>
#include <iris/jingle-ft.h>
#include <iris/xmpp_client.h>
#include <optional>

#include "sxeconnectionfixture.h"

namespace FT = J::FileTransfer;

namespace {
struct Answer {
    QByteArray received;
    QString    refusal;
};

// As in FileSharingManager: one copy for every account, and only the decision differs.
FileSharingResponder::Lookup lookupFor(const FileSharingStore &store, const QString &account, const QString &path,
                                       const Hash &hash)
{
    return [&store, account, path, hash](const QList<Hash> &hashes) -> std::optional<FileSharingResponder::ServedFile> {
        if (!store.isServed(account, hashes))
            return {};
        return FileSharingResponder::ServedFile { path, { hash } };
    };
}

Answer request(Endpoint &peer, Endpoint &account, const FileSharingResponder::Lookup &lookup, const FT::File &file,
               qsizetype expected)
{
    Answer answer;
    onStanza = [&](const Endpoint &from, QDomElement &stanza) {
        const auto jingle = stanza.firstChildElement(QStringLiteral("jingle"));
        if (from.jid == account.jid
            && jingle.attribute(QStringLiteral("action")) == QLatin1String("session-terminate")) {
            const auto reason = jingle.firstChildElement(QStringLiteral("reason"));
            if (!reason.firstChildElement(QStringLiteral("success")).isNull())
                return; // A finished transfer also ends this way.
            QString     text;
            QTextStream out(&text);
            reason.save(out, 0);
            answer.refusal = text;
        }
    };
    const auto incoming = QObject::connect(
        account.client.jingleManager(), &J::Manager::incomingSession, &account.client, [&lookup](J::Session *session) {
            check(FileSharingResponder::answer(session, lookup), "a file request was not answered");
        });
    FileSharingJingleSession pool(peer.client.jingleManager());
    QObject                  consumer;
    auto                     release = pool.request(account.jid, file, &consumer, [&](FT::Application *app) {
        check(app, "request setup failed");
        QObject::connect(app, &FT::Application::connectionReady, &consumer, [&, app] {
            auto connection = app->connection();
            auto read       = [&answer, connection] { answer.received += connection->readAll(); };
            QObject::connect(connection.data(), &QIODevice::readyRead, &consumer, read);
            read();
        });
    });
    check(pumpUntil(peer, account, [&] { return answer.received.size() >= expected || !answer.refusal.isEmpty(); }),
          "the request got neither the file nor a refusal");
    release();
    pumpUntil(peer, account, [] { return false; }, 100);
    QObject::disconnect(incoming);
    onStanza = {};
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    return answer;
}

// A plain file offer stays for the user to decide on: it is not a request.
void offerIsLeftToTheUser(Endpoint &peer, Endpoint &account, const FileSharingResponder::Lookup &lookup,
                          const FT::File &file)
{
    bool       handled = false, answered = true;
    const auto incoming = QObject::connect(account.client.jingleManager(), &J::Manager::incomingSession,
                                           &account.client, [&](J::Session *session) {
                                               handled  = true;
                                               answered = FileSharingResponder::answer(session, lookup);
                                               session->terminate(J::Reason::Decline);
                                           });
    auto      *session  = peer.client.jingleManager()->newSession(account.jid);
    auto      *transfer = static_cast<FT::Application *>(session->newContent(FT::NS, session->role()));
    check(transfer, "offer setup failed");
    transfer->setFile(file);
    session->addContent(transfer);
    session->initiate();
    check(pumpUntil(peer, account, [&] { return handled; }), "the offer did not arrive");
    check(!answered, "a file offer was taken for a request");
    pumpUntil(peer, account, [] { return false; }, 100);
    QObject::disconnect(incoming);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCA::Initializer qca;
    Client           probe;
    if (!probe.jingleManager()->discoFeatures().contains(SXE::NS_CONNECTIONS)) {
        qInfo("File request test skipped: no DTLS/SCTP backend");
        return 77;
    }

    TcpPortReserver reserver;
    Endpoint peer(QStringLiteral("peer")), corporate(QStringLiteral("corporate")), personal(QStringLiteral("personal"));
    const auto features = peer.client.jingleManager()->discoFeatures();
    for (auto *endpoint : { &peer, &corporate, &personal }) {
        endpoint->client.setTcpPortReserver(&reserver);
        endpoint->client.jingleICEManager()->setSelfAddress(QHostAddress::LocalHost);
    }
    for (auto *endpoint : { &corporate, &personal }) {
        setPeerFeatures(peer.client, endpoint->jid, features);
        setPeerFeatures(endpoint->client, peer.jid, features);
    }

    QTemporaryDir dir;
    check(dir.isValid(), "no temporary directory");
    const QByteArray payload = QByteArray(16384, 'x') + QByteArray(16384, 'y');
    const auto       path    = dir.filePath(QStringLiteral("cached-copy"));
    QFile            copy(path);
    check(copy.open(QIODevice::WriteOnly) && copy.write(payload) == payload.size(), "cannot write the cached copy");
    copy.close();

    const auto hash = Hash::from(Hash::Sha256, payload);
    FT::File   file;
    file.setName(QStringLiteral("report.bin"));
    file.setSize(quint64(payload.size()));
    file.addHash(hash);
    FT::File unknown;
    unknown.setName(QStringLiteral("unknown.bin"));
    unknown.setSize(quint64(payload.size()));
    unknown.addHash(Hash::from(Hash::Sha256, QByteArray("some other file")));

    FileSharingStore store(dir.filePath(QStringLiteral("shares.sqlite")));
    check(store.markServed(QStringLiteral("corporate"), { hash }), "cannot publish the file");
    const auto corporateLookup = lookupFor(store, QStringLiteral("corporate"), path, hash);
    const auto personalLookup  = lookupFor(store, QStringLiteral("personal"), path, hash);

    const auto published = request(peer, corporate, corporateLookup, file, payload.size());
    check(published.refusal.isEmpty() && published.received == payload, "a published file was not served intact");

    const auto cached = request(peer, personal, personalLookup, file, payload.size());
    check(cached.received.isEmpty() && !cached.refusal.isEmpty(), "another account served a file it merely holds");

    const auto missing = request(peer, corporate, corporateLookup, unknown, payload.size());
    check(missing.received.isEmpty() && !missing.refusal.isEmpty(), "an unknown file was not refused");
    check(cached.refusal == missing.refusal, "a requester can tell a file not shared here from an unknown one");

    offerIsLeftToTheUser(peer, personal, personalLookup, file);

    // Joining the publication of a file already held is this account's own decision.
    check(store.markServed(QStringLiteral("personal"), { hash }), "cannot join the publication");
    const auto joined = request(peer, personal, personalLookup, file, payload.size());
    check(joined.refusal.isEmpty() && joined.received == payload, "a joined file was not served intact");

    qInfo("File requests are answered per account");
}
