/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PSI_EMOJI_SUPPORT_H
#define PSI_EMOJI_SUPPORT_H

#include "emojiregistry.h"
#include <QFont>
#include <QFontInfo>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QTextCharFormat>
#include <QtGlobal>
#include <utility>

namespace EmojiSupport {

inline QString platformFamily()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("Segoe UI Emoji");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("Apple Color Emoji");
#else
    return QStringLiteral("Noto Color Emoji");
#endif
}

// Qt 6.9+ detects emoji sequences and selects color emoji fonts itself.
// Specifying a family there bypasses that selection, so only do it for Qt 5/6.8.
inline void applyLegacyFamily(QFont &font)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
    font.setFamily(platformFamily());
#else
    Q_UNUSED(font);
#endif
}

inline void applyLegacyFamily(QTextCharFormat &format)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
    format.setFontFamilies({ platformFamily() });
#else
    Q_UNUSED(format);
#endif
}

// An editor highlight should affect only the font size on Qt 6.9+.
// A QTextCharFormat with just FontPointSize/FontPixelSize leaves the ordinary
// text family unspecified so Qt's emoji segmenter can select the color font.
inline QTextCharFormat editorFormat(const QFont &base)
{
    QTextCharFormat format;
    if (base.pointSizeF() > 0)
        format.setFontPointSize(base.pointSizeF() * 1.3);
    else if (base.pixelSize() > 0)
        format.setProperty(QTextFormat::FontPixelSize, qRound(base.pixelSize() * 1.3));
    else
        format.setFontPointSize(QFontInfo(base).pointSizeF() * 1.3);
    applyLegacyFamily(format);
    return format;
}

struct ClassicSmiley {
    const char *text;
    const char *emoji;
};

// Small, deliberately conservative map. A smiley is replaced only when it
// occupies a whole whitespace-delimited token and the user types a space.
inline constexpr ClassicSmiley classicSmileys[] = {
    { ":-)", "🙂" }, { ":)", "🙂" }, { ":-(", "🙁" }, { ":(", "🙁" },
    { ":-D", "😃" }, { ":D", "😃" }, { ";-)", "😉" }, { ";)", "😉" },
    { ":-P", "😛" }, { ":P", "😛" }, { ":-p", "😛" }, { ":p", "😛" },
    { ":-O", "😮" }, { ":O", "😮" }, { ":'(", "😢" }, { "<3", "❤️" },
};

// Inspect only the end of the current line; no scanning over the document on
// each keystroke. Return the alias length along with the replacement.
inline std::pair<int, QString> completedClassicSmiley(QStringView beforeCursor)
{
    for (const auto &entry : classicSmileys) {
        const QLatin1String alias(entry.text);
        if (!beforeCursor.endsWith(alias))
            continue;
        const int start = beforeCursor.size() - alias.size();
        if (start == 0 || beforeCursor.at(start - 1).isSpace())
            return { alias.size(), QString::fromUtf8(entry.emoji) };
    }
    return { 0, QString() };
}

inline QString description(const QString &emoji)
{
    for (const auto &entry : EmojiRegistry::instance()) {
        if (entry.code == emoji)
            return entry.name;
    }
    for (const auto &entry : classicSmileys) {
        if (QString::fromUtf8(entry.emoji) == emoji)
            return QLatin1String(entry.text);
    }
    // Use a textual Unicode identifier rather than showing an undecodable
    // emoji in its own tooltip when the registry lacks a description.
    QStringList points;
    for (const auto point : emoji.toUcs4())
        points.append(QStringLiteral("U+") + QString::number(point, 16).toUpper());
    return points.join(QLatin1Char(' '));
}

} // namespace EmojiSupport

#endif // PSI_EMOJI_SUPPORT_H
