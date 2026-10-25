/* SPDX-License-Identifier: GPL-2.0-or-later */
// NativeChatAvatars: one lookup per sender (including "no avatar"),
// invalidation only for this timeline's senders.
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
    void directChatResolvesEachSenderOnce()
    {
        PsiAccount account;
        account.factory.contacts.insert(QStringLiteral("me@example.org"), pixmap(Qt::red));
        NativeChatAvatars avatars(&account, false);
        avatars.setPeer(XMPP::Jid(QStringLiteral("friend@example.org/phone")));
        for (int i = 0; i < 5; ++i) {
            QVERIFY(avatars.avatar(MessageView(false)).isNull()); // the peer has no avatar
            QVERIFY(!avatars.avatar(MessageView(true)).isNull());
        }
        QCOMPARE(account.factory.contactLookups, 2); // missing avatars are cached too
        QCOMPARE(account.factory.occupantLookups, 0);
    }

    void directChatInvalidatesOnlyItsSenders()
    {
        PsiAccount        account;
        NativeChatAvatars avatars(&account, false);
        avatars.setPeer(XMPP::Jid(QStringLiteral("friend@example.org/phone")));
        avatars.avatar(MessageView(false));
        avatars.avatar(MessageView(true));
        QVERIFY(!avatars.invalidate(XMPP::Jid(QStringLiteral("stranger@example.org"))));
        QVERIFY(avatars.invalidate(XMPP::Jid(QStringLiteral("friend@example.org")))); // any resource
        QVERIFY(!avatars.invalidate(XMPP::Jid(QStringLiteral("friend@example.org"))));  // not cached now
        account.factory.contacts.insert(QStringLiteral("friend@example.org"), pixmap(Qt::blue));
        QVERIFY(!avatars.avatar(MessageView(false)).isNull());
        QVERIFY(avatars.invalidate(XMPP::Jid(QStringLiteral("me@example.org"))));
        QCOMPARE(account.factory.contactLookups, 3);
    }

    void roomOccupantChatUsesTheOccupantNotTheRoom()
    {
        PsiAccount account;
        account.factory.contacts.insert(QStringLiteral("room@muc.example.org"), pixmap(Qt::green)); // room logo
        account.factory.occupants.insert(QStringLiteral("room@muc.example.org/alice"), pixmap(Qt::yellow));
        NativeChatAvatars avatars(&account, false);
        avatars.setPeer(XMPP::Jid(QStringLiteral("room@muc.example.org/alice")), true);
        QCOMPARE(avatars.avatar(MessageView(false)).toImage().pixelColor(0, 0), QColor(Qt::yellow));
        QVERIFY(!avatars.invalidate(XMPP::Jid(QStringLiteral("room@muc.example.org"))));     // room avatar
        QVERIFY(!avatars.invalidate(XMPP::Jid(QStringLiteral("room@muc.example.org/bob")))); // other occupant
        QVERIFY(avatars.invalidate(XMPP::Jid(QStringLiteral("room@muc.example.org/alice"))));
        QVERIFY(!avatars.setPeer(XMPP::Jid(QStringLiteral("room@muc.example.org/alice")), true));
        QVERIFY(avatars.setPeer(XMPP::Jid(QStringLiteral("room@muc.example.org/carol")), true));
        QCOMPARE(account.factory.contactLookups, 0);
    }

    void rejoiningSameRoomDropsPositiveAndNegativeOccupantMemo()
    {
        PsiAccount account;
        const XMPP::Jid room(QStringLiteral("room@muc.example.org/me"));
        NativeChatAvatars avatars(&account, true);
        QVERIFY(avatars.setPeer(room));

        avatars.avatar(MessageView(false, QStringLiteral("alice"))); // null, memoized
        account.factory.occupants.insert(QStringLiteral("room@muc.example.org/bob"), pixmap(Qt::red));
        const auto previousBob = avatars.avatar(MessageView(false, QStringLiteral("bob")));
        QVERIFY(!previousBob.isNull());
        QCOMPARE(account.factory.occupantLookups, 2);
        QVERIFY(!avatars.setPeer(room)); // Same room: setPeer alone cannot detect rejoin.

        account.factory.occupants.insert(QStringLiteral("room@muc.example.org/alice"), pixmap(Qt::green));
        account.factory.occupants.insert(QStringLiteral("room@muc.example.org/bob"), pixmap(Qt::blue));
        QVERIFY(avatars.resetRoom());
        QVERIFY(!avatars.resetRoom()); // Idempotent; no repaint required.
        QVERIFY(!avatars.avatar(MessageView(false, QStringLiteral("alice"))).isNull());
        QCOMPARE(avatars.avatar(MessageView(false, QStringLiteral("bob"))).toImage().pixelColor(0, 0),
                 QColor(Qt::blue));
        QCOMPARE(account.factory.occupantLookups, 4);
    }

    void groupChatCachesOccupantsByNick()
    {
        PsiAccount        account;
        NativeChatAvatars avatars(&account, true);
        avatars.setPeer(XMPP::Jid(QStringLiteral("room@muc.example.org/me")));
        for (int i = 0; i < 3; ++i) {
            avatars.avatar(MessageView(false, QStringLiteral("alice")));
            avatars.avatar(MessageView(false, QStringLiteral("bob")));
        }
        QCOMPARE(account.factory.occupantLookups, 2);
        QVERIFY(!avatars.invalidate(XMPP::Jid(QStringLiteral("other@muc.example.org/alice"))));
        QVERIFY(!avatars.invalidate(XMPP::Jid(QStringLiteral("room@muc.example.org/carol")))); // never shown
        QVERIFY(avatars.invalidate(XMPP::Jid(QStringLiteral("room@muc.example.org/alice"))));
        // Our own nick change keeps the occupants; another room drops them.
        QVERIFY(!avatars.setPeer(XMPP::Jid(QStringLiteral("room@muc.example.org/newme"))));
        QVERIFY(avatars.setPeer(XMPP::Jid(QStringLiteral("other@muc.example.org/me"))));
        avatars.avatar(MessageView(false, QStringLiteral("bob"))); // re-resolved in the new room
        QCOMPARE(account.factory.occupantLookups, 3);
    }
};

QTEST_MAIN(NativeChatAvatarsTest)
#include "nativechatavatars.moc"
