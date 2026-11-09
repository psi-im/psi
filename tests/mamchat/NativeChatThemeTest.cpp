/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "widgets/NativeChatThemeRegistry.h"
#include "../../plugins/include/nativechatrenderer.h"
#include <QSignalSpy>
#include <QtTest>
#include <memory>

class PlainRenderer final : public NativeChatMessageRenderer {
public:
    NativeChatPartOptions partOptions(const NativeChatMessageInfo &) const override { return {}; }
    void arrange(NativeChatMessageCanvas &, const NativeChatMessageInfo &) const override { }
};

class RendererPlugin final : public NativeChatThemeProvider {
public:
    std::shared_ptr<PlainRenderer> renderer = std::make_shared<PlainRenderer>();
    QList<NativeChatTheme> nativeChatThemes() const override
    {
        NativeChatTheme coded(QStringLiteral("org.example.coded"), QStringLiteral("Coded"));
        coded.setRenderer(renderer);
        NativeChatTheme described(QStringLiteral("org.example.described"), QStringLiteral("Described"));
        described.layoutSpec = QStringLiteral(R"({"direction":"vertical","items":["body"],"timestampFormat":"HH:mm {relative}"})");
        return { coded, described };
    }
};

class NativeChatThemeTest : public QObject {
    Q_OBJECT
private slots:
    void pluginLifecycle()
    {
        auto *registry = NativeChatThemes::instance();
        QCOMPARE(registry->themes().size(), 3);
        QCOMPARE(registry->themes().at(0).id, QStringLiteral("modern"));
        QCOMPARE(registry->themes().at(1).id, QStringLiteral("classic"));
        QCOMPARE(registry->themes().at(2).id, QStringLiteral("bubble"));
        QCOMPARE(registry->theme(QStringLiteral("missing")).id, QStringLiteral("modern"));

        QObject owner;
        QSignalSpy changed(registry, &NativeChatThemes::changed);
        NativeChatTheme plugin(QStringLiteral("org.example.theme"), QStringLiteral("Example"), 500, -10,
                               QStringLiteral("QLabel { color: palette(text); }"));
        plugin.description = QStringLiteral("Example theme");
        plugin.layoutSpec = QStringLiteral(R"({"direction":"horizontal","items":["body","author"]})");
        plugin.hoverHighlight = false;
        plugin.timestampFormat = QStringLiteral("HH:mm:ss");
        plugin.background = QColor(0x10, 0x20, 0x30);
        plugin.backgroundImage = QStringLiteral(":/example/paper.png");
        plugin.backgroundImageMode = NativeChatTheme::Scale;
        registry->add(&owner, { plugin, plugin, registry->themes().first() });
        QCOMPARE(changed.count(), 1);
        QCOMPARE(registry->themes().size(), 4);
        QVERIFY(!registry->theme(plugin.id).hasCustomRenderer());
        QVERIFY(!registry->hasCustomRenderers(&owner));
        QVERIFY(registry->theme(plugin.id).renderer());

        const auto copied = registry->theme(plugin.id);
        QCOMPARE(copied.avatarSize, 96);
        QCOMPARE(copied.spacing, 0);
        QVERIFY(copied.id.constData() != plugin.id.constData());
        QVERIFY(copied.name.constData() != plugin.name.constData());
        QVERIFY(copied.description.constData() != plugin.description.constData());
        QVERIFY(copied.styleSheet.constData() != plugin.styleSheet.constData());
        QVERIFY(copied.layoutSpec.constData() != plugin.layoutSpec.constData());
        QVERIFY(copied.backgroundImage.constData() != plugin.backgroundImage.constData());
        QCOMPARE(copied.backgroundImage, plugin.backgroundImage);
        QCOMPARE(copied.timestampFormat, QStringLiteral("HH:mm:ss"));
        QVERIFY(!copied.hoverHighlight);
        QCOMPARE(copied.background, plugin.background);
        QCOMPARE(copied.backgroundImageMode, NativeChatTheme::Scale);

        registry->remove(&owner);
        QCOMPARE(changed.count(), 2);
        QCOMPARE(registry->themes().size(), 3);
        QCOMPARE(registry->theme(plugin.id).id, QStringLiteral("modern"));
        auto *temporary = new QObject;
        registry->add(temporary, { plugin });
        delete temporary;
        QCOMPARE(registry->themes().size(), 3);
    }

    void everyBuiltInThemeOwnsItsRenderer()
    {
        auto *registry = NativeChatThemes::instance();
        const auto modern = registry->theme(QStringLiteral("modern"));
        const auto classic = registry->theme(QStringLiteral("classic"));
        const auto bubble = registry->theme(QStringLiteral("bubble"));
        QVERIFY(modern.hasCustomRenderer());
        QVERIFY(classic.hasCustomRenderer());
        QVERIFY(bubble.hasCustomRenderer());
        QVERIFY(modern.renderer());
        QVERIFY(classic.renderer());
        QVERIFY(bubble.renderer());
        QCOMPARE(modern.renderer()->partOptions({}).avatarSize, 40);
        QCOMPARE(classic.renderer()->partOptions({}).header, NativeChatPartOptions::HeaderInText);
        QCOMPARE(classic.timestampFormat, QStringLiteral("HH:mm:ss"));
        QCOMPARE(bubble.renderer()->partOptions({}).avatarSize, 40);
        QVERIFY(!bubble.hoverHighlight);
        QVERIFY(modern.hoverHighlight);
        QVERIFY(!classic.hoverHighlight);
        QVERIFY(!modern.description.isEmpty());
        QVERIFY(!classic.description.isEmpty());
        QVERIFY(!bubble.description.isEmpty());

        QObject owner;
        registry->add(&owner, { NativeChatTheme(QStringLiteral("bubble"), QStringLiteral("Mine")) });
        QCOMPARE(registry->theme(QStringLiteral("bubble")).name, bubble.name);
        QCOMPARE(registry->themes().size(), 3);
    }

    void pluginRenderersBelongToThemes()
    {
        auto *registry = NativeChatThemes::instance();
        RendererPlugin plugin;
        QObject owner;
        registry->add(&owner, plugin.nativeChatThemes());
        const auto coded = registry->theme(QStringLiteral("org.example.coded"));
        const auto described = registry->theme(QStringLiteral("org.example.described"));
        QCOMPARE(coded.renderer().get(), plugin.renderer.get());
        QVERIFY(coded.hasCustomRenderer());
        QVERIFY(!described.hasCustomRenderer());
        QVERIFY(described.renderer());
        QCOMPARE(described.renderer()->partOptions({}).timeFormat, QStringLiteral("HH:mm {relative}"));
        QVERIFY(registry->hasCustomRenderers(&owner));
        registry->remove(&owner);
        QCOMPARE(registry->theme(QStringLiteral("org.example.coded")).id, QStringLiteral("modern"));
        QVERIFY(!registry->hasCustomRenderers(&owner));
        // A selected/copied theme keeps its renderer alive through removal.
        QCOMPARE(coded.renderer().get(), plugin.renderer.get());
    }
};
QTEST_MAIN(NativeChatThemeTest)
#include "NativeChatThemeTest.moc"
