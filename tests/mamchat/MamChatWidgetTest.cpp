/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/MamChatWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QtTest/QtTest>

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
        QCOMPARE(labels.size(), 2);
        QCOMPARE(labels.last()->text(), QStringLiteral("first body"));

        auto changed = message(QStringLiteral("m2"), QStringLiteral("edited body"), 2);
        changed.setReplaceId(QStringLiteral("m1"));
        list.appendMessage(changed);
        QCOMPARE(list.messageCount(), 1);
        QTRY_VERIFY(list.itemWidget(0) != nullptr);
        QTRY_COMPARE(list.itemWidget(0)->findChildren<QLabel *>().last()->text(),
                     QStringLiteral("edited body"));
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

    void emptyArchiveRequestsAnotherPageWithoutScrollGesture()
    {
        MamChatWidget list;
        list.resize(360, 240);
        list.setHistoryLoading(true);
        list.show();
        QSignalSpy requests(&list, &MamChatWidget::olderMessagesRequested);
        list.setHistoryLoading(false);
        QTRY_VERIFY(!requests.isEmpty());
        list.setHistoryExhausted(true);
        const int atStop = requests.count();
        list.setHistoryLoading(false);
        QTest::qWait(40);
        QCOMPARE(requests.count(), atStop);
    }
};

QTEST_MAIN(MamChatWidgetTest)
#include "MamChatWidgetTest.moc"
