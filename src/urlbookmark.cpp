/*
 * urlbookmark.cpp
 * Copyright (C) 2006  Remko Troncon
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <QDomDocument>
#include <QDomElement>

#include "urlbookmark.h"

URLBookmark::URLBookmark(const QString& name, const QString& url) : name_(name), url_(url) 
{ 
}

URLBookmark::URLBookmark(const QDomElement& el)
{
	fromXml(el);
}

const QString& URLBookmark::name() const 
{ 
	return name_; 
}

const QString& URLBookmark::url() const 
{ 
	return url_; 
}

bool URLBookmark::isNull() const
{
	return name_.isEmpty() && url_.isEmpty();
}

void URLBookmark::fromXml(const QDomElement& e)
{
	name_ = e.attribute("name");
	url_ = e.attribute("url");
}

QDomElement URLBookmark::toXml(QDomDocument& doc) const
{
	QDomElement e = doc.createElement("url");
	if (!name_.isEmpty())
		e.setAttribute("name",name_);
	if (!url_.isEmpty())
		e.setAttribute("url",url_);
	return e;
}

bool URLBookmark::operator==(const URLBookmark & other) const
{
	return
	    name_ == other.name_ &&
	    url_  == other.url_;
}
