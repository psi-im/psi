/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingpublication.h"
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QtCrypto>
#include <iris/jingle-pub.h>
#include <iris/xmpp.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_clientstream.h>
#include <iris/xmpp_task.h>

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
class PublicationTest : public QObject {
    Q_OBJECT
private slots:
    void publication_data()
    {
        QTest::addColumn<bool>("existing");
        QTest::addColumn<bool>("denied");
        QTest::newRow("new-private-node") << false << false;
        QTest::newRow("existing-private-node") << true << false;
        QTest::newRow("pubsub-unavailable") << false << true;
    }
    void publication()
    {
        QFETCH(bool, existing);
        QFETCH(bool, denied);
        NoNetworkConnector connector;
        RecordingStream    stream(&connector);
        Client             client;
        const Jid          jid("alice@example.test/device");
        client.start("example.test", "alice", {}, "device");
        client.connectToServer(&stream, jid);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile source(dir.filePath("file.txt"));
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write("payload"), qint64(7));
        source.close();
        Jingle::FileTransfer::File file;
        file.setName("file.txt");
        file.setSize(7);
        file.addHash(Hash::from(Hash::Sha256, QByteArray("payload")));
        bool completed = false, succeeded = false;
        QVERIFY(publishSharedFile(&client, file, source.fileName(), this, [&](bool success, const QString &) {
            completed = true;
            succeeded = success;
        }));
        QCoreApplication::processEvents();
        QVERIFY(!stream.written.isEmpty());
        const auto create = stream.written.takeLast();
        const auto pubsub = create.firstChildElement("pubsub");
        QCOMPARE(pubsub.firstChildElement("create").attribute("node"), QString("urn:psi:shared-files:1"));
        const auto             form = pubsub.firstChildElement("configure").firstChildElement("x");
        QMap<QString, QString> fields;
        for (auto field = form.firstChildElement("field"); !field.isNull(); field = field.nextSiblingElement("field"))
            fields.insert(field.attribute("var"), field.firstChildElement("value").text());
        QCOMPARE(fields.value("pubsub#access_model"), QString("whitelist"));
        QCOMPARE(fields.value("pubsub#deliver_notifications"), QString("0"));
        QVERIFY(!completed);
        auto reply = [&](const QDomElement &request, const QString &condition) {
            QDomDocument doc;
            auto         iq = doc.createElement("iq");
            iq.setAttribute("id", request.attribute("id"));
            iq.setAttribute("from", jid.bare());
            iq.setAttribute("to", jid.full());
            iq.setAttribute("type", condition.isEmpty() ? "result" : "error");
            if (!condition.isEmpty()) {
                auto error = doc.createElement("error");
                error.setAttribute("type", "cancel");
                error.appendChild(doc.createElementNS("urn:ietf:params:xml:ns:xmpp-stanzas", condition));
                iq.appendChild(error);
            }
            QVERIFY(client.rootTask()->take(iq));
            QCoreApplication::processEvents();
        };
        reply(create, denied ? "service-unavailable" : existing ? "conflict" : "");
        if (denied) {
            QVERIFY(completed);
            QVERIFY(!succeeded);
            return;
        }
        QVERIFY(!completed);
        QVERIFY(!stream.written.isEmpty());
        const auto publish = stream.written.takeLast();
        const auto item    = publish.firstChildElement("pubsub").firstChildElement("publish").firstChildElement("item");
        Jingle::JinglePub publication(item.firstChildElement("jinglepub"));
        QVERIFY(publication.isValid());
        QVERIFY(publication.from().compare(jid));
        QCOMPARE(publication.descriptions().first().firstChildElement("file").firstChildElement("name").text(),
                 QString("file.txt"));
        reply(publish, {});
        QVERIFY(completed);
        QVERIFY(succeeded);
        // The factory belongs to the Jingle manager, not to the transient IQ task.
        QVERIFY(client.jingleManager()->publishedSession(publication.id()).isValid());

        // Exercise the registered production factory, not just the announcement XML.
        QDomDocument startDoc;
        auto         request = startDoc.createElement("iq");
        request.setAttribute("type", "get");
        request.setAttribute("id", "request-shared-file");
        request.setAttribute("from", "bob@example.test/device");
        request.setAttribute("to", jid.full());
        auto start = startDoc.createElementNS(Jingle::JINGLEPUB_NS, "start");
        start.setAttribute("id", publication.id());
        request.appendChild(start);
        QVERIFY(client.rootTask()->take(request));
        bool starting = false;
        for (const auto &stanza : std::as_const(stream.written)) {
            if (stanza.attribute("id") == QString("request-shared-file"))
                starting = !stanza.firstChildElement("starting").attribute("sid").isEmpty();
        }
        QVERIFY(starting);
    }
};
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCA::Initializer qca;
    PublicationTest  test;
    return QTest::qExec(&test, argc, argv);
}
#include "publication.moc"
