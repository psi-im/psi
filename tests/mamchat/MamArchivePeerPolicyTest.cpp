/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/MamArchivePeerPolicy.h"

#include <QtTest/QtTest>

class MamArchivePeerPolicyTest final : public QObject {
    Q_OBJECT
private slots:
    void strictConversationMembership()
    {
        const QString me = QStringLiteral("alice@example.test");
        const QString peer = QStringLiteral("bob@example.test");
        const QString third = QStringLiteral("mallory@example.test");
        QVERIFY(PsiMam::belongsToConversation(me, peer, me, peer));
        QVERIFY(PsiMam::belongsToConversation(me, peer, peer, me));
        QVERIFY(!PsiMam::belongsToConversation(me, peer, peer, third));
        QVERIFY(!PsiMam::belongsToConversation(me, peer, third, peer));
        QVERIFY(!PsiMam::belongsToConversation(me, peer, me, third));
        QVERIFY(!PsiMam::belongsToConversation(me, peer, third, me));
        QVERIFY(!PsiMam::belongsToConversation(me, peer, QString(), peer));
        QVERIFY(!PsiMam::belongsToConversation(me, peer, peer, QString()));
        QVERIFY(PsiMam::belongsToConversation(me, me, me, me));
        QVERIFY(!PsiMam::belongsToConversation(QString(), peer, peer, me));
    }
};
QTEST_MAIN(MamArchivePeerPolicyTest)
#include "MamArchivePeerPolicyTest.moc"
