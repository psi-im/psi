/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/NativeChatRenderers.h"
#include "widgets/NativeChatThemeRegistry.h"
#include <QSignalSpy>
#include <QtTest>

class PlainRenderer final : public NativeChatMessageRenderer {
public:
    NativeChatPartOptions partOptions(const NativeChatMessageInfo &) const override { return {}; }
    void                  arrange(NativeChatMessageCanvas &, const NativeChatMessageInfo &) const override { }
};

class RendererPlugin final : public NativeChatRendererProvider {
public:
    PlainRenderer                    renderer;
    const NativeChatMessageRenderer *nativeChatRenderer(const QString &themeId) const override
    {
        return themeId == QLatin1String("org.example.coded") ? &renderer : nullptr;
    }
};

class NativeChatThemeTest : public QObject {
    Q_OBJECT
private slots:
    void pluginLifecycle()
    {
        auto *registry = NativeChatThemes::instance();
        QCOMPARE(registry->themes().size(), 3);
        QCOMPARE(registry->theme(QStringLiteral("missing")).id, QStringLiteral("modern"));
        QObject         owner;
        QSignalSpy      changed(registry, &NativeChatThemes::changed);
        NativeChatTheme plugin { QStringLiteral("org.example.theme"),
                                 QStringLiteral("Example"),
                                 NativeChatTheme::Classic,
                                 500,
                                 -10,
                                 QStringLiteral("QLabel { color: palette(text); }") };
        plugin.layoutSpec = QStringLiteral(R"({"direction":"horizontal","items":["body","author"]})");
        registry->add(&owner, { plugin, plugin, registry->themes().first() });
        QCOMPARE(changed.count(), 1);
        QCOMPARE(registry->themes().size(), 4);
        QVERIFY(!registry->renderer(plugin.id));
        QVERIFY(!registry->hasRenderers(&owner));
        const auto copied = registry->theme(plugin.id);
        QCOMPARE(copied.avatarSize, 96);
        QCOMPARE(copied.spacing, 0);
        // Values are detached from the plugin's potentially static storage.
        QVERIFY(copied.id.constData() != plugin.id.constData());
        QVERIFY(copied.name.constData() != plugin.name.constData());
        QVERIFY(copied.styleSheet.constData() != plugin.styleSheet.constData());
        QVERIFY(copied.layoutSpec.constData() != plugin.layoutSpec.constData());
        registry->remove(&owner);
        QCOMPARE(changed.count(), 2);
        QCOMPARE(registry->themes().size(), 3);
        QCOMPARE(registry->theme(plugin.id).id, QStringLiteral("modern"));
        auto *temporary = new QObject;
        registry->add(temporary, { plugin });
        delete temporary;
        QCOMPARE(registry->themes().size(), 3);
    }

    void builtInBubbleUsesItsRenderer()
    {
        auto *registry = NativeChatThemes::instance();
        QCOMPARE(registry->theme(QStringLiteral("bubble")).id, QStringLiteral("bubble"));
        QCOMPARE(registry->renderer(QStringLiteral("bubble")), NativeChatRenderers::bubble());
        QVERIFY(!registry->renderer(QStringLiteral("modern")));
        QVERIFY(!registry->renderer(QStringLiteral("classic")));
        // Built-in IDs are reserved.
        QObject owner;
        registry->add(&owner, { { QStringLiteral("bubble"), QStringLiteral("Mine"), NativeChatTheme::Modern, 40, 10, {} } });
        QCOMPARE(registry->theme(QStringLiteral("bubble")).name, registry->themes().at(2).name);
        QCOMPARE(registry->themes().size(), 3);
    }

    void pluginRenderersFollowTheirThemes()
    {
        auto          *registry = NativeChatThemes::instance();
        RendererPlugin plugin;
        QObject        owner;
        registry->add(&owner,
                      { { QStringLiteral("org.example.coded"), QStringLiteral("Coded"), NativeChatTheme::Modern, 40, 10, {} },
                        { QStringLiteral("org.example.described"), QStringLiteral("Described"), NativeChatTheme::Classic, 0,
                          4, {} } },
                      &plugin);
        QCOMPARE(registry->renderer(QStringLiteral("org.example.coded")), &plugin.renderer);
        QVERIFY(!registry->renderer(QStringLiteral("org.example.described")));
        QVERIFY(registry->hasRenderers(&owner));
        registry->remove(&owner);
        QVERIFY(!registry->renderer(QStringLiteral("org.example.coded")));
        QVERIFY(!registry->hasRenderers(&owner));
    }
};
QTEST_MAIN(NativeChatThemeTest)
#include "NativeChatThemeTest.moc"
