/****************************************************************************
** varlist.cpp - class for handling a list of string vars
** Copyright (C) 2001, 2002  Justin Karneges
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,USA.
**
****************************************************************************/

#include <QList>
#include <QStringList>
#include <QDomElement>
#include <QDomDocument>

#include "varlist.h"


VarList::VarList() :QList<VarListItem>()
{
}

QStringList VarList::varsToStringList()
{
	QStringList list;
	for(VarList::Iterator it = begin(); it != end(); ++it)
		list.append((*it).key());
	return list;
}

QDomElement VarList::toXml(QDomDocument &doc, const QString &tagName)
{
	QDomElement base = doc.createElement(tagName);
	for(VarList::Iterator it = begin(); it != end(); ++it) {
		QDomElement tag = doc.createElement("item");
		tag.setAttribute("name", (*it).key());
		QDomText text = doc.createTextNode((*it).data());
		tag.appendChild(text);
		base.appendChild(tag);
	}
	return base;
}

void VarList::fromXml(const QDomElement &e)
{
	clear();

	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull())
			continue;
		if(i.tagName() == "item") {
			QString var, val;
			var = i.attribute("name");
			QDomText t = i.firstChild().toText();
			if(!t.isNull())
				val = t.data();
			set(var, val);
		}
	}
}

void VarList::set(const QString &key, const QString &data)
{
	VarList::Iterator it = findByKey(key);
	if(it == end()) {
		VarListItem v;
		v.setKey(key);
		v.setData(data);
		append(v);
	}
	else {
		(*it).setData(data);
	}
}

void VarList::unset(const QString &key)
{
	VarList::Iterator it = findByKey(key);
	if(it == end())
		return;
	remove(it);
}

const QString & VarList::get(const QString &key)
{
	VarList::Iterator it = findByKey(key);
	return (*it).data();
}

VarList::Iterator VarList::findByKey(const QString &key)
{
	VarList::Iterator it;
	for(it = begin(); it != end(); ++it) {
		if((*it).key() == key)
			break;
	}

	return it;
}

VarList::Iterator VarList::findByNum(int x)
{
	int n = 0;
	VarList::Iterator it;
	for(it = begin(); it != end(); ++it) {
		if(n >= x)
			break;
		++n;
	}

	return it;
}
