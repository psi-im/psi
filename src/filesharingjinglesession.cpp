#include "filesharingjinglesession.h"
#include <QDateTime>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <algorithm>
#include <iris/jingle-ft.h>
#include <iris/jingle-session.h>
#include <iris/jingle-transport.h>
#include <iris/xmpp_caps.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_jid.h>
#include <utility>

namespace J  = XMPP::Jingle;
namespace FT = J::FileTransfer;

class FileSharingJingleSession::Private {
public:
    struct Request {
        FT::File                  file;
        QPointer<QObject>         consumer;
        Ready                     ready;
        QPointer<FT::Application> app;
        bool                      cancelled = false, verified = false, accepted = false, closing = false;
        bool                      retireScheduled = false, retireNow = false;
        bool                      preferredS5b = false;
    };
    struct Peer {
        XMPP::Jid                       jid;
        QPointer<J::Session>            session;
        QList<std::shared_ptr<Request>> queue, parts;
        std::shared_ptr<Request>        current;
    };
    FileSharingJingleSession             *q;
    QPointer<J::Manager>                  manager;
    QHash<QString, std::shared_ptr<Peer>> peers;
    bool                                  scheduled = false;
    // Successful S5B cannot be multiplexed across BUNDLE contents. Remember it
    // briefly for later bounded requests of the SAME file and full peer JID,
    // rather than retrying ICE2 and ICE-UDP for every range.
    QHash<QString, qint64>                successfulS5b;
    static constexpr int                  S5bAffinityLifetimeMs = 5 * 60 * 1000;
    static constexpr int                  MaxS5bAffinities = 64;
    static const QString                 &s5bNamespace()
    {
        static const QString ns = QStringLiteral("urn:xmpp:jingle:transports:s5b:1");
        return ns;
    }

    static QString affinityKey(const XMPP::Jid &jid, const FT::File &file)
    {
        if (!file.size())
            return {};
        // Ranged hashes vary between requests; only the immutable whole-file
        // content identity may carry a preference into another Jingle session.
        for (const auto &hash : file.computedHashes()) {
            if (hash.isValid() && !hash.data().isEmpty())
                return jid.full() + QChar(0x1f) + QString::number(*file.size()) + QChar(0x1f) + hash.toString();
        }
        return {};
    }

    bool preferS5b(const XMPP::Jid &jid, const FT::File &file)
    {
        const auto key = affinityKey(jid, file);
        if (key.isEmpty())
            return false;
        const auto it = successfulS5b.find(key);
        if (it == successfulS5b.end())
            return false;
        if (it.value() <= QDateTime::currentMSecsSinceEpoch()) {
            successfulS5b.erase(it);
            return false;
        }
        return true;
    }

    void rememberTransport(const XMPP::Jid &jid, const FT::File &file, const QString &transportNs)
    {
        const auto key = affinityKey(jid, file);
        if (key.isEmpty())
            return;
        if (transportNs != s5bNamespace()) {
            successfulS5b.remove(key);
            return;
        }
        if (!successfulS5b.contains(key) && successfulS5b.size() >= MaxS5bAffinities) {
            auto oldest = successfulS5b.begin();
            for (auto it = successfulS5b.begin(); it != successfulS5b.end(); ++it)
                if (it.value() < oldest.value())
                    oldest = it;
            successfulS5b.erase(oldest);
        }
        successfulS5b.insert(key, QDateTime::currentMSecsSinceEpoch() + S5bAffinityLifetimeMs);
    }
    // Iris's Jingle FT finalize/receipt deadline is 30 seconds. Allow a
    // delayed next media range to reuse BUNDLE (including slow decode or
    // cached reads), but release the previous receipt well before that
    // protocol deadline so the sender cannot time out waiting for it.
    static constexpr int                  IdleBundleGraceMs = 20000;

    void schedule()
    {
        if (scheduled)
            return;
        scheduled = true;
        QTimer::singleShot(0, q, [this] {
            scheduled                                         = false;
            const auto                               snapshot = peers;
            const QPointer<FileSharingJingleSession> guard(q);
            for (const auto &peer : snapshot) {
                plan(peer);
                if (!guard)
                    return;
            }
        });
    }

    static bool grouped(const std::shared_ptr<Request> &part, J::Session *session)
    {
        if (!part || !part->app || !session || session->state() != J::State::Active || !part->app->transport()
            || !part->app->transport()->supportsSharedTransport())
            return false;
        for (const auto &g : session->negotiatedGroupings())
            if (g.semantics == QLatin1String("BUNDLE") && g.contents.contains(part->app->contentName()))
                return true;
        return false;
    }

    void close(const std::shared_ptr<Request> &part)
    {
        if (!part || part->closing || !part->app)
            return;
        part->closing = true;
        if (part->verified)
            part->app->acknowledgeReceived();
        else
            part->app->remove(J::Reason::Cancel, QStringLiteral("Media request cancelled"));
    }

    void plan(const std::shared_ptr<Peer> &peer)
    {
        peer->queue.erase(std::remove_if(peer->queue.begin(), peer->queue.end(),
                                         [](const auto &r) { return r->cancelled || !r->consumer; }),
                          peer->queue.end());
        // Retire a previous member before proposing another BUNDLE topology.
        for (const auto &r : std::as_const(peer->parts))
            if (r->app && r->closing && r->app->state() != J::State::Finished)
                return;
        peer->parts.erase(
            std::remove_if(peer->parts.begin(), peer->parts.end(),
                           [](const auto &r) { return !r->app || r->app->state() == J::State::Finished; }),
            peer->parts.end());
        if (peer->current && (!peer->current->app || peer->current->app->state() == J::State::Finished))
            peer->current.reset();

        if (peer->current) {
            const auto current = peer->current;
            if (current->cancelled || !current->consumer) {
                // A verified request can already have delivered its bytes and
                // released its consumer. Keep it until its queued successor is accepted.
                if (!current->verified) {
                    close(current);
                    return;
                }
            }
            if (!current->accepted || !current->verified)
                return;
            if (peer->queue.isEmpty() && grouped(current, peer->session)) {
                // The media reader enqueues its next range only after the previous
                // one has been stored. Keep a verified BUNDLE transport briefly
                // so a later range can use content-add instead of another ICE/DTLS setup.
                if (!current->retireScheduled) {
                    current->retireScheduled = true;
                    QTimer::singleShot(IdleBundleGraceMs, q, [this, current] {
                        current->retireNow = true;
                        schedule();
                    });
                }
                if (!current->retireNow)
                    return;
            }
            if (peer->queue.isEmpty() || !grouped(current, peer->session)) {
                close(current);
                return;
            }
        }
        if (peer->queue.isEmpty()) {
            if (peer->parts.isEmpty())
                peers.remove(peer->jid.full());
            return;
        }
        const auto                               previous = peer->current;
        const bool                               shared   = grouped(previous, peer->session);
        auto                                     part     = peer->queue.takeFirst();
        const QPointer<FileSharingJingleSession> guard(q);
        if (!manager) {
            auto ready = std::exchange(part->ready, {});
            if (part->consumer)
                ready(nullptr);
            if (guard)
                schedule();
            return;
        }
        if (!shared) {
            peer->session = manager->newSession(peer->jid);
            QObject::connect(peer->session, &J::Session::newContentReceived, q, [peer] {
                if (peer->session)
                    peer->session->terminate(J::Reason::Decline, QStringLiteral("Unexpected media content"));
            });
        }
        auto *session = peer->session.data();
        auto *app     = static_cast<FT::Application *>(session->newContent(FT::NS, session->peerRole()));
        if (!app) {
            // A missing application manager is a terminal local setup failure.
            auto ready = std::exchange(part->ready, {});
            if (part->consumer)
                ready(nullptr);
            if (guard) {
                session->terminate(J::Reason::FailedApplication);
                schedule();
            }
            return;
        }
        part->app = app;
        peer->parts.append(part);
        peer->current = part;
        app->setFile(part->file);
        app->setStreamingMode(true);
        app->setKeepTransportUntilReceipt();
        app->setReceiptDeferred();
        if (shared) {
            auto transport = session->newOutgoingTransport(previous->app->transport()->pad()->ns());
            if (!transport || !app->setTransport(transport)) {
                if (transport)
                    transport->releaseIfUnused();
                // Let normal transport selection choose a supported alternative.
            }
        } else if (!session->isEndToEndRequired() && preferS5b(peer->jid, part->file)
                   && session->checkPeerCaps(s5bNamespace())) {
            // This initial selection still leaves the remaining Iris transports
            // available for normal transport-replace if S5B stops working.
            auto transport = session->newOutgoingTransport(s5bNamespace());
            if (transport && app->setTransport(transport)) {
                part->preferredS5b = true;
                qInfo("Jingle media: preferring previously successful S5B for %s",
                      qPrintable(peer->jid.full()));
            } else {
                if (transport)
                    transport->releaseIfUnused();
                successfulS5b.remove(affinityKey(peer->jid, part->file));
            }
        } else if (manager->client()->capsManager()->features(peer->jid).isEmpty()) {
            // A player starting in the disk cache may not need a Jingle session
            // until the first seek. In a MUC, the occupant can be online while
            // its entity capabilities are still unresolved. NSTransportsList
            // otherwise permanently discards every candidate because
            // checkPeerCaps() returns false, and no session-initiate is sent.
            // We can safely offer IBB without advertised DTLS support. ICE/SCTP
            // must NOT be offered blindly here: Iris enables outgoing DTLS
            // only when the peer's fingerprint capability is known, and SCTP
            // cannot operate without DTLS. Once verified peer caps arrive,
            // the normal selector can prefer ICE for subsequent ranges.
            const auto transports = manager->availableTransports(
                J::TransportFeature::Reliable | J::TransportFeature::Ordered | J::TransportFeature::DataOriented);
            // Jingle IBB's namespace is stable; its implementation header is
            // private to Iris and is not installed in the external SDK.
            const auto ibbNs = QStringLiteral("urn:xmpp:jingle:transports:ibb:1");
            if (transports.contains(ibbNs)) {
                auto transport = session->newOutgoingTransport(ibbNs);
                if (transport && app->setTransport(transport)) {
                    qInfo("Jingle media peer caps unavailable; offering safe IBB fallback to %s",
                          qPrintable(peer->jid.full()));
                } else {
                    if (transport)
                        transport->releaseIfUnused();
                    qWarning("Jingle media could not prepare IBB fallback for %s", qPrintable(peer->jid.full()));
                }
            } else {
                qWarning("Jingle media has unknown peer caps and no safe IBB fallback for %s",
                         qPrintable(peer->jid.full()));
            }
        }
        QObject::connect(app, &FT::Application::payloadVerified, q, [this, peer, part] {
            part->verified = true;
            if (part->app && part->app->transport())
                rememberTransport(peer->jid, part->file, part->app->transport()->pad()->ns());
            schedule();
        });
        QObject::connect(app, &J::Application::stateChanged, q, [this, peer, part, previous](J::State state) {
            if (state == J::State::Connecting || state == J::State::Active) {
                // A BUNDLE extension is committed when content-accept finishes,
                // before its new SCTP data channel becomes connected. The old
                // range may still be holding its receipt, and waiting for the
                // successor's Active state can deadlock the handover.
                // Defer until after the acceptance stack has committed the
                // negotiated group before releasing that receipt.
                QTimer::singleShot(0, q, [this, peer, part, previous] {
                    if (!part->app || part->app->state() >= J::State::Finishing)
                        return;
                    if (previous && !grouped(part, peer->session))
                        return; // The BUNDLE extension has not committed.
                    if (!part->accepted) {
                        part->accepted = true;
                        schedule();
                    }
                    close(previous);
                });
            } else if (state == J::State::Finished) {
                if (part->preferredS5b && !part->verified)
                    successfulS5b.remove(affinityKey(peer->jid, part->file));
                // A rejected successor must not leave the old receipt stranded.
                schedule();
                // The next content is already accepted before its predecessor's
                // receipt is released. Finished range applications have no more
                // consumers and must not accumulate for the life of the session.
                if (part->app)
                    part->app->deleteLater();
                close(previous);
            }
        });
        QObject::connect(app, &QObject::destroyed, q, [this] { schedule(); });
        auto ready = std::exchange(part->ready, {});
        if (part->consumer)
            ready(app);
        if (!guard)
            return;
        if (part->cancelled || !part->consumer) {
            // It has not been added to Session's content map yet.
            delete app;
            schedule();
            close(previous);
            return;
        }
        session->addContent(app);
        if (!shared)
            session->initiate();
    }
};

FileSharingJingleSession::FileSharingJingleSession(J::Manager *manager, QObject *parent) :
    QObject(parent), d(new Private)
{
    d->q       = this;
    d->manager = manager;
}

FileSharingJingleSession::~FileSharingJingleSession()
{
    for (const auto &peer : std::as_const(d->peers)) {
        for (const auto &part : std::as_const(peer->parts))
            if (part->app)
                part->app->disconnect(this);
        if (peer->session)
            peer->session->terminate(J::Reason::Cancel, QStringLiteral("Media source closed"));
    }
}

MediaChunkAbort FileSharingJingleSession::request(const XMPP::Jid &jid, const FT::File &file, QObject *consumer,
                                                  Ready ready)
{
    auto &peer = d->peers[jid.full()];
    if (!peer) {
        peer      = std::make_shared<Private::Peer>();
        peer->jid = jid;
    }
    auto part      = std::make_shared<Private::Request>();
    part->file     = file;
    part->consumer = consumer;
    part->ready    = std::move(ready);
    peer->queue.append(part);
    const QPointer<FileSharingJingleSession> guard(this);
    connect(consumer, &QObject::destroyed, this, [this] { d->schedule(); });
    d->schedule();
    return [guard, part] {
        part->cancelled = true;
        if (guard)
            guard->d->schedule();
    };
}
