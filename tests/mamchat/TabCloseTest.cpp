/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/tabbar.h"

#include <QAbstractButton>
#include <QApplication>
#include <QSignalSpy>
#include <QStyle>
#include <QTest>

// A delayed mouse release must not turn a removed tab's close button into a
// request to close an invalid page index.
class TabCloseTest : public QObject {
    Q_OBJECT

private slots:
    void removedTabButtonDoesNotEmitCloseRequested()
    {
        TabBar bar;
        bar.resize(480, 48);
        bar.setTabsClosable(true);
        bar.setMultiRow(true);
        bar.addTab(QStringLiteral("First"));
        bar.addTab(QStringLiteral("Second"));
        bar.show();
        const auto side = static_cast<QTabBar::ButtonPosition>(
            bar.style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, &bar));
        auto *removed = qobject_cast<QAbstractButton *>(bar.tabButton(1, side));
        auto *remaining = qobject_cast<QAbstractButton *>(bar.tabButton(0, side));
        QVERIFY(removed);
        QVERIFY(remaining);
        QVERIFY(removed != remaining);

        QSignalSpy requested(&bar, &QTabBar::tabCloseRequested);
        bar.removeTab(1);
        QCOMPARE(bar.count(), 1);
        QVERIFY(!removed->isEnabled());
        // The close button survives until DeferredDelete is processed.
        // Even an already queued clicked(bool) signal must be disconnected.
        QVERIFY(QMetaObject::invokeMethod(removed, "clicked", Qt::DirectConnection, Q_ARG(bool, false)));
        QCOMPARE(requested.count(), 0);

        remaining->click();
        QCOMPARE(requested.count(), 1);
        QCOMPARE(requested.takeFirst().at(0).toInt(), 0);
    }

    void removedAndUnmappedTabsCannotBeClosed()
    {
        TabBar bar;
        bar.resize(480, 48);
        bar.setTabsClosable(true);
        bar.setMultiRow(true);
        bar.addTab(QStringLiteral("First"));
        bar.addTab(QStringLiteral("Second"));
        bar.show();
        const auto side = static_cast<QTabBar::ButtonPosition>(
            bar.style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, &bar));
        auto *last = qobject_cast<QAbstractButton *>(bar.tabButton(1, side));
        QVERIFY(last);
        QSignalSpy requested(&bar, &QTabBar::tabCloseRequested);
        bar.removeTab(1);
        // A direct, late click on the removed close-button must not be
        // interpreted as tab -1 by TabDlg.
        last->click();
        QCOMPARE(requested.count(), 0);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
};

QTEST_MAIN(TabCloseTest)
#include "TabCloseTest.moc"
