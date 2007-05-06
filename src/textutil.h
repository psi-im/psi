#ifndef TEXTUTIL_H
#define TEXTUTIL_H

class QString;

namespace TextUtil 
{
	QString quote(const QString &, int width=60, bool quoteEmpty=false);
	QString plain2rich(const QString &);
	QString rich2plain(const QString &);
	QString resolveEntities(const QString &);
	QString linkify(const QString &);
	QString legacyFormat(const QString &);
	QString emoticonify(const QString &in);
};

#endif
