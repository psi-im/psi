// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QtCrypto>

namespace PsiCrypto {
inline bool preferNativeProvider()
{
#ifdef Q_OS_WIN
    if (QCA::findProvider(QStringLiteral("qca-wincrypto"))) {
        QCA::setProviderPriority(QStringLiteral("qca-wincrypto"), 0);
        return true;
    }
#endif
    return false;
}
}
