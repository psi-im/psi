#include "desktoputil.h"

#include <QDesktopServices>
#include <QUrl>

/**
 *	\brief Opens URL using OS default handler
 *	\param url the url to open
 *
 *	\a url may be either percent encoded or not.
 *	If it contains only ASCII characters, it is decoded,
 *	else it is converted to QUrl in QUrl::TolerantMode mode.
 *	Resulting QUrl object is passed to QDesktopServices::openUrl().
 *
 *	\sa QDesktopServices::openUrl()
 */
bool DesktopUtil::openUrl(const QString &url)
{
	QByteArray ascii = url.toAscii();
	if (ascii == url)
		return QDesktopServices::openUrl(QUrl::fromEncoded(ascii));
	else
		return QDesktopServices::openUrl(QUrl(url, QUrl::TolerantMode));
}
