/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/MamMessageIdentity.h"

#include <QtTest/QtTest>

class MamIdentityTest : public QObject {
    Q_OBJECT
private slots:
    void distinctArchiveAndLiveDeliveries()
    {
        const auto now = QDateTime::fromString(QStringLiteral("2026-10-18T09:00:00Z"), Qt::ISODate);
        const PsiMam::DeliveryIdentity live{QStringLiteral("abc"), QStringLiteral("hello"), true, now};
        auto archived = live;
        archived.timestamp = now.addSecs(90);
        QVERIFY(PsiMam::sameDelivery(live, archived));

        archived.outgoing = false;
        QVERIFY(!PsiMam::sameDelivery(live, archived));
        archived = live;
        archived.richText = QStringLiteral("other");
        QVERIFY(!PsiMam::sameDelivery(live, archived));
        archived = live;
        archived.stanzaId = QStringLiteral("abc2");
        QVERIFY(!PsiMam::sameDelivery(live, archived));
        archived = live;
        archived.stanzaId.clear();
        QVERIFY(!PsiMam::sameDelivery(live, archived));
        archived = live;
        archived.timestamp = now.addSecs(301);
        QVERIFY(!PsiMam::sameDelivery(live, archived));
        archived.timestamp = now.addSecs(-300);
        QVERIFY(PsiMam::sameDelivery(live, archived));
    }

    void replyTarget_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<QString>("to");
        QTest::addColumn<bool>("groupChat");
        QTest::addColumn<bool>("expected");
        const QString anna = QStringLiteral("anna@example.com");
        const QString occupant = QStringLiteral("room@muc.example.com/anna");
        // 1:1 candidate: anna@example.com/tablet, target "origin", client ids "client" and "corrected".
        QTest::newRow("direct, target, bare to") << "origin" << anna << false << true;
        QTest::newRow("direct, target, sending resource") << "origin" << anna + "/tablet" << false << true;
        QTest::newRow("direct, target, other resource, as in XEP-0461 example 4")
            << "origin" << anna + "/laptop" << false << true;
        QTest::newRow("direct, target, no to") << "origin" << QString() << false << true;
        QTest::newRow("direct, client id") << "client" << anna << false << true;
        QTest::newRow("direct, correction id") << "corrected" << anna << false << true;
        QTest::newRow("direct, other author") << "origin" << "bob@example.com/tablet" << false << false;
        QTest::newRow("direct, unknown id") << "other" << anna << false << false;
        QTest::newRow("direct, plain-text quote") << QString() << anna << false << false;
        // Room candidate: room@muc.example.com/anna, target "stanza", client id "client".
        QTest::newRow("room, stanza-id, occupant") << "stanza" << occupant << true << true;
        QTest::newRow("room, stanza-id, no to") << "stanza" << QString() << true << true;
        QTest::newRow("room, stanza-id, other occupant") << "stanza" << "room@muc.example.com/bob" << true << false;
        QTest::newRow("room, stanza-id, the room itself") << "stanza" << "room@muc.example.com" << true << false;
        QTest::newRow("room, the 'id' attribute must not be used") << "client" << occupant << true << false;
        QTest::newRow("room, plain-text quote") << QString() << occupant << true << false;
    }
    void replyTarget()
    {
        QFETCH(QString, id);
        QFETCH(QString, to);
        QFETCH(bool, groupChat);
        QFETCH(bool, expected);
        const PsiMam::ReplyCandidate candidate = groupChat
            ? PsiMam::ReplyCandidate { QStringLiteral("room@muc.example.com/anna"), QStringLiteral("stanza"),
                                       { QStringLiteral("client") } }
            : PsiMam::ReplyCandidate { QStringLiteral("anna@example.com/tablet"), QStringLiteral("origin"),
                                       { QStringLiteral("client"), QStringLiteral("corrected") } };
        QCOMPARE(PsiMam::isReplyTarget(id, to, groupChat, candidate), expected);
    }
    void roomMessageWithoutStanzaIdCannotBeReferenced()
    {
        // Its target is empty (XEP-0461 "Using the correct ID"), and so is a plain-text quote's id.
        const PsiMam::ReplyCandidate candidate { QStringLiteral("room@muc.example.com/anna"), {}, {} };
        QVERIFY(!PsiMam::isReplyTarget({}, {}, true, candidate));
        QVERIFY(!PsiMam::isReplyTarget(QStringLiteral("client"), {}, true, candidate));
    }
    void onlyAnUnambiguousReferenceHasATarget()
    {
        const QList<PsiMam::ReplyCandidate> rows {
            { QStringLiteral("anna@example.com/tablet"), QStringLiteral("first"), {} },
            { QStringLiteral("anna@example.com/tablet"), QStringLiteral("reused"), {} },
            { QStringLiteral("bob@example.com/phone"), QStringLiteral("second"), {} },
            { QStringLiteral("anna@example.com/phone"), QStringLiteral("reused"), {} },
            { QStringLiteral("bob@example.com/phone"), QStringLiteral("reused"), {} },
        };
        const auto target = [&rows](const QString &id, const QString &to) {
            return PsiMam::uniqueReplyTarget(id, to, false, rows.size(), [&rows](int at) { return rows.at(at); });
        };
        QCOMPARE(target(QStringLiteral("first"), {}), 0);
        QCOMPARE(target(QStringLiteral("second"), QStringLiteral("bob@example.com")), 2);
        QCOMPARE(target(QStringLiteral("missing"), {}), -1);
        QCOMPARE(target(QStringLiteral("reused"), {}), -1);
        // The author tells a reused id apart, unless the same author reused it.
        QCOMPARE(target(QStringLiteral("reused"), QStringLiteral("bob@example.com")), 4);
        QCOMPARE(target(QStringLiteral("reused"), QStringLiteral("anna@example.com")), -1);
    }
};
QTEST_MAIN(MamIdentityTest)
#include "MamIdentityTest.moc"
