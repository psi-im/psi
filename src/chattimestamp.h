/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef CHATTIMESTAMP_H
#define CHATTIMESTAMP_H

#include <QCoreApplication>
#include <QDateTime>
#include <QLocale>
#include <QString>
#include <QtGlobal>

// Shared presentation policy for all native message themes and reply previews.
// The original QDateTime is always retained for the full-time tooltip.
namespace ChatTimestamp {
enum class Mode { Clock, Relative };
inline constexpr const char *OptionKey = "options.ui.chat.timestamp-mode";

inline Mode fromOption(const QString &value)
{
    return value == QLatin1String("clock") ? Mode::Clock : Mode::Relative;
}

inline QString optionValue(Mode mode)
{
    return mode == Mode::Clock ? QStringLiteral("clock") : QStringLiteral("relative");
}

inline QString relative(const QDateTime &when, const QDateTime &now = QDateTime::currentDateTime())
{
    if (!when.isValid())
        return {};
    const qint64 age = when.msecsTo(now);
    if (age < 0 || age < 60000)
        return QCoreApplication::translate("ChatTimestamp", "just now");
    const qint64 minutes = age / 60000;
    if (minutes < 60)
        return minutes == 1 ? QCoreApplication::translate("ChatTimestamp", "1 minute ago")
                            : QCoreApplication::translate("ChatTimestamp", "%n minutes ago", nullptr, int(minutes));
    const qint64 hours = minutes / 60;
    if (hours < 24)
        return hours == 1 ? QCoreApplication::translate("ChatTimestamp", "1 hour ago")
                          : QCoreApplication::translate("ChatTimestamp", "%n hours ago", nullptr, int(hours));
    const qint64 days = hours / 24;
    if (days < 30)
        return days == 1 ? QCoreApplication::translate("ChatTimestamp", "1 day ago")
                         : QCoreApplication::translate("ChatTimestamp", "%n days ago", nullptr, int(days));
    const qint64 months = days / 30;
    if (days < 365)
        return months == 1 ? QCoreApplication::translate("ChatTimestamp", "1 month ago")
                           : QCoreApplication::translate("ChatTimestamp", "%n months ago", nullptr, int(months));
    const qint64 years = days / 365;
    const int yearsForPlural = int(qMin<qint64>(years, 1000000));
    return years == 1 ? QCoreApplication::translate("ChatTimestamp", "1 year ago")
                      : QCoreApplication::translate("ChatTimestamp", "%n years ago", nullptr,
                                                    yearsForPlural);
}

inline QString format(const QDateTime &when, Mode mode, const QDateTime &now = QDateTime::currentDateTime())
{
    if (!when.isValid())
        return {};
    return mode == Mode::Clock ? when.toLocalTime().toString(QStringLiteral("HH:mm")) : relative(when, now);
}

// An empty format delegates to the user preference. Explicit formats use
// Qt date/time tokens; {relative} can also appear anywhere in the format.
inline bool usesRelative(const QString &pattern, Mode fallback)
{
    return pattern.isEmpty() ? fallback == Mode::Relative : pattern.contains(QLatin1String("{relative}"));
}

inline QString formatPattern(const QDateTime &when, const QString &pattern, Mode fallback,
                             const QDateTime &now = QDateTime::currentDateTime())
{
    if (!when.isValid())
        return {};
    if (pattern.isEmpty())
        return format(when, fallback, now);
    if (!pattern.contains(QLatin1String("{relative}")))
        return when.toLocalTime().toString(pattern);
    // Keep the Qt date formatter responsible for its quotes and tokens while
    // replacing a special token that it does not recognize.
    constexpr QChar marker(0xe000);
    QString qtPattern = pattern;
    qtPattern.replace(QLatin1String("{relative}"), QString(marker));
    return when.toLocalTime().toString(qtPattern).replace(marker, relative(when, now));
}

// No polling: wake up only when the currently displayed relative label changes.
inline QString full(const QDateTime &when)
{
    if (!when.isValid())
        return {};
    const auto local = when.toLocalTime();
    return QLocale().toString(local.date(), QLocale::LongFormat) + QStringLiteral(" ")
        + local.toString(QStringLiteral("HH:mm:ss t"));
}

inline qint64 nextChangeMs(const QDateTime &when, const QDateTime &now = QDateTime::currentDateTime())
{
    if (!when.isValid())
        return 0;
    const qint64 age = when.msecsTo(now);
    if (age < 0)
        return -age + 50;
    constexpr qint64 minute = 60000;
    constexpr qint64 hour   = 60 * minute;
    constexpr qint64 day    = 24 * hour;
    qint64 unit = minute;
    if (age >= 365 * day)
        unit = 365 * day;
    else if (age >= 30 * day)
        unit = 30 * day;
    else if (age >= day)
        unit = day;
    else if (age >= hour)
        unit = hour;
    return unit - age % unit + 50;
}
}

#endif // CHATTIMESTAMP_H
