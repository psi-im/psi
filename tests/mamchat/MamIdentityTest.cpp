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
};
QTEST_MAIN(MamIdentityTest)
#include "MamIdentityTest.moc"
