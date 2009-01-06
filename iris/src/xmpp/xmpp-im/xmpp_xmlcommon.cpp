/*
 * xmlcommon.cpp - helper functions for dealing with XML
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

#include "xmpp_xmlcommon.h"
#include "xmpp_stanza.h"

#include <qstring.h>
#include <qdom.h>
#include <qdatetime.h>
#include <qsize.h>
#include <qrect.h>
#include <qstringlist.h>
#include <qcolor.h>

//----------------------------------------------------------------------------
// XDomNodeList
//----------------------------------------------------------------------------
XDomNodeList::XDomNodeList()
{
}

XDomNodeList::XDomNodeList(const XDomNodeList &from) :
	list(from.list)
{
}

XDomNodeList::XDomNodeList(const QDomNodeList &from)
{
	for(int n = 0; n < from.count(); ++n)
		list += from.item(n);
}

XDomNodeList::~XDomNodeList()
{
}

XDomNodeList & XDomNodeList::operator=(const XDomNodeList &from)
{
	list = from.list;
	return *this;
}

bool XDomNodeList::isEmpty() const
{
	return list.isEmpty();
}

QDomNode XDomNodeList::item(int index) const
{
	return list.value(index);
}

uint XDomNodeList::length() const
{
	return (uint)list.count();
}

bool XDomNodeList::operator==(const XDomNodeList &a) const
{
	return (list == a.list);
}

void XDomNodeList::append(const QDomNode &i)
{
	list += i;
}


QDateTime stamp2TS(const QString &ts)
{
	if(ts.length() != 17)
		return QDateTime();

	int year  = ts.mid(0,4).toInt();
	int month = ts.mid(4,2).toInt();
	int day   = ts.mid(6,2).toInt();

	int hour  = ts.mid(9,2).toInt();
	int min   = ts.mid(12,2).toInt();
	int sec   = ts.mid(15,2).toInt();

	QDate xd;
	xd.setYMD(year, month, day);
	if(!xd.isValid())
		return QDateTime();

	QTime xt;
	xt.setHMS(hour, min, sec);
	if(!xt.isValid())
		return QDateTime();

	return QDateTime(xd, xt);
}

bool stamp2TS(const QString &ts, QDateTime *d)
{
	QDateTime dateTime = stamp2TS(ts);
	if (dateTime.isNull())
		return false;

	*d = dateTime;

	return true;
}

QString TS2stamp(const QDateTime &d)
{
	QString str;

	str.sprintf("%04d%02d%02dT%02d:%02d:%02d",
		d.date().year(),
		d.date().month(),
		d.date().day(),
		d.time().hour(),
		d.time().minute(),
		d.time().second());

	return str;
}

QDomElement textTag(QDomDocument *doc, const QString &name, const QString &content)
{
	QDomElement tag = doc->createElement(name);
	QDomText text = doc->createTextNode(content);
	tag.appendChild(text);

	return tag;
}

QString tagContent(const QDomElement &e)
{
	// look for some tag content
	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomText i = n.toText();
		if(i.isNull())
			continue;
		return i.data();
	}

	return "";
}


/**
 * \brief find an direct child element by name
 * \param e parent element
 * \param name name of element to find
 * \param found (optional/out) found?
 * \return the element (or a null QDomElemnt if not found)
 */
QDomElement findSubTag(const QDomElement &e, const QString &name, bool *found)
{
	if(found)
		*found = false;

	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull())
			continue;
		if(i.tagName() == name) {
			if(found)
				*found = true;
			return i;
		}
	}

	QDomElement tmp;
	return tmp;
}


/**
 * \brief obtain direct child elements of a certain kind.  unlike
 *        elementsByTagNameNS, this function does not descend beyond the first
 *        level of children.
 * \param e parent element
 * \param nsURI namespace of the elements to find
 * \param localName local name of the elements to find
 * \return the node list of found elements (empty list if none are found)
 */
XDomNodeList childElementsByTagNameNS(const QDomElement &e, const QString &nsURI, const QString &localName)
{
	XDomNodeList out;
	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		if(!n.isElement())
			continue;
		QDomElement i = n.toElement();
		if(i.namespaceURI() == nsURI && i.localName() == localName)
			out.append(i);
	}
	return out;
}


/**
 * \brief create a new IQ stanza
 * \param doc 
 * \param type 
 * \param to destination jid
 * \param id stanza id
 * \return the created stanza
*/
QDomElement createIQ(QDomDocument *doc, const QString &type, const QString &to, const QString &id)
{
	QDomElement iq = doc->createElement("iq");
	if(!type.isEmpty())
		iq.setAttribute("type", type);
	if(!to.isEmpty())
		iq.setAttribute("to", to);
	if(!id.isEmpty())
		iq.setAttribute("id", id);

	return iq;
}

/** \brief returns direct child element named "query"
 * \return the element (or a null QDomElemnt if not found)
*/
QDomElement queryTag(const QDomElement &e)
{
	bool found;
	QDomElement q = findSubTag(e, "query", &found);
	return q;
}

QString queryNS(const QDomElement &e)
{
	bool found;
	QDomElement q = findSubTag(e, "query", &found);
	if(found)
		return q.attribute("xmlns");

	return "";
}

/**
	\brief Extracts the error code and description from the stanza element.

	This function finds the error element in the given stanza element \a e.

	You need to provide the base namespace of the stream to which this stanza belongs to
	(probably by using stream.baseNS() function).

	The error description is either error text extracted from XML
	or - if no text is found - the error name and description (separated by '\n') taken from RFC-3920
	or - if the error is not defined in the RFC - the empty string.

	Note: This function uses the Stanza::Error class,
	so it may guess missing values as defined in JEP-0086.

	\param e	the element representing stanza
	\param baseNS	the base namespace of the stream
	\param code	if not NULL, will be filled with numeric error code
	\param str	if not NULL, will be filled with human readable error description
*/

void getErrorFromElement(const QDomElement &e, const QString &baseNS, int *code, QString *str)
{
	bool found;
	QDomElement tag = findSubTag(e, "error", &found);
	if(!found)
		return;

	XMPP::Stanza::Error err;
	err.fromXml(tag, baseNS);

	if(code)
		*code = err.code();
	if(str) {
		QPair<QString, QString> desc = err.description();
		if (err.text.isEmpty())
			*str = desc.first + ".\n" + desc.second;
		else
			*str = desc.first + ".\n" + desc.second + "\n" + err.text;
	}

}

QDomElement addCorrectNS(const QDomElement &e)
{
	int x;

	// grab child nodes
	/*QDomDocumentFragment frag = e.ownerDocument().createDocumentFragment();
	QDomNodeList nl = e.childNodes();
	for(x = 0; x < nl.count(); ++x)
		frag.appendChild(nl.item(x).cloneNode());*/

	// find closest xmlns
	QDomNode n = e;
	while(!n.isNull() && !n.toElement().hasAttribute("xmlns"))
		n = n.parentNode();
	QString ns;
	if(n.isNull() || !n.toElement().hasAttribute("xmlns"))
		ns = "jabber:client";
	else
		ns = n.toElement().attribute("xmlns");

	// make a new node
	QDomElement i = e.ownerDocument().createElementNS(ns, e.tagName());

	// copy attributes
	QDomNamedNodeMap al = e.attributes();
	for(x = 0; x < al.count(); ++x) {
		QDomAttr a = al.item(x).toAttr();
		if(a.name() != "xmlns")
			i.setAttributeNodeNS(a.cloneNode().toAttr());
	}

	// copy children
	QDomNodeList nl = e.childNodes();
	for(x = 0; x < nl.count(); ++x) {
		QDomNode n = nl.item(x);
		if(n.isElement())
			i.appendChild(addCorrectNS(n.toElement()));
		else
			i.appendChild(n.cloneNode());
	}

	//i.appendChild(frag);
	return i;
}

//----------------------------------------------------------------------------
// XMLHelper
//----------------------------------------------------------------------------

namespace XMLHelper {

QDomElement emptyTag(QDomDocument *doc, const QString &name)
{
	QDomElement tag = doc->createElement(name);

	return tag;
}

bool hasSubTag(const QDomElement &e, const QString &name)
{
	bool found;
	findSubTag(e, name, &found);
	return found;
}

QString subTagText(const QDomElement &e, const QString &name)
{
	bool found;
	QDomElement i = findSubTag(e, name, &found);
	if ( found )
		return i.text();
	return QString::null;
}

QDomElement textTag(QDomDocument &doc, const QString &name, const QString &content)
{
	QDomElement tag = doc.createElement(name);
	QDomText text = doc.createTextNode(content);
	tag.appendChild(text);

	return tag;
}

QDomElement textTag(QDomDocument &doc, const QString &name, int content)
{
	QDomElement tag = doc.createElement(name);
	QDomText text = doc.createTextNode(QString::number(content));
	tag.appendChild(text);

	return tag;
}

QDomElement textTag(QDomDocument &doc, const QString &name, bool content)
{
	QDomElement tag = doc.createElement(name);
	QDomText text = doc.createTextNode(content ? "true" : "false");
	tag.appendChild(text);

	return tag;
}

QDomElement textTag(QDomDocument &doc, const QString &name, QSize &s)
{
	QString str;
	str.sprintf("%d,%d", s.width(), s.height());

	QDomElement tag = doc.createElement(name);
	QDomText text = doc.createTextNode(str);
	tag.appendChild(text);

	return tag;
}

QDomElement textTag(QDomDocument &doc, const QString &name, QRect &r)
{
	QString str;
	str.sprintf("%d,%d,%d,%d", r.x(), r.y(), r.width(), r.height());

	QDomElement tag = doc.createElement(name);
	QDomText text = doc.createTextNode(str);
	tag.appendChild(text);

	return tag;
}

QDomElement stringListToXml(QDomDocument &doc, const QString &name, const QStringList &l)
{
	QDomElement tag = doc.createElement(name);
	for(QStringList::ConstIterator it = l.begin(); it != l.end(); ++it)
		tag.appendChild(textTag(doc, "item", *it));

	return tag;
}

/*QString tagContent(const QDomElement &e)
{
	// look for some tag content
	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomText i = n.toText();
		if(i.isNull())
			continue;
		return i.data();
	}

	return "";
}*/

/*QDomElement findSubTag(const QDomElement &e, const QString &name, bool *found)
{
	if(found)
		*found = FALSE;

	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull())
			continue;
		if(i.tagName() == name) {
			if(found)
				*found = TRUE;
			return i;
		}
	}

	QDomElement tmp;
	return tmp;
}*/

void readEntry(const QDomElement &e, const QString &name, QString *v)
{
	bool found = FALSE;
	QDomElement tag = findSubTag(e, name, &found);
	if(!found)
		return;
	*v = tagContent(tag);
}

void readNumEntry(const QDomElement &e, const QString &name, int *v)
{
	bool found = FALSE;
	QDomElement tag = findSubTag(e, name, &found);
	if(!found)
		return;
	*v = tagContent(tag).toInt();
}

void readBoolEntry(const QDomElement &e, const QString &name, bool *v)
{
	bool found = FALSE;
	QDomElement tag = findSubTag(e, name, &found);
	if(!found)
		return;
	*v = (tagContent(tag) == "true") ? TRUE: FALSE;
}

void readSizeEntry(const QDomElement &e, const QString &name, QSize *v)
{
	bool found = FALSE;
	QDomElement tag = findSubTag(e, name, &found);
	if(!found)
		return;
	QStringList list = tagContent(tag).split(',');
	if(list.count() != 2)
		return;
	QSize s;
	s.setWidth(list[0].toInt());
	s.setHeight(list[1].toInt());
	*v = s;
}

void readRectEntry(const QDomElement &e, const QString &name, QRect *v)
{
	bool found = FALSE;
	QDomElement tag = findSubTag(e, name, &found);
	if(!found)
		return;
	QStringList list = tagContent(tag).split(',');
	if(list.count() != 4)
		return;
	QRect r;
	r.setX(list[0].toInt());
	r.setY(list[1].toInt());
	r.setWidth(list[2].toInt());
	r.setHeight(list[3].toInt());
	*v = r;
}

void readColorEntry(const QDomElement &e, const QString &name, QColor *v)
{
	bool found = FALSE;
	QDomElement tag = findSubTag(e, name, &found);
	if(!found)
		return;
	QColor c;
	c.setNamedColor(tagContent(tag));
	if(c.isValid())
		*v = c;
}

void xmlToStringList(const QDomElement &e, const QString &name, QStringList *v)
{
	bool found = false;
	QDomElement tag = findSubTag(e, name, &found);
	if(!found)
		return;
	QStringList list;
	for(QDomNode n = tag.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull())
			continue;
		if(i.tagName() == "item")
			list += tagContent(i);
	}
	*v = list;
}

void setBoolAttribute(QDomElement e, const QString &name, bool b)
{
	e.setAttribute(name, b ? "true" : "false");
}

void readBoolAttribute(QDomElement e, const QString &name, bool *v)
{
	if(e.hasAttribute(name)) {
		QString s = e.attribute(name);
		*v = (s == "true") ? TRUE: FALSE;
	}
}

};

