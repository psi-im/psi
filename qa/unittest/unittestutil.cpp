#include <QDebug>

#include "unittestutil.h"

QDomElement UnitTestUtil::createElement(const QString& text)
{
	QDomDocument doc;
	if (!doc.setContent(text)) {
		qWarning() << "Unable to parse element: " << text;
		return QDomElement();
	}
	return doc.documentElement();
}
