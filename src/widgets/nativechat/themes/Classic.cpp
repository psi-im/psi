/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BuiltinThemes.h"
#include <QCoreApplication>
namespace NativeChatBuiltins {
NativeChatTheme classic()
{
    NativeChatTheme theme;
    theme.id = QStringLiteral("classic");
    theme.name = QCoreApplication::translate("NativeChatThemes", "Classic");
    theme.layout = NativeChatTheme::Classic;
    theme.avatarSize = 0;
    theme.spacing = 4;
    return theme;
}
}
