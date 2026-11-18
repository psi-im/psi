/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PSI_EMOJI_SUPPORT_H
#define PSI_EMOJI_SUPPORT_H

#include <QString>
#include <QStringView>
#include <utility>

class QFont;
class QTextCharFormat;
class QTextEdit;

namespace EmojiSupport {

// Qt 6.9+ can select color emoji fonts itself. On older Qt versions these
// helpers provide the platform-specific family; otherwise they leave it alone.
QString platformFamily();
void applyLegacyFamily(QFont &font);
void applyLegacyFamily(QTextCharFormat &format);

// A display-only format for composer emoji, sized at 130% of the input font.
QTextCharFormat editorFormat(const QFont &base);

// Recognize a complete classic smiley immediately before the cursor.
std::pair<int, QString> completedClassicSmiley(QStringView beforeCursor);

// The composer converts only one freshly typed character at a time. Keep the
// latest substitution so Backspace can restore the original alias, without
// adding hidden formatting to the document or scanning the whole message.
class ClassicSmileyConversion {
public:
    void clear();
    void replaceTyped(QTextEdit *editor, int previousPosition);
    bool restoreOnBackspace(QTextEdit *editor);

private:
    QString alias_;
    QString emoji_;
    int     endPosition_ = -1;
};

// Unicode emoji description, with textual fallbacks if none is registered.
QString description(const QString &emoji);

} // namespace EmojiSupport

#endif // PSI_EMOJI_SUPPORT_H
