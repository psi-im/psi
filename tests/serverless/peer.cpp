// SPDX-License-Identifier: GPL-2.0-or-later
// A separate, real Iris Client for the application UI integration test.
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <iris/processquit.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_linklocalservice.h>
#include <iris/xmpp_message.h>
#include <iris/xmpp_reference.h>
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
        if (!message.retraction().isEmpty()) {
            qCritical("Unexpected message retraction during local deletion");
            app.exit(5);
            return;
        }
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
        reply.setId("serverless-ui-echo");
        // Too large to be fetched by itself, and nothing listens at the source.
        XMPP::MediaSharing media;
        media.file.setName(QStringLiteral("peer-file.bin"));
        media.file.setMediaType(QStringLiteral("application/octet-stream"));
        media.file.setSize(quint64(1) << 40);
        media.file.addHash(XMPP::Hash::from(XMPP::Hash::Sha256, QByteArrayLiteral("serverless-ui-file")));
        media.sources.append(QStringLiteral("https://example.invalid/peer-file.bin"));
        XMPP::Reference reference(XMPP::Reference::Data, media.sources.first());
        reference.setMediaSharing(media);
        reply.setReferences({ reference });
        XMPP::StatelessFileSharing::Sources sources;
        sources.add(XMPP::StatelessFileSharing::Source::fromUrl(QUrl("https://example.invalid/shared-file.bin")));
        media.file.setName(QStringLiteral("shared-file.bin"));
        media.file.setHashes({ XMPP::Hash::from(XMPP::Hash::Sha256, QByteArrayLiteral("serverless-ui-sharing")) });
        XMPP::StatelessFileSharing::FileSharing sharing;
        sharing.setDisposition(XMPP::StatelessFileSharing::Disposition::Attachment);
        sharing.setFile(media.file);
        sharing.setSources(sources);
        reply.addFileSharing(sharing);
        client.sendMessage(reply);
        qInfo("PEER-MESSAGE-OK");
        if (qEnvironmentVariableIsSet("PSI_UI_DELETE_TEST")) {
            QTimer::singleShot(4000, &app, [&, reply]() mutable { client.sendMessage(reply); });
            QTimer::singleShot(6000, &app, [&, reply]() mutable {
                reply.setId("serverless-ui-correction");
                reply.setReplaceId("serverless-ui-echo");
                reply.setBody("serverless-ui-correction");
                client.sendMessage(reply);
            });
        }
    });
    QObject::connect(XMPP::ProcessQuit::instance(), &XMPP::ProcessQuit::quit, &service, &XMPP::Stream::close);
    QObject::connect(&service, &XMPP::Stream::connectionClosed, &app, &QCoreApplication::quit);
    if (!service.start(jid, QHostAddress::LocalHost, qEnvironmentVariable("PSI_UI_PEER_NAME", "UI Test Peer")))
        return 2;
    QTimer::singleShot(85000, &app, [&] { app.exit(4); });
    return app.exec();
}
