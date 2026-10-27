/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "NativeChatThemeRegistry.h"
#include "NativeChatRenderers.h"
#include <QApplication>
#include <QSet>

NativeChatThemes::NativeChatThemes(QObject *parent) : QObject(parent) { }
NativeChatThemes *NativeChatThemes::instance()
{
    static auto *registry = new NativeChatThemes(qApp);
    return registry;
}
QList<NativeChatTheme> NativeChatThemes::themes() const
{
    // Bubble is laid out by NativeChatRenderers::bubble(); the description is
    // what plugins and settings see. Its bubbles already set each message
    // apart, so it does not tint the message under the pointer.
    NativeChatTheme bubble { QStringLiteral("bubble"), tr("Bubble"), NativeChatTheme::Modern, 40, 6, {}, {} };
    bubble.hoverHighlight = false;
    QList<NativeChatTheme> result { { QStringLiteral("modern"), tr("Modern"), NativeChatTheme::Modern, 40, 10, {}, {} },
                                    { QStringLiteral("classic"), tr("Classic"), NativeChatTheme::Classic, 0, 4, {}, {} },
                                    bubble };
    for (const auto &plugin : plugins_)
        result.append(plugin.themes);
    return result;
}
NativeChatTheme NativeChatThemes::theme(const QString &id) const
{
    const auto all = themes();
    for (const auto &theme : all)
        if (theme.id == id)
            return theme;
    return all.first();
}
const NativeChatMessageRenderer *NativeChatThemes::renderer(const QString &id) const
{
    if (id == QLatin1String("bubble"))
        return NativeChatRenderers::bubble();
    for (const auto &plugin : plugins_)
        if (auto *renderer = plugin.renderers.value(id, nullptr))
            return renderer;
    return nullptr;
}
void NativeChatThemes::add(QObject *owner, const QList<NativeChatTheme> &themes,
                           const NativeChatRendererProvider *renderers)
{
    if (!owner)
        return;
    remove(owner);
    QSet<QString> ids;
    for (const auto &theme : this->themes())
        ids.insert(theme.id);
    PluginThemes accepted;
    for (auto theme : themes) {
        if (theme.id.isEmpty() || theme.name.isEmpty() || ids.contains(theme.id))
            continue;
        ids.insert(theme.id);
        // QStringLiteral data may live in the plugin's shared library.
        theme.id.detach();
        theme.name.detach();
        theme.styleSheet.detach();
        theme.layoutSpec.detach();
        theme.backgroundImage.detach();
        theme.avatarSize = qBound(0, theme.avatarSize, 96);
        theme.spacing    = qBound(0, theme.spacing, 32);
        accepted.themes.append(theme);
        if (renderers)
            if (auto *renderer = renderers->nativeChatRenderer(theme.id))
                accepted.renderers.insert(theme.id, renderer);
    }
    if (accepted.themes.isEmpty())
        return;
    plugins_.insert(owner, accepted);
    connect(owner, &QObject::destroyed, this, [this, owner] { remove(owner); });
    emit changed();
}
void NativeChatThemes::remove(QObject *owner)
{
    disconnect(owner, nullptr, this, nullptr);
    if (plugins_.remove(owner))
        emit changed();
}
bool NativeChatThemes::hasRenderers(QObject *owner) const
{
    const auto it = plugins_.constFind(owner);
    return it != plugins_.cend() && !it->renderers.isEmpty();
}
