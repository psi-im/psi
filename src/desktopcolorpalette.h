/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QPalette>
#include <QString>

// Storage and preview policy; application palettes themselves remain owned by Qt.
namespace DesktopColorPalette {
inline QString name(const QPalette &desktop)
{
    return desktop.color(QPalette::Base).lightness() < 128 ? QStringLiteral("dark") : QStringLiteral("light");
}
inline QString optionPath(const QString &logical, const QString &scheme)
{
    const QString prefix = QStringLiteral("options.ui.look.colors.");
    return logical.startsWith(prefix)
        ? QStringLiteral("options.ui.look.palettes.") + scheme + QLatin1Char('.') + logical.mid(prefix.size())
        : logical;
}
inline QPalette preview(const QString &scheme, const QPalette &desktop)
{
    if (scheme == name(desktop))
        return desktop;
    QPalette   result = desktop;
    const bool dark   = scheme == QLatin1String("dark");
    for (auto role : { QPalette::Text, QPalette::WindowText, QPalette::ButtonText, QPalette::ToolTipText })
        result.setColor(role, dark ? QColor(232, 232, 232) : QColor(32, 32, 32));
    result.setColor(QPalette::Base, dark ? QColor(32, 32, 32) : QColor(Qt::white));
    for (auto role : { QPalette::Window, QPalette::Button, QPalette::ToolTipBase })
        result.setColor(role, dark ? QColor(48, 48, 48) : QColor(239, 239, 239));
    result.setColor(QPalette::Dark, dark ? QColor(24, 24, 24) : QColor(160, 160, 160));
    result.setColor(QPalette::Link, dark ? QColor(128, 184, 255) : QColor(0, 70, 150));
    result.setColor(QPalette::LinkVisited, dark ? QColor(199, 151, 255) : QColor(100, 40, 140));
    return result;
}
}
