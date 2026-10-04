// SPDX-License-Identifier: GPL-2.0-or-later
#include "../avcallpolicy.h"
#include <QCoreApplication>
#include <QtCrypto>
#include <iris/dtls.h>
#include <iris/jingle-session.h>
#include <iris/xmpp_client.h>

namespace J = XMPP::Jingle;
namespace R = J::RTP;
static void check(bool ok, const char *message)
{
    if (!ok)
        qFatal("%s", message);
}
static void flush()
{
    for (int i = 0; i < 12; ++i)
        QCoreApplication::processEvents();
}
struct Counters {
    int answers = 0;
};
class Endpoint : public R::MediaEndpoint {
public:
    Endpoint(std::shared_ptr<Counters> counters, QString media) : counters(counters), media(media) { }
    R::Description localOffer() const override
    {
        R::Description description;
        description.media   = media;
        description.rtcpMux = true;
        R::PayloadType payload;
        payload.id        = 96;
        payload.name      = media == "video" ? "VP8" : "opus";
        payload.clockrate = media == "video" ? 90000 : 48000;
        description.payloads.append(payload);
        return description;
    }
    std::optional<R::Description> makeAnswer(const R::Description &offer) const override
    {
        ++counters->answers;
        return offer;
    }
    bool acceptsAnswer(const R::Description &, const R::Description &) const override { return true; }
    bool configure(const R::Description &, const R::Description &) override { return true; }
    void stop() override { }
    std::shared_ptr<Counters> counters;
    QString                   media;
};
class Media : public R::MediaSession {
public:
    explicit Media(std::shared_ptr<Counters> counters) : counters(counters) { }
    std::unique_ptr<R::MediaEndpoint> createEndpoint(const QString &, const QString &media) override
    {
        return std::make_unique<Endpoint>(counters, media);
    }
    std::shared_ptr<Counters> counters;
};
class Provider : public R::MediaProvider {
public:
    explicit Provider(std::shared_ptr<Counters> counters) : counters(counters) { }
    std::unique_ptr<R::MediaSession> createSession() override { return std::make_unique<Media>(counters); }
    QStringList                      secureRtpProfiles() const override { return { "SRTP_AES128_CM_HMAC_SHA1_80" }; }
    std::shared_ptr<Counters>        counters;
};
class TransportPad : public J::TransportManagerPad {
public:
    explicit TransportPad(J::Session *s) : s(s) { }
    J::Session          *session() const override { return s; }
    QString              ns() const override { return "urn:psi:test:incoming-video"; }
    J::TransportManager *manager() const override { return nullptr; }
    J::Session          *s;
};
class Transport : public J::Transport, public R::PacketTransport {
public:
    Transport(J::Session *s, J::Origin creator) :
        J::Transport(QSharedPointer<TransportPad>::create(s), creator), association(nullptr, "incoming-video-test")
    {
    }
    void prepare() override
    {
        setState(J::State::ApprovedToSend);
        emit updated();
    }
    void                           start() override { setState(J::State::Active); }
    bool                           update(const QDomElement &) override { return true; }
    bool                           hasUpdates() const override { return state() == J::State::ApprovedToSend; }
    J::OutgoingTransportInfoUpdate takeOutgoingUpdate(bool) override
    {
        return { pad()->doc()->createElementNS(pad()->ns(), "transport"), {} };
    }
    bool                      isValid() const override { return true; }
    J::TransportFeatures      features() const override { return J::TransportFeature::LiveOriented; }
    J::Connection::Ptr        addChannel(J::TransportFeatures, const QString &, int) override { return {}; }
    QList<J::Connection::Ptr> channels() const override { return {}; }
    bool                      enableRtpMux(const QStringList &profiles) override { return !profiles.isEmpty(); }
    R::SecureRtpAssociation  *rtpAssociation() const override
    {
        return const_cast<R::SecureRtpAssociation *>(&association);
    }
    bool                    sendProtectedRtpPacket(QByteArray, R::PacketKind, quint64) override { return false; }
    R::SecureRtpAssociation association;
};
static void incomingVideo(J::Origin role, J::Origin senders)
{
    XMPP::Client client;
    J::Session   session(client.jingleManager(), XMPP::Jid("peer@example.test/resource"), role);
    auto         counters = std::make_shared<Counters>();
    auto         provider = std::make_shared<Provider>(counters);
    auto pad = QSharedPointer<R::Pad>::create(client.jingleManager()->rtpManager(), &session, provider, QStringList());
    R::Application content(pad, "screen-new", AvCallPolicy::peerRole(role), senders);
    auto           transport = QSharedPointer<Transport>::create(&session, content.creator());
    Endpoint       offered(counters, "video");
    QDomDocument   doc;
    check(content.setRemoteOffer(offered.localOffer().toXml(doc)) == R::Application::Ok, "remote video offer rejected");
    check(content.setTransport(transport), "incoming video transport rejected");
    check(content.state() == J::State::Created, "offer was approved before application decision");
    AvCallPolicy::prepareIncomingContent(&content, false);
    flush();
    check(counters->answers == 0, "inactive call accepted video without initial call consent");
    AvCallPolicy::prepareIncomingContent(&content, true);
    AvCallPolicy::prepareIncomingContent(&content, true);
    flush();
    check(counters->answers == 1, "active call did not prepare exactly one video answer");
    check(!pad->directionController()->allowsLocalSending(&content), "incoming video granted local capture consent");
    if (XMPP::Dtls::supportedSRTPProfiles().isEmpty()) {
        qInfo("Signaling assertions skipped: this QCA build has no DTLS-SRTP profiles");
        return;
    }
    check(content.evaluateOutgoingUpdate().action == J::Action::ContentAccept, "incoming video has no content-accept");
    const auto           update = content.takeOutgoingUpdate();
    const auto           xml    = std::get<0>(update).first();
    const J::ContentBase answer(xml);
    check(answer.name == "screen-new" && answer.creator == content.creator(),
          "content-accept lost the offered content identity");
    check(answer.senders == AvCallPolicy::peerRole(role), "content-accept did not preserve receive-only policy");
    check(!xml.firstChildElement("description").isNull() && !xml.firstChildElement("transport").isNull(),
          "content-accept lacks RTP answer or transport");
    AvCallPolicy::prepareIncomingContent(&content, true);
    flush();
    check(counters->answers == 1, "rewiring pending content renegotiated its answer");
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCA::Initializer qca;
    for (const auto role : { J::Origin::Initiator, J::Origin::Responder }) {
        incomingVideo(role, AvCallPolicy::peerRole(role));
        incomingVideo(role, J::Origin::Both);
    }
    qInfo("Active-call incoming video answer and capture-consent regressions passed");
}
