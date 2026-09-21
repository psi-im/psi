#include "gstprovider.h"
#include "psimedia.h"
#include "psimediajingle.h"

#include <iris/jingle-ice.h>
#include <iris/jingle-rtp.h>
#include <iris/dtls.h>
#include <iris/jingle-session.h>
#include <iris/tcpportreserver.h>
#include <iris/xmpp.h>
#include <iris/xmpp_caps.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_clientstream.h>
#include <iris/xmpp_tasks.h>

#include <QtCrypto>

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <utility>

using namespace XMPP;
namespace J = XMPP::Jingle;
namespace RTP = XMPP::Jingle::RTP;
namespace ICE = XMPP::Jingle::ICE;

static void installPeerFeatures(Client &client, const Jid &peer)
{
    DiscoItem disco = client.makeDiscoResult(QStringLiteral("urn:psi-im:ci:jingle-call-live"));
    disco.setJid(peer);
    const CapsSpec caps(disco);
    CapsRegistry::instance()->registerCaps(caps, disco);
    client.capsManager()->updateCaps(peer, caps);
}

class XmppEndpoint final : public QObject {
public:
    XmppEndpoint(Jid jid, QString password, QObject *parent = nullptr)
        : QObject(parent), jid_(std::move(jid)), password_(std::move(password))
    {
        // Production Psi gives every Client the process-wide port reserver.
        // ICE does not require a named TCP scope today, but mirror that ownership
        // contract in the live fixture rather than relying on the null-safe path.
        client_.setTcpPortReserver(&portReserver_);
    }

    ~XmppEndpoint() override
    {
        if (stream_) {
            client_.close();
            delete stream_;
        }
        delete connector_;
    }

    Client *client() { return &client_; }

    void start(std::function<void()> ready, std::function<void(const QString &)> failed)
    {
        ready_ = std::move(ready);
        failed_ = std::move(failed);

        connector_ = new AdvancedConnector;
        connector_->setOptHostPort(QStringLiteral("127.0.0.1"), 5222);
        connector_->setOptSSL(false);

        stream_ = new ClientStream(connector_, nullptr);
        stream_->setAllowPlain(ClientStream::AllowPlain);
        stream_->setRequireMutualAuth(false);
        stream_->setCompress(false);
        stream_->setNoopTime(0);

        connect(stream_, &ClientStream::needAuthParams, this, [this](bool user, bool pass, bool realm) {
            if (user)
                stream_->setUsername(jid_.node());
            if (pass)
                stream_->setPassword(password_);
            if (realm)
                stream_->setRealm(jid_.domain());
            stream_->continueAfterParams();
        });
        connect(stream_, &ClientStream::warning, this, [this](int warning) {
            qInfo() << "XMPP warning" << warning << "- continuing in local CI mode";
            stream_->continueAfterWarning();
        });
        connect(stream_, &ClientStream::authenticated, this, [this]() {
            const Jid bound = stream_->jid();
            const QString resource = bound.resource().isEmpty() ? jid_.resource() : bound.resource();
            client_.start(jid_.domain(), jid_.node(), password_, resource);
            if (client_.isSessionRequired()) {
                auto *task = new JT_Session(client_.rootTask());
                connect(task, &Task::finished, this, [this, task]() {
                    if (!task->success()) {
                        fail(QStringLiteral("legacy XMPP session establishment failed"));
                        return;
                    }
                    becomeReady();
                });
                task->go(true);
            } else {
                becomeReady();
            }
        });
        connect(stream_, &Stream::error, this, [this](int error) {
            fail(QStringLiteral("XMPP stream error %1").arg(error));
        });
        connect(stream_, &Stream::connectionClosed, this, [this]() {
            if (!readyState_)
                fail(QStringLiteral("XMPP stream closed before authentication"));
        });
        client_.connectToServer(stream_, jid_);
    }

private:
    void becomeReady()
    {
        if (readyState_)
            return;
        readyState_ = true;
        connect(&client_, &Client::xmlIncoming, this, [](const QString &xml) {
            if (xml.contains(QStringLiteral("urn:xmpp:jingle:1")))
                qInfo().noquote() << QStringLiteral("XMPP_IN=%1").arg(xml.trimmed());
        });
        connect(&client_, &Client::xmlOutgoing, this, [](const QString &xml) {
            if (xml.contains(QStringLiteral("urn:xmpp:jingle:1")))
                qInfo().noquote() << QStringLiteral("XMPP_OUT=%1").arg(xml.trimmed());
        });
        qInfo().noquote() << QStringLiteral("XMPP_READY=%1").arg(client_.jid().full());
        if (ready_)
            ready_();
    }

    void fail(const QString &message)
    {
        if (failed_)
            failed_(message);
    }

    TcpPortReserver portReserver_;
    Client client_;
    Jid jid_;
    QString password_;
    AdvancedConnector *connector_ = nullptr;
    ClientStream *stream_ = nullptr;
    bool readyState_ = false;
    std::function<void()> ready_;
    std::function<void(const QString &)> failed_;
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCA::Initializer qca;

    if (argc < 7) {
        qCritical() << "Usage:" << argv[0]
                    << "<caller|callee> <jid/resource> <password> <peer/resource> <output.raw> <ready-file>";
        return 2;
    }

    const QString role = QString::fromLocal8Bit(argv[1]);
    const Jid localJid(QString::fromLocal8Bit(argv[2]));
    const QString password = QString::fromLocal8Bit(argv[3]);
    const Jid peerJid(QString::fromLocal8Bit(argv[4]));
    const QString outputPath = QString::fromLocal8Bit(argv[5]);
    const QString readyPath = QString::fromLocal8Bit(argv[6]);

    if ((role != QLatin1String("caller") && role != QLatin1String("callee"))
        || !localJid.isValid() || localJid.resource().isEmpty()
        || !peerJid.isValid() || peerJid.resource().isEmpty()) {
        qCritical() << "Invalid live-call arguments";
        return 3;
    }

    QFile::remove(outputPath);

    PsiMedia::GstProvider gstProvider;
    if (!gstProvider.isInitialized()) {
        qCritical("LIVE_CALL_RESULT=failure GstProvider failed to initialize");
        return 4;
    }
    PsiMedia::setProvider(&gstProvider);

    bool finishing = false;
    bool sessionActive = false;
    bool mediaActive = false;
    bool mediaStarted = false;
    bool mediaWindowArmed = false;
    bool localMediaVerified = false;
    QPointer<J::Session> liveSession;
    QPointer<RTP::Application> audioApp;

    auto outputHasMedia = [&]() {
        QFileInfo fi(outputPath);
        return fi.exists() && fi.size() >= 4096;
    };

    std::function<void()> armMediaWindow;

    auto finish = [&](int code, const QString &message) {
        if (finishing)
            return;
        finishing = true;
        if (liveSession)
            stopPsiMediaJingleTransmit(liveSession);
        if (code == 0)
            qInfo().noquote() << QStringLiteral("LIVE_CALL_RESULT=success %1").arg(message);
        else
            qCritical().noquote() << QStringLiteral("LIVE_CALL_RESULT=failure %1").arg(message);
        QTimer::singleShot(250, &app, [&, code]() { app.exit(code); });
    };

    armMediaWindow = [&]() {
        if (mediaWindowArmed || !mediaStarted)
            return;
        mediaWindowArmed = true;
        qInfo("CALL_MEDIA_WINDOW=started");
        QTimer::singleShot(3500, &app, [&]() {
            if (finishing || !liveSession || liveSession->state() == J::State::Finished)
                return;
            localMediaVerified = outputHasMedia();
            qInfo().noquote()
                << QStringLiteral("CALL_DECODED_BYTES=%1").arg(QFileInfo(outputPath).size());
            if (!sessionActive || !mediaActive || !mediaStarted || !localMediaVerified) {
                finish(16, QStringLiteral("call did not carry decoded bidirectional audio"));
                return;
            }
            if (role == QLatin1String("caller")) {
                qInfo("CALL_HANGUP=local-success");
                stopPsiMediaJingleTransmit(liveSession);
                liveSession->terminate(J::Reason::Success);
            }
        });
    };

    QTimer::singleShot(45000, &app, [&]() {
        if (!finishing)
            finish(124, QStringLiteral("timeout"));
    });

    XmppEndpoint endpoint(localJid, password, &app);

    auto configureClient = [&]() -> bool {
        auto *manager = endpoint.client()->jingleManager();
        auto *rtpManager = manager ? manager->rtpManager() : nullptr;
        auto *iceManager = endpoint.client()->jingleICEManager();
        if (!manager || !rtpManager || !iceManager)
            return false;

        PsiMediaJingleCapabilities caps;
        caps.backendAvailable = true;
        caps.probeComplete = true;
        const auto backendProfiles = PsiMedia::RtpSession::supportedSecureRtpProfiles();
        const auto dtlsProfiles    = XMPP::Dtls::supportedSRTPProfiles();
        caps.secureRtp = std::any_of(backendProfiles.cbegin(), backendProfiles.cend(),
                                     [&dtlsProfiles](const QString &profile) {
                                         return dtlsProfiles.contains(profile);
                                     });
        caps.audio = true;
        caps.audioInput = true;
        caps.audioOutput = true;
        if (!caps.secureRtp)
            return false;

        rtpManager->setMediaProvider(makePsiMediaJingleProvider(caps));
        rtpManager->setTransportNamespaces({ ICE::NS, ICE::NS_ICE_UDP });
        iceManager->setAllowIpExposure(true);
        return true;
    };

    auto wireSession = [&](J::Session *session) {
        liveSession = session;
        QObject::connect(session, &J::Session::activated, session, [&]() {
            sessionActive = true;
            qInfo("CALL_SESSION=activated");
            if (mediaActive && !mediaStarted && liveSession) {
                mediaStarted = startPsiMediaJingleTransmit(
                    liveSession, true, true,
                    QStringLiteral("audiotestsrc is-live=true wave=sine freq=440"),
                    false, QString());
                qInfo().noquote() << QStringLiteral("CALL_MEDIA_STARTED=%1").arg(mediaStarted ? 1 : 0);
                if (!mediaStarted)
                    finish(31, QStringLiteral("psimedia audio transmit did not start"));
                else
                    armMediaWindow();
            }
        });
        QObject::connect(session, &J::Session::terminated, session, [&, session]() {
            qInfo().noquote() << QStringLiteral("CALL_SESSION=terminated state=%1").arg(int(session->state()));
            if (!localMediaVerified)
                localMediaVerified = outputHasMedia();
            qInfo().noquote()
                << QStringLiteral("CALL_DECODED_BYTES=%1").arg(QFileInfo(outputPath).size());
            if (!localMediaVerified) {
                finish(32, QStringLiteral("no decoded remote audio reached output sink"));
                return;
            }
            finish(0, role == QLatin1String("caller")
                          ? QStringLiteral("caller completed")
                          : QStringLiteral("callee completed"));
        });
    };

    auto wireAudio = [&](RTP::Application *rtp) {
        audioApp = rtp;
        QObject::connect(rtp, &J::Application::stateChanged, rtp, [&, rtp](J::State state) {
            qInfo().noquote() << QStringLiteral("CALL_AUDIO_STATE=%1").arg(int(state));
            if (state != J::State::Active)
                return;
            mediaActive = true;
            const auto local = rtp->localDescription();
            const auto remote = rtp->remoteDescription();
            qInfo().noquote()
                << QStringLiteral("CALL_NEGOTIATED local_payloads=%1 remote_payloads=%2")
                       .arg(local ? local->payloads.size() : 0)
                       .arg(remote ? remote->payloads.size() : 0);
            if (sessionActive && !mediaStarted && liveSession) {
                mediaStarted = startPsiMediaJingleTransmit(
                    liveSession, true, true,
                    QStringLiteral("audiotestsrc is-live=true wave=sine freq=440"),
                    false, QString());
                qInfo().noquote() << QStringLiteral("CALL_MEDIA_STARTED=%1").arg(mediaStarted ? 1 : 0);
                if (!mediaStarted)
                    finish(33, QStringLiteral("psimedia audio transmit did not start"));
                else
                    armMediaWindow();
            }
        });
    };

    if (role == QLatin1String("callee")) {
        QObject::connect(endpoint.client()->jingleManager(), &J::Manager::incomingSession, &app,
                         [&](J::Session *session) {
            if (liveSession) {
                session->terminate(J::Reason::Busy);
                finish(20, QStringLiteral("duplicate incoming call"));
                return;
            }
            if (session->preferredApplication() != RTP::Description::ns()) {
                session->terminate(J::Reason::UnsupportedApplications);
                finish(21, QStringLiteral("incoming session is not RTP"));
                return;
            }

            RTP::Application *rtp = nullptr;
            for (auto *base : session->contentList()) {
                rtp = dynamic_cast<RTP::Application *>(base);
                if (rtp && rtp->media() == QLatin1String("audio"))
                    break;
                rtp = nullptr;
            }
            if (!rtp) {
                session->terminate(J::Reason::UnsupportedApplications);
                finish(22, QStringLiteral("incoming call has no audio RTP content"));
                return;
            }

            wireSession(session);
            wireAudio(rtp);
            const QString sink = QStringLiteral("filesink location=\"%1\" sync=false").arg(outputPath);
            if (!configurePsiMediaJingleSession(session, sink, QString(), false, 128)) {
                finish(23, QStringLiteral("cannot configure callee psimedia backend"));
                return;
            }
            session->accept();
        });
    }

    endpoint.start(
        [&]() {
            if (!configureClient()) {
                finish(10, QStringLiteral("cannot configure RTP/ICE/media backend"));
                return;
            }
            installPeerFeatures(*endpoint.client(), peerJid);

            if (role == QLatin1String("callee")) {
                if (readyPath.isEmpty()) {
                    finish(11, QStringLiteral("callee ready-file missing"));
                    return;
                }
                QFile ready(readyPath);
                if (!ready.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    finish(12, QStringLiteral("cannot create callee ready file"));
                    return;
                }
                ready.write(endpoint.client()->jid().full().toUtf8());
                ready.write("\n");
                ready.close();
                qInfo("Callee armed for incoming audio call");
                return;
            }

            auto *manager = endpoint.client()->jingleManager();
            auto *rtpManager = manager->rtpManager();
            auto *session = manager->newSession(peerJid);
            if (!session) {
                finish(13, QStringLiteral("cannot create outgoing Jingle session"));
                return;
            }
            wireSession(session);

            auto *rtp = rtpManager->createOutgoing(session, QStringLiteral("audio"), J::Origin::Both);
            if (!rtp) {
                finish(14, QStringLiteral("cannot create outgoing audio RTP application"));
                return;
            }
            wireAudio(rtp);

            const QString sink = QStringLiteral("filesink location=\"%1\" sync=false").arg(outputPath);
            if (!configurePsiMediaJingleSession(session, sink, QString(), false, 128)) {
                finish(15, QStringLiteral("cannot configure caller psimedia backend"));
                return;
            }

            session->initiate();
        },
        [&](const QString &message) { finish(9, message); });

    return app.exec();
}
