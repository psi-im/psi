#include <iris/jingle-ft.h>
#include <iris/jingle-ice.h>
#include <iris/jingle-session.h>
#include <iris/tcpportreserver.h>
#include <iris/xmpp.h>
#include <iris/xmpp_caps.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_clientstream.h>
#include <iris/xmpp_tasks.h>

#include <QtCrypto>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDomDocument>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

#ifdef __GLIBC__
#include <malloc.h>
#endif

#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

using namespace XMPP;
namespace J  = XMPP::Jingle;
namespace FT = XMPP::Jingle::FileTransfer;

static bool isIceNamespace(const QString &ns) { return ns == J::ICE::NS || ns == J::ICE::NS_ICE_UDP; }

static void installIcePeerFeatures(Client &client, const Jid &peer)
{
    DiscoItem disco = client.makeDiscoResult(QStringLiteral("urn:psi-im:ci:jingle-ft-memory"));
    disco.setJid(peer);

    QStringList filtered;
    for (const auto &feature : disco.features().list()) {
        if (feature == J::S5B::NS || feature == J::IBB::NS)
            continue;
        filtered.append(feature);
    }
    disco.setFeatures(Features(filtered));

    const CapsSpec caps(disco);
    CapsRegistry::instance()->registerCaps(caps, disco);
    client.capsManager()->updateCaps(peer, caps);
}

class XmppEndpoint final : public QObject {
public:
    XmppEndpoint(Jid jid, QString password, QObject *parent = nullptr) :
        QObject(parent), jid_(std::move(jid)), password_(std::move(password))
    {
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

    Client    *client() { return &client_; }
    const Jid &jid() const { return jid_; }

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
        connect(stream_, &ClientStream::warning, this, [this](int) { stream_->continueAfterWarning(); });
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

struct MemorySample {
    int     iteration = 0;
    qint64  rssKb = -1;
    qint64  privateKb = -1;
    qint64  heapBytes = -1;
};

static qint64 smapsValueKb(const QByteArray &line, const QByteArray &key)
{
    if (!line.startsWith(key))
        return -1;
    const auto value = line.mid(key.size()).trimmed().split(' ').value(0);
    bool       ok    = false;
    const auto parsed = value.toLongLong(&ok);
    return ok ? parsed : -1;
}

static MemorySample sampleMemory(int iteration)
{
#ifdef __GLIBC__
    malloc_trim(0);
#endif

    MemorySample sample;
    sample.iteration = iteration;

    QFile smaps(QStringLiteral("/proc/self/smaps_rollup"));
    if (smaps.open(QIODevice::ReadOnly)) {
        qint64 privateClean = 0;
        qint64 privateDirty = 0;
        while (!smaps.atEnd()) {
            const auto line = smaps.readLine();
            const auto rss  = smapsValueKb(line, QByteArrayLiteral("Rss:"));
            if (rss >= 0)
                sample.rssKb = rss;
            const auto clean = smapsValueKb(line, QByteArrayLiteral("Private_Clean:"));
            if (clean >= 0)
                privateClean = clean;
            const auto dirty = smapsValueKb(line, QByteArrayLiteral("Private_Dirty:"));
            if (dirty >= 0)
                privateDirty = dirty;
        }
        sample.privateKb = privateClean + privateDirty;
    }

#ifdef __GLIBC__
    const auto info = mallinfo2();
    sample.heapBytes = qint64(info.uordblks);
#endif

    qInfo().noquote() << QStringLiteral("FT_MEMORY iteration=%1 rss_kb=%2 private_kb=%3 heap_bytes=%4")
                             .arg(sample.iteration)
                             .arg(sample.rssKb)
                             .arg(sample.privateKb)
                             .arg(sample.heapBytes);
    return sample;
}

static QByteArray fileSha256(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const auto chunk = file.read(64 * 1024);
        if (chunk.isEmpty() && file.error() != QFile::NoError)
            return {};
        hash.addData(chunk);
    }
    return hash.result();
}

static double regressionSlope(const std::vector<MemorySample> &samples, qint64 MemorySample::*member)
{
    // Sample 0 is before the first transfer. Sample 1 includes one-time SCTP,
    // ICE and crypto initialization. Treat it as warm-up and fit the remaining
    // completed transfer samples only.
    if (samples.size() < 4)
        return 0.0;

    const size_t first = 2;
    const size_t count = samples.size() - first;
    double       sx = 0.0;
    double       sy = 0.0;
    double       sxx = 0.0;
    double       sxy = 0.0;
    size_t       valid = 0;
    for (size_t i = first; i < samples.size(); ++i) {
        const qint64 yValue = samples[i].*member;
        if (yValue < 0)
            continue;
        const double x = double(samples[i].iteration);
        const double y = double(yValue);
        sx += x;
        sy += y;
        sxx += x * x;
        sxy += x * y;
        ++valid;
    }
    if (valid < 2)
        return 0.0;
    const double denominator = double(valid) * sxx - sx * sx;
    return denominator == 0.0 ? 0.0 : (double(valid) * sxy - sx * sy) / denominator;
}

class FtMemoryProbe final : public QObject {
public:
    FtMemoryProbe(QCoreApplication &app, Jid senderJid, QString senderPassword, Jid receiverJid,
                  QString receiverPassword, QString sourcePath, QString destinationPath, int iterations) :
        app_(app), sender_(std::move(senderJid), std::move(senderPassword), this),
        receiver_(std::move(receiverJid), std::move(receiverPassword), this), sourcePath_(std::move(sourcePath)),
        destinationPath_(std::move(destinationPath)), iterations_(iterations)
    {
        timeout_.setSingleShot(true);
        timeout_.setInterval(180000);
        connect(&timeout_, &QTimer::timeout, this, [this]() { fail(124, QStringLiteral("probe timeout")); });

        connect(receiver_.client()->jingleManager(), &J::Manager::incomingSession, this,
                [this](J::Session *session) { acceptIncoming(session); });
    }

    void start()
    {
        timeout_.start();
        sender_.start([this]() { endpointReady(true); }, [this](const QString &error) { fail(10, error); });
        receiver_.start([this]() { endpointReady(false); }, [this](const QString &error) { fail(11, error); });
    }

private:
    void endpointReady(bool sender)
    {
        if (sender)
            senderReady_ = true;
        else
            receiverReady_ = true;
        if (!senderReady_ || !receiverReady_ || started_)
            return;

        started_ = true;
        installIcePeerFeatures(*sender_.client(), receiver_.client()->jid());
        installIcePeerFeatures(*receiver_.client(), sender_.client()->jid());

        const J::TransportFeatures requirements
            = J::TransportFeature::Reliable | J::TransportFeature::Ordered | J::TransportFeature::DataOriented;
        bool hasIce = false;
        for (const auto &ns : sender_.client()->jingleManager()->availableTransports(requirements))
            hasIce = hasIce || isIceNamespace(ns);
        if (!hasIce) {
            fail(12, QStringLiteral("ICE is not data-capable"));
            return;
        }

        samples_.push_back(sampleMemory(0));
        QTimer::singleShot(0, this, [this]() { startNextTransfer(); });
    }

    void resetIterationState()
    {
        senderSawFinishing_       = false;
        receiverSawFinishing_     = false;
        senderTransferFinished_   = false;
        receiverTransferFinished_ = false;
        senderSawReceipt_         = false;
        senderTerminated_         = false;
        receiverTerminated_       = false;
        senderDestroyed_          = false;
        receiverDestroyed_        = false;
        settleScheduled_          = false;
    }

    void startNextTransfer()
    {
        if (failed_)
            return;
        if (iteration_ >= iterations_) {
            reportAndFinish();
            return;
        }

        ++iteration_;
        resetIterationState();
        QFile::remove(destinationPath_);
        qInfo().noquote() << QStringLiteral("FT_MEMORY_TRANSFER_BEGIN iteration=%1").arg(iteration_);

        auto *session = sender_.client()->jingleManager()->newSession(receiver_.client()->jid());
        if (!session) {
            fail(20, QStringLiteral("failed to create outgoing Jingle session"));
            return;
        }

        connect(session, &QObject::destroyed, this, [this]() {
            senderDestroyed_ = true;
            maybeSettleIteration();
        });
        connect(session, &J::Session::terminated, this, [this, session]() {
            senderTerminated_ = true;
            const auto error = session->lastError();
            if (error) {
                fail(21, QStringLiteral("sender session terminated with error: %1").arg(error->toString()));
                return;
            }
            if (!senderTransferFinished_) {
                fail(22, QStringLiteral("sender session terminated before transfer completion"));
                return;
            }
            maybeSettleIteration();
        });

        auto *base     = session->newContent(FT::NS, session->role());
        auto *transfer = static_cast<FT::Application *>(base);
        if (!transfer) {
            fail(23, QStringLiteral("Jingle file-transfer application unavailable"));
            return;
        }

        connect(sender_.client(), &Client::xmlIncoming, transfer, [this, session, transfer](const QString &xml) {
            QDomDocument doc;
            if (!doc.setContent(xml, true))
                return;
            const auto iq     = doc.documentElement();
            const auto jingle = iq.firstChildElement(QStringLiteral("jingle"));
            if (iq.tagName() != QLatin1String("iq") || iq.attribute(QStringLiteral("type")) != QLatin1String("set")
                || jingle.namespaceURI() != QLatin1String("urn:xmpp:jingle:1")
                || jingle.attribute(QStringLiteral("sid")) != session->sid()
                || jingle.attribute(QStringLiteral("action")) != QLatin1String("session-info"))
                return;
            for (auto received = jingle.firstChildElement(); !received.isNull();
                 received = received.nextSiblingElement()) {
                if (received.tagName() == QLatin1String("received") && received.namespaceURI() == FT::NS
                    && received.attribute(QStringLiteral("name")) == transfer->contentName()) {
                    senderSawReceipt_ = true;
                }
            }
        });

        connect(transfer, &FT::Application::deviceRequested, transfer,
                [this, transfer](quint64 offset, std::optional<quint64>) {
                    auto *input = new QFile(sourcePath_, transfer);
                    if (!input->open(QIODevice::ReadOnly) || !input->seek(qint64(offset))) {
                        delete input;
                        transfer->setDevice(nullptr);
                        fail(24, QStringLiteral("cannot open source file"));
                        return;
                    }
                    transfer->setDevice(input);
                });

        connect(transfer, &J::Application::stateChanged, transfer, [this, transfer](J::State state) {
            if (state == J::State::Active) {
                if (!transfer->transport() || !isIceNamespace(transfer->transport()->pad()->ns()))
                    fail(25, QStringLiteral("memory probe selected a non-ICE transport"));
                return;
            }
            if (state == J::State::Finishing) {
                senderSawFinishing_ = true;
                return;
            }
            if (state != J::State::Finished)
                return;

            const auto reason = transfer->lastReason();
            if (!senderSawFinishing_ || transfer->connection() || !reason.isValid()
                || reason.condition() != J::Reason::Success || !senderSawReceipt_) {
                fail(26, QStringLiteral("sender did not complete cleanly"));
                return;
            }
            senderTransferFinished_ = true;
            maybeSettleIteration();
        });

        const QFileInfo input(sourcePath_);
        if (!input.isFile() || !input.isReadable()) {
            fail(27, QStringLiteral("source file is not readable"));
            return;
        }
        transfer->setFile(input, QStringLiteral("Repeated Jingle FT memory probe"), Thumbnail());
        session->addContent(transfer);
        session->initiate();
    }

    void acceptIncoming(J::Session *session)
    {
        if (receiverActive_) {
            session->terminate(J::Reason::Condition::GeneralError);
            fail(30, QStringLiteral("overlapping incoming Jingle sessions"));
            return;
        }
        receiverActive_ = true;

        connect(session, &QObject::destroyed, this, [this]() {
            receiverDestroyed_ = true;
            receiverActive_    = false;
            maybeSettleIteration();
        });
        connect(session, &J::Session::terminated, this, [this, session]() {
            receiverTerminated_ = true;
            const auto error = session->lastError();
            if (error) {
                fail(31, QStringLiteral("receiver session terminated with error: %1").arg(error->toString()));
                return;
            }
            if (!receiverTransferFinished_) {
                fail(32, QStringLiteral("receiver session terminated before transfer completion"));
                return;
            }
            maybeSettleIteration();
        });

        if (session->preferredApplication() != FT::NS || session->contentList().size() != 1) {
            session->terminate(J::Reason::Condition::UnsupportedApplications);
            fail(33, QStringLiteral("unexpected incoming Jingle application"));
            return;
        }
        auto *base = session->contentList().constBegin().value();
        if (!base || base->pad()->ns() != FT::NS) {
            session->terminate(J::Reason::Condition::UnsupportedApplications);
            fail(34, QStringLiteral("incoming content is not Jingle file transfer"));
            return;
        }
        auto *transfer = static_cast<FT::Application *>(base);

        connect(transfer, &FT::Application::deviceRequested, transfer,
                [this, transfer](quint64 offset, std::optional<quint64>) {
                    auto               *output = new QFile(destinationPath_, transfer);
                    QIODevice::OpenMode mode   = QIODevice::WriteOnly;
                    if (offset == 0)
                        mode |= QIODevice::Truncate;
                    if (!output->open(mode) || !output->seek(qint64(offset))) {
                        delete output;
                        transfer->setDevice(nullptr);
                        fail(35, QStringLiteral("cannot open destination file"));
                        return;
                    }
                    transfer->setDevice(output);
                });

        connect(transfer, &J::Application::stateChanged, transfer, [this, session, transfer](J::State state) {
            if (state == J::State::Finishing) {
                receiverSawFinishing_ = true;
                return;
            }
            if (state != J::State::Finished)
                return;

            const auto reason = transfer->lastReason();
            if (!receiverSawFinishing_ || transfer->connection() || !reason.isValid()
                || reason.condition() != J::Reason::Success) {
                fail(36, QStringLiteral("receiver did not complete cleanly"));
                return;
            }
            receiverTransferFinished_ = true;
            session->terminate(J::Reason::Condition::Success);
            maybeSettleIteration();
        });

        session->accept();
    }

    void maybeSettleIteration()
    {
        if (failed_ || settleScheduled_ || !senderTransferFinished_ || !receiverTransferFinished_ || !senderTerminated_
            || !receiverTerminated_ || !senderDestroyed_ || !receiverDestroyed_)
            return;

        settleScheduled_ = true;
        QTimer::singleShot(250, this, [this]() {
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QCoreApplication::processEvents(QEventLoop::AllEvents);

            const auto sourceHash      = fileSha256(sourcePath_);
            const auto destinationHash = fileSha256(destinationPath_);
            if (sourceHash.isEmpty() || sourceHash != destinationHash) {
                fail(40, QStringLiteral("destination checksum mismatch"));
                return;
            }

            samples_.push_back(sampleMemory(iteration_));
            qInfo().noquote() << QStringLiteral("FT_MEMORY_TRANSFER_END iteration=%1").arg(iteration_);
            QTimer::singleShot(0, this, [this]() { startNextTransfer(); });
        });
    }

    void reportAndFinish()
    {
        timeout_.stop();
        const double privateSlope = regressionSlope(samples_, &MemorySample::privateKb);
        const double heapSlope    = regressionSlope(samples_, &MemorySample::heapBytes);
        qint64       privateGrowth = -1;
        qint64       heapGrowth    = -1;
        if (samples_.size() >= 3) {
            if (samples_[1].privateKb >= 0 && samples_.back().privateKb >= 0)
                privateGrowth = samples_.back().privateKb - samples_[1].privateKb;
            if (samples_[1].heapBytes >= 0 && samples_.back().heapBytes >= 0)
                heapGrowth = samples_.back().heapBytes - samples_[1].heapBytes;
        }

        qInfo().noquote()
            << QStringLiteral("FT_MEMORY_RESULT iterations=%1 private_growth_kb=%2 heap_growth_bytes=%3 "
                              "private_slope_kb_per_transfer=%4 heap_slope_bytes_per_transfer=%5")
                   .arg(iterations_)
                   .arg(privateGrowth)
                   .arg(heapGrowth)
                   .arg(privateSlope, 0, 'f', 2)
                   .arg(heapSlope, 0, 'f', 2);
        qInfo("LIVE_FT_MEMORY_RESULT=success");
        app_.exit(0);
    }

    void fail(int code, const QString &message)
    {
        if (failed_)
            return;
        failed_ = true;
        timeout_.stop();
        qCritical().noquote() << QStringLiteral("LIVE_FT_MEMORY_RESULT=failure %1").arg(message);
        app_.exit(code);
    }

    QCoreApplication &app_;
    XmppEndpoint      sender_;
    XmppEndpoint      receiver_;
    QString           sourcePath_;
    QString           destinationPath_;
    int               iterations_ = 0;
    int               iteration_  = 0;
    QTimer            timeout_;
    bool              senderReady_ = false;
    bool              receiverReady_ = false;
    bool              started_ = false;
    bool              failed_ = false;
    bool              receiverActive_ = false;
    bool              senderSawFinishing_ = false;
    bool              receiverSawFinishing_ = false;
    bool              senderTransferFinished_ = false;
    bool              receiverTransferFinished_ = false;
    bool              senderSawReceipt_ = false;
    bool              senderTerminated_ = false;
    bool              receiverTerminated_ = false;
    bool              senderDestroyed_ = false;
    bool              receiverDestroyed_ = false;
    bool              settleScheduled_ = false;
    std::vector<MemorySample> samples_;
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCA::Initializer qca;

    if (argc < 7 || argc > 8) {
        qCritical() << "Usage:" << argv[0]
                    << "<sender-jid/resource> <sender-password> <receiver-jid/resource> <receiver-password> "
                       "<source-file> <destination-file> [iterations]";
        return 2;
    }

    const Jid senderJid(QString::fromLocal8Bit(argv[1]));
    const QString senderPassword = QString::fromLocal8Bit(argv[2]);
    const Jid receiverJid(QString::fromLocal8Bit(argv[3]));
    const QString receiverPassword = QString::fromLocal8Bit(argv[4]);
    const QString sourcePath = QString::fromLocal8Bit(argv[5]);
    const QString destinationPath = QString::fromLocal8Bit(argv[6]);
    bool ok = true;
    const int iterations = argc == 8 ? QString::fromLocal8Bit(argv[7]).toInt(&ok) : 12;

    if (!senderJid.isValid() || senderJid.resource().isEmpty() || !receiverJid.isValid()
        || receiverJid.resource().isEmpty() || !ok || iterations < 3) {
        qCritical() << "Invalid full JID or iteration count";
        return 3;
    }

    FtMemoryProbe probe(app, senderJid, senderPassword, receiverJid, receiverPassword, sourcePath, destinationPath,
                        iterations);
    probe.start();
    return app.exec();
}
