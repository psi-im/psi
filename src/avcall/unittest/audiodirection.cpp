// SPDX-License-Identifier: GPL-2.0-or-later
#include "../avcallaudiodirection.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QtCrypto>
#include <iris/jingle-session.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_task.h>

namespace J = XMPP::Jingle;
namespace R = J::RTP;
using Op    = R::DirectionOperation;
static void check(bool ok, const char *message)
{
    if (!ok)
        qFatal("%s", message);
}
static void flush()
{
    for (int i = 0; i < 8; ++i)
        QCoreApplication::processEvents();
}

class Transport : public J::Transport {
public:
    Transport() : J::Transport({}, J::Origin::Initiator) { setState(J::State::Active); }
    void                           prepare() override { }
    void                           start() override { }
    bool                           update(const QDomElement &) override { return true; }
    bool                           hasUpdates() const override { return false; }
    J::OutgoingTransportInfoUpdate takeOutgoingUpdate(bool) override { return {}; }
    bool                           isValid() const override { return true; }
    J::TransportFeatures           features() const override { return {}; }
    J::Connection::Ptr             addChannel(J::TransportFeatures, const QString &, int) override { return {}; }
    QList<J::Connection::Ptr>      channels() const override { return {}; }
};
class Content : public R::Application {
public:
    Content(const QSharedPointer<R::Pad> &pad, J::Origin senders = J::Origin::Both) :
        R::Application(pad, "audio", J::Origin::Initiator, senders)
    {
        _transport = QSharedPointer<Transport>::create();
        setState(J::State::Active);
    }
};
class Result : public XMPP::Task {
public:
    Result(XMPP::Task *parent, int error = 0) : Task(parent)
    {
        if (error)
            setError(error);
        else
            setSuccess();
    }
};
static J::OutgoingUpdate consume(Content &content)
{
    check(content.evaluateOutgoingUpdate().action == J::Action::ContentModify, "missing content-modify");
    return content.takeOutgoingUpdate();
}
static void ack(const J::OutgoingUpdate &update, Result &result) { std::get<1>(update)(&result); }
struct Fixture {
    XMPP::Client client;
    J::Session   session { client.jingleManager(), XMPP::Jid("peer@example.test/resource"), J::Origin::Responder };
    QSharedPointer<R::Pad> pad = QSharedPointer<R::Pad>::create(client.jingleManager()->rtpManager(), &session,
                                                                std::shared_ptr<R::MediaProvider>(), QStringList());
    Content                content { pad };
    Result                 success { client.rootTask() };
    AvCallAudioDirection   policy;
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCA::Initializer qca;
    {
        Fixture f;
        // Explicit receive-only accept remains receive-only even if a peer later
        // offers Both, or a microphone disappears and returns.
        f.content.incomingContentModify(J::Origin::Initiator);
        f.policy.bind(&f.content, false, true);
        flush();
        check(f.policy.operation()->state() == Op::State::Succeeded && !f.policy.allowsCapture(&f.content),
              "receive-only acceptance granted capture consent");
        f.content.incomingContentModify(J::Origin::Both);
        f.policy.setCaptureAvailable(false);
        f.policy.setCaptureAvailable(true);
        check(!f.policy.allowsCapture(&f.content), "device return or remote update granted capture consent");
        flush();
        ack(consume(f.content), f.success);
        flush();
        check(f.content.senders() == J::Origin::Initiator, "receive-only policy did not preserve peer sending");
    }
    {
        Fixture f;
        f.policy.bind(&f.content, true, true);
        flush();
        check(f.policy.allowsCapture(&f.content), "initial send consent was lost");
        bool stoppedSynchronously = false;
        QObject::connect(&f.policy, &AvCallAudioDirection::changed, &app,
                         [&] { stoppedSynchronously = !f.policy.allowsCapture(&f.content); });
        f.policy.setCaptureAvailable(false);
        check(stoppedSynchronously, "device loss did not synchronously notify capture stop");
        flush();
        auto mute = consume(f.content);
        f.policy.setCaptureAvailable(true);
        ack(mute, f.success);
        check(!f.policy.allowsCapture(&f.content), "device return bypassed negotiated local sending bit");
        flush();
        ack(consume(f.content), f.success);
        flush();
        check(f.policy.allowsCapture(&f.content) && f.policy.operation()->state() == Op::State::Succeeded,
              "late mute ACK displaced restored device policy");
        f.policy.setCaptureAvailable(false);
        f.policy.setLocalSending(false);
        f.policy.setCaptureAvailable(true);
        check(!f.policy.allowsCapture(&f.content), "device return resurrected revoked consent");
    }
    for (int error : { 500, int(XMPP::Task::ErrTimeout) }) {
        Fixture f;
        f.policy.bind(&f.content, true, false);
        flush();
        Result failure(f.client.rootTask(), error);
        ack(consume(f.content), failure);
        flush();
        check(f.policy.operation()->state() == Op::State::Failed && !f.policy.allowsCapture(&f.content),
              "IQ failure left Psi policy pending or capture open");
        const auto revision = f.pad->directionController()->policy(&f.content)->revision;
        f.policy.setCaptureAvailable(false);
        flush();
        check(f.pad->directionController()->policy(&f.content)->revision == revision
                  && f.policy.operation()->state() == Op::State::Failed,
              "unchanged capability notification restarted failed policy");
        check(f.policy.operation()->items()[0].failure
                  == (error == XMPP::Task::ErrTimeout ? R::DirectionController::Failure::Timeout
                                                      : R::DirectionController::Failure::Signaling),
              "Psi observer lost the concrete failure category");
    }
    {
        Fixture              f;
        AvCallAudioDirection shortDeadline(nullptr, 20);
        shortDeadline.bind(&f.content, true, false);
        QElapsedTimer timer;
        timer.start();
        while (shortDeadline.operation()->state() == Op::State::Pending && timer.elapsed() < 2000) {
            flush();
            QThread::msleep(1);
        }
        check(shortDeadline.operation()->error() == Op::Error::Timeout
                  && f.pad->directionController()->policy(&f.content)->desiredSending,
              "observation timeout either stayed pending or destroyed durable consent");
    }
    {
        Fixture f;
        f.content.incomingContentModify(J::Origin::Initiator);
        f.policy.bind(&f.content, true, true);
        flush();
        Result failure(f.client.rootTask(), 500);
        ack(consume(f.content), failure);
        flush();
        check(f.policy.operation()->state() == Op::State::Failed && !f.policy.allowsCapture(&f.content),
              "rejected enable opened capture despite available hardware");
        f.policy.setCaptureAvailable(true);
        flush();
        check(f.policy.operation()->state() == Op::State::Failed, "unchanged present device retried rejection");
        f.policy.setLocalSending(true); // explicit retry is a new user operation
        flush();
        ack(consume(f.content), f.success);
        flush();
        check(f.policy.operation()->state() == Op::State::Succeeded && f.policy.allowsCapture(&f.content),
              "explicit retry did not recover from rejection");
    }
    {
        Fixture f;
        auto    first = std::make_unique<Content>(f.pad);
        f.policy.bind(first.get(), true, false); // keep observation pending across removal
        flush();
        first.reset();
        Content replacement(f.pad);
        flush();
        check(f.policy.operation()->error() == Op::Error::ContentGone && !f.policy.allowsCapture(&replacement),
              "new content inherited a removed incarnation's consent");
    }
    {
        Fixture f;
        auto    owner = std::make_unique<AvCallAudioDirection>();
        owner->bind(&f.content, true, true);
        flush();
        QObject::connect(owner.get(), &AvCallAudioDirection::changed, &app, [&] { owner.reset(); });
        owner->setCaptureAvailable(false);
        flush();
        check(!owner && !f.pad->directionController()->allowsLocalSending(&f.content),
              "reentrant owner deletion removed constraint without revoking consent");
    }
    return 0;
}
