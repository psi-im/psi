/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QRegularExpression>
#include <QString>
namespace TextUtil {
inline QString sizeUnit(qlonglong, qlonglong *div)
{
    *div = 1;
    return QStringLiteral("B");
}
inline QString roundedNumber(qint64 size, qlonglong div) { return QString::number(size / div); }
inline QString escape(const QString &plain) { return plain.toHtmlEscaped(); }
inline QString plain2rich(const QString &plain)
{
    return plain.toHtmlEscaped().replace(QLatin1Char('\n'), QLatin1String("<br>"));
}
inline QString linkify(const QString &rich)
{
    QString result = rich;
    static const QRegularExpression url(QStringLiteral(R"((https?://[^\s<]+))"));
    return result.replace(url, QStringLiteral(R"(<a href="\1">\1</a>)"));
}
}
