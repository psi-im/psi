#include <iris/jingle-rtp.h>
#include <iris/jingle.h>
#include <iris/xmpp.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_clientstream.h>
#include <iris/xmpp_message.h>
#include <iris/xmpp_status.h>
#include <iris/xmpp_tasks.h>

#include <QtCrypto>

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTimer>

#include <functional>
#include <utility>

using namespace XMPP;
namespace J = XMPP::Jingle;
namespace RTP = XMPP::Jingle::RTP;

class XmppEndpoint final : public QObject {
public:
    XmppEndpoint(Jid jid, QString password, QObject *parent = nullptr)
        : QObject(parent), jid_(std::move(jid)), password_(std::move(password))
    {
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
            if (xml.contains(QStringLiteral("urn:xmpp:jingle-message:0")))
                qInfo().noquote() << QStringLiteral("XMPP_IN=%1").arg(xml.trimmed());
        });
        connect(&client_, &Client::xmlOutgoing, this, [](const QString &xml) {
            if (xml.contains(QStringLiteral("urn:xmpp:jingle-message:0")))
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

    if (argc != 6) {
        qCritical() << "Usage:" << argv[0]
                    << "<sender|receiver> <jid/resource> <password> <peer-jid> <ready-file>";
        return 2;
    }

    const QString role = QString::fromLocal8Bit(argv[1]);
    const Jid localJid(QString::fromLocal8Bit(argv[2]));
    const QString password = QString::fromLocal8Bit(argv[3]);
    const Jid peerJid(QString::fromLocal8Bit(argv[4]));
    const QString readyPath = QString::fromLocal8Bit(argv[5]);

    if ((role != QLatin1String("sender") && role != QLatin1String("receiver"))
        || !localJid.isValid() || localJid.resource().isEmpty() || !peerJid.isValid()) {
        qCritical() << "Invalid live-JMI arguments";
        return 3;
    }

    bool finishing = false;
    auto finish = [&](int code, const QString &message) {
        if (finishing)
            return;
        finishing = true;
        if (code == 0)
            qInfo().noquote() << QStringLiteral("LIVE_JMI_RESULT=success %1").arg(message);
        else
            qCritical().noquote() << QStringLiteral("LIVE_JMI_RESULT=failure %1").arg(message);
        QTimer::singleShot(50, &app, [&, code]() { app.exit(code); });
    };

    QTimer::singleShot(15000, &app, [&]() {
        if (!finishing)
            finish(124, QStringLiteral("timeout"));
    });

    XmppEndpoint endpoint(localJid, password, &app);

    bool genericSeen = false;
    bool typedSeen = false;
    bool messageSeen = false;

    if (role == QLatin1String("receiver")) {
        auto *manager = endpoint.client()->jingleManager();
        auto *rtp = manager->rtpManager();
        manager->setMessageInitiationEnabled(true);

        QObject::connect(manager, &J::Manager::incomingMessageInitiation, &app,
                         [&](const Message &message, const J::MessageInitiation &initiation) {
            qInfo().noquote() << "LIVE_JMI_GENERIC id=" << initiation.id()
                              << "from=" << message.from().full()
                              << "descriptions=" << initiation.descriptions().size();
            if (initiation.action() != J::MessageInitiation::Action::Propose
                || initiation.id() != QLatin1String("jmi-live-audio")
                || initiation.descriptions().size() != 1) {
                finish(20, QStringLiteral("generic JMI proposal mismatch"));
                return;
            }
            genericSeen = true;
        });

        QObject::connect(rtp, &RTP::Manager::incomingProposal, &app,
                         [&](const Message &message, const QString &id, RTP::MediaSet media) {
            qInfo().noquote() << "LIVE_JMI_TYPED id=" << id
                              << "from=" << message.from().full()
                              << "audio=" << media.testFlag(RTP::Media::Audio)
                              << "video=" << media.testFlag(RTP::Media::Video);
            if (id != QLatin1String("jmi-live-audio")
                || !media.testFlag(RTP::Media::Audio)
                || media.testFlag(RTP::Media::Video)) {
                finish(21, QStringLiteral("typed RTP proposal mismatch"));
                return;
            }
            typedSeen = true;
            if (genericSeen && messageSeen)
                finish(0, QStringLiteral("typed audio proposal delivered through Prosody"));
        });

        QObject::connect(endpoint.client(), &Client::messageReceived, &app, [&](const Message &message) {
            const auto effective = message.displayMessage();
            const auto initiation = effective.jingleMessageInitiation();
            if (!initiation.isValid())
                return;
            const bool receipt = effective.messageReceipt() == ReceiptRequest;
            const bool store = effective.processingHints().testFlag(Message::Store);
            qInfo().noquote() << "LIVE_JMI_MESSAGE id=" << initiation.id()
                              << "receipt=" << receipt << "store=" << store;
            if (!receipt || !store) {
                finish(22, QStringLiteral("receipt/store metadata was not preserved"));
                return;
            }
            messageSeen = true;
            if (genericSeen && typedSeen)
                finish(0, QStringLiteral("typed audio proposal delivered through Prosody"));
        });
    }

    endpoint.start(
        [&]() {
            if (role == QLatin1String("receiver")) {
                // The real JMI proposal is addressed to a bare JID. Publish
                // available presence first so Prosody has a routable resource.
                endpoint.client()->setPresence(Status());
                QTimer::singleShot(200, &app, [&]() {
                    QFile ready(readyPath);
                    if (!ready.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                        finish(10, QStringLiteral("cannot create receiver ready file"));
                        return;
                    }
                    ready.write(endpoint.client()->jid().full().toUtf8());
                    ready.write("\n");
                    ready.close();
                    qInfo("Receiver armed for bare-JID JMI proposal");
                });
                return;
            }

            const QString raw = QStringLiteral(
                "<message id='jm-propose-live' to='%1' type='chat' xml:lang='en'>"
                "<propose xmlns='urn:xmpp:jingle-message:0' id='jmi-live-audio'>"
                "<description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'/>"
                "</propose>"
                "<request xmlns='urn:xmpp:receipts'/>"
                "<store xmlns='urn:xmpp:hints'/>"
                "</message>")
                .arg(peerJid.bare());

            qInfo().noquote() << "LIVE_JMI_SEND=" << raw;
            endpoint.client()->send(raw);
            QTimer::singleShot(500, &app, [&]() {
                finish(0, QStringLiteral("proposal sent"));
            });
        },
        [&](const QString &message) { finish(9, message); });

    return app.exec();
}
