/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BuiltinThemes.h"
#include <QCoreApplication>
namespace NativeChatBuiltins {
NativeChatTheme modern()
{
    NativeChatTheme theme;
    theme.id = QStringLiteral("modern");
    theme.name = QCoreApplication::translate("NativeChatThemes", "Modern");
    return theme;
}
}
