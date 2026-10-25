/* SPDX-License-Identifier: GPL-2.0-or-later */
// Geometry and hit-testing regression for the MatterLeast one-sided splitter.
#include "widgets/ThinSplitter.h"

#include <QCoreApplication>
#include <QSplitterHandle>
#include <QtTest>

class ThinSplitterTest : public QObject {
    Q_OBJECT
private slots:
    void horizontalHitAreaDoesNotCoverPreviousPane()
    {
        ThinSplitter splitter(Qt::Horizontal);
        splitter.resize(420, 180);
        auto *left = new QWidget;
        auto *right = new QWidget;
        splitter.addWidget(left);
        splitter.addWidget(right);
        splitter.setSizes({200, 220});
        splitter.show();
        QCoreApplication::processEvents();

        QSplitterHandle *handle = splitter.handle(1);
        QVERIFY(handle);
        QCOMPARE(splitter.handleWidth(), ThinSplitter::VisibleHandleExtent);
        QCOMPARE(right->geometry().left() - left->geometry().right() - 1,
                 ThinSplitter::VisibleHandleExtent);
        QCOMPARE(handle->geometry().left(), left->geometry().right() + 1);
        QCOMPARE(handle->contentsRect().width(), ThinSplitter::VisibleHandleExtent);
        QCOMPARE(handle->geometry().right() - right->geometry().left() + 1,
                 ThinSplitter::TrailingHitPadding);
        QCOMPARE(handle->mask().boundingRect(), handle->contentsRect());
        QVERIFY(handle->testAttribute(Qt::WA_MouseNoMask));
    }

    void verticalHitAreaDoesNotCoverPreviousPane()
    {
        ThinSplitter splitter(Qt::Vertical);
        splitter.resize(240, 420);
        auto *top = new QWidget;
        auto *bottom = new QWidget;
        splitter.addWidget(top);
        splitter.addWidget(bottom);
        splitter.setSizes({200, 220});
        splitter.show();
        QCoreApplication::processEvents();

        QSplitterHandle *handle = splitter.handle(1);
        QVERIFY(handle);
        QCOMPARE(bottom->geometry().top() - top->geometry().bottom() - 1,
                 ThinSplitter::VisibleHandleExtent);
        QCOMPARE(handle->geometry().top(), top->geometry().bottom() + 1);
        QCOMPARE(handle->contentsRect().height(), ThinSplitter::VisibleHandleExtent);
        QCOMPARE(handle->geometry().bottom() - bottom->geometry().top() + 1,
                 ThinSplitter::TrailingHitPadding);
        QCOMPARE(handle->mask().boundingRect(), handle->contentsRect());
        QVERIFY(handle->testAttribute(Qt::WA_MouseNoMask));
    }

    void rtlHandleOverlapsOnlyFollowingPane()
    {
        ThinSplitter splitter(Qt::Horizontal);
        splitter.setLayoutDirection(Qt::RightToLeft);
        splitter.resize(420, 180);
        auto *first = new QWidget;
        auto *second = new QWidget;
        splitter.addWidget(first);
        splitter.addWidget(second);
        splitter.setSizes({200, 220});
        splitter.show();
        QCoreApplication::processEvents();

        const QSplitterHandle *handle = splitter.handle(1);
        QVERIFY(handle);
        QCOMPARE(handle->contentsRect().width(), ThinSplitter::VisibleHandleExtent);
        QCOMPARE(handle->contentsMargins().left(), ThinSplitter::TrailingHitPadding);
        QCOMPARE(handle->contentsMargins().right(), 0);
        QCOMPARE(handle->mask().boundingRect(), handle->contentsRect());
    }

    void legacyHandleWidthCanBeResetWithoutChangingHitArea()
    {
        QSplitter legacy(Qt::Horizontal);
        legacy.setHandleWidth(4);
        legacy.addWidget(new QWidget);
        legacy.addWidget(new QWidget);
        const QByteArray state = legacy.saveState();

        ThinSplitter splitter(Qt::Horizontal);
        splitter.addWidget(new QWidget);
        splitter.addWidget(new QWidget);
        QVERIFY(splitter.restoreState(state));
        splitter.setHandleWidth(ThinSplitter::VisibleHandleExtent);
        QCOMPARE(splitter.handle(1)->sizeHint().width(), ThinSplitter::VisibleHandleExtent);
    }
};
QTEST_MAIN(ThinSplitterTest)
#include "ThinSplitterTest.moc"
