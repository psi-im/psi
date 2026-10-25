// SPDX-License-Identifier: GPL-2.0-or-later
// A separate, real Iris Client for the application UI integration test.
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <iris/processquit.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_linklocalservice.h>
#include <iris/xmpp_message.h>
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 2)
        return 2;
    XMPP::Client           client;
    XMPP::LinkLocalService service;
    const XMPP::Jid        jid(QString::fromLocal8Bit(argv[1]));
    client.connectToStream(&service, jid);
    QObject::connect(&service, &XMPP::LinkLocalService::online, &app, [&] {
        client.setPresence(XMPP::Status(XMPP::Status::Online));
        qInfo("PEER-READY");
    });
    QObject::connect(&service, &XMPP::Stream::error, &app, [&](int) { app.exit(2); });
    QObject::connect(&client, &XMPP::Client::messageReceived, &app, [&](const XMPP::Message &message) {
        if (message.body().isEmpty())
            return;
        if (message.body() != QLatin1String("serverless-ui-test")) {
            app.exit(3);
            return;
        }
        XMPP::Message reply;
        reply.setTo(message.from());
        reply.setType(XMPP::Message::Type::Chat);
        reply.setBody(message.body());
        client.sendMessage(reply);
        qInfo("PEER-MESSAGE-OK");
    });
    QObject::connect(XMPP::ProcessQuit::instance(), &XMPP::ProcessQuit::quit, &service, &XMPP::Stream::close);
    QObject::connect(&service, &XMPP::Stream::connectionClosed, &app, &QCoreApplication::quit);
    if (!service.start(jid, QHostAddress::LocalHost, "UI Test Peer"))
        return 2;
    QTimer::singleShot(85000, &app, [&] { app.exit(4); });
    return app.exec();
}
