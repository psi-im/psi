/*
 * contactlistitem.h - base class for contact list items
 * Copyright (C) 2008-2010  Yandex LLC (Michail Pishchagin)
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

#ifndef CONTACTLISTITEM_H
#define CONTACTLISTITEM_H

#include <QObject>
#include <QString>

#include "contactlistmodel.h"

class ContactListItemMenu;

class ContactListItem : public QObject
{
public:
	ContactListItem(QObject* parent = 0);
	virtual ~ContactListItem();

	virtual ContactListModel::Type type() const = 0;

	virtual const QString& displayName() const;
	virtual const QString& name() const = 0;
	virtual void setName(const QString& name) = 0;
	virtual QString comparisonName() const;

	virtual bool isEditable() const;
	virtual bool isDragEnabled() const;
	virtual bool isRemovable() const;

	virtual bool isExpandable() const;
	virtual bool expanded() const;
	virtual void setExpanded(bool expanded);

	virtual ContactListItemMenu* contextMenu(ContactListModel* model);

	virtual bool isFixedSize() const;

	virtual bool compare(const ContactListItem* other) const;

	virtual bool editing() const;
	virtual void setEditing(bool editing);

private:
	bool editing_;
};

#endif
