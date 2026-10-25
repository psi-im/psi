/* SPDX-License-Identifier: GPL-2.0-or-later */
// NativeChatAvatars is a sender router. Central AvatarCache owns both decoded
// positive images and short-lived negatives; no per-window memo is permitted.
#include "nativechatavatars.h"
#include "messageview.h"
#include "psiaccount.h"

#include <QTest>

namespace {
QPixmap pixmap(Qt::GlobalColor color)
{
    QPixmap p(8, 8);
    p.fill(color);
    return p;
}
}

class NativeChatAvatarsTest : public QObject {
    Q_OBJECT
private slots:
    void directChatAlwaysDelegatesToCentralFactory()
    {
        PsiAccount account;
        account.factory.contacts.insert(QStringLiteral("me@example.org"), pixmap(Qt::red));
        NativeChatAvatars avatars(&account, false);
        avatars.setPeer(XMPP::Jid(QStringLiteral("friend@example.org/phone")));
        for (int i = 0; i < 5; ++i) {
            QVERIFY(avatars.avatar(MessageView(false)).isNull());
            QVERIFY(!avatars.avatar(MessageView(true)).isNull());
        }
        QCOMPARE(account.factory.contactLookups, 10);
        QCOMPARE(account.factory.occupantLookups, 0);
        // Actual negative lookup throttling is separately tested against the
        // production AvatarNegativeMemo and source invalidation.
    }
    void directChatInvalidationDoesNotDependOnOldLocalMemo()
    {
        PsiAccount account;
        NativeChatAvatars avatars(&account, false);
        avatars.setPeer(XMPP::Jid(QStringLiteral("friend@example.org/phone")));
        QVERIFY(!avatars.invalidate(XMPP::Jid(QStringLiteral("stranger@example.org"))));
        QVERIFY(avatars.invalidate(XMPP::Jid(QStringLiteral("friend@example.org"))));
        QVERIFY(avatars.invalidate(XMPP::Jid(QStringLiteral("friend@example.org"))));
        account.factory.contacts.insert(QStringLiteral("friend@example.org"), pixmap(Qt::blue));
        QVERIFY(!avatars.avatar(MessageView(false)).isNull());
        QVERIFY(avatars.invalidate(XMPP::Jid(QStringLiteral("me@example.org"))));
        QCOMPARE(account.factory.contactLookups, 1);
    }
    void roomOccupantChatUsesOccupantNotRoom()
    {
        PsiAccount account;
        account.factory.contacts.insert(QStringLiteral("room@muc.example.org"), pixmap(Qt::green));
        account.factory.occupants.insert(QStringLiteral("room@muc.example.org/alice"), pixmap(Qt::yellow));
        NativeChatAvatars avatars(&account, false);
        avatars.setPeer(XMPP::Jid(QStringLiteral("room@muc.example.org/alice")), true);
        QCOMPARE(avatars.avatar(MessageView(false)).toImage().pixelColor(0, 0), QColor(Qt::yellow));
        QVERIFY(!avatars.invalidate(XMPP::Jid(QStringLiteral("room@muc.example.org"))));
        QVERIFY(!avatars.invalidate(XMPP::Jid(QStringLiteral("room@muc.example.org/bob"))));
        QVERIFY(avatars.invalidate(XMPP::Jid(QStringLiteral("room@muc.example.org/alice"))));
        QVERIFY(!avatars.setPeer(XMPP::Jid(QStringLiteral("room@muc.example.org/alice")), true));
        QVERIFY(avatars.setPeer(XMPP::Jid(QStringLiteral("room@muc.example.org/carol")), true));
        QCOMPARE(account.factory.contactLookups, 0);
    }
    void groupChatRoutesEachNickWithoutPermanentNulls()
    {
        PsiAccount account;
        NativeChatAvatars avatars(&account, true);
        avatars.setPeer(XMPP::Jid(QStringLiteral("room@muc.example.org/me")));
        for (int i = 0; i < 3; ++i) {
            avatars.avatar(MessageView(false, QStringLiteral("alice")));
            avatars.avatar(MessageView(false, QStringLiteral("bob")));
        }
        QCOMPARE(account.factory.occupantLookups, 6);
        QVERIFY(!avatars.invalidate(XMPP::Jid(QStringLiteral("other@muc.example.org/alice"))));
        QVERIFY(avatars.invalidate(XMPP::Jid(QStringLiteral("room@muc.example.org/carol"))));
        QVERIFY(avatars.invalidate(XMPP::Jid(QStringLiteral("room@muc.example.org/alice"))));
        QVERIFY(!avatars.setPeer(XMPP::Jid(QStringLiteral("room@muc.example.org/newme"))));
        QVERIFY(avatars.setPeer(XMPP::Jid(QStringLiteral("other@muc.example.org/me"))));
        avatars.avatar(MessageView(false, QStringLiteral("bob")));
        QCOMPARE(account.factory.occupantLookups, 7);
    }
    void rejoinAndNickReuseAlwaysReresolve()
    {
        PsiAccount account;
        const XMPP::Jid room(QStringLiteral("room@muc.example.org/me"));
        NativeChatAvatars avatars(&account, true);
        QVERIFY(avatars.setPeer(room));
        avatars.avatar(MessageView(false, QStringLiteral("alice")));
        account.factory.occupants.insert(QStringLiteral("room@muc.example.org/bob"), pixmap(Qt::red));
        QVERIFY(!avatars.avatar(MessageView(false, QStringLiteral("bob"))).isNull());
        QCOMPARE(account.factory.occupantLookups, 2);

        QVERIFY(!avatars.setPeer(room));
        account.factory.occupants.insert(QStringLiteral("room@muc.example.org/alice"), pixmap(Qt::green));
        account.factory.occupants.insert(QStringLiteral("room@muc.example.org/bob"), pixmap(Qt::blue));
        QVERIFY(avatars.resetRoom());
        QCOMPARE(avatars.avatar(MessageView(false, QStringLiteral("bob"))).toImage().pixelColor(0, 0),
                 QColor(Qt::blue));
        QVERIFY(!avatars.avatar(MessageView(false, QStringLiteral("alice"))).isNull());
        QCOMPARE(account.factory.occupantLookups, 4);
    }
    void twoAccountsWithSameContactRemainSeparate()
    {
        PsiAccount a, b;
        a.factory.contacts.insert(QStringLiteral("same@example.org"), pixmap(Qt::red));
        b.factory.contacts.insert(QStringLiteral("same@example.org"), pixmap(Qt::blue));
        NativeChatAvatars first(&a, false), second(&b, false);
        const XMPP::Jid peer(QStringLiteral("same@example.org"));
        first.setPeer(peer);
        second.setPeer(peer);
        QCOMPARE(first.avatar(MessageView(false)).toImage().pixelColor(0, 0), QColor(Qt::red));
        QCOMPARE(second.avatar(MessageView(false)).toImage().pixelColor(0, 0), QColor(Qt::blue));
        QCOMPARE(a.factory.contactLookups, 1);
        QCOMPARE(b.factory.contactLookups, 1);
    }
};
QTEST_MAIN(NativeChatAvatarsTest)
#include "nativechatavatars.moc"
