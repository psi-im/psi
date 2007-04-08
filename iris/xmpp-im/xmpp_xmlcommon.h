/*
 * xmlcommon.h - helper functions for dealing with XML
 * Copyright (C) 2001, 2002  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef JABBER_XMLCOMMON_H
#define JABBER_XMLCOMMON_H

#include <qdom.h>

class QDateTime;
class QRect;
class QSize;
class QColor;
class QStringList;

bool stamp2TS(const QString &ts, QDateTime *d);
QString TS2stamp(const QDateTime &d);
QDomElement textTag(QDomDocument *doc, const QString &name, const QString &content);
QString tagContent(const QDomElement &e);
QDomElement findSubTag(const QDomElement &e, const QString &name, bool *found);
QDomElement createIQ(QDomDocument *doc, const QString &type, const QString &to, const QString &id);
QDomElement queryTag(const QDomElement &e);
QString queryNS(const QDomElement &e);
void getErrorFromElement(const QDomElement &e, const QString &baseNS, int *code, QString *str);
QDomElement addCorrectNS(const QDomElement &e);

namespace XMLHelper {
	//QDomElement findSubTag(const QDomElement &e, const QString &name, bool *found);
	bool hasSubTag(const QDomElement &e, const QString &name);

	QDomElement emptyTag(QDomDocument *doc, const QString &name);
	QString subTagText(const QDomElement &e, const QString &name);

	QDomElement textTag(QDomDocument &doc, const QString &name, const QString &content);
	QDomElement textTag(QDomDocument &doc, const QString &name, int content);
	QDomElement textTag(QDomDocument &doc, const QString &name, bool content);
	QDomElement textTag(QDomDocument &doc, const QString &name, QSize &s);
	QDomElement textTag(QDomDocument &doc, const QString &name, QRect &r);
	QDomElement stringListToXml(QDomDocument &doc, const QString &name, const QStringList &l);

	void readEntry(const QDomElement &e, const QString &name, QString *v);
	void readNumEntry(const QDomElement &e, const QString &name, int *v);
	void readBoolEntry(const QDomElement &e, const QString &name, bool *v);
	void readSizeEntry(const QDomElement &e, const QString &name, QSize *v);
	void readRectEntry(const QDomElement &e, const QString &name, QRect *v);
	void readColorEntry(const QDomElement &e, const QString &name, QColor *v);

	void xmlToStringList(const QDomElement &e, const QString &name, QStringList *v);

	void setBoolAttribute(QDomElement e, const QString &name, bool b);
	void readBoolAttribute(QDomElement e, const QString &name, bool *v);

	//QString tagContent(const QDomElement &e); // obsolete;
};

#endif
