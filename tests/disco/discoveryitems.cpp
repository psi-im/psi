// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise the real tree items and Iris IQ dispatcher without a server or profile.
#include "discoitems_p.h"
#include "tasklist.h"
#include <QHeaderView>
#include <QSignalSpy>
#include <QTest>
#include <QTextDocument>
#include <QtCrypto>
#include <iris/xmpp.h>
#include <iris/xmpp_client.h>
#include <iris/xmpp_clientstream.h>

class NoNetworkConnector final : public XMPP::Connector {
public:
    void        setOptHostPort(const QString &, quint16) override { }
    void        connectToServer(const QString &) override { }
    ByteStream *stream() const override { return nullptr; }
    void        done() override { }
};
class RecordingStream final : public XMPP::ClientStream {
public:
    explicit RecordingStream(Connector *connector) : ClientStream(connector) { }
    void write(const Stanza &stanza) override { sent.append(stanza.element().cloneNode(true).toElement()); }
    QList<QDomElement> sent;
};

static const QString itemsNs   = QStringLiteral("http://jabber.org/protocol/disco#items");
static const QString infoNs    = QStringLiteral("http://jabber.org/protocol/disco#info");
static const QString firstPage = QStringLiteral(
    "<item jid='a.example.test' name='A'/><item jid='b.example.test' name='B'/>"
    "<set xmlns='http://jabber.org/protocol/rsm'><first index='0'>a</first><last>b</last><count>3</count></set>");
static void flushDeletes() { QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); }

struct Fixture {
    NoNetworkConnector connector;
    RecordingStream    stream { &connector };
    XMPP::Client       client;
    TaskList           tasks;
    DiscoConnector     updates { nullptr };
    DiscoData          data;
    QTreeWidget        tree;
    QStringList        errors;
    bool               autoItems = false, autoInfo = false;
    DiscoListItem     *root = nullptr;

    Fixture()
    {
        client.connectToStream(&stream, Jid(QStringLiteral("user@example.test/resource")));
        data.rootTask         = client.rootTask();
        data.tasks            = &tasks;
        data.d                = &updates;
        data.autoItemsEnabled = [this] { return autoItems; };
        data.autoInfoEnabled  = [this] { return autoInfo; };
        data.itemsPerPage     = [] { return 10; };
        data.itemIcon         = [](const DiscoItem &, bool) { return QIcon(); };
        data.showError        = [this](const QString &message) { errors.append(message); };
        tree.setColumnCount(3);
        tree.setSortingEnabled(true);
        tree.sortByColumn(1, Qt::AscendingOrder);
        DiscoItem item;
        item.setJid(Jid(QStringLiteral("services.example.test")));
        root = new DiscoListItem(item, &data, &tree);
        if (!reply(stream.sent.constLast(), { }))
            qFatal("Initial discovery info reply was not accepted");
        flushDeletes();
        stream.sent.clear();
    }
    ~Fixture()
    {
        data.cancelTasks();
        tree.clear();
        flushDeletes();
    }
    QList<QDomElement> requests(const QString &ns) const
    {
        QList<QDomElement> result;
        for (const auto &iq : stream.sent) {
            if (iq.firstChildElement(QStringLiteral("query")).namespaceURI() == ns)
                result.append(iq);
        }
        return result;
    }
    bool reply(const QDomElement &request, const QString &body, bool error = false)
    {
        QDomDocument doc;
        auto         iq = doc.createElement(QStringLiteral("iq"));
        iq.setAttribute(QStringLiteral("from"), request.attribute(QStringLiteral("to")));
        iq.setAttribute(QStringLiteral("id"), request.attribute(QStringLiteral("id")));
        iq.setAttribute(QStringLiteral("type"), error ? QStringLiteral("error") : QStringLiteral("result"));
        QDomDocument payload;
        const auto   ns  = request.firstChildElement(QStringLiteral("query")).namespaceURI();
        const auto   xml = error ? body : QStringLiteral("<query xmlns='%1'>%2</query>").arg(ns, body);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        if (!payload.setContent(xml, QDomDocument::ParseOption::UseNamespaceProcessing))
#else
        if (!payload.setContent(xml, true))
#endif
            qFatal("Invalid discovery fixture XML");
        iq.appendChild(doc.importNode(payload.documentElement(), true));
        return client.rootTask()->take(iq);
    }
    bool page(const QString &body)
    {
        root->updateItems();
        return reply(requests(itemsNs).constLast(), body);
    }
    DiscoListItem *item(const QString &jid) const
    {
        for (int i = 0; i < root->childCount(); ++i) {
            auto *child = DiscoListItem::fromItem(root->child(i));
            if (child && child->item().jid().full() == jid)
                return child;
        }
        return nullptr;
    }
    Task *task(const QDomElement &request) const
    {
        for (auto *pending : tasks) {
            if (pending->id() == request.attribute(QStringLiteral("id")))
                return pending;
        }
        return nullptr;
    }
    DiscoExtraItem *more() const
    {
        for (int i = 0; i < root->childCount(); ++i) {
            if (root->child(i)->type() == MoreItemsType)
                return static_cast<DiscoExtraItem *>(root->child(i));
        }
        return nullptr;
    }
};

class DiscoveryItemsTest : public QObject {
    Q_OBJECT
private slots:
    void specialRowsAreNotDiscoveryItems()
    {
        Fixture f;
        QVERIFY(f.page(firstPage));
        QVERIFY(f.more());
        QVERIFY(!DiscoListItem::fromItem(f.more()));
        QVERIFY(!DiscoListItem::fromItem(nullptr));
        QCOMPARE(DiscoListItem::fromItem(f.root), f.root);
    }
    void continuationReusesItems()
    {
        Fixture f;
        QVERIFY(f.page(firstPage));
        auto *a = f.item(QStringLiteral("a.example.test"));
        auto *b = f.item(QStringLiteral("b.example.test"));
        QVERIFY(a && b && f.more());
        f.more()->itemClicked();
        const auto next = f.requests(itemsNs).constLast();
        QCOMPARE(next.firstChildElement("query").firstChildElement("set").firstChildElement("after").text(),
                 QStringLiteral("b"));
        const auto requestCount = f.stream.sent.size();
        f.more()->itemClicked();
        QCOMPARE(f.stream.sent.size(), requestCount);
        QVERIFY(f.reply(next,
                        QStringLiteral("<item jid='b.example.test' name='Updated B'/><item jid='c.example.test'/>"
                                       "<set xmlns='http://jabber.org/protocol/rsm'><first "
                                       "index='1'>b</first><last>c</last><count>3</count></set>")));
        QCOMPARE(f.root->childCount(), 3);
        QCOMPARE(f.item(QStringLiteral("a.example.test")), a);
        QCOMPARE(f.item(QStringLiteral("b.example.test")), b);
        QCOMPARE(b->item().name(), QStringLiteral("Updated B"));
        QVERIFY(!f.more());
    }
    void refreshReplacesPagedResults()
    {
        Fixture f;
        QVERIFY(f.page(firstPage));
        QPointer<DiscoListItem> removed = f.item(QStringLiteral("a.example.test"));
        QPointer<DiscoListItem> reused  = f.item(QStringLiteral("b.example.test"));
        QVERIFY(f.page(QStringLiteral("<item jid='b.example.test' name='Refreshed'/>")));
        QVERIFY(removed.isNull());
        QCOMPARE(f.root->childCount(), 1);
        QCOMPARE(f.item(QStringLiteral("b.example.test")), reused.data());
        QCOMPARE(reused->item().name(), QStringLiteral("Refreshed"));
        QVERIFY(!f.more());
        QVERIFY(f.page({ }));
        QVERIFY(reused.isNull());
        QCOMPARE(f.root->childCount(), 0);
    }
    void duplicateItemsWithinOnePage()
    {
        Fixture f;
        QVERIFY(f.page(
            QStringLiteral("<item jid='a.example.test' name='First'/><item jid='a.example.test' name='Last'/>")));
        QCOMPARE(f.root->childCount(), 1);
        QCOMPARE(f.item(QStringLiteral("a.example.test"))->item().name(), QStringLiteral("Last"));
    }
    void automaticRequests_data()
    {
        QTest::addColumn<bool>("automaticInfo");
        QTest::addColumn<int>("sortOrder");
        QTest::newRow("lazy-info-ascending") << false << int(Qt::AscendingOrder);
        QTest::newRow("lazy-info-descending") << false << int(Qt::DescendingOrder);
        QTest::newRow("auto-info-ascending") << true << int(Qt::AscendingOrder);
        QTest::newRow("auto-info-descending") << true << int(Qt::DescendingOrder);
    }
    void automaticRequests()
    {
        QFETCH(bool, automaticInfo);
        QFETCH(int, sortOrder);
        Fixture f;
        f.autoItems = true;
        f.autoInfo  = automaticInfo;
        f.tree.sortByColumn(1, Qt::SortOrder(sortOrder));
        f.root->setExpanded(true);
        QCOMPARE(f.requests(itemsNs).size(), 1);
        QVERIFY(f.reply(f.requests(itemsNs).constLast(), firstPage));
        QCOMPARE(f.requests(itemsNs).size(), 3); // Root plus two real children; no request for the extra row.
        QCOMPARE(f.root->child(f.root->childCount() - 1), static_cast<QTreeWidgetItem *>(f.more()));
        QCOMPARE(f.requests(infoNs).size(), automaticInfo ? 2 : 0);
        f.root->setExpanded(true);
        QCOMPARE(f.requests(itemsNs).size(), 3); // In-flight child requests are reused.
        auto *a = f.item(QStringLiteral("a.example.test"));
        a->itemSelected();
        a->itemSelected();
        QCOMPARE(f.requests(infoNs).size(), automaticInfo ? 2 : 1);
        const auto childRequests = f.requests(itemsNs);
        QVERIFY(f.reply(childRequests.at(1), { }));
        QVERIFY(f.reply(childRequests.at(2), { }));
        f.root->setExpanded(true);
        QCOMPARE(f.requests(itemsNs).size(), 3);
    }
    void supersededResponsesDoNotUpdateTree()
    {
        Fixture f;
        f.root->updateItems();
        const auto old = f.requests(itemsNs).constLast();
        f.root->updateItems();
        QVERIFY(f.reply(f.requests(itemsNs).constLast(), QStringLiteral("<item jid='current.example.test'/>")));
        QVERIFY(f.reply(old, QStringLiteral("<item jid='old.example.test'/>")));
        QCOMPARE(f.root->childCount(), 1);
        QVERIFY(f.item(QStringLiteral("current.example.test")));
        QVERIFY(!f.item(QStringLiteral("old.example.test")));
    }
    void stopIgnoresResponsesBeforeDeferredDeletion()
    {
        Fixture f;
        f.root->updateItems();
        const auto pending = f.requests(itemsNs).constLast();
        QSignalSpy finished(&f.tasks, &TaskList::finished);
        f.data.cancelTasks();
        QVERIFY(f.reply(pending, QStringLiteral("<item jid='late.example.test'/>")));
        QCOMPARE(f.root->childCount(), 0);
        flushDeletes();
        QVERIFY(f.tasks.isEmpty());
        QVERIFY(!finished.isEmpty());
        QVERIFY(f.errors.isEmpty());
        QVERIFY(f.page(QStringLiteral("<item jid='new.example.test'/>")));
        QVERIFY(f.item(QStringLiteral("new.example.test")));
    }
    void deletingItemsCancelsTheirRequests()
    {
        Fixture f;
        f.autoInfo  = true;
        f.autoItems = true;
        f.root->setExpanded(true);
        QVERIFY(f.reply(f.requests(itemsNs).constLast(), firstPage));
        const auto     pendingInfo  = f.requests(infoNs).constLast();
        const auto     pendingItems = f.requests(itemsNs).constLast();
        QPointer<Task> infoTask     = f.task(pendingInfo);
        QPointer<Task> itemsTask    = f.task(pendingItems);
        auto          *b            = f.item(QStringLiteral("b.example.test"));
        QVERIFY(b);
        delete b;
        QVERIFY(infoTask && itemsTask);
        flushDeletes();
        QVERIFY(infoTask.isNull());
        QVERIFY(itemsTask.isNull());
        f.reply(pendingInfo, { });
        f.reply(pendingItems, { });
        QCOMPARE(f.root->childCount(), 2);
        QVERIFY(f.errors.isEmpty());
    }
    void failuresRemainRetryable()
    {
        Fixture f;
        QVERIFY(f.page(firstPage));
        f.more()->itemClicked();
        const auto next = f.requests(itemsNs).constLast();
        QVERIFY(
            f.reply(next,
                    QStringLiteral(
                        "<error type='cancel'><item-not-found xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/></error>"),
                    true));
        QCOMPARE(f.errors.size(), 1);
        const auto count = f.requests(itemsNs).size();
        f.more()->itemClicked();
        QCOMPARE(f.requests(itemsNs).size(), count + 1);
        QCOMPARE(f.requests(itemsNs)
                     .constLast()
                     .firstChildElement("query")
                     .firstChildElement("set")
                     .firstChildElement("after")
                     .text(),
                 QStringLiteral("b"));
        QVERIFY(f.reply(f.requests(itemsNs).constLast(), { }));
        QVERIFY(!f.more());
        QCOMPARE(f.root->childCount(), 2);
    }
    void backgroundErrorsStayNonModal()
    {
        Fixture f;
        f.autoItems = true;
        f.autoInfo  = true;
        f.root->setExpanded(true);
        QVERIFY(f.reply(f.requests(itemsNs).constLast(), firstPage));
        const QString error = QStringLiteral(
            "<error type='cancel'><service-unavailable xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
            "<text xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'>&lt;img src='remote'/&gt;</text></error>");
        QVERIFY(f.reply(f.requests(itemsNs).at(1), error, true));
        QVERIFY(f.reply(f.requests(infoNs).at(0), error, true));
        QVERIFY(f.errors.isEmpty());
        auto *a = f.item(QStringLiteral("a.example.test"));
        QVERIFY(!a->getErrorInfo().isEmpty());
        a->updateInfo();
        QVERIFY(f.reply(f.requests(infoNs).constLast(), { }));
        QVERIFY(a->getErrorInfo().isEmpty());
        f.root->updateItems();
        QVERIFY(f.reply(f.requests(itemsNs).constLast(), error, true));
        QCOMPARE(f.errors.size(), 1);
        QVERIFY(!f.errors.constLast().contains(QStringLiteral("<img")));
        QVERIFY(f.errors.constLast().contains(QStringLiteral("&lt;img")));
    }
    void errorDialogCanDeleteItem_data()
    {
        QTest::addColumn<bool>("info");
        QTest::newRow("items") << false;
        QTest::newRow("info") << true;
    }
    void errorDialogCanDeleteItem()
    {
        QFETCH(bool, info);
        Fixture                 f;
        QPointer<DiscoListItem> root = f.root;
        f.data.showError             = [&f](const QString &) {
            delete f.root;
            f.root = nullptr;
        };
        if (info)
            f.root->updateInfo();
        else
            f.root->updateItems();
        const auto request = f.requests(info ? infoNs : itemsNs).constLast();
        QVERIFY(f.reply(
            request,
            QStringLiteral(
                "<error type='cancel'><service-unavailable xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/></error>"),
            true));
        QVERIFY(root.isNull());
        flushDeletes();
    }
    void tooltipEscapesRemoteText()
    {
        const QString text = QStringLiteral("<img src='remote'>& payload");
        DiscoItem     item;
        item.setJid(Jid(QStringLiteral("service.example.test")));
        item.setName(text);
        item.setNode(text);
        DiscoItem::Identity id;
        id.name     = text;
        id.category = text;
        id.type     = text;
        item.setIdentities({ id });
        item.setFeatures(Features(text));
        const auto tip = discoItemToolTip(item, text + QLatin1Char('\n') + text);
        QVERIFY(!tip.contains(QStringLiteral("<img")));
        QVERIFY(tip.contains(QStringLiteral("&lt;img")));
        QTextDocument document;
        document.setHtml(tip);
        QVERIFY(document.toPlainText().contains(text));
        QVERIFY(!document.toHtml().contains(QStringLiteral("<img")));
    }

private:
    QCA::Initializer qca;
};
QTEST_MAIN(DiscoveryItemsTest)
#include "discoveryitems.moc"
