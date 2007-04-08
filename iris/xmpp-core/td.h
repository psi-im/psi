#ifndef TESTDEBUG_H
#define TESTDEBUG_H

#include <qdom.h>

class TD
{
public:
	TD();
	~TD();

	static void msg(const QString &);
	static void outgoingTag(const QString &);
	static void incomingTag(const QString &);
	static void outgoingXml(const QDomElement &);
	static void incomingXml(const QDomElement &);
};

#endif

