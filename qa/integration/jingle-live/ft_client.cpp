#include <iris/jingle-ft.h>
#include <iris/xmpp-im/jingle-ibb.h>
#include <iris/jingle-ice.h>
#include <iris/xmpp-im/jingle-s5b.h>
#include <iris/s5b.h>
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

#include <functional>
#include <optional>

using namespace XMPP;
namespace J = XMPP::Jingle;
namespace FT = XMPP::Jingle::FileTransfer;

static bool isIceNamespace(const QString &ns)
{
    return ns == J::ICE::NS || ns == J::ICE::NS_ICE_UDP;
}

static bool isKnownTransportNamespace(const QString &ns)
{
    return isIceNamespace(ns) || ns == J::S5B::NS || ns == J::IBB::NS;
}

static bool transportProfileSupported(const QString &profile)
{
    return profile == QLatin1String("all") || profile == QLatin1String("ice")
        || profile == QLatin1String("s5b") || profile == QLatin1String("ibb")
        || profile == QLatin1String("ice-s5b-replace") || profile == QLatin1String("s5b-ibb-replace");
}

static bool transportProfileAllows(const QString &profile, const QString &ns)
{
    if (profile == QLatin1String("all"))
        return true;
    if (profile == QLatin1String("ice"))
        return ns == J::ICE::NS;
    if (profile == QLatin1String("s5b"))
        return ns == J::S5B::NS;
    if (profile == QLatin1String("ibb"))
        return ns == J::IBB::NS;
    if (profile == QLatin1String("ice-s5b-replace"))
        return ns == J::ICE::NS || ns == J::S5B::NS;
    if (profile == QLatin1String("s5b-ibb-replace"))
        return ns == J::S5B::NS || ns == J::IBB::NS;
    return false;
}

static bool transportProfileNeedsReplace(const QString &profile)
{
    return profile.endsWith(QLatin1String("-replace"));
}

static QString expectedInitialTransport(const QString &profile)
{
    if (profile == QLatin1String("ice") || profile == QLatin1String("ice-s5b-replace"))
        return J::ICE::NS;
    if (profile == QLatin1String("s5b") || profile == QLatin1String("s5b-ibb-replace"))
        return J::S5B::NS;
    if (profile == QLatin1String("ibb"))
        return J::IBB::NS;
    return {};
}

static QString expectedActiveTransport(const QString &profile)
{
    if (profile == QLatin1String("ice-s5b-replace"))
        return J::S5B::NS;
    if (profile == QLatin1String("s5b-ibb-replace"))
        return J::IBB::NS;
    return expectedInitialTransport(profile);
}

static void installPeerFeatures(Client &client, const Jid &peer, const QString &transportProfile)
{
    // Mirror the feature set a real Psi/Iris client publishes, but allow the
    // live gate to constrain transport capabilities without bypassing normal
    // NSTransportsList selection. Hash/XEP-0300 and all non-transport features
    // remain exactly as Client::makeDiscoResult() publishes them.
    DiscoItem disco = client.makeDiscoResult(QStringLiteral("urn:psi-im:ci:jingle-live"));
    disco.setJid(peer);

    QStringList filtered;
    for (const auto &feature : disco.features().list()) {
        if (!isKnownTransportNamespace(feature) || transportProfileAllows(transportProfile, feature))
            filtered.append(feature);
    }
    disco.setFeatures(Features(filtered));

    const CapsSpec caps(disco);
    CapsRegistry::instance()->registerCaps(caps, disco);
    client.capsManager()->updateCaps(peer, caps);
}

class XmppEndpoint final : public QObject {
public:
    XmppEndpoint(Jid jid, QString password, QObject *parent = nullptr)
        : QObject(parent), jid_(std::move(jid)), password_(std::move(password))
    {
        // Production Psi registers the S5B server scope before handing the
        // reserver to Iris. Mirror that setup in the live fixture so
        // S5B::Transport::prepare() can discover direct SOCKS5 candidates.
        portReserver_.registerScope(QStringLiteral("s5b"), new S5BServersProducer);
        client_.setTcpPortReserver(&portReserver_);
    }

    ~XmppEndpoint() override
    {
        if (stream_) {
            client_.close();
            delete stream_;
            stream_ = nullptr;
        }
        delete connector_;
    }

    Client *client() { return &client_; }
    const Jid &requestedJid() const { return jid_; }

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

        connect(stream_, &ClientStream::needAuthParams, this,
                [this](bool user, bool pass, bool realm) {
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
        connect(&client_, &Client::xmlIncoming, this, [this](const QString &xml) {
            if (!readyState_)
                return;
            if (xml.contains(QStringLiteral("<iq"))
                && (xml.contains(QStringLiteral("urn:xmpp:jingle:1"))
                    || xml.contains(QStringLiteral("type=\\\"error\\\""))
                    || xml.contains(QStringLiteral("type='error'"))
                    || xml.contains(QStringLiteral("type=\\\"result\\\""))
                    || xml.contains(QStringLiteral("type='result'")))) {
                qInfo().noquote() << QStringLiteral("XMPP_IN=%1").arg(xml.trimmed());
            }
        });
        connect(&client_, &Client::xmlOutgoing, this, [this](const QString &xml) {
            if (!readyState_)
                return;
            if (xml.contains(QStringLiteral("<iq"))
                && (xml.contains(QStringLiteral("urn:xmpp:jingle:1"))
                    || xml.contains(QStringLiteral("type=\\\"error\\\""))
                    || xml.contains(QStringLiteral("type='error'"))
                    || xml.contains(QStringLiteral("type=\\\"result\\\""))
                    || xml.contains(QStringLiteral("type='result'")))) {
                qInfo().noquote() << QStringLiteral("XMPP_OUT=%1").arg(xml.trimmed());
            }
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

    if (argc < 6) {
        qCritical() << "Usage:" << argv[0]
                    << "<sender|receiver> <jid/resource> <password> <peer/resource> <file> [ready-file] [transport-profile]";
        return 2;
    }

    const QString role = QString::fromLocal8Bit(argv[1]);
    const Jid localJid(QString::fromLocal8Bit(argv[2]));
    const QString password = QString::fromLocal8Bit(argv[3]);
    const Jid peerJid(QString::fromLocal8Bit(argv[4]));
    const QString filePath = QString::fromLocal8Bit(argv[5]);
    const QString readyPath = argc > 6 ? QString::fromLocal8Bit(argv[6]) : QString();
    const QString transportProfile = argc > 7 ? QString::fromLocal8Bit(argv[7]) : QStringLiteral("all");

    if ((role != QLatin1String("sender") && role != QLatin1String("receiver"))
        || !localJid.isValid() || localJid.resource().isEmpty()
        || !peerJid.isValid() || peerJid.resource().isEmpty()
        || !transportProfileSupported(transportProfile)) {
        qCritical() << "Invalid role, full JID arguments, or transport profile";
        return 3;
    }

    qInfo().noquote() << QStringLiteral("FT_PROFILE=%1").arg(transportProfile);

    bool finishing = false;
    bool senderSawFinishing = false;
    bool receiverSawFinishing = false;
    bool senderTransferFinished = false;
    bool receiverTransferFinished = false;
    bool replaceRequested = false;
    bool gotSession = false;
    QString initialTransport;
    QString activeTransport;
    auto finish = [&](int code, const QString &message) {
        if (finishing)
            return;
        finishing = true;
        if (code == 0)
            qInfo().noquote() << QStringLiteral("LIVE_FT_RESULT=success %1").arg(message);
        else
            qCritical().noquote() << QStringLiteral("LIVE_FT_RESULT=failure %1").arg(message);
        QTimer::singleShot(250, &app, [&, code]() { app.exit(code); });
    };

    QTimer::singleShot(45000, &app, [&]() {
        if (!finishing)
            finish(124, QStringLiteral("timeout"));
    });

    XmppEndpoint endpoint(localJid, password, &app);

    if (role == QLatin1String("receiver")) {
        QObject::connect(endpoint.client()->jingleManager(), &J::Manager::incomingSession, &app,
                         [&](J::Session *session) {
                             if (gotSession) {
                                 finish(20, QStringLiteral("duplicate incoming Jingle session"));
                                 return;
                             }
                             gotSession = true;

                             if (session->preferredApplication() != FT::NS || session->contentList().size() != 1) {
                                 session->terminate(J::Reason::Condition::UnsupportedApplications);
                                 finish(21, QStringLiteral("unexpected incoming Jingle application"));
                                 return;
                             }

                             auto *base = session->contentList().constBegin().value();
                             if (!base || base->pad()->ns() != FT::NS) {
                                 session->terminate(J::Reason::Condition::UnsupportedApplications);
                                 finish(22, QStringLiteral("incoming content is not Jingle file transfer"));
                                 return;
                             }
                             auto *transfer = static_cast<FT::Application *>(base);
                             QObject::connect(session, &J::Session::activated, session, []() {
                                 qInfo("FT_RECEIVER_SESSION=activated");
                             });
                             QObject::connect(session, &J::Session::terminated, session, [&, session]() {
                                 qInfo().noquote()
                                     << QStringLiteral("FT_RECEIVER_SESSION=terminated state=%1")
                                            .arg(int(session->state()));
                                 const auto error = session->lastError();
                                 if (error) {
                                     qInfo().noquote()
                                         << QStringLiteral("FT_RECEIVER_SESSION_ERROR=%1 cond=%2 app=%3")
                                                .arg(error->toString())
                                                .arg(int(error->condition))
                                                .arg(error->appSpec.tagName());
                                 }
                                 if (receiverTransferFinished)
                                     finish(0, QStringLiteral("receiver completed"));
                                 else if (!finishing)
                                     finish(25, QStringLiteral("receiver session terminated before file completion"));
                             });
                             qInfo().noquote()
                                 << QStringLiteral("FT_INCOMING_FILE=%1 SIZE=%2")
                                        .arg(transfer->file().name())
                                        .arg(transfer->file().size().value_or(0));

                             if (transfer->transport()) {
                                 qInfo().noquote()
                                     << QStringLiteral("FT_RECEIVER_TRANSPORT=%1")
                                            .arg(transfer->transport()->pad()->ns());
                             }

                             QObject::connect(transfer, &FT::Application::deviceRequested, transfer,
                                              [&, transfer](quint64 offset, std::optional<quint64>) {
                                                  auto *out = new QFile(filePath, transfer);
                                                  QIODevice::OpenMode mode = QIODevice::WriteOnly;
                                                  if (offset == 0)
                                                      mode |= QIODevice::Truncate;
                                                  if (!out->open(mode) || !out->seek(qint64(offset))) {
                                                      delete out;
                                                      transfer->setDevice(nullptr);
                                                      finish(23, QStringLiteral("cannot open destination file"));
                                                      return;
                                                  }
                                                  transfer->setDevice(out);
                                              });

                             QObject::connect(transfer, &J::Application::stateChanged, transfer,
                                              [&, session, transfer](J::State state) {
                                                  qInfo().noquote()
                                                      << QStringLiteral("FT_RECEIVER_APP_STATE=%1").arg(int(state));
                                                  if (state == J::State::Finishing) {
                                                      receiverSawFinishing = true;
                                                      qInfo("FT_RECEIVER_DRAIN=finishing");
                                                      if (!transfer->connection())
                                                          finish(26, QStringLiteral("receiver lost connection before drain"));
                                                      return;
                                                  }
                                                  if (state != J::State::Finished)
                                                      return;
                                                  if (!receiverSawFinishing) {
                                                      finish(27, QStringLiteral("receiver skipped Finishing state"));
                                                      return;
                                                  }
                                                  if (transfer->connection()) {
                                                      finish(28, QStringLiteral("receiver retained connection after drain"));
                                                      return;
                                                  }
                                                  const auto reason = transfer->lastReason();
                                                  if (!reason.isValid() || reason.condition() != J::Reason::Success) {
                                                      finish(24, QStringLiteral("receiver transfer finished unsuccessfully"));
                                                      return;
                                                  }
                                                  receiverTransferFinished = true;
                                                  qInfo("FT_RECEIVER_TRANSFER=finished; terminating Jingle session");
                                                  session->terminate(J::Reason::Condition::Success);
                                              });

                             session->accept();
                         });
    }

    endpoint.start(
        [&]() {
            installPeerFeatures(*endpoint.client(), peerJid, transportProfile);

            if (role == QLatin1String("receiver")) {
                if (readyPath.isEmpty()) {
                    finish(30, QStringLiteral("receiver ready-file path missing"));
                    return;
                }
                QFile ready(readyPath);
                if (!ready.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    finish(31, QStringLiteral("cannot create receiver ready file"));
                    return;
                }
                ready.write(endpoint.client()->jid().full().toUtf8());
                ready.write("\n");
                ready.close();
                qInfo("Receiver armed for incoming Jingle FT");
                return;
            }

            const J::TransportFeatures requirements
                = J::TransportFeature::Reliable | J::TransportFeature::Ordered | J::TransportFeature::DataOriented;
            const QStringList transports = endpoint.client()->jingleManager()->availableTransports(requirements);
            qInfo().noquote()
                << QStringLiteral("FT_AVAILABLE_TRANSPORTS=%1").arg(transports.join(QLatin1Char(',')));
            bool hasIce = false;
            for (const auto &ns : transports)
                hasIce = hasIce || isIceNamespace(ns);
            if (!hasIce) {
                finish(40, QStringLiteral("ICE is not data-capable in this SCTP/QCA3 build"));
                return;
            }

            auto *session = endpoint.client()->jingleManager()->newSession(peerJid);
            if (!session) {
                finish(41, QStringLiteral("failed to create outgoing Jingle session"));
                return;
            }
            QObject::connect(session, &J::Session::terminated, session, [&, session]() {
                qInfo().noquote()
                    << QStringLiteral("FT_SENDER_SESSION=terminated state=%1").arg(int(session->state()));
                const auto error = session->lastError();
                if (error) {
                    qInfo().noquote()
                        << QStringLiteral("FT_SENDER_SESSION_ERROR=%1 cond=%2 app=%3")
                               .arg(error->toString())
                               .arg(int(error->condition))
                               .arg(error->appSpec.tagName());
                }
                if (senderTransferFinished)
                    finish(0, QStringLiteral("sender completed"));
                else if (!finishing)
                    finish(46, QStringLiteral("Jingle session terminated before file completion"));
            });

            auto *base = session->newContent(FT::NS, session->role());
            auto *transfer = static_cast<FT::Application *>(base);
            if (!transfer) {
                finish(42, QStringLiteral("Jingle file-transfer application unavailable"));
                return;
            }

            QObject::connect(session, &J::Session::activated, session, [&, transfer]() {
                qInfo("FT_SENDER_SESSION=activated");
                if (!transportProfileNeedsReplace(transportProfile) || replaceRequested)
                    return;

                const auto current = transfer->transport();
                const auto expected = expectedInitialTransport(transportProfile);
                const auto currentNs = current ? current->pad()->ns() : QString();
                if (!current || transfer->state() != J::State::Connecting || currentNs != expected) {
                    finish(48, QStringLiteral("replacement boundary did not retain expected initial transport"));
                    return;
                }

                replaceRequested = true;
                qInfo().noquote() << QStringLiteral("FT_REPLACE_FROM=%1").arg(currentNs);
                current->stop();
                if (!transfer->selectNextTransport()) {
                    finish(49, QStringLiteral("failed to select replacement transport"));
                    return;
                }
                const auto replacement = transfer->transport();
                if (!replacement) {
                    finish(50, QStringLiteral("replacement transport is null"));
                    return;
                }
                qInfo().noquote()
                    << QStringLiteral("FT_REPLACE_SELECTED=%1").arg(replacement->pad()->ns());
            });

            QObject::connect(transfer, &J::Application::stateChanged, transfer,
                             [&, transfer](J::State state) {
                                 qInfo().noquote()
                                     << QStringLiteral("FT_SENDER_APP_STATE=%1").arg(int(state));
                                 if (initialTransport.isEmpty() && transfer->transport()) {
                                     initialTransport = transfer->transport()->pad()->ns();
                                     qInfo().noquote()
                                         << QStringLiteral("FT_INITIAL_TRANSPORT=%1").arg(initialTransport);
                                     const auto expected = expectedInitialTransport(transportProfile);
                                     if (!expected.isEmpty() && initialTransport != expected) {
                                         finish(43, QStringLiteral("file transfer selected unexpected initial transport"));
                                         return;
                                     }
                                 }
                                 if (state == J::State::Active && transfer->transport()) {
                                     activeTransport = transfer->transport()->pad()->ns();
                                     qInfo().noquote()
                                         << QStringLiteral("FT_ACTIVE_TRANSPORT=%1").arg(activeTransport);
                                     const auto expected = expectedActiveTransport(transportProfile);
                                     if (!expected.isEmpty() && activeTransport != expected) {
                                         finish(51, QStringLiteral("file transfer activated unexpected transport"));
                                         return;
                                     }
                                     if (transportProfileNeedsReplace(transportProfile) && !replaceRequested) {
                                         finish(52, QStringLiteral("replacement profile reached Active without replacement"));
                                         return;
                                     }
                                 }
                                 if (state == J::State::Finishing) {
                                     senderSawFinishing = true;
                                     qInfo("FT_SENDER_DRAIN=finishing");
                                     if (!transfer->connection())
                                         finish(53, QStringLiteral("sender lost connection before drain"));
                                     return;
                                 }
                                 if (state != J::State::Finished)
                                     return;
                                 if (!senderSawFinishing) {
                                     finish(54, QStringLiteral("sender skipped Finishing state"));
                                     return;
                                 }
                                 if (transfer->connection()) {
                                     finish(55, QStringLiteral("sender retained connection after drain"));
                                     return;
                                 }
                                 const auto reason = transfer->lastReason();
                                 if (!reason.isValid() || reason.condition() != J::Reason::Success) {
                                     finish(44, QStringLiteral("sender transfer finished unsuccessfully"));
                                     return;
                                 }
                                 senderTransferFinished = true;
                                 qInfo("FT_SENDER_TRANSFER=finished; waiting for peer session-terminate");
                             });

            QObject::connect(transfer, &FT::Application::deviceRequested, transfer,
                             [&, transfer](quint64 offset, std::optional<quint64>) {
                                 auto *in = new QFile(filePath, transfer);
                                 if (!in->open(QIODevice::ReadOnly) || !in->seek(qint64(offset))) {
                                     delete in;
                                     transfer->setDevice(nullptr);
                                     finish(45, QStringLiteral("cannot open source file"));
                                     return;
                                 }
                                 transfer->setDevice(in);
                             });

            const QFileInfo input(filePath);
            if (!input.isFile() || !input.isReadable()) {
                finish(47, QStringLiteral("source file is not readable"));
                return;
            }
            transfer->setFile(input, QStringLiteral("Prosody live Jingle FT smoke"), Thumbnail());
            if (qEnvironmentVariableIsSet("PSI_JINGLE_FT_TEST_TRUNCATE_SOURCE")) {
                QFile truncated(filePath);
                if (!truncated.open(QIODevice::ReadWrite)) {
                    finish(56, QStringLiteral("cannot open source for truncation fault"));
                    return;
                }
                const auto advertisedSize = truncated.size();
                if (advertisedSize < 2 || !truncated.resize(advertisedSize / 2)) {
                    finish(57, QStringLiteral("cannot truncate source for fault injection"));
                    return;
                }
                qInfo().noquote()
                    << QStringLiteral("FT_TEST_SOURCE_TRUNCATED advertised=%1 actual=%2")
                           .arg(advertisedSize)
                           .arg(truncated.size());
                truncated.close();
            }
            session->addContent(transfer);
            session->initiate();
        },
        [&](const QString &message) { finish(10, message); });

    return app.exec();
}
