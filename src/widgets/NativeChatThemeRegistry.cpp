/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "NativeChatThemeRegistry.h"
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
    QList<NativeChatTheme> result { { QStringLiteral("modern"), tr("Modern"), NativeChatTheme::Modern, 40, 10, {} },
                                    { QStringLiteral("classic"), tr("Classic"), NativeChatTheme::Classic, 0, 4, {} } };
    for (const auto &themes : plugins_)
        result.append(themes);
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
void NativeChatThemes::add(QObject *owner, const QList<NativeChatTheme> &themes)
{
    if (!owner)
        return;
    remove(owner);
    QSet<QString> ids;
    for (const auto &theme : this->themes())
        ids.insert(theme.id);
    QList<NativeChatTheme> accepted;
    for (auto theme : themes) {
        if (theme.id.isEmpty() || theme.name.isEmpty() || ids.contains(theme.id))
            continue;
        ids.insert(theme.id);
        // QStringLiteral data may live in the plugin's shared library.
        theme.id.detach();
        theme.name.detach();
        theme.styleSheet.detach();
        theme.avatarSize = qBound(0, theme.avatarSize, 96);
        theme.spacing    = qBound(0, theme.spacing, 32);
        accepted.append(theme);
    }
    if (accepted.isEmpty())
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
