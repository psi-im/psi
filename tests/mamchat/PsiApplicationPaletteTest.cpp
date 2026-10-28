/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "psiapplication.h"

#include <QEvent>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif
#include <QSignalSpy>
#include <QtTest/QtTest>

class PsiApplicationPaletteTest : public QObject {
    Q_OBJECT

private slots:
    void applicationPaletteChangesAreCoalesced()
    {
        auto *app = qobject_cast<PsiApplication *>(qApp);
        QVERIFY(app);
        QSignalSpy changed(app, &PsiApplication::applicationPaletteChanged);
        QVERIFY(changed.isValid());

        QEvent event(QEvent::ApplicationPaletteChange);
        for (int i = 0; i < 4; ++i)
            QCoreApplication::sendEvent(app, &event);

        QTRY_COMPARE(changed.count(), 1);
        // Ordinary palette changes do not need the separate theme-settling
        // timer; repeated events in one event-loop turn produce one update.
        QTest::qWait(60);
        QCOMPARE(changed.count(), 1);
    }

    void themeChangesAlsoRefreshAfterQtHasSettled()
    {
        auto *app = qobject_cast<PsiApplication *>(qApp);
        QVERIFY(app);
        QSignalSpy changed(app, &PsiApplication::applicationPaletteChanged);
        QVERIFY(changed.isValid());

        // Some backends report a theme change before the default application
        // palette is installed. Keep a final refresh after the 250 ms debounce.
        QEvent theme(QEvent::ThemeChange);
        QCoreApplication::sendEvent(app, &theme);
        QTRY_COMPARE(changed.count(), 1);
        QTest::qWait(75);
        QCoreApplication::sendEvent(app, &theme);
        QTRY_COMPARE(changed.count(), 2);
        QTest::qWait(100);
        QCOMPARE(changed.count(), 2);
        QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 3, 1000);
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    void styleHintsSignalIsConnectedIndependentlyOfPaletteEvents()
    {
        auto *app = qobject_cast<PsiApplication *>(qApp);
        QVERIFY(app);
        QSignalSpy changed(app, &PsiApplication::applicationPaletteChanged);
        QVERIFY(changed.isValid());

        // Emulate QPA notifying the color-scheme change before posting a
        // Qt application palette event, without changing a user setting.
        const bool invoked = QMetaObject::invokeMethod(
            QGuiApplication::styleHints(), "colorSchemeChanged", Qt::DirectConnection,
            Q_ARG(Qt::ColorScheme, Qt::ColorScheme::Dark));
        QVERIFY(invoked);
        QTRY_COMPARE(changed.count(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 2, 1000);
    }
#endif
};

int main(int argc, char **argv)
{
    PsiApplication app(argc, argv);
    PsiApplicationPaletteTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "PsiApplicationPaletteTest.moc"
