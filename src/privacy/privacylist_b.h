/*
 * privacylist.h
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

#ifndef PRIVACYLIST_H
#define PRIVACYLIST_H

#include <QList>
#include <QString>

#include "privacylistitem.h"

class QDomElement;
class QDomDocument;

class PrivacyList
{
public:
	PrivacyList(const QString& name, const QList<PrivacyListItem>& items = QList<PrivacyListItem>());
	PrivacyList(const QDomElement&);

	const QString& name() const { return name_; }
	void setName(const QString& name) { name_ = name; }
	bool isEmpty() const { return items_.isEmpty(); }
	void clear() { items_.clear(); }
	const QList<PrivacyListItem>& items() const { return items_; }
	const PrivacyListItem& item(int index) const { return items_.at(index); }
	void removeItem(int index) { items_.removeAt(index); }
	void insertItem(int index, const PrivacyListItem& item);
	void appendItem(const PrivacyListItem& item);
	bool moveItemUp(int index);
	bool moveItemDown(int index);
	bool onlyBlockItems() const;
	void updateItem(int index, const PrivacyListItem& item);
	QDomElement toXml(QDomDocument&) const;
	void fromXml(const QDomElement& e);
	QString toString() const;

private:
	void reNumber();
	QString name_;
	QList<PrivacyListItem> items_;
};

#endif
