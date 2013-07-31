#ifndef TEXTUTIL_H
#define TEXTUTIL_H

class QString;

namespace TextUtil
{
	QString escape(const QString &plain);
	QString unescape(const QString& escaped);

	QString quote(const QString &, int width=60, bool quoteEmpty=false);
	QString plain2rich(const QString &);
	QString rich2plain(const QString &, bool collapseSpaces = true);
	QString resolveEntities(const QString &);
	QString linkify(const QString &);
	QString legacyFormat(const QString &);
	QString emoticonify(const QString &in);
	QString img2title(const QString &in);

	QString prepareMessageText(const QString& text, bool isEmote=false, bool isHtml=false);
}

#endif
