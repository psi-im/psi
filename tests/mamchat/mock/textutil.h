/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QString>
namespace TextUtil {
inline QString sizeUnit(qlonglong, qlonglong *div)
{
    *div = 1;
    return QStringLiteral("B");
}
inline QString roundedNumber(qint64 size, qlonglong div) { return QString::number(size / div); }
inline QString escape(const QString &plain) { return plain.toHtmlEscaped(); }
}
