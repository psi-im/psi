/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/NativeChatThemeRegistry.h"
#include <QSignalSpy>
#include <QtTest>

class NativeChatThemeTest : public QObject {
    Q_OBJECT
private slots:
    void pluginLifecycle()
    {
        auto *registry = NativeChatThemes::instance();
        QCOMPARE(registry->themes().size(), 2);
        QCOMPARE(registry->theme(QStringLiteral("missing")).id, QStringLiteral("modern"));
        QObject         owner;
        QSignalSpy      changed(registry, &NativeChatThemes::changed);
        NativeChatTheme plugin { QStringLiteral("org.example.theme"),
                                 QStringLiteral("Example"),
                                 NativeChatTheme::Classic,
                                 500,
                                 -10,
                                 QStringLiteral("QLabel { color: palette(text); }") };
        registry->add(&owner, { plugin, plugin, registry->themes().first() });
        QCOMPARE(changed.count(), 1);
        QCOMPARE(registry->themes().size(), 3);
        const auto copied = registry->theme(plugin.id);
        QCOMPARE(copied.avatarSize, 96);
        QCOMPARE(copied.spacing, 0);
        // Values are detached from the plugin's potentially static storage.
        QVERIFY(copied.id.constData() != plugin.id.constData());
        QVERIFY(copied.name.constData() != plugin.name.constData());
        QVERIFY(copied.styleSheet.constData() != plugin.styleSheet.constData());
        registry->remove(&owner);
        QCOMPARE(changed.count(), 2);
        QCOMPARE(registry->themes().size(), 2);
        QCOMPARE(registry->theme(plugin.id).id, QStringLiteral("modern"));
        auto *temporary = new QObject;
        registry->add(temporary, { plugin });
        delete temporary;
        QCOMPARE(registry->themes().size(), 2);
    }
};
QTEST_MAIN(NativeChatThemeTest)
#include "NativeChatThemeTest.moc"
