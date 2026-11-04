/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "NativeChatThemeRegistry.h"
#include "nativechat/themes/BuiltinThemes.h"
#include <QApplication>
#include <QSet>

NativeChatThemes::NativeChatThemes(QObject *parent) : QObject(parent) { }
NativeChatThemes *NativeChatThemes::instance()
{
    static auto *registry = [] {
        auto *value = new NativeChatThemes(qApp);
        NativeChatBuiltins::registerThemes(value);
        return value;
    }();
    return registry;
}
QList<NativeChatTheme> NativeChatThemes::themes() const
{
    QList<NativeChatTheme> result;
    for (const auto &entry : registrations_)
        result.append(entry.themes);
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
    for (const auto &entry : registrations_)
        if (auto *renderer = entry.renderers.value(id, nullptr))
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
    RegisteredThemes accepted;
    accepted.owner = owner;
    for (auto theme : themes) {
        if (theme.id.isEmpty() || theme.name.isEmpty() || ids.contains(theme.id))
            continue;
        ids.insert(theme.id);
        // QStringLiteral data may live in a plugin's shared library.
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
    registrations_.append(accepted);
    connect(owner, &QObject::destroyed, this, [this, owner] { remove(owner); });
    emit changed();
}
void NativeChatThemes::remove(QObject *owner)
{
    disconnect(owner, nullptr, this, nullptr);
    for (int i = 0; i < registrations_.size(); ++i) {
        if (registrations_.at(i).owner != owner)
            continue;
        registrations_.removeAt(i);
        emit changed();
        return;
    }
}
bool NativeChatThemes::hasRenderers(QObject *owner) const
{
    for (const auto &entry : registrations_)
        if (entry.owner == owner)
            return !entry.renderers.isEmpty();
    return false;
}
