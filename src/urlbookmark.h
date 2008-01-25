/*
 * urlbookmark.h
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

#ifndef URLBOOKMARK_H
#define URLBOOKMARK_H

#include <QString>

class QDomElement;
class QDomDocument;

class URLBookmark
{
public:
	URLBookmark(const QString& name, const QString& url);
	URLBookmark(const QDomElement&);
	
	const QString& name() const;
	const QString& url() const;
	bool isNull() const;

	void fromXml(const QDomElement&);
	QDomElement toXml(QDomDocument&) const;

	bool operator==(const URLBookmark& other) const;

private:
	QString name_;
	QString url_;
};

#endif
