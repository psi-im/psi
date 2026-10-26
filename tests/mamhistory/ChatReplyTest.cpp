/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "chatreply.h"
#include "historymessageextras.h"
#include <QtTest>
#include <iris/xmpp.h>
#include <iris/xmpp_clientstream.h>
#include <iris/xmpp_message.h>
using namespace XMPP;

class ReplyConnector : public Connector {
public:
    void        setOptHostPort(const QString &, quint16) override { }
    void        connectToServer(const QString &) override { }
    ByteStream *stream() const override { return nullptr; }
    void        done() override { }
};

class ChatReplyTest : public QObject {
    Q_OBJECT
private slots:
    void wireAndLocalHistoryRetainReplyAndUnicodeFallback()
    {
        ReplyConnector connector;
        ClientStream   stream(&connector);
        Message        outgoing(Jid("room@example.org"));
        outgoing.setType(Message::Type::Groupchat);
        outgoing.setId("new-message");
        outgoing.setBody(QString::fromUtf8("Ответ 🦊"));
        ChatReply reply { "room-stanza-id", "room@example.org/Алиса", "Алиса", QString::fromUtf8("Цитата 🦊\nстрока") };
        applyMessageReply(outgoing, reply);
        const auto stanza = outgoing.toStanza(&stream);
        Message    received;
        QVERIFY(received.fromStanza(stanza));
        QCOMPARE(received.reply().id, reply.id);
        QCOMPARE(received.reply().to.full(), reply.to);
        QCOMPARE(messageReplyBody(received), QString::fromUtf8("Ответ 🦊"));
        QVERIFY(messageReply(received).text.contains(reply.text));
        QVERIFY(received.body().startsWith(QLatin1Char('>')));
        const auto range = received.bodyFallbacks("urn:xmpp:reply:0").first();
        int        from, to;
        QVERIFY(range.toUtf16(received.body(), from, to));
        QCOMPARE(received.body().mid(to), QString::fromUtf8("Ответ 🦊"));
        QVERIFY(to > range.end); // Non-BMP characters occupy two UTF-16 units.

        Message restored;
        restored.setBody(received.body());
        HistoryMessageExtras::restore(HistoryMessageExtras::encode(received), restored);
        QCOMPARE(restored.reply().id, reply.id);
        QCOMPARE(restored.reply().to.full(), reply.to);
        QCOMPARE(messageReplyBody(restored), QString::fromUtf8("Ответ 🦊"));
        Message oldHistory;
        oldHistory.setBody("Old history");
        HistoryMessageExtras::restore("{\"jabber:x:oob\":[[\"https://example.org/file\"]]}", oldHistory);
        QVERIFY(!oldHistory.reply().isValid());
        QCOMPARE(oldHistory.body(), QString("Old history"));
        QCOMPARE(oldHistory.urlList().size(), 1);
    }

    void quoteWithoutRoomStanzaIdIsPlainBodyText()
    {
        // Rooms without XEP-0359 stanza-ids give nothing to reference: send the
        // name and quote in the body only, with neither <reply/> nor <fallback/>.
        ReplyConnector connector;
        ClientStream   stream(&connector);
        Message        outgoing(Jid("room@example.org"));
        outgoing.setType(Message::Type::Groupchat);
        outgoing.setId("new-message");
        outgoing.setBody("Answer");
        const ChatReply quote { {}, "room@example.org/Peer", "Peer", "first\nsecond" };
        QVERIFY(quote.isValid());
        QVERIFY(!quote.hasReference());
        applyMessageReply(outgoing, quote);
        QCOMPARE(outgoing.body(), QString("> Peer:\n> first\n> second\nAnswer"));

        Message received;
        QVERIFY(received.fromStanza(outgoing.toStanza(&stream)));
        QVERIFY(!received.reply().isValid());
        QVERIFY(received.fallbacks().isEmpty());
        QCOMPARE(received.body(), outgoing.body());
        QVERIFY(!messageReply(received).isValid());
        QCOMPARE(messageReplyBody(received), outgoing.body());

        // Existing ranges of another feature must not be shifted.
        Message media;
        media.setBody("media description");
        media.setFallbacks({ { "urn:example:media", { { 0, 5 } }, {} } });
        applyMessageReply(media, quote);
        QCOMPARE(media.body(), QString("media description"));
        QVERIFY(!media.reply().isValid());

        QVERIFY(!ChatReply {}.isValid());
    }

    void invalidFallbackDoesNotEraseMessageAndOverlapsAreMerged()
    {
        Message message;
        message.setBody("quote\nanswer");
        message.setReply({ {}, "target" });
        message.setFallbacks({ { "urn:xmpp:reply:0", { { 0, 200 } }, {} } });
        QCOMPARE(messageReplyBody(message), message.body());
        message.setFallbacks({ { "urn:xmpp:reply:0", { { 0, 3 }, { 1, 6 } }, {} } });
        QCOMPARE(messageReplyBody(message), QString("answer"));
        message.setReply({});
        QCOMPARE(messageReplyBody(message), message.body());
    }

    void anotherFeaturesFallbackRangesAreNotShiftedByReply()
    {
        Message message;
        message.setBody("media description");
        message.setFallbacks({ { "urn:example:media", { { 0, 5 } }, {} } });
        applyMessageReply(message, { "target", "peer@example.org", "Peer", "quoted text" });
        QCOMPARE(message.body(), QString("media description"));
        QCOMPARE(message.reply().id, QString("target"));
        QCOMPARE(message.bodyFallbacks("urn:example:media").first().end, 5);
        QVERIFY(message.bodyFallbacks("urn:xmpp:reply:0").isEmpty());
    }
};
QTEST_GUILESS_MAIN(ChatReplyTest)
#include "ChatReplyTest.moc"
