/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "filesharingpublication.h"
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QtCrypto>
#include <iris/jingle-pub.h>
#include <iris/xmpp.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_clientstream.h>
#include <iris/xmpp_message.h>
#include <iris/xmpp_task.h>

Q_DECLARE_METATYPE(XMPP::Message::Type)

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

class ControlledReply final : public QNetworkReply {
public:
    ControlledReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent) :
        QNetworkReply(parent)
    {
        setOperation(op);
        setRequest(request);
        setUrl(request.url());
        open(QIODevice::ReadOnly);
    }
    void complete(int status, NetworkError error = NoError, const QVariant &length = {})
    {
        if (isFinished())
            return;
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        if (length.isValid())
            setHeader(QNetworkRequest::ContentLengthHeader, length);
        if (error != NoError)
            setError(error, QStringLiteral("Controlled HTTP failure"));
        if (operation() == QNetworkAccessManager::PutOperation)
            emit uploadProgress(7, 7);
        setFinished(true);
        emit finished();
    }
    void   abort() override { complete(0, OperationCanceledError); }
    qint64 readData(char *, qint64) override { return -1; }
};

class ControlledNAM final : public QNetworkAccessManager {
public:
    QList<QPointer<ControlledReply>> requests;
    QByteArray                       uploaded;

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *body) override
    {
        auto *reply = new ControlledReply(op, request, this);
        requests.append(reply);
        if (body)
            uploaded = body->readAll();
        return reply;
    }
};

struct PublicationFixture {
    NoNetworkConnector         connector;
    RecordingStream            stream { &connector };
    ControlledNAM              nam;
    Client                     client;
    const Jid                  jid { "alice@example.test/device" };
    QTemporaryDir              dir;
    Jingle::FileTransfer::File file;

    bool init()
    {
        client.start("example.test", "alice", {}, "device");
        client.connectToServer(&stream, jid);
        client.setNetworkAccessManager(&nam);
        QFile source(path());
        if (!source.open(QIODevice::WriteOnly) || source.write("payload") != 7)
            return false;
        file.setName("file.txt");
        file.setMediaType("text/plain");
        file.setSize(7);
        file.addHash(Hash::from(Hash::Sha256, QByteArray("payload")));
        return true;
    }
    QString     path() const { return dir.filePath("file.txt"); }
    QDomElement request(const QString &child, const QString &ns = {}) const
    {
        for (const auto &iq : stream.written) {
            const auto element = iq.firstChildElement(child);
            if (!element.isNull() && (ns.isEmpty() || element.namespaceURI() == ns))
                return iq;
        }
        return {};
    }
    bool answer(const QDomElement &request, const QString &payload = {}, bool error = false)
    {
        if (request.isNull())
            return false;
        QDomDocument doc;
        auto         iq = doc.createElement("iq");
        iq.setAttribute("id", request.attribute("id"));
        iq.setAttribute("from", request.attribute("to", jid.bare()));
        iq.setAttribute("to", jid.full());
        iq.setAttribute("type", error ? "error" : "result");
        if (error) {
            auto e = doc.createElement("error");
            e.setAttribute("type", "cancel");
            e.appendChild(doc.createElementNS("urn:ietf:params:xml:ns:xmpp-stanzas", "service-unavailable"));
            iq.appendChild(e);
        } else if (!payload.isEmpty()) {
            QDomDocument data;
            if (!data.setContent(payload, true))
                return false;
            iq.appendChild(doc.importNode(data.documentElement(), true));
        }
        for (int i = 0; i < stream.written.size(); ++i) {
            if (stream.written[i].attribute("id") == request.attribute("id")) {
                stream.written.removeAt(i);
                break;
            }
        }
        return client.rootTask()->take(iq);
    }
};

class PublicationTest : public QObject {
    Q_OBJECT

    void announce(PublicationFixture &f, bool denied = false)
    {
        QTRY_VERIFY(!f.request("pubsub").isNull());
        QVERIFY(f.answer(f.request("pubsub"), {}, denied));
        if (denied)
            return;
        QTRY_VERIFY(!f.request("pubsub").isNull());
        QVERIFY(f.answer(f.request("pubsub")));
    }
    void uploadSlot(PublicationFixture &f, bool available = true)
    {
        const QString discoItems = "http://jabber.org/protocol/disco#items";
        QTRY_VERIFY(!f.request("query", discoItems).isNull());
        QVERIFY(f.answer(f.request("query", discoItems),
                         available ? "<query xmlns='http://jabber.org/protocol/disco#items'>"
                                     "<item jid='upload.example.test'/></query>"
                                   : "<query xmlns='http://jabber.org/protocol/disco#items'/>"));
        if (!available)
            return;
        const QString discoInfo = "http://jabber.org/protocol/disco#info";
        QTRY_VERIFY(!f.request("query", discoInfo).isNull());
        QVERIFY(f.answer(f.request("query", discoInfo),
                         "<query xmlns='http://jabber.org/protocol/disco#info'>"
                         "<identity category='store' type='file'/>"
                         "<feature var='urn:xmpp:http:upload:0'/></query>"));
        QTRY_VERIFY(!f.request("request").isNull());
        QVERIFY(f.answer(f.request("request"),
                         "<slot xmlns='urn:xmpp:http:upload:0'>"
                         "<put url='https://upload.example.test/new/file.txt'/>"
                         "<get url='https://download.example.test/new/file.txt'/></slot>"));
        QTRY_VERIFY(!f.nam.requests.isEmpty() && f.nam.requests.last()
                    && f.nam.requests.last()->operation() == QNetworkAccessManager::PutOperation);
        QCOMPARE(f.nam.uploaded, QByteArray("payload"));
    }

private slots:
    void allMethods_data()
    {
        QTest::addColumn<bool>("httpFirst");
        QTest::addColumn<bool>("httpFails");
        QTest::addColumn<bool>("catalogFails");
        QTest::newRow("jingle-first") << false << false << false;
        QTest::newRow("http-first") << true << false << false;
        QTest::newRow("jingle-first-http-fails") << false << true << false;
        QTest::newRow("http-fails-first") << true << true << false;
        QTest::newRow("catalog-fails-first") << false << false << true;
        QTest::newRow("http-first-catalog-fails") << true << false << true;
    }
    void allMethods()
    {
        QFETCH(bool, httpFirst);
        QFETCH(bool, httpFails);
        QFETCH(bool, catalogFails);
        PublicationFixture f;
        QVERIFY(f.init());
        FileSharingPublication publication(&f.client, f.file, f.path(), f.jid);
        QSignalSpy             finished(&publication, &FileSharingPublication::finished);
        QSignalSpy             progress(&publication, &FileSharingPublication::progress);
        publication.start({}, true);
        // Starting twice while publishing must not duplicate HTTP requests or completion.
        publication.start({}, true);
        QCOMPARE(finished.count(), 0);
        uploadSlot(f);
        QCOMPARE(finished.count(), 0);
        auto completeHttp = [&] {
            f.nam.requests.last()->complete(
                httpFails ? 503 : 201, httpFails ? QNetworkReply::ServiceUnavailableError : QNetworkReply::NoError);
        };
        if (httpFirst)
            completeHttp();
        else
            announce(f, catalogFails);
        QCOMPARE(finished.count(), 0);
        if (httpFirst)
            announce(f, catalogFails);
        else
            completeHttp();
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(progress.count(), 1);
        QVERIFY(publication.sources().contains("xmpp:alice@example.test/device?jingle-ft"));
        QCOMPARE(publication.sources().contains("https://download.example.test/new/file.txt"), !httpFails);
        QCOMPARE(publication.sources().size(), httpFails ? 1 : 2);
        QCoreApplication::processEvents();
        QCOMPARE(finished.count(), 1);
    }
    void cachedSources_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<QVariant>("length");
        QTest::addColumn<bool>("valid");
        QTest::newRow("exists") << 200 << QVariant(7) << true;
        QTest::newRow("no-content-length") << 200 << QVariant() << true;
        QTest::newRow("deleted") << 404 << QVariant() << false;
        QTest::newRow("expired") << 410 << QVariant() << false;
        QTest::newRow("head-unsupported") << 405 << QVariant() << false;
        QTest::newRow("server-error") << 503 << QVariant() << false;
        QTest::newRow("wrong-size") << 200 << QVariant(8) << false;
        QTest::newRow("cancelled") << 0 << QVariant() << false;
    }
    void cachedSources()
    {
        QFETCH(int, status);
        QFETCH(QVariant, length);
        QFETCH(bool, valid);
        PublicationFixture f;
        QVERIFY(f.init());
        FileSharingPublication publication(&f.client, f.file, f.path(), f.jid);
        QSignalSpy             finished(&publication, &FileSharingPublication::finished);
        const QString          old = "https://download.example.test/old/file.txt";
        publication.start({ old, old, "xmpp:alice@example.test/old-device?jingle-ft" }, true);
        QCOMPARE(f.nam.requests.size(), 1);
        auto reply = f.nam.requests.first();
        QCOMPARE(reply->operation(), QNetworkAccessManager::HeadOperation);
        QCOMPARE(reply->url(), QUrl(old));
        QCOMPARE(reply->request().attribute(QNetworkRequest::CacheLoadControlAttribute).toInt(),
                 int(QNetworkRequest::AlwaysNetwork));
        announce(f);
        QCOMPARE(finished.count(), 0);
        if (status)
            reply->complete(status, QNetworkReply::NoError, length);
        else
            reply->abort();
        if (!valid) {
            QCOMPARE(finished.count(), 0);
            uploadSlot(f);
            f.nam.requests.last()->complete(201);
        }
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(publication.sources().contains(old), valid);
        QCOMPARE(publication.sources().contains("https://download.example.test/new/file.txt"), !valid);
        QVERIFY(!publication.sources().contains("xmpp:alice@example.test/old-device?jingle-ft"));
        QCOMPARE(publication.sources().size(), 2);
        QCOMPARE(f.nam.requests.size(), valid ? 1 : 2);
    }
    void mixedCachedSources()
    {
        PublicationFixture f;
        QVERIFY(f.init());
        FileSharingPublication publication(&f.client, f.file, f.path(), f.jid);
        QSignalSpy             finished(&publication, &FileSharingPublication::finished);
        const QString          good = "https://download.example.test/good";
        const QString          gone = "https://download.example.test/gone";
        publication.start({ good, gone }, true);
        QCOMPARE(f.nam.requests.size(), 2);
        announce(f);
        f.nam.requests[0]->complete(200);
        QCOMPARE(finished.count(), 0);
        f.nam.requests[1]->complete(404);
        QCOMPARE(finished.count(), 1);
        QVERIFY(publication.sources().contains(good));
        QVERIFY(!publication.sources().contains(gone));
        QCOMPARE(publication.sources().size(), 2);
        QCOMPARE(f.nam.requests.size(), 2); // No upload when a checked source remains available.
    }
    void httpOnly_data()
    {
        QTest::addColumn<bool>("httpFails");
        QTest::newRow("http-only") << false;
        QTest::newRow("all-failed") << true;
    }
    void httpOnly()
    {
        QFETCH(bool, httpFails);
        PublicationFixture f;
        QVERIFY(f.init());
        FileSharingPublication publication(&f.client, f.file, f.path(), f.jid);
        QSignalSpy             finished(&publication, &FileSharingPublication::finished);
        publication.start({ "xmpp:alice@example.test/stale?jingle-ft" }, false);
        uploadSlot(f);
        QCOMPARE(finished.count(), 0);
        f.nam.requests.last()->complete(httpFails ? 500 : 201,
                                        httpFails ? QNetworkReply::InternalServerError : QNetworkReply::NoError);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(publication.sources().size(), httpFails ? 0 : 1);
        for (const auto &source : publication.sources())
            QVERIFY(source.startsWith("https://"));
    }
    void noUploadService()
    {
        PublicationFixture f;
        QVERIFY(f.init());
        FileSharingPublication publication(&f.client, f.file, f.path(), f.jid);
        QSignalSpy             finished(&publication, &FileSharingPublication::finished);
        publication.start({}, true);
        uploadSlot(f, false);
        QCOMPARE(finished.count(), 0);
        announce(f);
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(publication.sources(), QStringList { "xmpp:alice@example.test/device?jingle-ft" });
    }
    void oob_data()
    {
        QTest::addColumn<Message::Type>("type");
        QTest::newRow("chat") << Message::Type::Chat;
        QTest::newRow("muc") << Message::Type::Groupchat;
    }
    void oob()
    {
        QFETCH(Message::Type, type);
        PublicationFixture f;
        QVERIFY(f.init());
        MediaSharing media;
        media.file    = f.file;
        media.sources = QStringList { "https://download.example.test/a?x=1&y=2", "http://download.example.test/b",
                                      "xmpp:alice@example.test/device?jingle-ft" };
        Reference reference(Reference::Data, media.sources.first());
        reference.setMediaSharing(media);
        Message message(Jid("bob@example.test"));
        message.setType(type);
        message.setReferences({ reference, reference });
        message.setBody(media.sources.first());
        message.setUrlList(fileSharingHttpUrls(message.references()));
        const auto  xml = message.toStanza(&f.stream).element();
        QStringList urls;
        for (auto x = xml.firstChildElement("x"); !x.isNull(); x = x.nextSiblingElement("x")) {
            if (x.namespaceURI() == "jabber:x:oob")
                urls.append(x.firstChildElement("url").text());
        }
        QCOMPARE(urls, media.sources.mid(0, 2));
        QCOMPARE(xml.firstChildElement("body").text(), media.sources.first());
        QCOMPARE(xml.elementsByTagName("media-sharing").size(), 2);
        media.sources = QStringList { "xmpp:alice@example.test/device?jingle-ft" };
        reference.setMediaSharing(media);
        QVERIFY(fileSharingHttpUrls({ reference }).isEmpty());
    }
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
