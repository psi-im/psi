#include "gstprovider.h"
#include "psimedia.h"
#include "psimediajingle.h"

#include <iris/dtls.h>
#include <iris/jingle-ice.h>
#include <iris/jingle-rtp.h>
#include <iris/jingle-session.h>
#include <iris/tcpportreserver.h>
#include <iris/xmpp.h>
#include <iris/xmpp_caps.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_clientstream.h>
#include <iris/xmpp_tasks.h>

#include <QtCrypto>

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

using namespace XMPP;
namespace J   = XMPP::Jingle;
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
    XmppEndpoint(Jid jid, QString password, QObject *parent = nullptr) :
        QObject(parent), jid_(std::move(jid)), password_(std::move(password))
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
        ready_  = std::move(ready);
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
            const Jid     bound    = stream_->jid();
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
        connect(stream_, &Stream::error, this,
                [this](int error) { fail(QStringLiteral("XMPP stream error %1").arg(error)); });
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

    TcpPortReserver                      portReserver_;
    Client                               client_;
    Jid                                  jid_;
    QString                              password_;
    AdvancedConnector                   *connector_  = nullptr;
    ClientStream                        *stream_     = nullptr;
    bool                                 readyState_ = false;
    std::function<void()>                ready_;
    std::function<void(const QString &)> failed_;
};

int main(int argc, char **argv)
{
    QApplication     app(argc, argv);
    QCA::Initializer qca;

    if (argc < 7 || argc > 8) {
        qCritical() << "Usage:" << argv[0]
                    << "<caller|callee> <jid/resource> <password> <peer/resource> <output.raw> <ready-file>"
                       " [audio|av-bundle|av-bundle-repeat]";
        return 2;
    }

    const QString role = QString::fromLocal8Bit(argv[1]);
    const Jid     localJid(QString::fromLocal8Bit(argv[2]));
    const QString password = QString::fromLocal8Bit(argv[3]);
    const Jid     peerJid(QString::fromLocal8Bit(argv[4]));
    const QString outputPath   = QString::fromLocal8Bit(argv[5]);
    const QString readyPath    = QString::fromLocal8Bit(argv[6]);
    const QString mode         = argc == 8 ? QString::fromLocal8Bit(argv[7]) : QStringLiteral("audio");
    const bool    repeatBundle = mode == QLatin1String("av-bundle-repeat");
    const bool    avBundle     = mode == QLatin1String("av-bundle") || repeatBundle;
    const int     targetCalls  = repeatBundle ? 2 : 1;

    if ((role != QLatin1String("caller") && role != QLatin1String("callee"))
        || (mode != QLatin1String("audio") && !avBundle) || !localJid.isValid() || localJid.resource().isEmpty()
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

    bool                                   finishing          = false;
    bool                                   sessionActive      = false;
    bool                                   audioActive        = false;
    bool                                   videoActive        = false;
    bool                                   mediaStarted       = false;
    bool                                   mediaWindowArmed   = false;
    bool                                   localMediaVerified = false;
    bool                                   videoDecoded       = false;
    int                                    completedCalls     = 0;
    QPointer<J::Session>                   liveSession;
    QPointer<RTP::Application>             audioApp;
    QPointer<RTP::Application>             videoApp;
    std::unique_ptr<PsiMedia::VideoWidget> videoOutput;

    auto prepareIteration = [&]() {
        QFile::remove(outputPath);
        sessionActive      = false;
        audioActive        = false;
        videoActive        = false;
        mediaStarted       = false;
        mediaWindowArmed   = false;
        localMediaVerified = false;
        videoDecoded       = false;
        audioApp           = nullptr;
        videoApp           = nullptr;
        videoOutput.reset();
        if (avBundle) {
            videoOutput = std::make_unique<PsiMedia::VideoWidget>();
            QObject::connect(videoOutput.get(), &PsiMedia::VideoWidget::videoSizeChanged, &app, [&]() {
                videoDecoded = true;
                qInfo().noquote() << QStringLiteral("CALL_VIDEO_SIZE=%1x%2")
                                         .arg(videoOutput->sizeHint().width())
                                         .arg(videoOutput->sizeHint().height());
            });
        }
        qInfo().noquote() << QStringLiteral("CALL_ITERATION_ARMED=%1/%2").arg(completedCalls + 1).arg(targetCalls);
    };

    auto outputHasMedia = [&]() {
        QFileInfo fi(outputPath);
        return fi.exists() && fi.size() >= 4096;
    };

    std::function<void()> armMediaWindow;
    std::function<void()> maybeStartMedia;
    std::function<void()> startOutgoingCall;

    auto descriptionHasMid = [](const std::optional<RTP::Description> &description) {
        if (!description)
            return false;
        return std::any_of(description->headerExtensions.cbegin(), description->headerExtensions.cend(),
                           [](const RTP::HeaderExtension &extension) {
                               return extension.uri == QLatin1String("urn:ietf:params:rtp-hdrext:sdes:mid");
                           });
    };

    auto bundleNegotiated = [&]() {
        if (!avBundle || !liveSession || !audioApp || !videoApp)
            return !avBundle;
        auto containsPair = [&](const QList<J::ContentGroup> &groups) {
            return std::any_of(groups.cbegin(), groups.cend(), [&](const J::ContentGroup &group) {
                return group.semantics == QLatin1String("BUNDLE") && group.contents.contains(audioApp->contentName())
                    && group.contents.contains(videoApp->contentName());
            });
        };
        return containsPair(liveSession->groupings()) && containsPair(liveSession->remoteGroupings());
    };

    auto midNegotiated = [&]() {
        if (!avBundle)
            return true;
        return audioApp && videoApp && descriptionHasMid(audioApp->localDescription())
            && descriptionHasMid(audioApp->remoteDescription()) && descriptionHasMid(videoApp->localDescription())
            && descriptionHasMid(videoApp->remoteDescription());
    };

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
        QTimer::singleShot(avBundle ? 6000 : 3500, &app, [&]() {
            if (finishing || !liveSession || liveSession->state() == J::State::Finished)
                return;
            localMediaVerified = outputHasMedia();
            qInfo().noquote() << QStringLiteral("CALL_DECODED_BYTES=%1").arg(QFileInfo(outputPath).size());
            if (!sessionActive || !audioActive || !mediaStarted || !localMediaVerified) {
                finish(16, QStringLiteral("call did not carry decoded bidirectional audio"));
                return;
            }
            if (avBundle) {
                qInfo().noquote() << QStringLiteral("CALL_VIDEO_DECODED=%1").arg(videoDecoded ? 1 : 0);
                const bool midOk    = midNegotiated();
                const bool bundleOk = bundleNegotiated();
                qInfo().noquote() << QStringLiteral("CALL_MID_NEGOTIATED=%1").arg(midOk ? 1 : 0);
                qInfo().noquote() << QStringLiteral("CALL_BUNDLE_NEGOTIATED=%1").arg(bundleOk ? 1 : 0);
                if (!videoActive || !videoDecoded || !midOk || !bundleOk) {
                    finish(17, QStringLiteral("A/V BUNDLE call did not satisfy video/MID/grouping gates"));
                    return;
                }
            }
            if (role == QLatin1String("caller")) {
                qInfo("CALL_HANGUP=local-success");
                stopPsiMediaJingleTransmit(liveSession);
                liveSession->terminate(J::Reason::Success);
            }
        });
    };

    maybeStartMedia = [&]() {
        if (!liveSession || mediaStarted || !sessionActive || !audioActive || (avBundle && !videoActive))
            return;
        mediaStarted = startPsiMediaJingleTransmit(
            liveSession, true, true, QStringLiteral("audiotestsrc is-live=true wave=sine freq=440"), avBundle,
            avBundle ? QStringLiteral("videotestsrc is-live=true pattern=ball") : QString());
        qInfo().noquote() << QStringLiteral("CALL_MEDIA_STARTED=%1").arg(mediaStarted ? 1 : 0);
        if (!mediaStarted) {
            finish(31, QStringLiteral("psimedia transmit did not start"));
            return;
        }
        armMediaWindow();
    };

    QTimer::singleShot(repeatBundle ? 90000 : 45000, &app, [&]() {
        if (!finishing)
            finish(124, QStringLiteral("timeout"));
    });

    XmppEndpoint endpoint(localJid, password, &app);

    auto configureClient = [&]() -> bool {
        auto *manager    = endpoint.client()->jingleManager();
        auto *rtpManager = manager ? manager->rtpManager() : nullptr;
        auto *iceManager = endpoint.client()->jingleICEManager();
        if (!manager || !rtpManager || !iceManager)
            return false;

        PsiMediaJingleCapabilities caps;
        caps.backendAvailable      = true;
        caps.probeComplete         = true;
        const auto backendProfiles = PsiMedia::RtpSession::supportedSecureRtpProfiles();
        const auto dtlsProfiles    = XMPP::Dtls::supportedSRTPProfiles();
        caps.secureRtp
            = std::any_of(backendProfiles.cbegin(), backendProfiles.cend(),
                          [&dtlsProfiles](const QString &profile) { return dtlsProfiles.contains(profile); });
        caps.audio       = true;
        caps.audioInput  = true;
        caps.audioOutput = true;
        caps.video       = avBundle;
        caps.videoInput  = avBundle;
        if (!caps.secureRtp)
            return false;

        rtpManager->setMediaProvider(makePsiMediaJingleProvider(caps));
        // Match production preference: NSTransportsList selects from the end.\n        rtpManager->setTransportNamespaces({ ICE::NS_ICE_UDP, ICE::NS });
        iceManager->setAllowIpExposure(true);
        return true;
    };

    auto wireSession = [&](J::Session *session) {
        liveSession = session;
        QObject::connect(session, &J::Session::activated, session, [&]() {
            sessionActive = true;
            qInfo("CALL_SESSION=activated");
            maybeStartMedia();
        });
        QObject::connect(session, &J::Session::terminated, session, [&, session]() {
            if (liveSession != session) {
                qWarning("CALL_STALE_TERMINATION_IGNORED=1");
                return;
            }

            qInfo().noquote() << QStringLiteral("CALL_SESSION=terminated state=%1").arg(int(session->state()));
            if (!localMediaVerified)
                localMediaVerified = outputHasMedia();
            qInfo().noquote() << QStringLiteral("CALL_DECODED_BYTES=%1").arg(QFileInfo(outputPath).size());
            if (!localMediaVerified) {
                finish(32, QStringLiteral("no decoded remote audio reached output sink"));
                return;
            }
            if (avBundle) {
                const bool midOk    = midNegotiated();
                const bool bundleOk = bundleNegotiated();
                qInfo().noquote() << QStringLiteral("CALL_VIDEO_DECODED=%1").arg(videoDecoded ? 1 : 0);
                qInfo().noquote() << QStringLiteral("CALL_MID_NEGOTIATED=%1").arg(midOk ? 1 : 0);
                qInfo().noquote() << QStringLiteral("CALL_BUNDLE_NEGOTIATED=%1").arg(bundleOk ? 1 : 0);
                if (!videoDecoded || !midOk || !bundleOk) {
                    finish(34, QStringLiteral("terminated A/V call missed video/MID/BUNDLE evidence"));
                    return;
                }
            }

            stopPsiMediaJingleTransmit(session);
            ++completedCalls;
            qInfo().noquote()
                << QStringLiteral("CALL_ITERATION_RESULT=success %1/%2").arg(completedCalls).arg(targetCalls);

            if (completedCalls >= targetCalls) {
                finish(0,
                       role == QLatin1String("caller") ? QStringLiteral("caller completed")
                                                       : QStringLiteral("callee completed"));
                return;
            }

            liveSession = nullptr;
            audioApp    = nullptr;
            videoApp    = nullptr;

            const int completedSnapshot = completedCalls;
            const int restartDelayMs    = role == QLatin1String("caller") ? 800 : 300;
            QTimer::singleShot(restartDelayMs, &app, [&, completedSnapshot]() {
                if (finishing || completedCalls != completedSnapshot)
                    return;
                prepareIteration();
                if (role == QLatin1String("caller")) {
                    qInfo("CALL_RESTART=caller-starting-next");
                    startOutgoingCall();
                } else {
                    qInfo("CALL_RESTART=callee-ready-next");
                }
            });
        });
    };

    auto wireAudio = [&](RTP::Application *rtp) {
        audioApp = rtp;
        QObject::connect(rtp, &J::Application::stateChanged, rtp, [&, rtp](J::State state) {
            qInfo().noquote() << QStringLiteral("CALL_AUDIO_STATE=%1").arg(int(state));
            if (state != J::State::Active)
                return;
            audioActive       = true;
            const auto local  = rtp->localDescription();
            const auto remote = rtp->remoteDescription();
            qInfo().noquote() << QStringLiteral("CALL_NEGOTIATED media=audio local_payloads=%1 remote_payloads=%2")
                                     .arg(local ? local->payloads.size() : 0)
                                     .arg(remote ? remote->payloads.size() : 0);
            maybeStartMedia();
        });
    };

    auto wireVideo = [&](RTP::Application *rtp) {
        videoApp = rtp;
        QObject::connect(rtp, &J::Application::stateChanged, rtp, [&, rtp](J::State state) {
            qInfo().noquote() << QStringLiteral("CALL_VIDEO_STATE=%1").arg(int(state));
            if (state != J::State::Active)
                return;
            videoActive       = true;
            const auto local  = rtp->localDescription();
            const auto remote = rtp->remoteDescription();
            qInfo().noquote() << QStringLiteral("CALL_NEGOTIATED media=video local_payloads=%1 remote_payloads=%2")
                                     .arg(local ? local->payloads.size() : 0)
                                     .arg(remote ? remote->payloads.size() : 0);
            maybeStartMedia();
        });
    };

    startOutgoingCall = [&]() {
        if (finishing || role != QLatin1String("caller"))
            return;

        auto *manager    = endpoint.client()->jingleManager();
        auto *rtpManager = manager ? manager->rtpManager() : nullptr;
        if (!manager || !rtpManager) {
            finish(13, QStringLiteral("cannot access outgoing Jingle/RTP managers"));
            return;
        }

        qInfo().noquote() << QStringLiteral("CALL_ITERATION_START=%1/%2").arg(completedCalls + 1).arg(targetCalls);

        auto *session = manager->newSession(peerJid);
        if (!session) {
            finish(13, QStringLiteral("cannot create outgoing Jingle session"));
            return;
        }
        wireSession(session);

        auto *outgoingAudio = rtpManager->createOutgoing(session, QStringLiteral("audio"), J::Origin::Both);
        if (!outgoingAudio) {
            finish(14, QStringLiteral("cannot create outgoing audio RTP application"));
            return;
        }
        wireAudio(outgoingAudio);

        RTP::Application *outgoingVideo = nullptr;
        if (avBundle) {
            outgoingVideo = rtpManager->createOutgoing(session, QStringLiteral("video"), J::Origin::Both);
            if (!outgoingVideo) {
                finish(18, QStringLiteral("cannot create outgoing video RTP application"));
                return;
            }
            wireVideo(outgoingVideo);
            if (!session->setGroupings({ J::ContentGroup {
                    QStringLiteral("BUNDLE"), { outgoingAudio->contentName(), outgoingVideo->contentName() } } })) {
                finish(19, QStringLiteral("cannot propose outgoing A/V BUNDLE"));
                return;
            }
        }

        const QString sink = QStringLiteral("filesink location=\"%1\" sync=false").arg(outputPath);
        if (!configurePsiMediaJingleSession(session, sink, QString(), false, 128)) {
            finish(15, QStringLiteral("cannot configure caller psimedia backend"));
            return;
        }
        if (avBundle && !setPsiMediaJingleVideoOutput(session, videoOutput.get())) {
            finish(26, QStringLiteral("cannot configure caller video output"));
            return;
        }

        session->initiate();
    };

    if (role == QLatin1String("callee")) {
        QObject::connect(
            endpoint.client()->jingleManager(), &J::Manager::incomingSession, &app, [&](J::Session *session) {
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

                RTP::Application *incomingAudio = nullptr;
                RTP::Application *incomingVideo = nullptr;
                for (auto *base : session->contentList()) {
                    auto *rtp = dynamic_cast<RTP::Application *>(base);
                    if (!rtp)
                        continue;
                    if (rtp->media() == QLatin1String("audio"))
                        incomingAudio = rtp;
                    else if (rtp->media() == QLatin1String("video"))
                        incomingVideo = rtp;
                }
                if (!incomingAudio || (avBundle && !incomingVideo)) {
                    session->terminate(J::Reason::UnsupportedApplications);
                    finish(22, QStringLiteral("incoming call is missing required RTP content"));
                    return;
                }

                if (avBundle) {
                    std::optional<J::ContentGroup> offeredBundle;
                    for (const auto &group : session->remoteGroupings()) {
                        if (group.semantics == QLatin1String("BUNDLE")
                            && group.contents.contains(incomingAudio->contentName())
                            && group.contents.contains(incomingVideo->contentName())) {
                            offeredBundle = group;
                            break;
                        }
                    }
                    if (!offeredBundle || !session->setGroupings({ *offeredBundle })) {
                        session->terminate(J::Reason::IncompatibleParameters);
                        finish(24, QStringLiteral("incoming A/V call did not offer acceptable BUNDLE"));
                        return;
                    }
                }

                qInfo().noquote()
                    << QStringLiteral("CALL_ITERATION_START=%1/%2").arg(completedCalls + 1).arg(targetCalls);
                wireSession(session);
                wireAudio(incomingAudio);
                if (avBundle)
                    wireVideo(incomingVideo);
                const QString sink = QStringLiteral("filesink location=\"%1\" sync=false").arg(outputPath);
                if (!configurePsiMediaJingleSession(session, sink, QString(), false, 128)) {
                    finish(23, QStringLiteral("cannot configure callee psimedia backend"));
                    return;
                }
                if (avBundle && !setPsiMediaJingleVideoOutput(session, videoOutput.get())) {
                    finish(25, QStringLiteral("cannot configure callee video output"));
                    return;
                }

                if (!avBundle) {
                    session->accept();
                    return;
                }

                // Incoming transport payloads are committed asynchronously by Iris.
                // Do not accept a BUNDLE offer from inside incomingSession() before
                // both deferred ICE updates have reached Pending; otherwise shared
                // DTLS setup can run without the committed remote fingerprint.
                auto acceptBundleWhenReady = std::make_shared<std::function<void(int)>>();
                *acceptBundleWhenReady     = [&, session, incomingAudio, incomingVideo,
                                          acceptBundleWhenReady](int attempts) {
                    if (!session || session->state() >= J::State::Finishing)
                        return;
                    const auto audioTransport
                        = incomingAudio ? incomingAudio->transport() : QSharedPointer<J::Transport>();
                    const auto videoTransport
                        = incomingVideo ? incomingVideo->transport() : QSharedPointer<J::Transport>();
                    const bool ready = audioTransport && videoTransport && audioTransport->state() >= J::State::Pending
                        && videoTransport->state() >= J::State::Pending;
                    if (ready) {
                        qInfo("CALL_BUNDLE_REMOTE_TRANSPORTS=ready");
                        session->accept();
                        return;
                    }
                    if (attempts >= 100) {
                        finish(27, QStringLiteral("incoming BUNDLE ICE updates did not commit"));
                        return;
                    }
                    QTimer::singleShot(10, session,
                                           [acceptBundleWhenReady, attempts]() { (*acceptBundleWhenReady)(attempts + 1); });
                };
                QTimer::singleShot(0, session, [acceptBundleWhenReady]() { (*acceptBundleWhenReady)(0); });
            });
    }

    prepareIteration();

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
                qInfo().noquote() << QStringLiteral("Callee armed for incoming %1 call").arg(mode);
                return;
            }

            startOutgoingCall();
        },
        [&](const QString &message) { finish(9, message); });

    return app.exec();
}
