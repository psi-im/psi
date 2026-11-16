/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "EmojiSupport.h"

#include "emojiregistry.h"

#include <QFont>
#include <QFontInfo>
#include <QStringList>
#include <QTextCharFormat>
#include <QtGlobal>

namespace {

struct ClassicSmiley {
    const char *text;
    const char *emoji;
};

// Keep this deliberately small: replacements apply only to whole tokens
// terminated by a typed space, never to punctuation embedded in other text.
constexpr ClassicSmiley classicSmileys[] = {
    { ":-)", "🙂" }, { ":)", "🙂" }, { ":-(", "🙁" }, { ":(", "🙁" },
    { ":-D", "😃" }, { ":D", "😃" }, { ";-)", "😉" }, { ";)", "😉" },
    { ":-P", "😛" }, { ":P", "😛" }, { ":-p", "😛" }, { ":p", "😛" },
    { ":-O", "😮" }, { ":O", "😮" }, { ":'(", "😢" }, { "<3", "❤️" },
};

} // namespace

namespace EmojiSupport {

QString platformFamily()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("Segoe UI Emoji");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("Apple Color Emoji");
#else
    return QStringLiteral("Noto Color Emoji");
#endif
}

void applyLegacyFamily(QFont &font)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
    font.setFamily(platformFamily());
#else
    Q_UNUSED(font);
#endif
}

void applyLegacyFamily(QTextCharFormat &format)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
    format.setFontFamilies({ platformFamily() });
#else
    Q_UNUSED(format);
#endif
}

QTextCharFormat editorFormat(const QFont &base)
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

std::pair<int, QString> completedClassicSmiley(QStringView beforeCursor)
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

QString description(const QString &emoji)
{
    for (const auto &entry : EmojiRegistry::instance()) {
        if (entry.code == emoji)
            return entry.name;
    }
    for (const auto &entry : classicSmileys) {
        if (QString::fromUtf8(entry.emoji) == emoji)
            return QLatin1String(entry.text);
    }

    // A Unicode identifier is preferable to repeating an undecodable emoji
    // in its own tooltip when the registry has no description.
    QStringList points;
    for (const auto point : emoji.toUcs4())
        points.append(QStringLiteral("U+") + QString::number(point, 16).toUpper());
    return points.join(QLatin1Char(' '));
}

} // namespace EmojiSupport
