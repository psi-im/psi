// SPDX-License-Identifier: GPL-2.0-or-later
#include "accountincomingmessage.h"

#include <QTest>
#include <QTimer>
#include <QtCrypto>
#include <iris/xmpp.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_clientstream.h>

using namespace XMPP;

class NoNetworkConnector final : public Connector {
public:
    void        setOptHostPort(const QString &, quint16) override { }
    void        connectToServer(const QString &) override { }
    ByteStream *stream() const override { return nullptr; }
    void        done() override { }
};

class RecordingStream final : public ClientStream {
public:
    explicit RecordingStream(Connector *connector) : ClientStream(connector) { }
    void write(const Stanza &stanza) override { written.append(stanza.element().cloneNode(true).toElement()); }
    QList<QDomElement> written;
};

class RecordingMethod final : public EncryptionMethod {
public:
    QString           methodId;
    bool              asynchronous = false;
    bool              fail         = false;
    int               starts       = 0;
    EncryptionContext lastContext;
    QDomElement       plaintext;

    explicit RecordingMethod(const QString &id) : methodId(id) { }
    QString      id() const override { return methodId; }
    QString      name() const override { return methodId; }
    Capabilities capabilities() const override { return XmppStanza; }
    Features     features() const override { return {}; }
    bool         canDecrypt(const QDomElement &) const override { return false; }

    class Session final : public EncryptedSession {
    public:
        Session(RecordingMethod *method, const EncryptionContext &context) :
            EncryptedSession(method->id(), context), method(method)
        {
        }
        EncryptionJob *encrypt(const QDomElement &stanza) override
        {
            method->plaintext = stanza.cloneNode(true).toElement();
            auto *job         = new EncryptionJob(this);
            auto  complete    = [job, stanza, fail = method->fail] {
                if (fail) {
                    job->fail(EncryptionJob::Error::NetworkError, "Controlled encryption failure");
                    return;
                }
                QDomDocument doc;
                auto         message = doc.createElement("message");
                message.setAttribute("to", stanza.attribute("to"));
                message.appendChild(doc.createElementNS("urn:psi:test:encryption", "encrypted"));
                job->complete(message);
            };
            if (method->asynchronous)
                QTimer::singleShot(0, job, complete);
            else
                complete();
            return job;
        }

    private:
        RecordingMethod *method;
    };

    EncryptedSession *startSession(Capabilities, const EncryptionContext &context) override
    {
        ++starts;
        lastContext = context;
        return new Session(this, context);
    }
};

struct Fixture {
    NoNetworkConnector connector;
    RecordingStream    stream { &connector };
    Client             client;
    Fixture()
    {
        client.start("example.test", "alice", {}, "device");
        client.connectToServer(&stream, Jid("alice@example.test/device"));
    }
    Message request() const
    {
        Message message(Jid("alice@example.test/device"));
        message.setFrom(Jid("bob@example.test/plain-client"));
        message.setId("original-id");
        message.setBody("SIMS attachment");
        message.setMessageReceipt(ReceiptRequest);
        return message;
    }
};

class MessageReceiptTest : public QObject {
    Q_OBJECT
private slots:
    void plaintextWithEncryptionAvailable_data()
    {
        QTest::addColumn<bool>("claimedEncryption");
        QTest::newRow("plaintext") << false;
        QTest::newRow("unauthenticated-eme-label") << true;
    }
    void plaintextWithEncryptionAvailable()
    {
        QFETCH(bool, claimedEncryption);
        Fixture         f;
        RecordingMethod method("omemo");
        QVERIFY(f.client.encryptionManager()->registerMethod(&method));
        auto request = f.request();
        request.setWasEncrypted(claimedEncryption);
        request.setEncryptionProtocol(claimedEncryption ? "OMEMO" : "");
        AccountIncomingMessage(request).sendReceipt(&f.client);
        QTRY_COMPARE(f.stream.written.size(), 1);
        QCOMPARE(method.starts, 0);
        const auto wire = f.stream.written.first();
        QVERIFY(wire.firstChildElement("encrypted").isNull());
        QCOMPARE(wire.attribute("to"), request.from().full());
        QCOMPARE(wire.firstChildElement("received").namespaceURI(), QString("urn:xmpp:receipts"));
        QCOMPARE(wire.firstChildElement("received").attribute("id"), request.id());
        QVERIFY(wire.firstChildElement("request").isNull());
        QVERIFY(wire.firstChildElement("body").isNull());
    }
    void encrypted_data()
    {
        QTest::addColumn<bool>("asynchronous");
        QTest::newRow("synchronous") << false;
        QTest::newRow("asynchronous") << true;
    }
    void encrypted()
    {
        QFETCH(bool, asynchronous);
        Fixture         f;
        RecordingMethod original("original-method"), other("other-preference");
        original.asynchronous = asynchronous;
        QVERIFY(f.client.encryptionManager()->registerMethod(&original));
        QVERIFY(f.client.encryptionManager()->registerMethod(&other));
        EncryptionMetadata metadata;
        metadata.methodId       = original.id();
        metadata.sender         = Jid("bob@example.test");
        metadata.senderDeviceId = 123;
        metadata.senderKey      = "authenticated-key";
        metadata.details.insert("omemo-profile", "urn:xmpp:omemo:2");
        const auto                   request = f.request();
        const AccountIncomingMessage incoming(request, &metadata);
        // Simulate delivery after a queued message outlives its decryption job.
        metadata.methodId       = other.id();
        metadata.senderDeviceId = 456;
        metadata.details.clear();
        incoming.sendReceipt(&f.client);
        QTRY_COMPARE(f.stream.written.size(), 1);
        QCOMPARE(original.starts, 1);
        QCOMPARE(other.starts, 0);
        QVERIFY(original.lastContext.replyTo);
        QCOMPARE(original.lastContext.replyTo->senderDeviceId, quint32(123));
        QCOMPARE(original.lastContext.replyTo->senderKey, QByteArray("authenticated-key"));
        QCOMPARE(original.lastContext.replyTo->details.value("omemo-profile").toString(), QString("urn:xmpp:omemo:2"));
        QCOMPARE(original.lastContext.recipients.size(), 1);
        QCOMPARE(original.lastContext.recipients.first().full(), QString("bob@example.test"));
        QCOMPARE(original.plaintext.firstChildElement("received").attribute("id"), request.id());
        QCOMPARE(original.plaintext.attribute("to"), request.from().full());
        const auto wire = f.stream.written.first();
        QVERIFY(!wire.firstChildElement("encrypted").isNull());
        QVERIFY(wire.firstChildElement("received").isNull());

        // The next plaintext request must not inherit the previous request's encryption.
        AccountIncomingMessage(request).sendReceipt(&f.client);
        QTRY_COMPARE(f.stream.written.size(), 2);
        QVERIFY(!f.stream.written.last().firstChildElement("received").isNull());
        QCOMPARE(original.starts, 1);
    }
    void encryptionFailure_data() { encrypted_data(); }
    void encryptionFailure()
    {
        QFETCH(bool, asynchronous);
        Fixture         f;
        RecordingMethod method("failing-method");
        method.asynchronous = asynchronous;
        method.fail         = true;
        QVERIFY(f.client.encryptionManager()->registerMethod(&method));
        EncryptionMetadata metadata;
        metadata.methodId = method.id();
        metadata.sender   = f.request().from();
        AccountIncomingMessage(f.request(), &metadata).sendReceipt(&f.client);
        QCoreApplication::processEvents();
        QCOMPARE(method.starts, 1);
        QVERIFY(f.stream.written.isEmpty()); // No plaintext retry on an encryption error.
    }
    void unavailableMethod()
    {
        Fixture            f;
        EncryptionMetadata metadata;
        metadata.methodId = "unloaded-method";
        metadata.sender   = f.request().from();
        AccountIncomingMessage(f.request(), &metadata).sendReceipt(&f.client);
        QCoreApplication::processEvents();
        QVERIFY(f.stream.written.isEmpty());
    }
    void noRequestOrId()
    {
        Fixture f;
        auto    request = f.request();
        request.setMessageReceipt(ReceiptReceived);
        AccountIncomingMessage(request).sendReceipt(&f.client);
        request.setMessageReceipt(ReceiptRequest);
        request.setId({});
        AccountIncomingMessage(request).sendReceipt(&f.client);
        QVERIFY(f.stream.written.isEmpty());
    }
};

int main(int argc, char **argv)
{
    QCoreApplication   app(argc, argv);
    QCA::Initializer   qca;
    MessageReceiptTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "messagereceipt.moc"
