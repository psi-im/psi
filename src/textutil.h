#ifndef TEXTUTIL_H
#define TEXTUTIL_H

#include <QtGlobal>

class QString;

namespace TextUtil {

QString escape(const QString &plain);
QString unescape(const QString &escaped);

QString quote(const QString &, int width = 60, bool quoteEmpty = false);
QString plain2rich(const QString &);
QString rich2plain(const QString &, bool collapseSpaces = true);
QString resolveEntities(const QString &);
QString linkify(const QString &);
QString legacyFormat(const QString &);
QString emoticonify(const QString &in);
QString img2title(const QString &in);

QString prepareMessageText(const QString &text, bool isEmote = false, bool isHtml = false);

QString sizeUnit(qlonglong n, qlonglong *div = nullptr);
QString roundedNumber(qlonglong n, qlonglong div);

} // namespace TextUtil

#endif // TEXTUTIL_H
