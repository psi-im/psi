/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QLoggingCategory>

namespace PsiMam {
inline const QLoggingCategory &historyLog()
{
    static const QLoggingCategory category("psi.history", QtWarningMsg);
    return category;
}
}
