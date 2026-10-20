/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/MamChatWidget.h"
#include "widgets/ChatMessageText.h"
#include "desktopcolorpalette.h"
#include "filesharingmanager.h"
#include "qiteaudio.h"
#include "qitevideo.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QFrame>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTextBlock>
#include <QTextFragment>
#include <QTimer>
#include <QUrlQuery>
#include <QWheelEvent>
#include <QtTest/QtTest>
#include <cmath>
#include <memory>

namespace {
MessageView message(const QString &id, const QString &body, int seconds = 0, bool local = false)
{
    MessageView view(MessageView::Message);
    view.setMessageId(id);
    view.setPlainText(body);
    view.setNick(local ? QStringLiteral("Me") : QStringLiteral("Peer"));
    view.setLocal(local);
    view.setDateTime(QDateTime::fromString(QStringLiteral("2026-10-18T10:00:00Z"),
                                           Qt::ISODate).addSecs(seconds));
    return view;
}
}

class MamChatWidgetTest final : public QObject {
    Q_OBJECT
private slots:
    void emoteUsesSenderPrefixWithoutChangingStoredMessage()
    {
        MamChatWidget chat;
        chat.resize(500, 280);
        chat.setHistoryLoading(true);
        auto action = message(QStringLiteral("action"), QStringLiteral("танцует"));
        action.setEmote();
        action.setNick(QStringLiteral("Алиса <&>"));
        chat.appendMessage(action);
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        auto *body = chat.itemWidget(0)->findChild<ChatMessageText *>();
        QVERIFY(body);
        QCOMPARE(body->toPlainText(), QStringLiteral("*Алиса <&> танцует"));
        QCOMPARE(chat.messageCount(), 1);
        // Regular messages are not decorated with the emote prefix.
        chat.appendMessage(message(QStringLiteral("normal"), QStringLiteral("танцует"), 1));
        QTRY_VERIFY(chat.itemWidget(1));
        QCOMPARE(chat.itemWidget(1)->findChild<ChatMessageText *>()->toPlainText(), QStringLiteral("танцует"));
    }

    void distinctPayloadsWithReusedClientIdAreNotDiscarded()
    {
        MamChatWidget chat;
        chat.setHistoryLoading(true);
        chat.appendMessage(message("same-id", "first"));
        chat.appendMessage(message("same-id", "second", 1));
        QCOMPARE(chat.messageCount(), 2);
    }

    void resolvedCorrectionUpdatesKnownArchiveKeyWithoutMovingOriginalTime()
    {
        MamChatWidget chat;
        chat.resize(400, 250);
        chat.setHistoryLoading(true);
        auto original = message("a", "original");
        original.setHistoryId(1);
        chat.prependArchivePage({ qMakePair(QString("uid-a"), original) });
        auto corrected = original;
        corrected.setMessageId("b");
        corrected.setReplaceId("a");
        corrected.setPlainText("corrected");
        corrected.setRevisionTime(original.dateTime().addSecs(30));
        chat.prependArchivePage({ qMakePair(QString("uid-a"), corrected) });
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        QCOMPARE(chat.messageCount(), 1);
        QCOMPARE(chat.itemWidget(0)->findChild<ChatMessageText *>()->toPlainText(), QString("corrected"));
        QCOMPARE(chat.itemWidget(0)->findChild<QLabel *>("mamTimestamp")->toolTip(),
                 MamChatWidget::fullTimestamp(original.dateTime()));
    }

    void reactionsReplaceAnActorsWholeSetAndSurviveLateMessageArrival()
    {
        MamChatWidget chat;
        chat.resize(400, 250);
        chat.setHistoryLoading(true);
        MessageView reaction(MessageView::Reactions);
        reaction.setReactionsId("original");
        reaction.setReactions({ QString::fromUtf8("👍"), QString::fromUtf8("❤️") });
        chat.appendMessage(reaction);
        chat.appendMessage(message("original", "hello"));
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        QCOMPARE(chat.messageCount(), 1);
        QCOMPARE(chat.itemWidget(0)->findChildren<QPushButton *>("mamReactionChip").size(), 2);
        reaction.setReactions({ QString::fromUtf8("❤️") });
        chat.appendMessage(reaction);
        QTRY_COMPARE(chat.itemWidget(0)->findChildren<QPushButton *>("mamReactionChip").size(), 1);
        QSignalSpy outgoing(&chat, &MamChatWidget::outgoingReactions);
        chat.toggleReaction("original", QString::fromUtf8("❤️"));
        QCOMPARE(outgoing.size(), 1);
        QCOMPARE(qvariant_cast<QSet<QString>>(outgoing.first().at(1)).size(), 1);
        chat.toggleReaction("original", QString::fromUtf8("❤️"));
        QCOMPARE(qvariant_cast<QSet<QString>>(outgoing.last().at(1)).size(), 0);
        reaction.setReactions({});
        chat.appendMessage(reaction);
        QTRY_VERIFY(chat.itemWidget(0)->findChildren<QPushButton *>("mamReactionChip").isEmpty());
    }

    void floatingReactionBarUsesPsiChooserSignalAndOriginalTarget()
    {
        MamChatWidget chat;
        chat.resize(500, 250);
        chat.setHistoryLoading(true);
        auto original = message("client-id", "hello");
        original.setReactionTargetId("origin-id");
        chat.appendMessage(original);
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(chat.itemWidget(0), &enter);
        auto *bar = chat.findChild<QFrame *>("mamMessageActions");
        QTRY_VERIFY(bar->isVisible());
        QSignalSpy chooser(&chat, &MamChatWidget::emojiChooserRequested);
        auto      *choose = bar->findChild<QPushButton *>("mamChooseReaction");
        QTest::mouseClick(choose, Qt::LeftButton);
        QCOMPARE(chooser.size(), 1);
        QCOMPARE(chooser.first().first().toString(), QString("origin-id"));
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(bar, &leave);
        QTRY_VERIFY(!bar->isVisible());
    }

    void messageActionsKeepRetractionBehindMoreMenu()
    {
        MamChatWidget chat;
        chat.resize(500, 250);
        chat.setHistoryLoading(true);
        auto own = message("message-id", "mine", 0, true);
        own.setReactionTargetId("origin-id");
        own.setRetractionTargetId("message-id");
        chat.appendMessage(own);
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(chat.itemWidget(0), &enter);
        auto *bar = chat.findChild<QFrame *>("mamMessageActions");
        QTRY_VERIFY(bar->isVisible());
        // Left to right: quick slot, react, more. No direct retract button.
        auto *quick  = bar->findChild<QWidget *>("mamQuickReactions");
        auto *choose = bar->findChild<QPushButton *>("mamChooseReaction");
        auto *more   = bar->findChild<QPushButton *>("mamMoreActions");
        QVERIFY(quick && choose && more);
        QVERIFY(more->isVisible());
        QCOMPARE(quick->maximumWidth(), 0);
        QVERIFY(choose->mapTo(bar, QPoint()).x() < more->mapTo(bar, QPoint()).x());
        for (auto *button : bar->findChildren<QPushButton *>())
            QVERIFY(!button->text().contains("Retract"));

        // Expanding quick reactions grows leftwards; react/more stay put.
        const QPoint chooseAt = choose->mapTo(chat.viewport(), QPoint());
        QApplication::sendEvent(choose, &enter);
        QTRY_VERIFY(quick->maximumWidth() > 0);
        QTRY_COMPARE(quick->maximumWidth(), quick->layout()->sizeHint().width());
        QCOMPARE(choose->mapTo(chat.viewport(), QPoint()), chooseAt);
        QVERIFY(quick->mapTo(bar, QPoint()).x() < choose->mapTo(bar, QPoint()).x());

        // XEP-0424: a direct chat retracts message@id, not the origin-id.
        QSignalSpy retraction(&chat, &MamChatWidget::outgoingMessageRetraction);
        more->click();
        auto *retract = chat.findChild<QAction *>("mamRetractMessage");
        QVERIFY(retract);
        retract->trigger();
        QCOMPARE(retraction.size(), 1);
        QCOMPARE(retraction.first().first().toString(), QString("message-id"));
        QTRY_COMPARE(chat.itemWidget(0)->findChild<ChatMessageText *>()->toPlainText(), QString("Message retracted"));
        if (auto *menu = chat.findChild<QMenu *>("mamMessageMenu"))
            menu->hide();
    }

    void peerMessageActionsHaveNoMoreMenu()
    {
        MamChatWidget chat;
        chat.resize(500, 250);
        chat.setHistoryLoading(true);
        chat.appendMessage(message("peer-id", "theirs"));
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(chat.itemWidget(0), &enter);
        auto *bar = chat.findChild<QFrame *>("mamMessageActions");
        QTRY_VERIFY(bar->isVisible());
        QVERIFY(bar->findChild<QPushButton *>("mamChooseReaction")->isVisible());
        QVERIFY(!bar->findChild<QPushButton *>("mamMoreActions")->isVisible());
    }

    void roomRowLearnsArchiveStanzaIdForActions()
    {
        MamChatWidget room;
        room.setGroupChat(true);
        room.resize(500, 250);
        room.setHistoryLoading(true);
        // Live copy without a room-assigned stanza-id: nothing to react to.
        room.appendMessage(message("client-id", "hello"));
        room.show();
        QTRY_VERIFY(room.itemWidget(0));
        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(room.itemWidget(0), &enter);
        auto *bar = room.findChild<QFrame *>("mamMessageActions");
        QVERIFY(!bar->isVisible());

        // The archived copy of the same message carries the stanza-id.
        auto archived = message("client-id", "hello");
        archived.setReactionTargetId("room-stanza-id");
        archived.setRetractionTargetId("room-stanza-id");
        archived.setSpooled(true);
        QCOMPARE(room.prependArchivePage({ qMakePair(QString("room-stanza-id"), archived) }), 0);
        QCOMPARE(room.messageCount(), 1);
        QTRY_VERIFY(room.itemWidget(0));
        QApplication::sendEvent(room.itemWidget(0), &enter);
        QTRY_VERIFY(bar->isVisible());
        QSignalSpy chooser(&room, &MamChatWidget::emojiChooserRequested);
        bar->findChild<QPushButton *>("mamChooseReaction")->click();
        QCOMPARE(chooser.size(), 1);
        QCOMPARE(chooser.first().first().toString(), QString("room-stanza-id"));
    }

    void olderArchiveReactionCannotReplaceNewLiveSet()
    {
        MamChatWidget chat;
        chat.resize(400, 250);
        chat.setHistoryLoading(true);
        chat.appendMessage(message("original", "hello"));
        MessageView current(MessageView::Reactions);
        current.setReactionsId("original");
        current.setReactions({ QString::fromUtf8("❤️") });
        chat.appendMessage(current);
        auto old = current;
        old.setReactions({ QString::fromUtf8("👍") });
        old.setDateTime(current.dateTime().addSecs(-60));
        old.setSpooled(true);
        chat.appendMessage(old);
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0));
        const auto chips = chat.itemWidget(0)->findChildren<QPushButton *>("mamReactionChip");
        QCOMPARE(chips.size(), 1);
        QVERIFY(chips.first()->text().contains(QString::fromUtf8("❤️")));
    }

    void retractionRequiresTheOriginalSenderAndDoesNotCreateARow()
    {
        MamChatWidget chat;
        chat.resize(400, 250);
        chat.setHistoryLoading(true);
        chat.appendMessage(message("original", "visible"));
        chat.show();
        MessageView event(MessageView::MessageRetraction);
        event.setRetractionId("original");
        event.setLocal(true); // Our account cannot retract a peer's message.
        chat.appendMessage(event);
        QTRY_VERIFY(chat.itemWidget(0));
        QCOMPARE(chat.itemWidget(0)->findChild<ChatMessageText *>()->toPlainText(), QString("visible"));
        event.setLocal(false);
        chat.appendMessage(event);
        QTRY_COMPARE(chat.itemWidget(0)->findChild<ChatMessageText *>()->toPlainText(), QString("Message retracted"));
        QCOMPARE(chat.messageCount(), 1);
        MessageView receipt(MessageView::DeliveryReceipt);
        receipt.setMessageId("original");
        chat.appendMessage(receipt);
        QCOMPARE(chat.messageCount(), 1);
    }

    void deliveryAndDisplayedMarkersUpdateTimestampChecksAndNeverRegress()
    {
        MamChatWidget chat;
        // A large font separates x-height from cap height by several pixels,
        // so a check sized to the line box fails the alignment check below.
        QFont font = chat.font();
        font.setPixelSize(28);
        chat.setFont(font);
        chat.resize(720, 360);
        chat.setHistoryLoading(true);
        chat.appendMessage(message("out-1", "one", 1, true));
        chat.appendMessage(message("out-2", "two", 2, true));
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0) && chat.itemWidget(1));

        MessageView delivered(MessageView::DeliveryReceipt);
        delivered.setMessageId("out-1");
        chat.appendMessage(delivered);
        QTRY_VERIFY(chat.itemWidget(0) && chat.itemWidget(0)->findChild<QLabel *>("mamDeliveryIndicator"));
        QCOMPARE(chat.itemWidget(0)->findChild<QLabel *>("mamDeliveryIndicator")->property("deliveryState").toString(),
                     QString("delivered"));
        auto *single = chat.itemWidget(0)->findChild<QLabel *>("mamDeliveryIndicator");
        QVERIFY(single->text().isEmpty()); // No font-dependent Unicode ticks.
        const auto *timestamp = chat.itemWidget(0)->findChild<QLabel *>("mamTimestamp");
        const QFontMetrics timestampMetrics(timestamp->font());
        // The icon's widget occupies the complete timestamp line box so the
        // check vertex can be painted at the text baseline inside that box.
        QCOMPARE(single->height(), timestampMetrics.height());
        // Measure the painted ink: the checks stand on the timestamp baseline
        // and reach midway between the x-height and the cap height, so they
        // suit both digit ("22:43") and lowercase ("minutes ago") timestamps.
        const auto inkRows = [](QWidget *widget) {
            const QImage image = widget->grab().toImage().convertToFormat(QImage::Format_ARGB32);
            const qreal  ratio = image.devicePixelRatio();
            int          top = -1, bottom = -1;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    if (qAlpha(image.pixel(x, y)) > 64
                        && qGray(image.pixel(x, y)) < qGray(widget->palette().color(QPalette::Window).rgb()) - 40) {
                        if (top < 0)
                            top = y;
                        bottom = y;
                        break;
                    }
            return qMakePair(qRound(top / ratio), qRound(bottom / ratio));
        };
        const int baseline = timestampMetrics.ascent();
        const auto ink = inkRows(single);
        QVERIFY2(ink.first >= 0, "Delivery check painted nothing");
        QVERIFY2(qAbs(ink.second - baseline) <= 1, "Check vertex is not on the timestamp baseline");
        const QFontMetricsF exact(timestamp->font());
        QVERIFY2(qAbs(ink.first - (baseline - (exact.xHeight() + exact.capHeight()) / 2.0)) <= 1,
                 "Check top is not midway between the x-height and the cap height");
        const int singleWidth = single->width();
        QVERIFY(singleWidth > 0);
        QCOMPARE(single->palette().color(QPalette::WindowText),
                 chat.itemWidget(0)->findChild<QLabel *>("mamTimestamp")->palette().color(QPalette::WindowText));
        QVERIFY(!chat.itemWidget(1)->findChild<QLabel *>("mamDeliveryIndicator"));

        MessageView read(MessageView::DisplayedMarker);
        read.setMessageId("out-2");
        chat.appendMessage(read);
        QTRY_VERIFY(chat.itemWidget(0) && chat.itemWidget(0)->findChild<QLabel *>("mamDeliveryIndicator"));
        QCOMPARE(chat.itemWidget(0)->findChild<QLabel *>("mamDeliveryIndicator")->property("deliveryState").toString(),
                     QString("read"));
        QTRY_VERIFY(chat.itemWidget(1) && chat.itemWidget(1)->findChild<QLabel *>("mamDeliveryIndicator"));
        QCOMPARE(chat.itemWidget(1)->findChild<QLabel *>("mamDeliveryIndicator")->toolTip(),
                     QString("Read"));
        auto *doubleChecks = chat.itemWidget(1)->findChild<QLabel *>("mamDeliveryIndicator");
        QVERIFY(doubleChecks->text().isEmpty());
        QCOMPARE(doubleChecks->height(), timestampMetrics.height());
        QCOMPARE(inkRows(doubleChecks), ink); // Both states share one vertical alignment.
        // What the user sees: in the laid-out timestamp group, the checks
        // occupy the same rows as lowercase timestamp text. Psi derives the
        // timestamp font from the chat font in points, whose fractional
        // metrics shifted the checks by a pixel, so try several sizes.
        const auto alignedWithText = [&chat](const QFont &chatFont) {
            chat.setFont(chatFont);
            auto *row   = chat.itemWidget(1);
            auto *time  = row->findChild<QLabel *>("mamTimestamp");
            auto *marks = row->findChild<QLabel *>("mamDeliveryIndicator");
            auto *group = time->parentWidget();
            const auto rows = [group](const QString &text, QWidget *widget) {
                qobject_cast<QLabel *>(group->findChild<QLabel *>("mamTimestamp"))->setText(text);
                group->layout()->activate();
                const QImage image      = group->grab().toImage().convertToFormat(QImage::Format_ARGB32);
                const qreal  ratio      = image.devicePixelRatio();
                const int    background = qGray(image.pixel(0, 0)); // above lowercase text
                const QRect  rect       = widget->geometry();
                int          top = -1, bottom = -1;
                for (int y = 0; y < image.height(); ++y)
                    for (int x = qRound(rect.left() * ratio); x <= qRound(rect.right() * ratio) && x < image.width(); ++x)
                        if (qAbs(qGray(image.pixel(x, y)) - background) > 48) {
                            if (top < 0)
                                top = y;
                            bottom = y;
                            break;
                        }
                return qMakePair(top, bottom);
            };
            const auto lower  = rows(QStringLiteral("xxxx"), time);
            const auto digits = rows(QStringLiteral("0000"), time);
            const auto checks = rows(QStringLiteral("xxxx"), marks);
            // On the baseline, no taller than digits, at least as tall as lowercase.
            if (lower.first < 0 || digits.first < 0 || checks.first < 0 || checks.second != lower.second
                || checks.first < digits.first - 1 || checks.first > lower.first + 1) {
                qWarning() << chatFont << "lowercase rows" << lower << "digit rows" << digits << "check rows" << checks;
                return false;
            }
            return true;
        };
        for (const qreal points : { 8.0, 9.0, 10.0, 11.0, 12.5, 14.0 }) {
            QFont pointFont = font;
            pointFont.setPointSizeF(points);
            QVERIFY2(alignedWithText(pointFont), "Checks are not aligned with the timestamp text");
        }
        QVERIFY2(alignedWithText(font), "Checks are not aligned with the timestamp text");
        QVERIFY(doubleChecks->width() > singleWidth);
        QVERIFY(doubleChecks->width() < singleWidth * 2); // Overlap, not two separate glyphs.

        read.setMessageId("out-1"); // A stale marker may not roll back the cursor.
        chat.appendMessage(read);
        QCOMPARE(chat.itemWidget(1)->findChild<QLabel *>("mamDeliveryIndicator")->property("deliveryState").toString(),
                 QString("read"));

        // A history prepend shifts indices, not the semantic read cursor.
        auto older = message("out-0", "older", 0, true);
        older.setHistoryId(10);
        chat.prependArchivePage({ qMakePair(QString("older-archive"), older) });
        QTRY_VERIFY(chat.itemWidget(0) && chat.itemWidget(0)->findChild<QLabel *>("mamDeliveryIndicator"));
        QCOMPARE(chat.itemWidget(0)->findChild<QLabel *>("mamDeliveryIndicator")->property("deliveryState").toString(),
                     QString("read"));
    }

    void archiveReadMarkerIsAppliedWhenTargetArrivesOnAnOlderPage()
    {
        MamChatWidget chat;
        chat.resize(520, 320);
        chat.setHistoryLoading(true);
        MessageView marker(MessageView::DisplayedMarker);
        marker.setMessageId("archived-out");
        chat.appendMessage(marker); // Newest MAM page contains marker, not target.
        QCOMPARE(chat.messageCount(), 0);

        auto original = message("archived-out", "older outgoing", 0, true);
        original.setHistoryId(10);
        chat.prependArchivePage({ qMakePair(QString("older-mam-id"), original) });
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0) && chat.itemWidget(0)->findChild<QLabel *>("mamDeliveryIndicator"));
        QCOMPARE(chat.itemWidget(0)->findChild<QLabel *>("mamDeliveryIndicator")->property("deliveryState").toString(),
                 QString("read"));
        QCOMPARE(chat.messageCount(), 1);
    }

    void visibleMarkableIncomingMessagesAdvanceCursorOnlyOnce()
    {
        MamChatWidget chat;
        chat.resize(520, 320);
        chat.setHistoryLoading(true);
        auto first = message("in-1", "first", 1);
        first.setMarkable(true);
        chat.appendMessage(first);
        auto second = message("in-2", "second", 2);
        second.setMarkable(true);
        chat.appendMessage(second);
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0) && chat.itemWidget(1));
        QCOMPARE(chat.takeVisibleMarkableIncomingId(), QString("in-2"));
        QCOMPARE(chat.takeVisibleMarkableIncomingId(), QString());
        // Receiving a delivery event never affects the outgoing read cursor.
        MessageView receipt(MessageView::DeliveryReceipt);
        receipt.setMessageId("in-1");
        chat.appendMessage(receipt);
        QCOMPARE(chat.takeVisibleMarkableIncomingId(), QString());
    }

    void linkContextMenuSelectsLinkFillingWholeMessage()
    {
        // A message that is only a link: the anchor touches both ends of the
        // per-message document, where cursor movement stops succeeding.
        ChatMessageText body;
        body.resize(400, 80);
        body.setRichText(QStringLiteral("<a href=\"mailto:user@example.org\">user@example.org</a>"));
        body.show();
        QVERIFY(QTest::qWaitForWindowExposed(&body));
        const QString link = QStringLiteral("user@example.org");
        for (const int at : { 1, int(link.size()) - 1 }) {
            QTextCursor probe(body.document());
            probe.setPosition(at);
            const QPoint position = body.cursorRect(probe).center();
            QVERIFY(!body.anchorAt(position).isEmpty());
            std::unique_ptr<QMenu> menu(body.createStandardContextMenu(position));
            QVERIFY(menu);
            QCOMPARE(body.textCursor().selectedText(), link);
        }
    }

    void simsMediaUsesExistingPsiTextViewObjects()
    {
        FileSharingDeviceOpener opener;
        opener.accounts.insert(QStringLiteral("account-a"), &opener);
        FileSharingItem video(QStringLiteral("video/webm"));
        FileSharingItem audio(QStringLiteral("audio/ogg"));
        FileSharingItem image(QStringLiteral("image/png"));
        image.preview_ = QImage(60,40,QImage::Format_RGB32);
        image.preview_.fill(Qt::green);
        opener.items.insert(QStringLiteral("video"),&video);
        opener.items.insert(QStringLiteral("audio"),&audio);
        opener.items.insert(QStringLiteral("image"),&image);
        ChatMessageText body;
        body.setMediaOpener(&opener);
        body.resize(600,450);
        body.setRichText(QStringLiteral("Before <share id=\"video\" account=\"account-a\"/> between "
                                        "<share id=\"audio\" account=\"account-a\"/> "
                                        "<share id=\"image\" account=\"account-a\"/> after"));
        body.show();
        int videoObjects=0,audioObjects=0,imageObjects=0;
        for (auto block=body.document()->begin();block.isValid();block=block.next())
            for (auto it=block.begin();!it.atEnd();++it) {
                const auto format=it.fragment().charFormat();
                if (format.isImageFormat()) {
                    ++imageObjects;
                    continue;
                }
                auto *handler=body.document()->documentLayout()->handlerForObject(format.objectType());
                if (dynamic_cast<ITEVideoController*>(handler)) ++videoObjects;
                else if (dynamic_cast<ITEAudioController*>(handler)) ++audioObjects;
                else continue;
                bool sourceFound=false;
                for (const auto &property:format.properties()) {
                    if (property.userType()!=qMetaTypeId<QUrl>()) continue;
                    const auto url=property.toUrl();
                    QCOMPARE(url.scheme(),QStringLiteral("share"));
                    QCOMPARE(QUrlQuery(url).queryItemValue(QStringLiteral("account")),QStringLiteral("account-a"));
                    sourceFound=true;
                }
                QVERIFY(sourceFound);
            }
        QCOMPARE(videoObjects,1);
        QCOMPARE(audioObjects,1);
        QCOMPARE(imageObjects,1);
        QVERIFY(body.heightForWidth(600)>=220);
        QVERIFY(body.toPlainText().contains(QStringLiteral("Before")));
        QVERIFY(body.toPlainText().contains(QStringLiteral("after")));
        body.setRichText(QStringLiteral("Replaced"));
        QCoreApplication::processEvents(); // Deferred qite cursor/player cleanup.
        QCOMPARE(body.toPlainText(),QStringLiteral("Replaced"));
    }
    void virtualRowsReceiveMediaOpenerAndRecreateAttachments()
    {
        FileSharingDeviceOpener opener;
        opener.accounts.insert(QStringLiteral("account-a"), &opener);
        FileSharingItem video(QStringLiteral("video/webm"));
        opener.items.insert(QStringLiteral("video"),&video);
        MamChatWidget list;
        list.setHistoryLoading(true);
        list.setMediaOpener(&opener);
        list.resize(550,400);
        list.appendMessage(message(QStringLiteral("sims"),QStringLiteral("<share id=\"video\" account=\"account-a\"/>")));
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        auto *body=list.itemWidget(0)->findChild<ChatMessageText*>();
        QVERIFY(body->document()->toPlainText().contains(QChar::ObjectReplacementCharacter));
        QVERIFY(body->heightForWidth(500)>=220);
        NativeChatTheme theme;
        theme.layout=NativeChatTheme::Classic;
        list.setTheme(theme); // Recreate materialized row with the same attachment.
        QTRY_VERIFY(list.itemWidget(0));
        body=list.itemWidget(0)->findChild<ChatMessageText*>();
        QVERIFY(body->document()->toPlainText().contains(QChar::ObjectReplacementCharacter));
        QCOMPARE(opener.opens,0); // Merely displaying video never opens a decoder/source.
    }
    void archivePrependKeepsAtEndUntilUserScrollsUp()
    {
        MamChatWidget list;
        list.resize(400, 220);
        list.setHistoryLoading(true);
        list.show();
        QList<QPair<QString, MessageView>> page;
        for (int i = 0; i < 35; ++i) {
            auto entry = message(QString::number(i), QStringLiteral("History"), i);
            entry.setHistoryId(i + 1);
            page.append(qMakePair(QString(), entry));
        }
        QCOMPARE(list.prependArchivePage(page), 35);
        QTRY_VERIFY(list.verticalScrollBar()->maximum() > 0);
        QVERIFY(list.isAtEnd());
        list.appendMessage(message(QStringLiteral("incoming"), QStringLiteral("Live"), 36));
        QVERIFY(list.isAtEnd());
        // A real scrollbar action (unlike geometry restoration) releases the
        // opening-at-end behavior before another MAM page arrives.
        list.verticalScrollBar()->triggerAction(QAbstractSlider::SliderToMinimum);
        QVERIFY(!list.isAtEnd());
        auto older = message(QStringLiteral("old"), QStringLiteral("Older"), -10);
        older.setHistoryId(101);
        list.prependArchivePage({qMakePair(QString(), older)});
        QVERIFY(!list.isAtEnd());
    }

    void openingRoomWhileHiddenFollowsTailUntilUserScroll()
    {
        MamChatWidget room;
        room.setGroupChat(true);
        room.setHistoryLoading(true);
        room.resize(400, 210);
        // MUC presence/status events and local history can arrive BEFORE show().
        for (int i = 0; i < 42; ++i)
            room.appendMessage(message(QStringLiteral("status-%1").arg(i),
                                       QStringLiteral("Room message"), i));
        room.show();
        QTRY_VERIFY(room.verticalScrollBar()->maximum() > 0);
        QTRY_VERIFY(room.isAtEnd());
        room.resize(480, 180); // Initial splitter and font geometry settle.
        QTRY_VERIFY(room.isAtEnd());

        QList<QPair<QString, MessageView>> page;
        for (int i = 0; i < 20; ++i) {
            auto historical = message(QStringLiteral("arch-%1").arg(i),
                                      QStringLiteral("Archived"), -100 + i);
            page.append(qMakePair(QStringLiteral("mam-%1").arg(i), historical));
        }
        QCOMPARE(room.prependArchivePage(page), 20);
        QTRY_VERIFY(room.isAtEnd());

        QSignalSpy userScroll(&room, &Mattermost::LongListWidget::userScrollStarted);
        room.verticalScrollBar()->triggerAction(QAbstractSlider::SliderToMinimum);
        QTRY_VERIFY(userScroll.count() > 0);
        QVERIFY(!room.isAtEnd());
        room.appendMessage(message(QStringLiteral("live-after-scroll"),
                                   QStringLiteral("Do not steal the viewport"), 100));
        QVERIFY(!room.isAtEnd());

        room.clearMessages();
        for (int i = 0; i < 25; ++i)
            room.appendMessage(message(QStringLiteral("new-%1").arg(i),
                                       QStringLiteral("Second conversation"), i));
        QTRY_VERIFY(room.isAtEnd());
        room.scrollToIndex(0, Mattermost::LongListWidget::Alignment::Top);
        QTRY_VERIFY(!room.isAtEnd());
        room.appendMessage(message(QStringLiteral("new-tail"),
                                   QStringLiteral("Explicit navigation remains in place"), 50));
        QVERIFY(!room.isAtEnd());
    }

    void timeFontColorAndClassicNicknamePaletteAreStable()
    {
        MamChatWidget list;
        list.setGroupChat(true);
        QFont configured = list.font();
        configured.setPointSize(12);
        list.setFont(configured);
        list.resize(500, 280);
        list.setHistoryLoading(true);
        list.setNicknameColorResolver([](const MessageView &) { return QColor(60, 95, 150); });
        list.appendMessage(message(QStringLiteral("time"), QStringLiteral("Time style")));
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        auto timestamp = [&]() -> QLabel * {
            return list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
        };
        QCOMPARE(timestamp()->font().pointSizeF(), 11.0);
        QCOMPARE(timestamp()->palette().color(QPalette::Text).alpha(), 188);

        NativeChatTheme classic;
        classic.layout = NativeChatTheme::Classic;
        classic.avatarSize = 0;
        list.setTheme(classic);
        QTRY_COMPARE(timestamp()->font().pointSizeF(), 12.0);
        QCOMPARE(timestamp()->palette().color(QPalette::Text),
                 QColor(60, 95, 150));
        NativeChatTheme modern;
        modern.layout = NativeChatTheme::Modern;
        modern.avatarSize = 40;
        list.setTheme(modern);
        QTRY_COMPARE(timestamp()->font().pointSizeF(), 11.0);
        QCOMPARE(timestamp()->palette().color(QPalette::Text).alpha(), 188);
        list.setTheme(classic);
        QTRY_COMPARE(timestamp()->font().pointSizeF(), 12.0);
        list.setTheme(modern);
        QTRY_COMPARE(timestamp()->font().pointSizeF(), 11.0);
    }

    void nativeModernClassicAndPluginPlacement()
    {
        MamChatWidget list;
        list.setHistoryLoading(true);
        list.resize(620, 360);
        list.appendMessage(message(QStringLiteral("layout"), QStringLiteral("Message body")));
        list.show();
        QTRY_VERIFY(list.itemWidget(0) != nullptr);
        auto *modern = list.itemWidget(0);
        auto *modernNick = modern->findChild<QLabel *>(QStringLiteral("mamAuthor"));
        auto *modernBody = modern->findChild<ChatMessageText *>(QStringLiteral("mamBody"));
        QCOMPARE(modernNick->text(), QStringLiteral("Peer"));
        QVERIFY(modernBody->mapTo(modern, QPoint()).y() > modernNick->mapTo(modern, QPoint()).y());

        NativeChatTheme classic;
        classic.layout = NativeChatTheme::Classic;
        classic.avatarSize = 0;
        list.setTheme(classic);
        // Virtual rows stage replacement until Qt's rich-text height settles.
        QTRY_COMPARE(list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamAuthor"))->text(),
                     QStringLiteral("Peer:"));
        auto *row = list.itemWidget(0);
        auto *nick = row->findChild<QLabel *>(QStringLiteral("mamAuthor"));
        auto *time = row->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
        auto *body = row->findChild<ChatMessageText *>(QStringLiteral("mamBody"));
        QCOMPARE(nick->text(), QStringLiteral("Peer:"));
        const QString hhmmss = QDateTime::fromString(QStringLiteral("2026-10-18T10:00:00Z"),
                                                      Qt::ISODate).toLocalTime().toString(QStringLiteral("HH:mm:ss"));
        QCOMPARE(time->text(), QStringLiteral("[") + hhmmss + QStringLiteral("]"));
        QVERIFY(time->mapTo(row, QPoint()).x() < nick->mapTo(row, QPoint()).x());
        QVERIFY(nick->mapTo(row, QPoint()).x() < body->mapTo(row, QPoint()).x());

        NativeChatTheme plugin;
        plugin.layout = NativeChatTheme::Modern;
        plugin.avatarSize = 0;
        plugin.layoutSpec = QStringLiteral(R"({"direction":"vertical","items":["body",{"direction":"horizontal","items":["timestamp","author"]}]})");
        list.setTheme(plugin);
        QTRY_VERIFY(list.itemWidget(0)->findChild<ChatMessageText *>(QStringLiteral("mamBody"))
                        ->mapTo(list.itemWidget(0), QPoint()).y()
                    <= list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamAuthor"))
                        ->mapTo(list.itemWidget(0), QPoint()).y());
        row = list.itemWidget(0);
        nick = row->findChild<QLabel *>(QStringLiteral("mamAuthor"));
        time = row->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
        body = row->findChild<ChatMessageText *>(QStringLiteral("mamBody"));
        QVERIFY(body->mapTo(row, QPoint()).y() <= nick->mapTo(row, QPoint()).y());
        QVERIFY(time->mapTo(row, QPoint()).x() < nick->mapTo(row, QPoint()).x());
        list.setNicknameColorResolver([](const MessageView &) { return QColor(Qt::red); });
        QCOMPARE(nick->palette().color(QPalette::Text), QColor(Qt::red));
        list.setTheme(classic);
        QTRY_COMPARE(list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamAuthor"))
                         ->palette().color(QPalette::Text), QColor(Qt::red));
    }

    void localIdentityMergesWithoutClientStanzaId()
    {
        MamChatWidget list;
        list.setHistoryLoading(true);
        list.resize(500,500);
        auto local=message({},QStringLiteral("Original local body"));
        local.setHistoryId(42);
        list.appendMessage(local);
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        list.setMessageSelected(0,true);
        auto archived=local;
        archived.setArchiveId(QStringLiteral("server-uid"));
        list.prependArchivePage({qMakePair(archived.archiveId(),archived)});
        QCOMPARE(list.messageCount(),1);
        QCOMPARE(list.selectedMessageCount(),1);
        // A source UID aliases the original EDB row, preserving semantic
        // selection anchors instead of replacing them on archive adoption.
        QCOMPARE(list.itemWidget(0)->property("_psiMamRowKey").toString(),QStringLiteral("edb:42"));
        list.appendMessage(archived);
        QCOMPARE(list.messageCount(),1);
    }
    void localOnlyRowsStayInChronologicalPosition()
    {
        MamChatWidget list;
        list.setHistoryLoading(true);
        list.resize(500,600);
        auto local=message({},QStringLiteral("Only in local history"),20);
        local.setHistoryId(42);
        list.appendMessage(local);
        auto old=message(QStringLiteral("old"),QStringLiteral("Old server message"),10);
        auto recent=message(QStringLiteral("recent"),QStringLiteral("Recent server message"),30);
        list.prependArchivePage({qMakePair(QStringLiteral("a"),old),qMakePair(QStringLiteral("b"),recent)});
        QCOMPARE(list.messageCount(),3);
        list.show();
        QTRY_VERIFY(list.itemWidget(2));
        QCOMPARE(list.itemWidget(0)->findChild<ChatMessageText*>()->toPlainText(),QStringLiteral("Old server message"));
        QCOMPARE(list.itemWidget(1)->findChild<ChatMessageText*>()->toPlainText(),QStringLiteral("Only in local history"));
        QCOMPARE(list.itemWidget(2)->findChild<ChatMessageText*>()->toPlainText(),QStringLiteral("Recent server message"));
    }
    void equalTimestampsKeepServerPageOrder()
    {
        MamChatWidget list;
        list.setHistoryLoading(true);
        list.resize(500,600);
        auto old=message(QStringLiteral("old"),QStringLiteral("First"));
        auto recent=message(QStringLiteral("new"),QStringLiteral("Second"));
        list.prependArchivePage({qMakePair(QStringLiteral("z"),old),qMakePair(QStringLiteral("a"),recent)});
        list.show();
        QTRY_VERIFY(list.itemWidget(1));
        QCOMPARE(list.itemWidget(0)->findChild<ChatMessageText*>()->toPlainText(),QStringLiteral("First"));
        QCOMPARE(list.itemWidget(1)->findChild<ChatMessageText*>()->toPlainText(),QStringLiteral("Second"));
    }
    void overlappingEqualTimestampPagesUseArchiveOrder()
    {
        MamChatWidget list;
        list.setHistoryLoading(true);
        list.resize(500,600);
        const auto first=message(QStringLiteral("first"),QStringLiteral("First"));
        const auto second=message(QStringLiteral("second"),QStringLiteral("Second"));
        const auto third=message(QStringLiteral("third"),QStringLiteral("Third"));
        list.prependArchivePage({qMakePair(QStringLiteral("first"),first),qMakePair(QStringLiteral("second"),second)});
        list.prependArchivePage({qMakePair(QStringLiteral("second"),second),qMakePair(QStringLiteral("third"),third)});
        list.show();
        QCOMPARE(list.messageCount(),3);
        QTRY_VERIFY(list.itemWidget(2));
        QCOMPARE(list.itemWidget(0)->findChild<ChatMessageText*>()->toPlainText(),QStringLiteral("First"));
        QCOMPARE(list.itemWidget(1)->findChild<ChatMessageText*>()->toPlainText(),QStringLiteral("Second"));
        QCOMPARE(list.itemWidget(2)->findChild<ChatMessageText*>()->toPlainText(),QStringLiteral("Third"));
    }
    void chatFontUpdatesExistingAndNewDocuments()
    {
        MamChatWidget list;
        QFont small = list.font();
        small.setPointSize(9);
        list.setFont(small);
        list.resize(550, 450);
        list.setHistoryLoading(true);
        list.appendMessage(message(QStringLiteral("old"), QStringLiteral("<b>Bold</b> plain text")));
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        auto *original = list.itemWidget(0);
        auto *body = original->findChild<ChatMessageText *>();
        auto cursor = body->document()->find(QStringLiteral("Bold"));
        QCOMPARE(cursor.charFormat().fontPointSize(), 9.0);
        const auto weight = cursor.charFormat().fontWeight();
        QFont large = small;
        large.setPointSize(18);
        list.setFont(large);
        list.appendMessage(message(QStringLiteral("new"), QStringLiteral("New message")));
        QTRY_VERIFY(list.itemWidget(1));
        QCOMPARE(list.itemWidget(0), original); // Formatting changes do not replace the row/model.
        for (int i = 0; i < 2; ++i) {
            auto *row = list.itemWidget(i);
            QCOMPARE(row->findChild<QLabel *>(QStringLiteral("mamAuthor"))->font().pointSize(), 18);
            auto *text = row->findChild<ChatMessageText *>();
            for (auto block = text->document()->begin(); block.isValid(); block = block.next())
                for (auto it = block.begin(); !it.atEnd(); ++it)
                    QCOMPARE(it.fragment().charFormat().fontPointSize(), 18.0);
        }
        QCOMPARE(body->document()->find(QStringLiteral("Bold")).charFormat().fontWeight(), weight);
    }

    void groupNicknameClickUsesPlainNickname()
    {
        MamChatWidget list;
        list.setGroupChat(true);
        list.resize(550, 450);
        list.setHistoryLoading(true);
        auto view = message(QStringLiteral("nick"), QStringLiteral("Message"));
        view.setNick(QStringLiteral("Alice <&> /"));
        list.appendMessage(view);
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        auto *author = list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamAuthor"));
        QSignalSpy clicked(&list, &MamChatWidget::nicknameClicked);
        QTest::mouseClick(author, Qt::LeftButton);
        QCOMPARE(clicked.count(), 1);
        QCOMPARE(clicked.first().first().toString(), view.nick());
        QTest::mouseClick(author, Qt::LeftButton, Qt::ControlModifier);
        QCOMPARE(list.selectedMessageCount(), 1);
        QCOMPARE(clicked.count(), 1);
        list.cancelMessageSelection();
        list.setGroupChat(false);
        QTest::mouseClick(author, Qt::LeftButton);
        QCOMPARE(clicked.count(), 1);
    }

    void paletteUpdatesExistingDocumentLinks()
    {
        ChatMessageText body;
        QPalette light = body.palette();
        light.setColor(QPalette::Link, QColor(0, 50, 120));
        body.setPalette(light);
        body.setRichText(QStringLiteral("<a href=\"https://example.org\" style=\"color:#000080\">Link</a>"));
        auto link = body.document()->find(QStringLiteral("Link"));
        QCOMPARE(link.charFormat().foreground().color(), light.color(QPalette::Link));
        QPalette dark = light;
        dark.setColor(QPalette::Link, QColor(128, 184, 255));
        body.setPalette(dark);
        link = body.document()->find(QStringLiteral("Link"));
        QCOMPARE(link.charFormat().foreground().color(), dark.color(QPalette::Link));
        QCOMPARE(link.charFormat().anchorHref(), QStringLiteral("https://example.org"));
    }

    void desktopPaletteStorageAndPreview()
    {
        QPalette light;
        light.setColor(QPalette::Base, Qt::white);
        light.setColor(QPalette::Text, Qt::black);
        const auto dark = DesktopColorPalette::preview(QStringLiteral("dark"), light);
        QCOMPARE(DesktopColorPalette::name(light), QStringLiteral("light"));
        QCOMPARE(DesktopColorPalette::name(dark), QStringLiteral("dark"));
        const QString logical = QStringLiteral("options.ui.look.colors.chat.background");
        QCOMPARE(DesktopColorPalette::optionPath(logical, QStringLiteral("dark")),
                 QStringLiteral("options.ui.look.palettes.dark.chat.background"));
        QVERIFY(DesktopColorPalette::optionPath(logical, QStringLiteral("light"))
            != DesktopColorPalette::optionPath(logical, QStringLiteral("dark")));
        QCOMPARE(DesktopColorPalette::preview(QStringLiteral("dark"), dark), dark);
        QCOMPARE(DesktopColorPalette::preview(QStringLiteral("light"), dark).color(QPalette::Base), QColor(Qt::white));
        MamChatWidget list;
        list.resize(550, 450);
        list.setPalette(light);
        list.setHistoryLoading(true);
        list.appendMessage(message(QStringLiteral("one"), QStringLiteral("Message")));
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        auto *row = list.itemWidget(0);
        list.setPalette(dark);
        QCOMPARE(list.itemWidget(0), row);
        QCOMPARE(row->palette().color(QPalette::Base), dark.color(QPalette::Base));
        QCOMPARE(row->findChild<ChatMessageText *>()->palette().color(QPalette::Text), dark.color(QPalette::Text));
        list.setPalette(light);
        QCOMPARE(row->palette().color(QPalette::Base), QColor(Qt::white));
    }

    void applicationPaletteUpdatesExistingRows_data()
    {
        QTest::addColumn<bool>("classic");
        QTest::addColumn<bool>("groupChat");
        QTest::newRow("modern-chat") << false << false;
        QTest::newRow("classic-chat") << true << false;
        QTest::newRow("modern-muc") << false << true;
        QTest::newRow("classic-muc") << true << true;
    }

    void applicationPaletteUpdatesExistingRows()
    {
        QFETCH(bool, classic);
        QFETCH(bool, groupChat);
        const QPalette original = QApplication::palette();
        const auto     restore  = qScopeGuard([&] { QApplication::setPalette(original); });
        QPalette       light    = DesktopColorPalette::preview(QStringLiteral("light"),
                                                               DesktopColorPalette::preview(QStringLiteral("dark"), original));
        const QPalette dark     = DesktopColorPalette::preview(QStringLiteral("dark"), light);
        QApplication::setPalette(light);
        QWidget window;
        // Chat windows have a stylesheet, even when it only styles correction
        // mode in the composer. Qt then changes palette propagation for all
        // of the window's children, including unrelated message widgets.
        window.setStyleSheet(QStringLiteral("QTextEdit[correction=\"true\"] { background-color: rgb(160,160,0); }"));
        window.resize(550, 450);
        MamChatWidget list(&window);
        list.setGroupChat(groupChat);
        const QColor nickColor(0, 170, 170);
        if (groupChat)
            list.setNicknameColorResolver([nickColor](const MessageView &) { return nickColor; });
        if (classic)
            list.setTheme({ "classic", "Classic", NativeChatTheme::Classic, 0, 4, {} });
        list.resize(550, 450);
        list.setHistoryLoading(true);
        auto apply = [&] {
            QPalette palette = QApplication::palette();
            palette.setColor(QPalette::Base, palette.color(QPalette::Base));
            palette.setColor(QPalette::Window, palette.color(QPalette::Base));
            palette.setColor(QPalette::Text, palette.color(QPalette::Text));
            const QSignalBlocker blocker(&list);
            list.setPalette(palette);
            list.viewport()->setPalette(list.palette());
            list.setInformationalColor(palette.color(QPalette::Base).lightness() < 128 ? QColor("#8bd58b")
                                                                                       : QColor("#008000"));
        };
        connect(&list, &MamChatWidget::paletteChanged, &list, apply);
        apply();
        list.appendMessage(message("existing",
                                   "<b>Existing text</b> <a href='https://example.org'>link</a> "
                                   "<span style='color:#ff0000'>red</span>"));
        MessageView subject(MessageView::Subject);
        subject.setPlainText("Room subject");
        list.appendMessage(subject);
        window.show();
        list.show();
        QTRY_VERIFY(list.itemWidget(0) && list.itemWidget(1));
        auto *row      = list.itemWidget(0);
        auto *body     = row->findChild<ChatMessageText *>();
        auto *document = body->document();
        body->selectAll();
        for (const auto &palette : { dark, light }) {
            QApplication::setPalette(palette);
            apply();
            window.setStyleSheet(window.styleSheet()); // KDE/Psi repolishes after the palette update.
            QCoreApplication::processEvents();
            QCOMPARE(list.itemWidget(0), row);
            QCOMPARE(row->palette().color(QPalette::Base), palette.color(QPalette::Base));
            QCOMPARE(body->palette().color(QPalette::Text), palette.color(QPalette::Text));
            QCOMPARE(body->viewport()->palette().color(QPalette::Base), palette.color(QPalette::Base));
            QCOMPARE(body->viewport()->palette().color(QPalette::Text), palette.color(QPalette::Text));
            QCOMPARE(row->findChild<QLabel *>("mamAuthor")->palette().color(QPalette::Text),
                     groupChat ? nickColor : palette.color(QPalette::Text));
            QColor timeColor = classic && groupChat ? nickColor : palette.color(QPalette::Text);
            if (!classic)
                timeColor.setAlpha(188);
            QCOMPARE(row->findChild<QLabel *>("mamTimestamp")->palette().color(QPalette::Text), timeColor);
            auto *subjectBody = list.itemWidget(1)->findChild<ChatMessageText *>();
            QCOMPARE(subjectBody->palette().color(QPalette::Base), palette.color(QPalette::Base));
            QCOMPARE(subjectBody->palette().color(QPalette::Text),
                     palette.color(QPalette::Base).lightness() < 128 ? QColor("#8bd58b") : QColor("#008000"));
            QCOMPARE(body->document(), document);
            QCOMPARE(body->textCursor().selectedText(), QString("Existing text link red"));
            QCOMPARE(document->find("link").charFormat().foreground().color(), palette.color(QPalette::Link));
            QCOMPARE(document->find("red").charFormat().foreground().color(), QColor(Qt::red));
            QCOMPARE(document->find("Existing").charFormat().fontWeight(), int(QFont::Bold));
        }
    }

    void wholeMessageSelectionColorsAndText_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }
    void wholeMessageSelectionColorsAndText()
    {
        QFETCH(bool, dark);
        MamChatWidget list;
        QPalette palette = list.palette();
        const QColor base = dark ? QColor(25, 25, 25) : QColor(Qt::white);
        palette.setColor(QPalette::Base, base);
        palette.setColor(QPalette::Window, base);
        palette.setColor(QPalette::Text, dark ? Qt::white : Qt::black);
        palette.setColor(QPalette::Highlight, QColor(0, 120, 230));
        list.setPalette(palette);
        list.resize(550, 450);
        list.setHistoryLoading(true);
        for (int i = 0; i < 3; ++i)
            list.appendMessage(message(QString::number(i), QStringLiteral("Selectable message text"), i));
        list.show();
        QTRY_VERIFY(list.itemWidget(0) && list.itemWidget(1) && list.itemWidget(2));
        auto *first = list.itemWidget(0)->findChild<ChatMessageText *>();
        auto *other = list.itemWidget(2)->findChild<ChatMessageText *>();
        first->selectAll();
        other->selectAll();
        QVERIFY(first->textCursor().hasSelection() && other->textCursor().hasSelection());
        list.selectMessageRange(0, 1);
        QVERIFY(!first->textCursor().hasSelection());
        QVERIFY(!other->textCursor().hasSelection());
        QCOMPARE(first->textInteractionFlags(), Qt::TextInteractionFlags(Qt::NoTextInteraction));
        const QColor selected = list.itemWidget(0)->palette().color(QPalette::Base);
        QVERIFY(selected != base);
        auto bodyPixel = [&list](int index) {
            auto *body = list.itemWidget(index)->findChild<ChatMessageText *>();
            const auto point = body->viewport()->mapTo(&list, QPoint(body->viewport()->width() - 3,
                                                                     body->viewport()->height() - 3));
            const auto image = list.grab().toImage();
            return image.pixelColor(qRound(point.x() * image.devicePixelRatio()),
                                    qRound(point.y() * image.devicePixelRatio()));
        };
        QTRY_COMPARE(bodyPixel(0), selected);
        QTRY_COMPARE(bodyPixel(1), selected);
        QTRY_COMPARE(bodyPixel(2), base);
        list.cancelMessageSelection();
        QTRY_COMPARE(bodyPixel(0), base);
        QVERIFY(first->textInteractionFlags().testFlag(Qt::TextSelectableByMouse));
        first->selectAll();
        QVERIFY(first->textCursor().hasSelection());
    }

    void draggingFromTextClearsThePartialSelection()
    {
        MamChatWidget list;
        list.resize(550, 450);
        list.setHistoryLoading(true);
        for (int i = 0; i < 3; ++i)
            list.appendMessage(message(QString::number(i), QStringLiteral("Selectable message text"), i));
        list.show();
        QTRY_VERIFY(list.itemWidget(0) && list.itemWidget(2));
        auto *body = list.itemWidget(0)->findChild<ChatMessageText *>();
        auto *target = body->viewport();
        const QPointF start(2, 7);
        QMouseEvent press(QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(target, &press);
        QMouseEvent inside(QEvent::MouseMove, QPointF(60, 7), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(target, &inside);
        QVERIFY(body->textCursor().hasSelection());
        const auto third = target->mapFromGlobal(list.itemWidget(2)->mapToGlobal(QPoint(60, 20)));
        QMouseEvent outside(QEvent::MouseMove, QPointF(third), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(target, &outside);
        QCOMPARE(list.selectedMessageCount(), 3);
        QVERIFY(!body->textCursor().hasSelection());
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(third), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(target, &release);
        QVERIFY(!body->textCursor().hasSelection());
        list.cancelMessageSelection();
        QApplication::sendEvent(target, &press);
        QApplication::sendEvent(target, &inside);
        QVERIFY(body->textCursor().hasSelection());
    }

    void informationalRowsHaveSemanticPresentation()
    {
        for (bool classic : {false, true}) {
            MamChatWidget list;
            if (classic)
                list.setTheme({QStringLiteral("classic"), QStringLiteral("Classic"), NativeChatTheme::Classic, 0, 4, {}});
            list.setInformationalColor(QColor(0, 128, 0));
            list.resize(650, 550);
            list.setHistoryLoading(true);
            const auto ordinary = message(QStringLiteral("ordinary"), QStringLiteral("Ordinary message"));
            list.appendMessage(ordinary);
            const QList<MessageView::Type> types {MessageView::System, MessageView::Subject, MessageView::Status,
                MessageView::MUCJoin, MessageView::MUCPart, MessageView::NickChange, MessageView::Urls,
                MessageView::FileTransferRequest, MessageView::FileTransferFinished};
            for (auto type : types) {
                MessageView view(type);
                view.setPlainText(type == MessageView::Subject ? QStringLiteral("Room subject") : QStringLiteral("System notice"));
                view.setDateTime(ordinary.dateTime());
                list.appendMessage(view);
            }
            list.show();
            QTRY_VERIFY(list.itemWidget(types.size()));
            auto *stamp = list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
            QVERIFY(stamp);
            QCOMPARE(stamp->toolTip(), MamChatWidget::fullTimestamp(ordinary.dateTime()));
            for (int i = 1; i <= types.size(); ++i) {
                auto *row = list.itemWidget(i);
                auto *body = row->findChild<ChatMessageText *>();
                QVERIFY(body);
                QVERIFY(!row->findChild<QLabel *>(QStringLiteral("mamTimestamp")));
                QVERIFY(!row->findChild<QLabel *>(QStringLiteral("mamAuthor")));
                QCOMPARE(body->palette().color(QPalette::Text), QColor(0, 128, 0));
                QVERIFY(body->viewport()->toolTip().isEmpty());
                QVERIFY(row->toolTip().isEmpty());
                QCOMPARE(body->document()->begin().blockFormat().alignment() & Qt::AlignHorizontal_Mask,
                         Qt::Alignment(types.at(i - 1) == MessageView::Subject ? Qt::AlignLeft : Qt::AlignHCenter));
            }
            if (!classic && !qEnvironmentVariableIsEmpty("PSI_NATIVE_CHAT_SYSTEM_SCREENSHOT"))
                list.grab().save(qEnvironmentVariable("PSI_NATIVE_CHAT_SYSTEM_SCREENSHOT"));
            list.setInformationalColor(QColor(100, 0, 140));
            QCOMPARE(list.itemWidget(1)->findChild<ChatMessageText *>()->palette().color(QPalette::Text), QColor(100, 0, 140));
        }
    }

    void messageSelectionChecks_data()
    {
        QTest::addColumn<bool>("classic");
        QTest::newRow("modern") << false;
        QTest::newRow("classic") << true;
    }
    void messageSelectionChecks()
    {
        QFETCH(bool, classic);
        MamChatWidget list;
        if (classic)
            list.setTheme({QStringLiteral("classic"), QStringLiteral("Classic"), NativeChatTheme::Classic, 0, 4, {}});
        list.resize(550, 480);
        list.setHistoryLoading(true);
        for (int i = 0; i < 3; ++i)
            list.appendMessage(message(QString::number(i), QStringLiteral("Message text"), i));
        list.show();
        QTRY_VERIFY(list.itemWidget(0) && list.itemWidget(1) && list.itemWidget(2));
        auto *first = list.itemWidget(0)->findChild<QCheckBox *>();
        auto *second = list.itemWidget(1)->findChild<QCheckBox *>();
        auto *third = list.itemWidget(2)->findChild<QCheckBox *>();
        QVERIFY(first && second && third);
        QVERIFY(first->isHidden() && second->isHidden() && third->isHidden());
        list.selectMessageRange(0, 1);
        QCOMPARE(list.selectedMessageCount(), 2);
        QVERIFY(first->isVisible() && first->isChecked());
        QVERIFY(second->isVisible() && second->isChecked());
        QVERIFY(third->isVisible() && !third->isChecked());
        QTest::mouseClick(third, Qt::LeftButton);
        QCOMPARE(list.selectedMessageCount(), 3);
        QVERIFY(third->isChecked());
        QTest::mouseClick(first, Qt::LeftButton);
        QCOMPARE(list.selectedMessageCount(), 2);
        QVERIFY(!first->isChecked());
        list.cancelMessageSelection();
        QCOMPARE(list.selectedMessageCount(), 0);
        QVERIFY(first->isHidden() && second->isHidden() && third->isHidden());
        list.selectMessageRange(0, 2);
        list.clearMessages(); // Cancel while rows still have valid model entries.
        QCOMPARE(list.selectedMessageCount(), 0);
        QCOMPARE(list.messageCount(), 0);
    }

    void selectionCheckUsesCurrentRowIdentity()
    {
        MamChatWidget list;
        list.resize(550, 480);
        list.setHistoryLoading(true);
        const auto live = message(QStringLiteral("live"), QStringLiteral("Live message"));
        list.appendMessage(live);
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        list.setMessageSelected(0, true);
        // Adopt an archive UID without replacing the visible row.
        QCOMPARE(list.prependArchivePage({qMakePair(QStringLiteral("adopted"), live)}), 0);
        QCOMPARE(list.prependArchivePage({qMakePair(QStringLiteral("older"),
            message(QStringLiteral("old"), QStringLiteral("Older message"), -10))}), 1);
        QTRY_VERIFY(list.itemWidget(1));
        auto *check = list.itemWidget(1)->findChild<QCheckBox *>();
        QVERIFY(check && check->isChecked());
        QTest::mouseClick(check, Qt::LeftButton);
        QCOMPARE(list.selectedMessageCount(), 0);
        QVERIFY(check->isHidden());
    }

    void rowsUseDocumentHeightAtActualWidth_data()
    {
        QTest::addColumn<bool>("modern");
        QTest::newRow("modern") << true;
        QTest::newRow("classic") << false;
    }
    void rowsUseDocumentHeightAtActualWidth()
    {
        QFETCH(bool, modern);
        MamChatWidget list;
        if (!modern)
            list.setTheme({QStringLiteral("classic"), QStringLiteral("Classic"), NativeChatTheme::Classic, 0, 4, {}});
        list.resize(750, 700);
        list.setHistoryLoading(true);
        list.appendMessage(message(QStringLiteral("short"), QStringLiteral("Short message")));
        list.appendMessage(message(QStringLiteral("long"), QStringLiteral("Wrapping message text with several words. ").repeated(12)));
        list.show();
        QTRY_VERIFY(list.itemWidget(0) && list.itemWidget(1));
        auto *row = list.itemWidget(0);
        auto *body = row->findChild<ChatMessageText *>();
        auto *author = row->findChild<QLabel *>(QStringLiteral("mamAuthor"));
        QVERIFY(body && author);
        const int authorBottom = author->mapTo(row, QPoint(0, author->height())).y();
        const int bodyTop = body->mapTo(row, QPoint()).y();
        if (modern)
            QVERIFY2(bodyTop - authorBottom <= 6, "Header/body spacing must not absorb excess row height");
        else
            QVERIFY(qAbs(bodyTop - author->mapTo(row, QPoint()).y()) <= 1);
        // Rich QTextDocument height-for-width settles after materialization.
        // Also count the avatar: on Qt6 a 40px avatar can be taller than the
        // combined author/body text, and determines the minimum row height.
        auto *avatar = row->findChild<QLabel *>(QStringLiteral("mamAvatar"));
        const int avatarHeight = avatar ? avatar->height() : 0;
        QTRY_VERIFY2(row->height() <= qMax(avatarHeight,
                                           author->height() + body->heightForWidth(body->width()) + 3)
                                            + 18,
                     qPrintable(QStringLiteral("row=%1 avatar=%2 author=%3 body=%4 width=%5")
                                    .arg(row->height()).arg(avatarHeight).arg(author->height())
                                    .arg(body->heightForWidth(body->width())).arg(body->width())));
        auto *wrapped = list.itemWidget(1)->findChild<ChatMessageText *>();
        QVERIFY(wrapped);
        QTRY_VERIFY(qAbs(wrapped->height() - wrapped->heightForWidth(wrapped->width())) <= 1);
        const int wideHeight = list.itemWidget(1)->height();
        list.resize(340, 700);
        QTRY_VERIFY(list.itemWidget(1)->height() > wideHeight);
        list.resize(750, 700);
        QTRY_COMPARE(list.itemWidget(1)->height(), wideHeight);
        if (modern && !qEnvironmentVariableIsEmpty("PSI_NATIVE_CHAT_GEOMETRY_SCREENSHOT"))
            list.grab().save(qEnvironmentVariable("PSI_NATIVE_CHAT_GEOMETRY_SCREENSHOT"));
    }

    void psiIconsRenderAndCopyAsEmoticons()
    {
        MamChatWidget list;
        list.resize(500, 220);
        list.setHistoryLoading(true);
        const QString rich = QStringLiteral("Hello <icon name=\"test/smile\" text=\":)\"> world");
        list.appendMessage(message(QStringLiteral("icon"), rich));
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        auto *body = list.itemWidget(0)->findChild<ChatMessageText *>();
        QVERIFY(body);
        QVERIFY(body->document()->documentLayout()->handlerForObject(QTextFormat::UserObject));
        QVERIFY(body->toPlainText().contains(QChar::ObjectReplacementCharacter));
        const auto pixels = body->viewport()->grab().toImage();
        int iconPixels = 0;
        for (int y = 0; y < pixels.height(); ++y)
            for (int x = 0; x < pixels.width(); ++x)
                if (pixels.pixelColor(x, y) == QColor(245, 10, 220))
                    ++iconPixels;
        QVERIFY2(iconPixels >= 100, "PsiRichText's actual object handler must paint the icon asset");
        body->selectAll();
        body->copy();
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("Hello :) world"));
        QTest::mouseClick(body->viewport(), Qt::LeftButton, Qt::ControlModifier, QPoint(2, 2));
        QCOMPARE(list.selectedMessageCount(), 1);
        QVERIFY(!body->textCursor().hasSelection());
        list.copySelectedMessages();
        QVERIFY(QApplication::clipboard()->text().contains(QStringLiteral("Hello :) world")));
    }

    void wheelOverTextScrollsTimeline()
    {
        MamChatWidget list;
        list.resize(500, 220);
        list.setHistoryLoading(true);
        for (int i = 0; i < 30; ++i)
            list.appendMessage(message(QString::number(i), QStringLiteral("Message text"), i));
        list.show();
        list.scrollToIndex(0, MamChatWidget::Alignment::Top);
        QTRY_VERIFY(list.verticalScrollBar()->maximum() > 0);
        QTRY_COMPARE(list.verticalScrollBar()->value(), 0);
        QTRY_VERIFY(list.itemWidget(0));
        auto *body = list.itemWidget(0)->findChild<ChatMessageText *>();
        QVERIFY(body);
        QWheelEvent wheel(QPointF(4, 4), QPointF(body->viewport()->mapToGlobal(QPoint(4, 4))),
                          QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(body->viewport(), &wheel);
        QTRY_VERIFY(list.verticalScrollBar()->value() > 0);
        QCOMPARE(body->verticalScrollBar()->value(), 0);
    }

    void semanticTimestampBoundaries()
    {
        const auto now = QDateTime::fromString(QStringLiteral("2026-10-18T12:34:56+03:00"), Qt::ISODate);
        QCOMPARE(MamChatWidget::displayTime({}, now), QString());
        QCOMPARE(MamChatWidget::displayTime(now, now), QStringLiteral("just now"));
        QCOMPARE(MamChatWidget::displayTime(now.addSecs(-59), now), QStringLiteral("just now"));
        QVERIFY(MamChatWidget::displayTime(now.addSecs(-60), now).contains(QStringLiteral("1")));
        QVERIFY(MamChatWidget::displayTime(now.addSecs(-3600), now).contains(QStringLiteral("1")));
        QVERIFY(MamChatWidget::displayTime(now.addSecs(-86399), now).contains(QStringLiteral("23")));
        QCOMPARE(MamChatWidget::displayTime(now.addSecs(-86400), now),
                 now.addSecs(-86400).toLocalTime().toString(QStringLiteral("HH:mm")));
        QCOMPARE(MamChatWidget::displayTime(now.addSecs(60), now),
                 now.addSecs(60).toLocalTime().toString(QStringLiteral("HH:mm")));
        QVERIFY(MamChatWidget::fullTimestamp(now).contains(now.toLocalTime().toString(QStringLiteral("HH:mm:ss"))));
    }

    void timestampUpdatesOnlyWhileVisibleAndOnlyAtBoundaries()
    {
        MamChatWidget list;
        auto *clock = list.findChild<QTimer *>(QStringLiteral("mamTimestampRefresh"));
        QVERIFY(clock);
        QVERIFY(clock->isSingleShot());
        QVERIFY(!clock->isActive());
        list.resize(460, 180);
        auto recent = message(QStringLiteral("recent"), QStringLiteral("Now"));
        recent.setDateTime(QDateTime::currentDateTime());
        list.appendMessage(recent);
        QVERIFY(!clock->isActive()); // Creation while hidden does no polling.
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        QTRY_VERIFY(clock->isActive());
        QVERIFY(clock->remainingTime() > 1000); // No more per-second wakeups.
        auto *row = list.itemWidget(0);
        auto *time = row->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
        auto *body = row->findChild<ChatMessageText *>();
        QVERIFY(time && body);
        QCOMPARE(time->text(), QStringLiteral("just now"));
        QVERIFY(!time->toolTip().isEmpty());
        QVERIFY(row->toolTip().isEmpty());
        QVERIFY(body->toolTip().isEmpty());
        QVERIFY(body->viewport()->toolTip().isEmpty());
        list.hide();
        QVERIFY(!clock->isActive());
        list.show();
        QTRY_VERIFY(clock->isActive());
        NativeChatTheme classic;
        classic.layout = NativeChatTheme::Classic;
        classic.avatarSize = 0;
        list.setTheme(classic);
        QVERIFY(!clock->isActive());
    }

    void avatarClickRequestsContactCard()
    {
        MamChatWidget list;
        list.resize(480, 200);
        list.setGroupChat(true);
        list.setHistoryLoading(true);
        QSignalSpy requested(&list, &MamChatWidget::avatarClicked);
        auto sender = message(QStringLiteral("avatar"), QStringLiteral("Hello"));
        sender.setNick(QStringLiteral("Alice"));
        list.appendMessage(sender);
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        auto *avatar = list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamAvatar"));
        QVERIFY(avatar);
        QTest::mouseClick(avatar, Qt::LeftButton);
        QCOMPARE(requested.count(), 1);
        QCOMPARE(requested.at(0).at(0).toString(), QStringLiteral("Alice"));
        QCOMPARE(requested.at(0).at(1).toBool(), false);
        QCOMPARE(list.selectedMessageCount(), 0);
    }

    void layoutsAvatarsAndThemeSwitch()
    {
        MamChatWidget list;
        QPalette light;
        light.setColor(QPalette::Base, Qt::white);
        light.setColor(QPalette::Text, QColor(24, 24, 24));
        list.setPalette(light);
        list.resize(640, 300);
        list.setHistoryLoading(true);
        QPixmap avatar(64, 64);
        avatar.fill(Qt::blue);
        list.setAvatarResolver([avatar](const MessageView &) { return avatar; });
        auto view = message(QStringLiteral("one"), QStringLiteral("A readable message with an avatar."));
        view.setNick(QStringLiteral("Alice <admin>"));
        view.setDateTime(QDateTime::currentDateTime().addSecs(-61));
        list.appendMessage(view);
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        auto *row = list.itemWidget(0);
        auto *author = row->findChild<QLabel *>(QStringLiteral("mamAuthor"));
        QVERIFY(author);
        QCOMPARE(author->textFormat(), Qt::PlainText);
        QCOMPARE(author->text(), view.nick());
        QVERIFY(author->font().bold());
        QCOMPARE(author->palette().color(author->foregroundRole()), light.color(QPalette::Text));
        auto *time = row->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
        QCOMPARE(time->toolTip(), MamChatWidget::fullTimestamp(view.dateTime()));
        QVERIFY(row->findChild<QLabel *>(QStringLiteral("mamAvatar")));
        if (!qEnvironmentVariableIsEmpty("PSI_NATIVE_CHAT_SCREENSHOT"))
            list.grab().save(qEnvironmentVariable("PSI_NATIVE_CHAT_SCREENSHOT"));
        list.setMessageSelected(0, true);
        list.setTheme({QStringLiteral("classic"), QStringLiteral("Classic"), NativeChatTheme::Classic, 0, 4, {}});
        QTRY_VERIFY(list.itemWidget(0));
        QTRY_VERIFY(!list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamAvatar")));
        QCOMPARE(list.selectedMessageCount(), 1);
        QCOMPARE(list.messageCount(), 1);
        list.setTheme({QStringLiteral("modern"), QStringLiteral("Modern"), NativeChatTheme::Modern, 40, 10, {}});
        QTRY_VERIFY(list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamAvatar")));
    }

    void clockRefreshKeepsMaterializedRow()
    {
        MamChatWidget list;
        list.resize(500, 220);
        list.setHistoryLoading(true);
        auto view = message(QStringLiteral("clock"), QStringLiteral("Time refresh"));
        // Near the first minute boundary: old 1-second polling is unnecessary,
        // but a single-shot wakeup must still update the existing row.
        view.setDateTime(QDateTime::currentDateTime().addMSecs(-58000));
        list.appendMessage(view);
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        QWidget *row = list.itemWidget(0);
        auto *time = row->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
        QVERIFY(time);
        QCOMPARE(time->text(), QStringLiteral("just now"));
        QTRY_COMPARE_WITH_TIMEOUT(time->text(), QStringLiteral("1 minute ago"), 6000);
        QCOMPARE(list.itemWidget(0), row);
    }

    void roomOverlapRequiresSameSender()
    {
        MamChatWidget list;
        list.setGroupChat(true);
        auto alice = message(QStringLiteral("shared-id"), QStringLiteral("hello"));
        alice.setNick(QStringLiteral("Alice"));
        auto bob = alice;
        bob.setNick(QStringLiteral("Bob"));
        list.appendMessage(alice);
        QCOMPARE(list.prependArchivePage({qMakePair(QStringLiteral("bob-uid"), bob)}), 1);
        QCOMPARE(list.messageCount(), 2);
        QCOMPARE(list.prependArchivePage({qMakePair(QStringLiteral("alice-uid"), alice)}), 0);
        QCOMPARE(list.messageCount(), 2);
    }

    void encryptionLockShowsHowTheMessageTravelled()
    {
        MamChatWidget chat;
        chat.resize(560, 320);
        chat.setHistoryLoading(true);

        // A received encrypted message knows its state immediately.
        auto incoming = message("in-1", "secret");
        incoming.setEncrypted(true, "OMEMO");
        chat.appendMessage(incoming);
        // A freshly sent message: unknown until its encryption job finishes.
        chat.appendMessage(message("out-1", "pending", 1, true));
        // The plaintext outcome can arrive before the local row exists.
        MessageView plain(MessageView::MessageSecurity);
        plain.setMessageId("out-2");
        plain.setEncrypted(false);
        chat.appendMessage(plain);
        QCOMPARE(chat.messageCount(), 2); // A security event never creates a row.
        auto sent = message("out-2", "plain", 2, true);
        sent.setEdited();
        chat.appendMessage(sent);
        chat.show();
        QTRY_VERIFY(chat.itemWidget(0) && chat.itemWidget(1) && chat.itemWidget(2));

        auto *closed = chat.itemWidget(0)->findChild<QLabel *>("mamEncryptionIndicator");
        QVERIFY(closed);
        QVERIFY(closed->property("encrypted").toBool());
        QCOMPARE(closed->toolTip(), QString("Encrypted with OMEMO"));
        QVERIFY(!chat.itemWidget(1)->findChild<QLabel *>("mamEncryptionIndicator"));

        MessageView secured(MessageView::MessageSecurity);
        secured.setMessageId("out-1");
        secured.setEncrypted(true, "OMEMO");
        chat.appendMessage(secured);
        QTRY_VERIFY(chat.itemWidget(1) && chat.itemWidget(1)->findChild<QLabel *>("mamEncryptionIndicator"));
        QVERIFY(chat.itemWidget(1)->findChild<QLabel *>("mamEncryptionIndicator")->property("encrypted").toBool());

        auto *open = chat.itemWidget(2)->findChild<QLabel *>("mamEncryptionIndicator");
        QVERIFY(open);
        QVERIFY(!open->property("encrypted").toBool());
        QCOMPARE(open->toolTip(), QString("Not encrypted"));

        // Order: time, (checks), lock, pencil; the lock matches the pencil's
        // size and vertical position.
        MessageView delivered(MessageView::DeliveryReceipt);
        delivered.setMessageId("out-2");
        chat.appendMessage(delivered);
        QTRY_VERIFY(chat.itemWidget(2) && chat.itemWidget(2)->findChild<QLabel *>("mamDeliveryIndicator"));
        auto *row    = chat.itemWidget(2);
        auto *checks = row->findChild<QLabel *>("mamDeliveryIndicator");
        auto *lock   = row->findChild<QLabel *>("mamEncryptionIndicator");
        auto *pencil = row->findChild<QLabel *>("mamEditedIndicator");
        QVERIFY(checks && lock && pencil);
        QVERIFY(checks->geometry().right() < lock->geometry().left());
        QVERIFY(lock->geometry().right() < pencil->geometry().left());
        QCOMPARE(lock->size(), pencil->size());
        QCOMPARE(lock->geometry().top(), pencil->geometry().top());
        QCOMPARE(lock->pixmap(Qt::ReturnByValue).size(), pencil->pixmap(Qt::ReturnByValue).size());
    }

    void nativeRowsAndCorrections()
    {
        MamChatWidget list;
        list.resize(400, 250);
        list.setHistoryLoading(true);
        list.show();
        auto first = message(QStringLiteral("m1"), QStringLiteral("first body"));
        list.appendMessage(first);
        QTRY_VERIFY(list.itemWidget(0) != nullptr);
        QWidget *row = list.itemWidget(0);
        auto labels = row->findChildren<QLabel *>();
        QCOMPARE(labels.size(), 3);
        QCOMPARE(row->findChild<ChatMessageText *>()->toPlainText(), QStringLiteral("first body"));

        auto changed = message(QStringLiteral("m2"), QStringLiteral("edited body"), 2);
        changed.setReplaceId(QStringLiteral("m1"));
        list.appendMessage(changed);
        QCOMPARE(list.messageCount(), 1);
        QTRY_VERIFY(list.itemWidget(0) != nullptr);
        QTRY_COMPARE(list.itemWidget(0)->findChild<ChatMessageText *>()->toPlainText(),
                     QStringLiteral("edited body"));
        auto *pencil = list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamEditedIndicator"));
        QVERIFY(pencil);
        QCOMPARE(pencil->toolTip(), QStringLiteral("The message was corrected"));
        // The standalone iconset stub returns magenta for Psi assets.
        // No custom resolver is required by the production widget.
        const QPixmap actual = pencil->pixmap(Qt::ReturnByValue);
        QVERIFY(!actual.isNull());
        QCOMPARE(actual.toImage().pixelColor(0, 0), QColor(245, 10, 220));
        auto *timestamp = list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
        QVERIFY(timestamp);
        QVERIFY(pencil->mapTo(list.itemWidget(0), QPoint()).x()
                > timestamp->mapTo(list.itemWidget(0), QPoint()).x() + timestamp->width());
        // The pencil is drawn at the timestamp font size (unscaled when
        // within a pixel of the 16 px icon) and centred on the check band:
        // the baseline up to midway between the x-height and cap height.
        // Psi's timestamp font is the chat font minus 1pt (or 1px for pixel
        // fonts). Point-to-pixel conversion depends on the environment, so
        // the near-native cases use pixel fonts.
        const auto pencilPlacement = [&list](const QFont &chatFont, int expectedSize) {
            list.setFont(chatFont);
            const auto timestampFontApplied = [&list, chatFont] {
                auto *row = list.itemWidget(0);
                auto *t   = row ? row->findChild<QLabel *>(QStringLiteral("mamTimestamp")) : nullptr;
                if (!t)
                    return false;
                return chatFont.pixelSize() > 0 ? t->font().pixelSize() == chatFont.pixelSize() - 1
                                                : qAbs(t->font().pointSizeF() - (chatFont.pointSizeF() - 1.0)) < 0.01;
            };
            if (!QTest::qWaitFor(timestampFontApplied) || !timestampFontApplied())
                return false;
            auto *row  = list.itemWidget(0);
            auto *time = row->findChild<QLabel *>(QStringLiteral("mamTimestamp"));
            auto *mark = row->findChild<QLabel *>(QStringLiteral("mamEditedIndicator"));
            if (!mark)
                return false;
            const int size = expectedSize > 0 ? expectedSize : QFontInfo(time->font()).pixelSize();
            if (qAbs(size - 16) <= 1 && expectedSize <= 0)
                return true; // Covered by the explicit unscaled case.
            time->setText(QStringLiteral("xxxx"));
            auto *group = time->parentWidget();
            // Let the recreated row lay out top-down for the new font before
            // measuring; activating only the group keeps its stale size.
            QTest::qWait(50);
            if (row->layout())
                row->layout()->activate();
            QCoreApplication::processEvents();
            const QImage image = group->grab().toImage().convertToFormat(QImage::Format_ARGB32);
            const qreal  ratio = image.devicePixelRatio();
            const QRect  rect  = mark->geometry();
            int          top = -1, bottom = -1;
            for (int y = 0; y < image.height(); ++y)
                for (int x = qRound(rect.left() * ratio); x <= qRound(rect.right() * ratio) && x < image.width(); ++x) {
                    const QColor c = image.pixelColor(x, y);
                    if (c.red() > 180 && c.green() < 90 && c.blue() > 160) { // stub icon is magenta
                        if (top < 0)
                            top = y;
                        bottom = y;
                        break;
                    }
                }
            const QFontMetricsF metrics(time->font());
            const qreal baseline = time->mapTo(group, QPoint()).y() * ratio
                + ((time->contentsRect().height() - (metrics.ascent() + metrics.descent())) / 2.0 + metrics.ascent()
                   + time->contentsRect().top())
                    * ratio;
            const qreal bandCentre = baseline - (metrics.xHeight() + metrics.capHeight()) / 4.0 * ratio;
            const int   height     = bottom - top + 1;
            const qreal centre     = (top + bottom + 1) / 2.0;
            if (top < 0 || qAbs(height - qRound(size * ratio)) > 1 || qAbs(centre - bandCentre) > 1.0) {
                qWarning() << chatFont << "pencil rows" << top << bottom << "expected size" << size << "centre"
                           << centre << "band centre" << bandCentre;
                return false;
            }
            return true;
        };
        const auto pixels = [&list](int px) {
            QFont f = list.font();
            f.setPixelSize(px);
            return f;
        };
        const auto points = [&list](qreal pt) {
            QFont f = list.font();
            f.setPointSizeF(pt);
            return f;
        };
        // Timestamp fonts of 16 and 17 px keep the icon's own 16 px.
        QVERIFY2(pencilPlacement(pixels(17), 16), "Edit pencil near its native size was resized");
        QVERIFY2(pencilPlacement(pixels(18), 16), "Edit pencil near its native size was resized");
        QVERIFY2(pencilPlacement(pixels(23), 22), "Edit pencil is not font-sized and centred on the checks");
        QVERIFY2(pencilPlacement(points(20.0), 0), "Edit pencil is not font-sized and centred on the checks");
        NativeChatTheme classic;
        classic.layout = NativeChatTheme::Classic;
        classic.avatarSize = 0;
        list.setTheme(classic);
        QTRY_VERIFY(list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamEditedIndicator")));
    }

    void sendingNewMessageScrollsToLiveTailButEditingDoesNot()
    {
        MamChatWidget chat;
        chat.resize(400,220);
        chat.setHistoryLoading(true);
        for (int i=0;i<44;++i)
            chat.appendMessage(message(QString::number(i),QStringLiteral("Earlier message"),i));
        chat.show();
        QTRY_VERIFY(chat.verticalScrollBar()->maximum()>0);
        chat.verticalScrollBar()->triggerAction(QAbstractSlider::SliderToMinimum);
        QTRY_VERIFY(!chat.isAtEnd());
        auto outgoing=message(QStringLiteral("my-new"),QStringLiteral("Sent"),100,true);
        chat.appendMessage(outgoing);
        QTRY_VERIFY(chat.isAtEnd());
        QCOMPARE(chat.messageCount(),45);

        chat.scrollToIndex(0,Mattermost::LongListWidget::Alignment::Top);
        QTRY_VERIFY(!chat.isAtEnd());
        auto corrected=message(QStringLiteral("my-edit"),QStringLiteral("Edited"),101,true);
        corrected.setReplaceId(outgoing.messageId());
        chat.appendMessage(corrected);
        QVERIFY(!chat.isAtEnd());
        QCOMPARE(chat.messageCount(),45);
    }

    void archivedCorrectionChainMergesInBothLoadOrders()
    {
        auto scenario = [this](bool archiveFirst) {
            MamChatWidget list;
            list.resize(510, 280);
            list.setHistoryLoading(true);
            list.show();
            auto original = message(QStringLiteral("a476b502-original"), QStringLiteral("6"), 1);
            auto edit7 = message(QStringLiteral("a476b502-edit7"), QStringLiteral("7"), 2);
            edit7.setReplaceId(original.messageId());
            auto edit8 = message(QStringLiteral("3f59b103-edit8"), QStringLiteral("8"), 3);
            edit8.setReplaceId(edit7.messageId());
            edit8.setNick(QStringLiteral("rion"));
            auto archived8 = edit8;
            archived8.setNick(QStringLiteral("rion@jabber.ru")); // A different display name is not a different peer.
            auto archived7 = edit7;
            archived7.setNick(QStringLiteral("rion@jabber.ru"));
            auto archived6 = original;
            archived6.setNick(QStringLiteral("rion@jabber.ru"));
            QList<QPair<QString, MessageView>> page {
                {QStringLiteral("mam-6"), archived6},
                {QStringLiteral("mam-7"), archived7},
                {QStringLiteral("mam-8"), archived8}
            };

            if (archiveFirst) {
                QCOMPARE(list.prependArchivePage(page), 1);
                list.appendMessage(edit8);
            } else {
                list.appendMessage(edit8);
                QCOMPARE(list.prependArchivePage(page), 0);
            }
            QCOMPARE(list.messageCount(), 1);
            QTRY_VERIFY(list.itemWidget(0));
            QCOMPARE(list.itemWidget(0)->findChild<ChatMessageText *>()->toPlainText(),
                     QStringLiteral("8"));
            QVERIFY(list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamEditedIndicator")));
            QCOMPARE(list.prependArchivePage(page), 0);
            list.appendMessage(edit8);
            QCOMPARE(list.messageCount(), 1);
        };
        scenario(false);
        scenario(true);
    }

    void archiveCorrectionCanFindEarlierRevisionAcrossPages()
    {
        MamChatWidget list;
        list.resize(420, 260);
        list.setHistoryLoading(true);
        auto original = message(QStringLiteral("id-6"), QStringLiteral("6"), 0);
        auto edited = message(QStringLiteral("id-7"), QStringLiteral("7"), 10);
        edited.setReplaceId(original.messageId());
        list.prependArchivePage({{QStringLiteral("uid-7"), edited}});
        QCOMPARE(list.messageCount(), 1);
        QCOMPARE(list.prependArchivePage({{QStringLiteral("uid-6"), original}}), 0);
        QCOMPARE(list.messageCount(), 1);
        list.show();
        QTRY_VERIFY(list.itemWidget(0));
        QTRY_COMPARE(list.itemWidget(0)->findChild<ChatMessageText *>()->toPlainText(),
                     QStringLiteral("7"));
    }

    void intermediateEditUnifiesPreviouslySeparateEDBAndLiveRows()
    {
        MamChatWidget list;
        list.resize(480, 260);
        list.setHistoryLoading(true);
        list.show();
        auto original = message(QStringLiteral("id-6"), QStringLiteral("6"), 1);
        original.setHistoryId(42);
        list.appendMessage(original); // Local EDB supplied the older version.
        auto edit8 = message(QStringLiteral("id-8"), QStringLiteral("8"), 3);
        edit8.setReplaceId(QStringLiteral("id-7"));
        list.appendMessage(edit8); // Middle revision is not loaded yet.
        QCOMPARE(list.messageCount(), 2);
        auto archive6 = original;
        archive6.setHistoryId(0);
        auto edit7 = message(QStringLiteral("id-7"), QStringLiteral("7"), 2);
        edit7.setReplaceId(original.messageId());
        QCOMPARE(list.prependArchivePage({
                     {QStringLiteral("mam-6"), archive6},
                     {QStringLiteral("mam-7"), edit7},
                     {QStringLiteral("mam-8"), edit8}}), 0);
        QCOMPARE(list.messageCount(), 1);
        QTRY_VERIFY(list.itemWidget(0));
        QCOMPARE(list.itemWidget(0)->findChild<ChatMessageText *>()->toPlainText(),
                 QStringLiteral("8"));
        QCOMPARE(list.itemWidget(0)->property("_psiMamRowKey").toString(),
                 QStringLiteral("edb:42"));
        QVERIFY(list.itemWidget(0)->findChild<QLabel *>(QStringLiteral("mamEditedIndicator")));
    }

    void correctionCannotCrossMucSenderOrDirection()
    {
        MamChatWidget list;
        list.setGroupChat(true);
        auto alice = message(QStringLiteral("base"), QStringLiteral("Alice"), 0);
        alice.setNick(QStringLiteral("Alice"));
        list.appendMessage(alice);
        auto bob = message(QStringLiteral("corrected"), QStringLiteral("Bob"), 2);
        bob.setNick(QStringLiteral("Bob"));
        bob.setReplaceId(QStringLiteral("base"));
        list.appendMessage(bob);
        QCOMPARE(list.messageCount(), 2);
        auto outgoing = message(QStringLiteral("another"), QStringLiteral("Outgoing"), 3, true);
        outgoing.setNick(QStringLiteral("Alice"));
        outgoing.setReplaceId(QStringLiteral("base"));
        list.appendMessage(outgoing);
        QCOMPARE(list.messageCount(), 3);
    }

    void liveArchiveOverlap()
    {
        MamChatWidget list;
        list.resize(400, 260);
        list.setHistoryLoading(true);
        list.show();
        auto live = message(QStringLiteral("live-1"), QStringLiteral("same body"), 5, true);
        list.appendMessage(live);
        QCOMPARE(list.messageCount(), 1);
        const auto archive = message(QStringLiteral("live-1"),
                                     QStringLiteral("same body"), 7, true);
        QList<QPair<QString, MessageView>> page;
        page.append(qMakePair(QStringLiteral("archive-uid-1"), archive));
        QCOMPARE(list.prependArchivePage(page), 0);
        QCOMPARE(list.messageCount(), 1);
        QCOMPARE(list.prependArchivePage(page), 0);
        QCOMPARE(list.messageCount(), 1);
        // A repeated live echo after MAM adoption must not duplicate the row.
        list.appendMessage(live);
        QCOMPARE(list.messageCount(), 1);

        auto unrelated = message(QStringLiteral("live-1"),
                                 QStringLiteral("same body"), 7, false);
        QCOMPARE(list.prependArchivePage({qMakePair(QStringLiteral("archive-uid-2"), unrelated)}), 1);
        QCOMPARE(list.messageCount(), 2);
    }

    void pagesMaterializeRealWidgetsWithoutAllRows()
    {
        MamChatWidget list;
        list.resize(420, 280);
        list.setHistoryLoading(true);
        list.show();
        for (int i = 0; i != 250; ++i)
            list.appendMessage(message(QStringLiteral("id-%1").arg(i),
                                       QStringLiteral("body-%1").arg(i), i));
        list.scrollToIndex(120, Mattermost::LongListWidget::Alignment::Center);
        QTRY_VERIFY(list.itemWidget(120) != nullptr);
        QVERIFY(list.materializedCount() <= list.materializationLimit());
        QCOMPARE(list.messageCount(), 250);

        QList<QPair<QString, MessageView>> older;
        for (int i = 0; i != 40; ++i)
            older.append(qMakePair(QStringLiteral("uid-%1").arg(i),
                                  message(QStringLiteral("old-%1").arg(i),
                                          QStringLiteral("earlier-%1").arg(i), -100 + i)));
        QCOMPARE(list.prependArchivePage(older), 40);
        QCOMPARE(list.messageCount(), 290);
        QTRY_VERIFY(list.itemWidget(160) != nullptr);
        QCOMPARE(list.materializedCount() <= list.materializationLimit(), true);
    }

    void copySelectionAcrossUnmaterializedRows()
    {
        MamChatWidget list;
        list.resize(440, 260);
        list.setHistoryLoading(true);
        list.show();
        for (int i = 0; i != 240; ++i)
            list.appendMessage(message(QStringLiteral("id-%1").arg(i),
                                       QStringLiteral("body-%1").arg(i), i));
        list.selectMessageRange(12, 10); // Reversed drag also selects chronological order.
        QCOMPARE(list.selectedMessageCount(), 3);
        list.scrollToIndex(200, Mattermost::LongListWidget::Alignment::Center);
        QTRY_VERIFY(list.itemWidget(200) != nullptr);
        QVERIFY(list.itemWidget(11) == nullptr); // The source, not widgets, owns selection.
        list.scrollToIndex(11, Mattermost::LongListWidget::Alignment::Center);
        QTRY_VERIFY(list.itemWidget(11));
        auto *restored = list.itemWidget(11)->findChild<QCheckBox *>();
        QVERIFY(restored && restored->isVisible() && restored->isChecked());
        list.copySelectedMessages();
        const QString clipboard = QApplication::clipboard()->text();
        QVERIFY(clipboard.contains(QStringLiteral("body-10")));
        QVERIFY(clipboard.contains(QStringLiteral("body-11")));
        QVERIFY(clipboard.contains(QStringLiteral("body-12")));
        QVERIFY(clipboard.indexOf(QStringLiteral("body-10"))
                < clipboard.indexOf(QStringLiteral("body-11")));
        QVERIFY(clipboard.indexOf(QStringLiteral("body-11"))
                < clipboard.indexOf(QStringLiteral("body-12")));
        QVERIFY(!clipboard.contains(QStringLiteral("body-13")));
        QCOMPARE(list.prependArchivePage({qMakePair(QStringLiteral("uid-old"),
                          message(QStringLiteral("old"), QStringLiteral("earlier"), -10))}), 1);
        QCOMPARE(list.selectedMessageCount(), 3);
        list.copySelectedMessages();
        QCOMPARE(QApplication::clipboard()->text(), clipboard);
        auto *toolbar = list.findChild<QFrame *>(QStringLiteral("mamSelectionToolbar"));
        QVERIFY(toolbar);
        QVERIFY(!toolbar->isHidden());
        list.cancelMessageSelection();
        QCOMPARE(list.selectedMessageCount(), 0);
        QVERIFY(toolbar->isHidden());
    }

    void archiveAdoptionPreservesSelectedLiveRow()
    {
        MamChatWidget list;
        list.resize(420, 280);
        list.setHistoryLoading(true);
        list.show();
        const auto live = message(QStringLiteral("same"), QStringLiteral("body"), 1, true);
        list.appendMessage(live);
        list.setMessageSelected(0, true);
        QCOMPARE(list.prependArchivePage({qMakePair(QStringLiteral("archive-uid"),
           message(QStringLiteral("same"), QStringLiteral("body"), 2, true))}), 0);
        QCOMPARE(list.selectedMessageCount(), 1);
        list.copySelectedMessages();
        QVERIFY(QApplication::clipboard()->text().contains(QStringLiteral("body")));
        QCOMPARE(list.prependArchivePage({qMakePair(QStringLiteral("archive-uid"),
           message(QStringLiteral("same"), QStringLiteral("body"), 2, true))}), 0);
        QCOMPARE(list.selectedMessageCount(), 1);
    }

    void mouseSelectionAndCopyToolbar()
    {
        MamChatWidget list;
        list.resize(430, 520);
        list.setHistoryLoading(true);
        list.show();
        for (int i = 0; i < 3; ++i)
            list.appendMessage(message(QStringLiteral("m-%1").arg(i),
                                       QStringLiteral("body-%1").arg(i), i));
        QTRY_VERIFY(list.itemWidget(0) && list.itemWidget(1) && list.itemWidget(2));
        QWidget *r0 = list.itemWidget(0);
        QWidget *r1 = list.itemWidget(1);
        QWidget *r2 = list.itemWidget(2);
        QVERIFY(r0);
        QVERIFY(r1);
        QVERIFY(r2);
        const auto labels0 = r0->findChildren<QLabel *>();
        const auto labels1 = r1->findChildren<QLabel *>();
        const auto labels2 = r2->findChildren<QLabel *>();
        QVERIFY(!labels0.isEmpty());
        QVERIFY(!labels1.isEmpty());
        QVERIFY(!labels2.isEmpty());
        auto *first = labels0.first();
        auto *second = labels1.first();
        auto *third = labels2.first();
        // mapTo(QWidget*) requires an ancestor, not a sibling QLabel.
        // Capture the target through global coordinates before virtualization
        // has a chance to recycle either of the row widgets.
        const QPoint dragTarget =
            first->mapFromGlobal(third->mapToGlobal(QPoint(10, 10)));
        QTest::mouseClick(first, Qt::LeftButton, Qt::ControlModifier, QPoint(3, 3));
        QCOMPARE(list.selectedMessageCount(), 1);
        QTest::mouseClick(third, Qt::LeftButton, Qt::ShiftModifier, QPoint(3, 3));
        QCOMPARE(list.selectedMessageCount(), 3);
        QTest::mouseClick(second, Qt::LeftButton, Qt::ControlModifier, QPoint(3, 3));
        QCOMPARE(list.selectedMessageCount(), 2);
        list.copySelectedMessages();
        const QString copied = QApplication::clipboard()->text();
        QVERIFY(copied.contains(QStringLiteral("body-0")));
        QVERIFY(!copied.contains(QStringLiteral("body-1")));
        QVERIFY(copied.contains(QStringLiteral("body-2")));
        list.cancelMessageSelection();
        QCOMPARE(list.selectedMessageCount(), 0);

        // A normal drag wholly within one row remains text selection.
        // Qt5 and Qt6 QTest::mouseMove differ in whether a held mouse button
        // is reflected in the synthetic event. Deliver the same real gesture
        // sequence with explicit button state in both Qt versions.
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(3, 3),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(first, &press);
        QMouseEvent inside(QEvent::MouseMove, QPointF(30, 3),
                           Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(first, &inside);
        QCOMPARE(list.selectedMessageCount(), 0);

        // Leaving the originating message row switches to whole-post range.
        QMouseEvent outside(QEvent::MouseMove, QPointF(dragTarget),
                            Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(first, &outside);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(dragTarget),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(first, &release);
        QCOMPARE(list.selectedMessageCount(), 3);
        list.copySelectedMessages();
        const QString full = QApplication::clipboard()->text();
        QVERIFY(full.contains(QStringLiteral("body-0")));
        QVERIFY(full.contains(QStringLiteral("body-1")));
        QVERIFY(full.contains(QStringLiteral("body-2")));
    }

    void clearingConversationDiscardsPlainClickSelectionAnchor()
    {
        MamChatWidget list;
        list.resize(430, 500);
        list.setHistoryLoading(true);
        list.show();
        list.appendMessage(message(QStringLiteral("old"),
                                   QStringLiteral("old-body"), 0));
        QTRY_VERIFY(list.itemWidget(0) != nullptr);
        const auto oldLabels = list.itemWidget(0)->findChildren<QLabel *>();
        QVERIFY(!oldLabels.isEmpty());

        // An ordinary click does not select a whole row, but records a
        // Shift-selection anchor. The next chat reuses generated row keys.
        QTest::mouseClick(oldLabels.first(), Qt::LeftButton,
                          Qt::NoModifier, QPoint(3, 3));
        QCOMPARE(list.selectedMessageCount(), 0);
        list.clearMessages();

        list.appendMessage(message(QStringLiteral("new-1"),
                                   QStringLiteral("first-new"), 1));
        list.appendMessage(message(QStringLiteral("new-2"),
                                   QStringLiteral("second-new"), 2));
        QTRY_VERIFY(list.itemWidget(1) != nullptr);
        const auto newLabels = list.itemWidget(1)->findChildren<QLabel *>();
        QVERIFY(!newLabels.isEmpty());
        QTest::mouseClick(newLabels.first(), Qt::LeftButton,
                          Qt::ShiftModifier, QPoint(3, 3));

        // Without clearing the old anchor, this accidentally selects both
        // new messages because live:generated:1 is reused.
        QCOMPARE(list.selectedMessageCount(), 1);
        list.copySelectedMessages();
        const auto copied = QApplication::clipboard()->text();
        QVERIFY(copied.contains(QStringLiteral("second-new")));
        QVERIFY(!copied.contains(QStringLiteral("first-new")));
    }

    void shortArchivePagesDoNotAutoPageAtTheLiveTail()
    {
        MamChatWidget list;
        list.resize(360, 240);
        list.setHistoryLoading(true);
        list.show();
        QSignalSpy requests(&list, &MamChatWidget::olderMessagesRequested);

        // Protocol-only MUC pages can leave the native list short (or empty).
        // Completing a page must not implicitly fetch the entire room archive.
        for (int page = 0; page < 4; ++page) {
            list.setHistoryLoading(false);
            QCoreApplication::processEvents();
            QCOMPARE(requests.count(), 0);
            QVERIFY(list.isAtEnd());
            list.setHistoryLoading(true);
        }

        // Replaying a small real page also must not trigger a background sweep.
        list.prependArchivePage({qMakePair(QStringLiteral("uid-1"),
                                          message(QStringLiteral("id-1"), QStringLiteral("Short reply")))});
        list.setHistoryLoading(false);
        QCoreApplication::processEvents();
        QCOMPARE(requests.count(), 0);
        QVERIFY(list.isAtEnd());
    }

    void upwardWheelAtEmptyOrShortHistoryRequestsOnePagePerGesture()
    {
        MamChatWidget list;
        list.resize(360, 240);
        list.setHistoryLoading(true);
        list.show();
        list.setHistoryLoading(false);
        QSignalSpy requests(&list, &MamChatWidget::olderMessagesRequested);

        const auto wheel = [&list](int delta) {
            QWheelEvent event(QPointF(4, 4), QPointF(list.viewport()->mapToGlobal(QPoint(4, 4))),
                              QPoint(), QPoint(0, delta), Qt::NoButton, Qt::NoModifier,
                              Qt::NoScrollPhase, false);
            QApplication::sendEvent(list.viewport(), &event);
        };
        // Scrolling down at the live tail is not an older-history request.
        wheel(-120);
        QCOMPARE(requests.count(), 0);
        wheel(120);
        QCOMPARE(requests.count(), 1);
        wheel(120); // A second gesture before the queued slot must not duplicate a request.
        QCOMPARE(requests.count(), 1);

        // Loading/completion alone must not queue subsequent MAM requests.
        list.setHistoryLoading(true);
        list.setHistoryLoading(false);
        QCoreApplication::processEvents();
        QCOMPARE(requests.count(), 1);
        wheel(120);
        QCOMPARE(requests.count(), 2);

        list.setHistoryExhausted(true);
        wheel(120);
        QCOMPARE(requests.count(), 2);
    }
};

QTEST_MAIN(MamChatWidgetTest)
#include "MamChatWidgetTest.moc"
