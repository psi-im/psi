/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "EmojiSupport.h"

#include "emojiregistry.h"

#include <QFont>
#include <QFontInfo>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
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

void ClassicSmileyConversion::clear()
{
    alias_.clear();
    emoji_.clear();
    endPosition_ = -1;
}

void ClassicSmileyConversion::replaceTyped(QTextEdit *editor, int previousPosition)
{
    if (!editor || previousPosition < 0)
        return;

    QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection() || cursor.position() != previousPosition + 1)
        return;

    // All aliases in classicSmileys are at most three characters long.
    // Inspect those and one preceding boundary character, not the entire
    // paragraph (which can be arbitrarily long).
    const int from = qMax(cursor.block().position(), cursor.position() - 4);
    QString suffix;
    suffix.reserve(cursor.position() - from);
    for (int i = from; i < cursor.position(); ++i)
        suffix.append(editor->document()->characterAt(i));

    const auto [length, emoji] = completedClassicSmiley(QStringView(suffix));
    if (length == 0)
        return;

    alias_ = suffix.right(length);
    emoji_ = emoji;

    cursor.beginEditBlock();
    cursor.setPosition(cursor.position() - length, QTextCursor::KeepAnchor);
    cursor.insertText(emoji);
    cursor.endEditBlock();
    editor->setTextCursor(cursor);
    endPosition_ = cursor.position();
}

bool ClassicSmileyConversion::restoreOnBackspace(QTextEdit *editor)
{
    if (!editor || endPosition_ < 0)
        return false;

    QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection() || cursor.position() != endPosition_)
        return false;

    cursor.setPosition(endPosition_ - emoji_.size(), QTextCursor::KeepAnchor);
    if (cursor.selectedText() != emoji_)
        return false;

    const QString alias = alias_;
    clear();
    cursor.beginEditBlock();
    cursor.insertText(alias);
    cursor.endEditBlock();
    editor->setTextCursor(cursor);
    return true;
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
